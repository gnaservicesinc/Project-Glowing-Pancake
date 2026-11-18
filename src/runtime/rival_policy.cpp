#include <pgp/runtime/rival_policy.hpp>
#include "rival_policy_internal.hpp"
#include <pgp/runtime/runtime.hpp>
#include <pgp/runtime/authority.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace pgp::runtime {
namespace {
using namespace core;
constexpr std::array<std::string_view,rival_policy_actions> actions{
    "wait","data","research","train","evaluate","deploy","price","hire","repay","finance","expand","publish",
    "frontier_system","frontier_trial","study","software","software_release","equipment","place","hire_individual",
    "accept_counter","offer_employment","buy_workstation","equip_workstation","collaborate"};
constexpr std::array<std::uint8_t,8> checkpoint_magic{'P','G','P','R','P','W','0','1'};
constexpr std::size_t checkpoint_header_bytes=36;
constexpr std::size_t checkpoint_weights_bytes=rival_policy_param_count*sizeof(std::uint32_t);
constexpr std::size_t checkpoint_checksum_bytes=sizeof(std::uint64_t);
thread_local RivalPolicyRecorder recorder=nullptr;
thread_local void* recorder_context=nullptr;
thread_local RivalPolicySelector selector=nullptr;
thread_local void* selector_context=nullptr;
thread_local const RivalPolicyWeights* active_weights=nullptr;
float bounded(std::int64_t amount,float scale) {
    return std::clamp(static_cast<float>(amount)/scale,-1.0F,1.0F);
}
std::uint64_t next_u64(std::uint64_t& state) noexcept {
    state+=0x9e3779b97f4a7c15ULL;
    std::uint64_t z=state;
    z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;
    z=(z^(z>>27))*0x94d049bb133111ebULL;
    return z^(z>>31);
}
float uniform_symmetric(std::uint64_t& state,float limit) noexcept {
    const auto bits=next_u64(state)>>40;
    const float unit=static_cast<float>(bits)/static_cast<float>(1ULL<<24);
    return (unit*2.0F-1.0F)*limit;
}
std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t hash=14695981039346656037ULL;
    for(const std::uint8_t b:bytes) {
        hash^=b;
        hash*=1099511628211ULL;
    }
    return hash;
}
void push_u32(std::vector<std::uint8_t>& out,std::uint32_t v) {
    for(int shift=0;shift<32;shift+=8) out.push_back(static_cast<std::uint8_t>((v>>shift)&0xffU));
}
void push_u64(std::vector<std::uint8_t>& out,std::uint64_t v) {
    for(int shift=0;shift<64;shift+=8) out.push_back(static_cast<std::uint8_t>((v>>shift)&0xffULL));
}
void push_floats(std::vector<std::uint8_t>& out,std::span<const float> values) {
    static_assert(sizeof(float)==sizeof(std::uint32_t));
    for(const float f:values) {
        std::uint32_t bits=0;
        std::memcpy(&bits,&f,sizeof(bits));
        push_u32(out,bits);
    }
}
void push_weights(std::vector<std::uint8_t>& out,const RivalPolicyWeights& w) {
    push_floats(out,w.w1);
    push_floats(out,w.b1);
    push_floats(out,w.w2);
    push_floats(out,w.b2);
}
std::uint32_t read_u32(std::span<const std::uint8_t> in,std::size_t& offset) {
    std::uint32_t v=0;
    for(int byte=0;byte<4;++byte) v|=static_cast<std::uint32_t>(in[offset++])<<(byte*8);
    return v;
}
std::uint64_t read_u64(std::span<const std::uint8_t> in,std::size_t& offset) {
    std::uint64_t v=0;
    for(int byte=0;byte<8;++byte) v|=static_cast<std::uint64_t>(in[offset++])<<(byte*8);
    return v;
}
void read_floats(std::span<const std::uint8_t> in,std::size_t& offset,std::span<float> dst) {
    for(float& f:dst) {
        const auto bits=read_u32(in,offset);
        std::memcpy(&f,&bits,sizeof(f));
        if(!std::isfinite(f)) throw std::invalid_argument("Policy checkpoint contains non-finite weights.");
    }
}
void read_weights(std::span<const std::uint8_t> in,std::size_t& offset,RivalPolicyWeights& w) {
    read_floats(in,offset,w.w1);
    read_floats(in,offset,w.b1);
    read_floats(in,offset,w.w2);
    read_floats(in,offset,w.b2);
}
void apply_adamw_param(
    std::span<const float> param_in,
    std::span<const float> grad,
    std::span<const float> m_in,
    std::span<const float> v_in,
    std::span<float> param_out,
    std::span<float> m_out,
    std::span<float> v_out,
    float clip_scale,
    float b1_corr,
    float b2_corr,
    float decay_factor,
    bool apply_decay,
    const RivalPolicyTrainingConfig& config) noexcept {
    const float one_minus_b1=1.0F-config.beta1;
    const float one_minus_b2=1.0F-config.beta2;
    for(std::size_t idx=0;idx<param_in.size();++idx) {
        const float g=grad[idx]*clip_scale;
        const float nm=config.beta1*m_in[idx]+one_minus_b1*g;
        const float nv=config.beta2*v_in[idx]+one_minus_b2*g*g;
        const float m_hat=nm/b1_corr;
        const float v_hat=nv/b2_corr;
        const float base=apply_decay?param_in[idx]*decay_factor:param_in[idx];
        m_out[idx]=nm;
        v_out[idx]=nv;
        param_out[idx]=base-config.learning_rate*m_hat/(std::sqrt(v_hat)+config.epsilon);
    }
}
std::vector<RivalTrainingSample> to_training_samples(std::span<const RivalLearningSample> samples) {
    std::vector<RivalTrainingSample> result;
    result.reserve(samples.size());
    for(const auto& s:samples) result.push_back({s.features,s.teacher,s.legal,1.0F});
    return result;
}
}
namespace detail {
bool all_finite(std::span<const float> values) noexcept {
    return std::ranges::all_of(values,[](float v){return std::isfinite(v);});
}
bool optimizer_state_valid(const RivalPolicyOptimizerState& opt) noexcept {
    return rival_policy_weights_valid(opt.m) && rival_policy_weights_valid(opt.v);
}
PreparedPolicyBatch prepare_policy_batch(
    const RivalPolicyWeights& weights,
    const RivalPolicyOptimizerState& opt,
    std::span<const RivalTrainingSample> samples,
    const RivalPolicyTrainingConfig& config) {
    if(!rival_policy_weights_valid(weights) || !optimizer_state_valid(opt))
        throw std::invalid_argument("Policy weights and optimizer state must be finite before training.");
    if(!std::isfinite(config.learning_rate) || config.learning_rate<0.0F ||
       !std::isfinite(config.beta1) || config.beta1<0.0F || config.beta1>=1.0F ||
       !std::isfinite(config.beta2) || config.beta2<0.0F || config.beta2>=1.0F ||
       !std::isfinite(config.epsilon) || config.epsilon<=0.0F ||
       !std::isfinite(config.weight_decay) || config.weight_decay<0.0F ||
       !std::isfinite(config.max_grad_norm) || config.max_grad_norm<=0.0F)
        throw std::invalid_argument("Policy training hyperparameters are invalid.");
    PreparedPolicyBatch batch;
    std::vector<double> raw_weights;
    double total_weight=0.0;
    for(const auto& sample:samples) {
        if(!all_finite(sample.features) || !std::isfinite(sample.weight) || sample.weight<0.0F)
            throw std::invalid_argument("Training sample contains non-finite features or negative weight.");
        if(sample.weight==0.0F) continue;
        if(sample.teacher>=rival_policy_actions || (sample.legal&(1U<<sample.teacher))==0U)
            throw std::invalid_argument("Training sample teacher action must be within range and legal.");
        batch.x.insert(batch.x.end(),sample.features.begin(),sample.features.end());
        for(std::size_t a=0;a<rival_policy_actions;++a) {
            batch.mask.push_back((sample.legal&(1U<<a))!=0U?0.0F:-1.0e9F);
            batch.one_hot.push_back(a==sample.teacher?1.0F:0.0F);
        }
        raw_weights.push_back(static_cast<double>(sample.weight));
        total_weight+=static_cast<double>(sample.weight);
        batch.teachers.push_back(sample.teacher);
        batch.legals.push_back(sample.legal);
        ++batch.count;
    }
    if(batch.count==0 || !(total_weight>0.0))
        throw std::invalid_argument("At least one positive-weight training sample is required.");
    batch.norm_weights.reserve(batch.count);
    for(const double w:raw_weights) batch.norm_weights.push_back(static_cast<float>(w/total_weight));
    return batch;
}
float compute_masked_accuracy(
    std::span<const float> masked_logits,
    const PreparedPolicyBatch& batch) noexcept {
    if(batch.count==0) return 0.0F;
    std::size_t correct=0;
    for(std::size_t n=0;n<batch.count;++n) {
        const auto row=masked_logits.subspan(n*rival_policy_actions,rival_policy_actions);
        const std::uint32_t legal=batch.legals[n];
        std::size_t best=batch.teachers[n];
        float best_val=-1.0e30F;
        bool found=false;
        for(std::size_t a=0;a<rival_policy_actions;++a) {
            if((legal&(1U<<a))==0U) continue;
            if(!found || row[a]>best_val) {
                found=true;
                best=a;
                best_val=row[a];
            }
        }
        if(found && best==batch.teachers[n]) ++correct;
    }
    return static_cast<float>(correct)/static_cast<float>(batch.count);
}
}
std::size_t rival_action_slot(std::string_view id) {
    const auto it=std::ranges::find(actions,id);
    return static_cast<std::size_t>(it-actions.begin());
}
core::ObjectiveKind rival_secondary(std::string_view id) {
    std::uint32_t hash=2166136261U;
    for(unsigned char character:std::span(reinterpret_cast<const unsigned char*>(id.data()),id.size()))
        hash=(hash^character)*16777619U;
    constexpr std::array goals{ObjectiveKind::monopoly,ObjectiveKind::domination,ObjectiveKind::utopia,ObjectiveKind::profit,ObjectiveKind::survival};
    return goals[hash%goals.size()];
}
float RivalAmbition::reward() const {
    return (achieved()?5.0F:0.0F)+(asi?1.0F:0.0F)+(controlled?0.75F:0.0F)+0.5F*progress;
}
RivalAmbition rival_ambition(const WorldState& world,const RivalCompany& rival) {
    const auto& s=rival.simulation;
    RivalAmbition result;
    if(!s.accounts || !s.capacity) return result;
    if(s.frontier) for(const auto& system:s.frontier->systems) {
        const auto qualification=frontier_qualification(*s.frontier,system.id);
        result.asi=result.asi || qualification.asi_on.has_value();
        result.controlled=result.controlled || (qualification.asi_on && qualification.conservative.back()>=8500 &&
            std::ranges::all_of(system.deployments,[&](std::uint64_t id){
                const auto& release=s.deployments->records.at(id-1);
                const auto work=std::ranges::find(s.capacity->projects,release.project_id,&CapacityProject::id);
                return release.human_review && work!=s.capacity->projects.end() && work->status==ProjectStatus::active;
            }));
    }
    const auto report=accounts_report(*s.accounts);
    const auto customers=s.market?market_report(*s.market).delivered_tasks:0;
    const auto own_research=s.lab?s.lab->publications.size():0;
    switch(rival_secondary(rival.definition_id)) {
    case ObjectiveKind::profit:
        result.progress=bounded(report.profit.value(),1'000'000'000.0F);
        break;
    case ObjectiveKind::monopoly: {
        std::uint64_t market=world.simulation.market?market_report(*world.simulation.market).delivered_tasks:0;
        for(const auto& company:world.competition->companies)
            if(company.simulation.market) market+=market_report(*company.simulation.market).delivered_tasks;
        result.progress=market && customers>=1000?static_cast<float>(customers)/static_cast<float>(market):0.0F;
        break;
    }
    case ObjectiveKind::domination:
        result.progress=std::min(1.0F,static_cast<float>(s.deployments->records.size())/5.0F);
        break;
    case ObjectiveKind::utopia:
        result.progress=std::min(1.0F,static_cast<float>(own_research)/8.0F);
        break;
    case ObjectiveKind::survival:
        result.progress=s.phase!=DayPhase::closed && s.capacity->available_staff && customers>=1000 &&
            s.liquidity->status==LiquidityStatus::operating?1.0F:0.0F;
        break;
    case ObjectiveKind::asi:
        break;
    }
    result.secondary=result.progress>=1.0F;
    return result;
}
RivalPolicyFeatures rival_policy_observation(const WorldState&,const RivalCompany& rival,const RivalObservation& public_data) {
    const auto& s=rival.simulation;
    RivalPolicyFeatures result{};
    const auto accounts=accounts_report(*s.accounts);
    result[0]=bounded(s.cash.value(),1'000'000'000.0F);
    result[1]=bounded(accounts.profit.value(),1'000'000'000.0F);
    result[2]=bounded(accounts.payables.value(),100'000'000.0F);
    result[3]=bounded(accounts.debt.value(),500'000'000.0F);
    result[4]=bounded(s.capacity->available_staff,32.0F);
    const auto capacity=capacity_report(*s.capacity);
    result[5]=bounded(capacity.assigned_staff,32.0F);
    result[6]=bounded(capacity.effective.value(),1'000'000.0F);
    result[7]=bounded(static_cast<std::int64_t>(std::ranges::count_if(s.capacity->projects,[](const auto& project){return project.status==ProjectStatus::active;})),32.0F);
    result[8]=bounded(static_cast<std::int64_t>(s.lab->licenses.size()),16.0F);
    result[9]=bounded(static_cast<std::int64_t>(s.lab->research.size()),64.0F);
    result[10]=bounded(static_cast<std::int64_t>(s.lab->publications.size()),32.0F);
    result[11]=bounded(static_cast<std::int64_t>(s.lab->models.size()),16.0F);
    result[12]=bounded(static_cast<std::int64_t>(s.lab->evaluations.size()),16.0F);
    result[13]=bounded(static_cast<std::int64_t>(s.deployments->records.size()),16.0F);
    result[14]=bounded(public_data.public_providers,32.0F);
    result[15]=bounded(public_data.public_price.value(),300.0F);
    result[16]=bounded(static_cast<std::int64_t>(public_data.demand_lower),1'000'000.0F);
    result[17]=bounded(static_cast<std::int64_t>(public_data.demand_upper),1'000'000.0F);
    result[18]=bounded(s.date.value()-s.accounts->starts_on.value(),20'000.0F);
    result[19]=s.liquidity->status==LiquidityStatus::workout?1.0F:0.0F;
    if(s.frontier) for(const auto& system:s.frontier->systems) {
        const auto evidence=frontier_qualification(*s.frontier,system.id);
        result[20]=std::max(result[20],evidence.agi_on?1.0F:0.0F);
        result[21]=std::max(result[21],evidence.asi_on?1.0F:0.0F);
        result[22]=std::max(result[22],bounded(evidence.conservative.back(),10000.0F));
    }
    result[23+static_cast<std::size_t>(rival_secondary(rival.definition_id))]=1.0F;
    if(s.management->people) {
        const auto& people=*s.management->people;
        result[29]=bounded(static_cast<std::int64_t>(people.employees.size()),32.0F);
        if(people.social) result[30]=bounded(static_cast<std::int64_t>(people.social->links.size()),64.0F);
    }
    result[31]=bounded(static_cast<std::int64_t>(std::ranges::count_if(s.lab->research,&ResearchRun::completed)),64.0F);
    return result;
}
void set_rival_policy_recorder(RivalPolicyRecorder value,void* context) {recorder=value;recorder_context=context;}
bool rival_policy_enabled() {return recorder || selector || active_weights;}
void set_rival_policy_selector(RivalPolicySelector value,void* context) {selector=value;selector_context=context;}
void set_rival_policy_weights(const RivalPolicyWeights* value) {
    if(value && !rival_policy_weights_valid(*value))
        throw std::invalid_argument("Cannot install non-finite rival policy weights.");
    active_weights=value;
}
std::size_t select_rival_policy_action(const RivalLearningSample& sample) {
    if(recorder) recorder(sample,recorder_context);
    std::size_t slot=sample.teacher;
    if(selector) slot=selector(sample,selector_context);
    else if(active_weights) slot=select_rival_policy_with_weights(*active_weights,sample);
    return slot<rival_policy_actions && (sample.legal&(1U<<slot))?slot:sample.teacher;
}
RivalPolicyWeights initialize_rival_policy_weights(std::uint64_t seed) {
    RivalPolicyWeights w;
    std::uint64_t state=seed?seed:1ULL;
    const float limit1=std::sqrt(2.0F/static_cast<float>(rival_policy_features));
    const float limit2=std::sqrt(2.0F/static_cast<float>(rival_policy_hidden));
    for(float& entry:w.w1) entry=uniform_symmetric(state,limit1);
    for(float& entry:w.w2) entry=uniform_symmetric(state,limit2);
    return w;
}
bool rival_policy_weights_valid(const RivalPolicyWeights& w) noexcept {
    return detail::all_finite(w.w1) && detail::all_finite(w.b1) &&
           detail::all_finite(w.w2) && detail::all_finite(w.b2);
}
std::array<float,rival_policy_actions> evaluate_rival_policy_logits(
    const RivalPolicyWeights& weights,
    const RivalPolicyFeatures& features) {
    if(!rival_policy_weights_valid(weights) || !detail::all_finite(features))
        throw std::invalid_argument("Cannot evaluate policy with non-finite weights or features.");
    std::array<float,rival_policy_hidden> hidden{};
    for(std::size_t j=0;j<rival_policy_hidden;++j) {
        float acc=weights.b1[j];
        for(std::size_t i=0;i<rival_policy_features;++i)
            acc+=features[i]*weights.w1[i*rival_policy_hidden+j];
        hidden[j]=acc>0.0F?acc:0.0F;
    }
    std::array<float,rival_policy_actions> logits{};
    for(std::size_t k=0;k<rival_policy_actions;++k) {
        float acc=weights.b2[k];
        for(std::size_t j=0;j<rival_policy_hidden;++j)
            acc+=hidden[j]*weights.w2[j*rival_policy_actions+k];
        logits[k]=acc;
    }
    return logits;
}
std::size_t select_rival_policy_with_weights(
    const RivalPolicyWeights& weights,
    const RivalLearningSample& sample) {
    const auto logits=evaluate_rival_policy_logits(weights,sample.features);
    std::size_t best=sample.teacher;
    bool found=false;
    float best_logit=0.0F;
    for(std::size_t a=0;a<rival_policy_actions;++a) {
        if((sample.legal&(1U<<a))==0U) continue;
        if(!found || logits[a]>best_logit) {
            found=true;
            best=a;
            best_logit=logits[a];
        }
    }
    return found?best:sample.teacher;
}
#if !defined(PGP_HAS_METAL_POLICY) || !PGP_HAS_METAL_POLICY
bool rival_policy_metal_available() noexcept {return false;}
namespace detail {
RivalPolicyStepMetrics train_rival_policy_step_metal_impl(
    RivalPolicyWeights&,
    RivalPolicyOptimizerState&,
    const PreparedPolicyBatch&,
    const RivalPolicyTrainingConfig&) {
    throw std::runtime_error("Metal MPSGraph policy training is only available on macOS.");
}
}
#endif
RivalPolicyStepMetrics train_rival_policy_step_cpu(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalTrainingSample> samples,
    const RivalPolicyTrainingConfig& config) {
    const auto batch=detail::prepare_policy_batch(weights,opt,samples,config);
    RivalPolicyWeights grads;
    std::vector<float> masked_logits(batch.count*rival_policy_actions);
    float loss=0.0F;
    for(std::size_t n=0;n<batch.count;++n) {
        const auto x=std::span<const float>(batch.x).subspan(n*rival_policy_features,rival_policy_features);
        const auto mask=std::span<const float>(batch.mask).subspan(n*rival_policy_actions,rival_policy_actions);
        const auto one_hot=std::span<const float>(batch.one_hot).subspan(n*rival_policy_actions,rival_policy_actions);
        const float sw=batch.norm_weights[n];

        std::array<float,rival_policy_hidden> z1{};
        std::array<float,rival_policy_hidden> h{};
        for(std::size_t j=0;j<rival_policy_hidden;++j) {
            float acc=weights.b1[j];
            for(std::size_t i=0;i<rival_policy_features;++i)
                acc+=x[i]*weights.w1[i*rival_policy_hidden+j];
            z1[j]=acc;
            h[j]=acc>0.0F?acc:0.0F;
        }
        auto ml=std::span<float>(masked_logits).subspan(n*rival_policy_actions,rival_policy_actions);
        float max_logit=-1.0e30F;
        for(std::size_t k=0;k<rival_policy_actions;++k) {
            float acc=weights.b2[k];
            for(std::size_t j=0;j<rival_policy_hidden;++j)
                acc+=h[j]*weights.w2[j*rival_policy_actions+k];
            ml[k]=acc+mask[k];
            max_logit=std::max(max_logit,ml[k]);
        }
        float sum_exp=0.0F;
        std::array<float,rival_policy_actions> exp_vals{};
        for(std::size_t k=0;k<rival_policy_actions;++k) {
            exp_vals[k]=std::exp(ml[k]-max_logit);
            sum_exp+=exp_vals[k];
        }
        const float log_sum_exp=std::log(sum_exp);
        const std::size_t teacher=batch.teachers[n];
        loss+=sw*(log_sum_exp-(ml[teacher]-max_logit));

        std::array<float,rival_policy_actions> dlogits{};
        for(std::size_t k=0;k<rival_policy_actions;++k) {
            const float prob=exp_vals[k]/sum_exp;
            dlogits[k]=sw*(prob-one_hot[k]);
            grads.b2[k]+=dlogits[k];
        }
        std::array<float,rival_policy_hidden> dh{};
        for(std::size_t j=0;j<rival_policy_hidden;++j) {
            float d_acc=0.0F;
            const auto row=std::span<float>(grads.w2).subspan(j*rival_policy_actions,rival_policy_actions);
            const auto w_row=std::span<const float>(weights.w2).subspan(j*rival_policy_actions,rival_policy_actions);
            for(std::size_t k=0;k<rival_policy_actions;++k) {
                d_acc+=dlogits[k]*w_row[k];
                row[k]+=h[j]*dlogits[k];
            }
            dh[j]=d_acc;
        }
        for(std::size_t j=0;j<rival_policy_hidden;++j) {
            const float dz=z1[j]>0.0F?dh[j]:0.0F;
            grads.b1[j]+=dz;
            for(std::size_t i=0;i<rival_policy_features;++i)
                grads.w1[i*rival_policy_hidden+j]+=x[i]*dz;
        }
    }
    double sq_norm=0.0;
    for(const auto span:{std::span<const float>(grads.w1),std::span<const float>(grads.b1),
                         std::span<const float>(grads.w2),std::span<const float>(grads.b2)}) {
        for(const float g:span) sq_norm+=static_cast<double>(g)*static_cast<double>(g);
    }
    const float grad_norm=static_cast<float>(std::sqrt(sq_norm));
    const float clip_scale=grad_norm>config.max_grad_norm?config.max_grad_norm/grad_norm:1.0F;

    RivalPolicyWeights next_w;
    RivalPolicyOptimizerState next_opt;
    next_opt.step=opt.step+1;
    const float b1_corr=static_cast<float>(1.0-std::pow(static_cast<double>(config.beta1),static_cast<double>(next_opt.step)));
    const float b2_corr=static_cast<float>(1.0-std::pow(static_cast<double>(config.beta2),static_cast<double>(next_opt.step)));
    const float decay_factor=static_cast<float>(1.0-static_cast<double>(config.learning_rate)*static_cast<double>(config.weight_decay));
    apply_adamw_param(weights.w1,grads.w1,opt.m.w1,opt.v.w1,next_w.w1,next_opt.m.w1,next_opt.v.w1,clip_scale,b1_corr,b2_corr,decay_factor,true,config);
    apply_adamw_param(weights.b1,grads.b1,opt.m.b1,opt.v.b1,next_w.b1,next_opt.m.b1,next_opt.v.b1,clip_scale,b1_corr,b2_corr,decay_factor,false,config);
    apply_adamw_param(weights.w2,grads.w2,opt.m.w2,opt.v.w2,next_w.w2,next_opt.m.w2,next_opt.v.w2,clip_scale,b1_corr,b2_corr,decay_factor,true,config);
    apply_adamw_param(weights.b2,grads.b2,opt.m.b2,opt.v.b2,next_w.b2,next_opt.m.b2,next_opt.v.b2,clip_scale,b1_corr,b2_corr,decay_factor,false,config);

    if(!std::isfinite(loss) || !std::isfinite(grad_norm) ||
       !rival_policy_weights_valid(next_w) || !detail::optimizer_state_valid(next_opt))
        throw std::runtime_error("Non-finite policy gradient or weights rejected.");
    weights=next_w;
    opt=next_opt;
    return {loss,detail::compute_masked_accuracy(masked_logits,batch),grad_norm,batch.count,RivalPolicyBackend::cpu};
}
RivalPolicyStepMetrics train_rival_policy_step_cpu(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalLearningSample> samples,
    const RivalPolicyTrainingConfig& config) {
    return train_rival_policy_step_cpu(weights,opt,to_training_samples(samples),config);
}
RivalPolicyStepMetrics train_rival_policy_step_metal(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalTrainingSample> samples,
    const RivalPolicyTrainingConfig& config) {
    const auto batch=detail::prepare_policy_batch(weights,opt,samples,config);
    return detail::train_rival_policy_step_metal_impl(weights,opt,batch,config);
}
RivalPolicyStepMetrics train_rival_policy_step_metal(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalLearningSample> samples,
    const RivalPolicyTrainingConfig& config) {
    return train_rival_policy_step_metal(weights,opt,to_training_samples(samples),config);
}
RivalPolicyStepMetrics train_rival_policy_step(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalTrainingSample> samples,
    const RivalPolicyTrainingConfig& config) {
    if(config.prefer_metal && rival_policy_metal_available())
        return train_rival_policy_step_metal(weights,opt,samples,config);
    return train_rival_policy_step_cpu(weights,opt,samples,config);
}
RivalPolicyStepMetrics train_rival_policy_step(
    RivalPolicyWeights& weights,
    RivalPolicyOptimizerState& opt,
    std::span<const RivalLearningSample> samples,
    const RivalPolicyTrainingConfig& config) {
    return train_rival_policy_step(weights,opt,to_training_samples(samples),config);
}
std::vector<std::uint8_t> serialize_rival_policy_checkpoint(
    const RivalPolicyWeights& weights,
    const RivalPolicyOptimizerState* opt) {
    if(!rival_policy_weights_valid(weights))
        throw std::invalid_argument("Cannot serialize non-finite rival policy weights.");
    if(opt && !detail::optimizer_state_valid(*opt))
        throw std::invalid_argument("Cannot serialize non-finite optimizer moments.");
    std::vector<std::uint8_t> out;
    out.reserve(checkpoint_header_bytes+checkpoint_weights_bytes*(opt?3U:1U)+checkpoint_checksum_bytes);
    out.insert(out.end(),checkpoint_magic.begin(),checkpoint_magic.end());
    push_u32(out,rival_policy_schema);
    push_u32(out,static_cast<std::uint32_t>(rival_policy_features));
    push_u32(out,static_cast<std::uint32_t>(rival_policy_hidden));
    push_u32(out,static_cast<std::uint32_t>(rival_policy_actions));
    out.push_back(opt?1U:0U);
    out.push_back(0U);
    out.push_back(0U);
    out.push_back(0U);
    push_u64(out,opt?opt->step:0ULL);
    push_weights(out,weights);
    if(opt) {
        push_weights(out,opt->m);
        push_weights(out,opt->v);
    }
    push_u64(out,fnv1a64(out));
    return out;
}
void deserialize_rival_policy_checkpoint(
    std::span<const std::uint8_t> bytes,
    RivalPolicyWeights& out_weights,
    RivalPolicyOptimizerState* out_opt) {
    constexpr std::size_t min_size=checkpoint_header_bytes+checkpoint_weights_bytes+checkpoint_checksum_bytes;
    if(bytes.size()<min_size) throw std::invalid_argument("Truncated policy checkpoint.");
    if(!std::equal(checkpoint_magic.begin(),checkpoint_magic.end(),bytes.begin()))
        throw std::invalid_argument("Invalid policy checkpoint header.");
    std::size_t offset=checkpoint_magic.size();
    const auto schema=read_u32(bytes,offset);
    const auto features=read_u32(bytes,offset);
    const auto hidden=read_u32(bytes,offset);
    const auto act=read_u32(bytes,offset);
    const auto has_opt=bytes[offset++];
    const auto r0=bytes[offset++],r1=bytes[offset++],r2=bytes[offset++];
    const auto step=read_u64(bytes,offset);
    if(schema!=rival_policy_schema || features!=rival_policy_features ||
       hidden!=rival_policy_hidden || act!=rival_policy_actions ||
       has_opt>1U || r0!=0U || r1!=0U || r2!=0U)
        throw std::invalid_argument("Incompatible policy checkpoint schema or dimensions.");
    const std::size_t expected=checkpoint_header_bytes+checkpoint_weights_bytes*(has_opt?3U:1U)+checkpoint_checksum_bytes;
    if(bytes.size()!=expected) throw std::invalid_argument("Policy checkpoint byte count mismatch.");
    std::size_t csum_offset=bytes.size()-checkpoint_checksum_bytes;
    const auto stored_checksum=read_u64(bytes,csum_offset);
    if(stored_checksum!=fnv1a64(bytes.first(bytes.size()-checkpoint_checksum_bytes)))
        throw std::invalid_argument("Policy checkpoint checksum verification failed.");
    RivalPolicyWeights decoded_weights;
    read_weights(bytes,offset,decoded_weights);
    RivalPolicyOptimizerState decoded_opt;
    if(has_opt) {
        decoded_opt.step=step;
        read_weights(bytes,offset,decoded_opt.m);
        read_weights(bytes,offset,decoded_opt.v);
    }
    out_weights=decoded_weights;
    if(out_opt) *out_opt=decoded_opt;
}
}
