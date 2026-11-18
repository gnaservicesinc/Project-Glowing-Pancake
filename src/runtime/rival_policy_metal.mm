#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <MetalPerformanceShadersGraph/MetalPerformanceShadersGraph.h>

#include "rival_policy_internal.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace pgp::runtime {
namespace {
id<MTLDevice> shared_metal_device() noexcept {
    static id<MTLDevice> device=nil;
    static dispatch_once_t once;
    dispatch_once(&once,^{device=MTLCreateSystemDefaultDevice();});
    return device;
}
id<MTLCommandQueue> shared_command_queue(id<MTLDevice> device) noexcept {
    static id<MTLCommandQueue> queue=nil;
    static dispatch_once_t once;
    dispatch_once(&once,^{
        if(device) queue=[device newCommandQueue];
    });
    return queue;
}
template<class Span>
id<MTLBuffer> make_shared_buffer(id<MTLDevice> device,Span values) {
    const auto bytes=static_cast<NSUInteger>(values.size()*sizeof(float));
    id<MTLBuffer> buf=[device newBufferWithBytes:values.data() length:bytes options:MTLResourceStorageModeShared];
    if(!buf) throw std::runtime_error("Failed to allocate shared Metal buffer for rival policy.");
    return buf;
}
MPSGraphTensorData* wrap_buffer(id<MTLBuffer> buf,NSArray<NSNumber*>* shape) {
    MPSGraphTensorData* td=[[MPSGraphTensorData alloc] initWithMTLBuffer:buf shape:shape dataType:MPSDataTypeFloat32];
    if(!td) throw std::runtime_error("Failed to wrap Metal buffer in MPSGraphTensorData.");
    return td;
}
void read_tensor_data(NSDictionary<MPSGraphTensor*,MPSGraphTensorData*>* results,MPSGraphTensor* tensor,std::span<float> dst) {
    MPSGraphTensorData* td=results[tensor];
    if(!td) throw std::runtime_error("MPSGraph training step missed a required output tensor.");
    [[td mpsndarray] readBytes:dst.data() strideBytes:nil];
}
}
bool rival_policy_metal_available() noexcept {
    @autoreleasepool {
        id<MTLDevice> device=shared_metal_device();
        return device!=nil && shared_command_queue(device)!=nil;
    }
}
namespace detail {
RivalPolicyStepMetrics train_rival_policy_step_metal_impl(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    const PreparedPolicyBatch& batch,
    const RivalPolicyTrainingConfig& config) {
    @autoreleasepool {
        id<MTLDevice> device=shared_metal_device();
        id<MTLCommandQueue> queue=shared_command_queue(device);
        if(!device || !queue)
            throw std::runtime_error("Metal GPU device or command queue is unavailable for policy training.");

        const NSUInteger n=static_cast<NSUInteger>(batch.count);
        NSArray<NSNumber*>* const sX=@[@(n),@(static_cast<NSUInteger>(rival_policy_features))];
        NSArray<NSNumber*>* const sOut=@[@(n),@(static_cast<NSUInteger>(rival_policy_actions))];
        NSArray<NSNumber*>* const sW1=@[@(static_cast<NSUInteger>(rival_policy_features)),@(static_cast<NSUInteger>(rival_policy_hidden))];
        NSArray<NSNumber*>* const sB1=@[@1,@(static_cast<NSUInteger>(rival_policy_hidden))];
        NSArray<NSNumber*>* const sW2=@[@(static_cast<NSUInteger>(rival_policy_hidden)),@(static_cast<NSUInteger>(rival_policy_actions))];
        NSArray<NSNumber*>* const sB2=@[@1,@(static_cast<NSUInteger>(rival_policy_actions))];
        NSArray<NSNumber*>* const sSampleW=@[@(n),@1];
        NSArray<NSNumber*>* const sScalar=@[@1,@1];

        MPSGraph* graph=[MPSGraph new];
        MPSGraphTensor* xT=[graph placeholderWithShape:sX dataType:MPSDataTypeFloat32 name:@"x"];
        MPSGraphTensor* maskT=[graph placeholderWithShape:sOut dataType:MPSDataTypeFloat32 name:@"mask"];
        MPSGraphTensor* oneHotT=[graph placeholderWithShape:sOut dataType:MPSDataTypeFloat32 name:@"one_hot"];
        MPSGraphTensor* sampleWT=[graph placeholderWithShape:sSampleW dataType:MPSDataTypeFloat32 name:@"sample_w"];

        MPSGraphTensor* w1T=[graph placeholderWithShape:sW1 dataType:MPSDataTypeFloat32 name:@"w1"];
        MPSGraphTensor* b1T=[graph placeholderWithShape:sB1 dataType:MPSDataTypeFloat32 name:@"b1"];
        MPSGraphTensor* w2T=[graph placeholderWithShape:sW2 dataType:MPSDataTypeFloat32 name:@"w2"];
        MPSGraphTensor* b2T=[graph placeholderWithShape:sB2 dataType:MPSDataTypeFloat32 name:@"b2"];

        MPSGraphTensor* mw1T=[graph placeholderWithShape:sW1 dataType:MPSDataTypeFloat32 name:@"mw1"];
        MPSGraphTensor* mb1T=[graph placeholderWithShape:sB1 dataType:MPSDataTypeFloat32 name:@"mb1"];
        MPSGraphTensor* mw2T=[graph placeholderWithShape:sW2 dataType:MPSDataTypeFloat32 name:@"mw2"];
        MPSGraphTensor* mb2T=[graph placeholderWithShape:sB2 dataType:MPSDataTypeFloat32 name:@"mb2"];

        MPSGraphTensor* vw1T=[graph placeholderWithShape:sW1 dataType:MPSDataTypeFloat32 name:@"vw1"];
        MPSGraphTensor* vb1T=[graph placeholderWithShape:sB1 dataType:MPSDataTypeFloat32 name:@"vb1"];
        MPSGraphTensor* vw2T=[graph placeholderWithShape:sW2 dataType:MPSDataTypeFloat32 name:@"vw2"];
        MPSGraphTensor* vb2T=[graph placeholderWithShape:sB2 dataType:MPSDataTypeFloat32 name:@"vb2"];

        MPSGraphTensor* mm1=[graph matrixMultiplicationWithPrimaryTensor:xT secondaryTensor:w1T name:@"mm1"];
        MPSGraphTensor* z1=[graph additionWithPrimaryTensor:mm1 secondaryTensor:b1T name:@"z1"];
        MPSGraphTensor* h=[graph reLUWithTensor:z1 name:@"h"];
        MPSGraphTensor* mm2=[graph matrixMultiplicationWithPrimaryTensor:h secondaryTensor:w2T name:@"mm2"];
        MPSGraphTensor* logits=[graph additionWithPrimaryTensor:mm2 secondaryTensor:b2T name:@"logits"];
        MPSGraphTensor* maskedLogits=[graph additionWithPrimaryTensor:logits secondaryTensor:maskT name:@"masked_logits"];

        MPSGraphTensor* ceNone=[graph softMaxCrossEntropyWithSourceTensor:maskedLogits
                                                             labelsTensor:oneHotT
                                                                     axis:-1
                                                            reductionType:MPSGraphLossReductionTypeNone
                                                                     name:@"ce_none"];
        MPSGraphTensor* perSample=[graph reshapeTensor:ceNone withShape:sSampleW name:@"per_sample"];
        MPSGraphTensor* weightedLoss=[graph multiplicationWithPrimaryTensor:sampleWT secondaryTensor:perSample name:@"weighted_loss"];
        MPSGraphTensor* lossT=[graph reductionSumWithTensor:weightedLoss axes:@[@0,@1] name:@"loss"];

        NSDictionary<MPSGraphTensor*,MPSGraphTensor*>* grads=[graph gradientForPrimaryTensor:lossT withTensors:@[w1T,b1T,w2T,b2T] name:@"grads"];
        MPSGraphTensor* gw1=grads[w1T];
        MPSGraphTensor* gb1=grads[b1T];
        MPSGraphTensor* gw2=grads[w2T];
        MPSGraphTensor* gb2=grads[b2T];
        if(!gw1 || !gb1 || !gw2 || !gb2)
            throw std::runtime_error("MPSGraph automatic differentiation failed to return all parameter gradients.");

        MPSGraphTensor* sqW1=[graph reductionSumWithTensor:[graph squareWithTensor:gw1 name:nil] axes:@[@0,@1] name:nil];
        MPSGraphTensor* sqB1=[graph reductionSumWithTensor:[graph squareWithTensor:gb1 name:nil] axes:@[@0,@1] name:nil];
        MPSGraphTensor* sqW2=[graph reductionSumWithTensor:[graph squareWithTensor:gw2 name:nil] axes:@[@0,@1] name:nil];
        MPSGraphTensor* sqB2=[graph reductionSumWithTensor:[graph squareWithTensor:gb2 name:nil] axes:@[@0,@1] name:nil];
        MPSGraphTensor* sqSum=[graph additionWithPrimaryTensor:
            [graph additionWithPrimaryTensor:sqW1 secondaryTensor:sqB1 name:nil]
            secondaryTensor:[graph additionWithPrimaryTensor:sqW2 secondaryTensor:sqB2 name:nil]
            name:@"sq_norm"];
        MPSGraphTensor* gradNormT=[graph squareRootWithTensor:sqSum name:@"grad_norm"];

        MPSGraphTensor* maxGradNormC=[graph constantWithScalar:static_cast<double>(config.max_grad_norm) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* clipDenom=[graph maximumWithPrimaryTensor:gradNormT secondaryTensor:maxGradNormC name:@"clip_denom"];
        MPSGraphTensor* clipScale=[graph divisionWithPrimaryTensor:maxGradNormC secondaryTensor:clipDenom name:@"clip_scale"];

        const std::uint64_t next_step=opt.step+1;
        const double b1_corr_d=1.0-std::pow(static_cast<double>(config.beta1),static_cast<double>(next_step));
        const double b2_corr_d=1.0-std::pow(static_cast<double>(config.beta2),static_cast<double>(next_step));
        const float decay_factor_f=static_cast<float>(1.0-static_cast<double>(config.learning_rate)*static_cast<double>(config.weight_decay));
        MPSGraphTensor* lrC=[graph constantWithScalar:static_cast<double>(config.learning_rate) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* beta1C=[graph constantWithScalar:static_cast<double>(config.beta1) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* oneMinusB1C=[graph constantWithScalar:static_cast<double>(1.0F-config.beta1) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* beta2C=[graph constantWithScalar:static_cast<double>(config.beta2) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* oneMinusB2C=[graph constantWithScalar:static_cast<double>(1.0F-config.beta2) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* b1CorrC=[graph constantWithScalar:static_cast<double>(static_cast<float>(b1_corr_d)) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* b2CorrC=[graph constantWithScalar:static_cast<double>(static_cast<float>(b2_corr_d)) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* epsC=[graph constantWithScalar:static_cast<double>(config.epsilon) shape:sScalar dataType:MPSDataTypeFloat32];
        MPSGraphTensor* decayC=[graph constantWithScalar:static_cast<double>(decay_factor_f) shape:sScalar dataType:MPSDataTypeFloat32];

        const auto add_adamw=[&](MPSGraphTensor* param,MPSGraphTensor* rawGrad,MPSGraphTensor* mIn,MPSGraphTensor* vIn,bool decay,NSString* tag) {
            MPSGraphTensor* g=[graph multiplicationWithPrimaryTensor:rawGrad secondaryTensor:clipScale name:[tag stringByAppendingString:@"_cg"]];
            MPSGraphTensor* nm=[graph additionWithPrimaryTensor:
                [graph multiplicationWithPrimaryTensor:beta1C secondaryTensor:mIn name:nil]
                secondaryTensor:[graph multiplicationWithPrimaryTensor:oneMinusB1C secondaryTensor:g name:nil]
                name:[tag stringByAppendingString:@"_nm"]];
            MPSGraphTensor* g2=[graph squareWithTensor:g name:nil];
            MPSGraphTensor* nv=[graph additionWithPrimaryTensor:
                [graph multiplicationWithPrimaryTensor:beta2C secondaryTensor:vIn name:nil]
                secondaryTensor:[graph multiplicationWithPrimaryTensor:oneMinusB2C secondaryTensor:g2 name:nil]
                name:[tag stringByAppendingString:@"_nv"]];
            MPSGraphTensor* mHat=[graph divisionWithPrimaryTensor:nm secondaryTensor:b1CorrC name:nil];
            MPSGraphTensor* vHat=[graph divisionWithPrimaryTensor:nv secondaryTensor:b2CorrC name:nil];
            MPSGraphTensor* adamDenom=[graph additionWithPrimaryTensor:[graph squareRootWithTensor:vHat name:nil] secondaryTensor:epsC name:nil];
            MPSGraphTensor* stepTerm=[graph multiplicationWithPrimaryTensor:lrC
                secondaryTensor:[graph divisionWithPrimaryTensor:mHat secondaryTensor:adamDenom name:nil] name:nil];
            MPSGraphTensor* base=decay?[graph multiplicationWithPrimaryTensor:param secondaryTensor:decayC name:nil]:param;
            MPSGraphTensor* np=[graph subtractionWithPrimaryTensor:base secondaryTensor:stepTerm name:[tag stringByAppendingString:@"_np"]];
            return std::array<MPSGraphTensor*,3>{np,nm,nv};
        };

        const auto uW1=add_adamw(w1T,gw1,mw1T,vw1T,true,@"w1");
        const auto uB1=add_adamw(b1T,gb1,mb1T,vb1T,false,@"b1");
        const auto uW2=add_adamw(w2T,gw2,mw2T,vw2T,true,@"w2");
        const auto uB2=add_adamw(b2T,gb2,mb2T,vb2T,false,@"b2");

        MPSGraphTensorDataDictionary* feeds=@{
            xT:wrap_buffer(make_shared_buffer(device,std::span<const float>(batch.x)),sX),
            maskT:wrap_buffer(make_shared_buffer(device,std::span<const float>(batch.mask)),sOut),
            oneHotT:wrap_buffer(make_shared_buffer(device,std::span<const float>(batch.one_hot)),sOut),
            sampleWT:wrap_buffer(make_shared_buffer(device,std::span<const float>(batch.norm_weights)),sSampleW),
            w1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(weights.w1)),sW1),
            b1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(weights.b1)),sB1),
            w2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(weights.w2)),sW2),
            b2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(weights.b2)),sB2),
            mw1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.m.w1)),sW1),
            mb1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.m.b1)),sB1),
            mw2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.m.w2)),sW2),
            mb2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.m.b2)),sB2),
            vw1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.v.w1)),sW1),
            vb1T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.v.b1)),sB1),
            vw2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.v.w2)),sW2),
            vb2T:wrap_buffer(make_shared_buffer(device,std::span<const float>(opt.v.b2)),sB2),
        };
        NSArray<MPSGraphTensor*>* targets=@[
            lossT,gradNormT,maskedLogits,
            uW1[0],uB1[0],uW2[0],uB2[0],
            uW1[1],uB1[1],uW2[1],uB2[1],
            uW1[2],uB1[2],uW2[2],uB2[2]
        ];
        MPSGraphTensorDataDictionary* results=[graph runWithMTLCommandQueue:queue feeds:feeds targetTensors:targets targetOperations:nil];

        float loss=0.0F,grad_norm=0.0F;
        std::vector<float> masked_logits_out(batch.count*rival_policy_actions);
        RivalPolicyWeights next_w;
        RivalPolicyOptimizerState next_opt;
        next_opt.step=next_step;

        read_tensor_data(results,lossT,std::span<float>(&loss,1));
        read_tensor_data(results,gradNormT,std::span<float>(&grad_norm,1));
        read_tensor_data(results,maskedLogits,masked_logits_out);
        read_tensor_data(results,uW1[0],next_w.w1);
        read_tensor_data(results,uB1[0],next_w.b1);
        read_tensor_data(results,uW2[0],next_w.w2);
        read_tensor_data(results,uB2[0],next_w.b2);
        read_tensor_data(results,uW1[1],next_opt.m.w1);
        read_tensor_data(results,uB1[1],next_opt.m.b1);
        read_tensor_data(results,uW2[1],next_opt.m.w2);
        read_tensor_data(results,uB2[1],next_opt.m.b2);
        read_tensor_data(results,uW1[2],next_opt.v.w1);
        read_tensor_data(results,uB1[2],next_opt.v.b1);
        read_tensor_data(results,uW2[2],next_opt.v.w2);
        read_tensor_data(results,uB2[2],next_opt.v.b2);

        if(!std::isfinite(loss) || !std::isfinite(grad_norm) ||
           !rival_policy_weights_valid(next_w) || !optimizer_state_valid(next_opt))
            throw std::runtime_error("Metal MPSGraph training produced non-finite values; weights preserved.");
        weights=next_w;
        opt=next_opt;
        return {loss,compute_masked_accuracy(masked_logits_out,batch),grad_norm,batch.count,RivalPolicyBackend::metal_mpsgraph};
    }
}
}
}
