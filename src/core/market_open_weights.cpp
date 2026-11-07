#include "market_open_weights.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
using WeightKey=std::tuple<std::string_view,std::string_view,std::string_view>;
WeightKey key(const WeightInstallation& row) {return {row.product_id,row.segment_id,row.project_id};}
template<class Rows> const auto* find(const Rows& rows,std::string_view id) {
    const auto row=std::ranges::find(rows,id,&Rows::value_type::id);
    return row==rows.end()?nullptr:&*row;
}
DayRows<WeightInstallation> audit_installations(const MarketState& state) {
    DayRows<WeightInstallation> result;
    if (std::ranges::none_of(state.products,[](const auto& row){return row.channel==ProductChannel::open_weights;})) return result;
    std::map<WeightKey,WeightInstallation> installed;
    std::map<std::string_view,const ProductCapacityBinding*> publications;
    std::vector<const ProductCapacityBinding*> bindings;
    for (const auto& row:state.capacity_bindings) bindings.push_back(&row);
    std::stable_sort(bindings.begin(),bindings.end(),[](const auto* a,const auto* b){return a->starts_on<b->starts_on;});
    auto binding=bindings.begin();
    for (const auto& day:state.days) {
        while (binding!=bindings.end() && (*binding)->starts_on<=day.day) {
            publications.insert_or_assign((*binding)->product_id,*binding);
            ++binding;
        }
        for (const auto& sale:day.discretionary) {
            const auto* product=find(state.products,sale.product_id);
            if (!product || product->channel!=ProductChannel::open_weights) continue;
            if (!sale.local_copy_project.empty()) {
                const auto installed_copy=installed.find({sale.product_id,sale.segment_id,sale.local_copy_project});
                if (installed_copy==installed.end() || installed_copy->second.received_on>=day.day ||
                    installed_copy->second.tasks_per_day<sale.tasks)
                    throw std::invalid_argument("Local weight work exceeds previously received copies for this buyer group");
                continue;
            }
            const auto published=publications.find(sale.product_id);
            if (published==publications.end() || sale.trial_tasks>sale.tasks)
                throw std::invalid_argument("Open weight delivery has no dated production publication");
            const auto& publication=*published->second;
            if (!publication.weight_audience.empty() && publication.weight_audience!=sale.segment_id)
                throw std::invalid_argument("Open weights were distributed outside the recorded grant");
            const auto acquired=sale.tasks-sale.trial_tasks;
            if (acquired==0) continue;
            auto [row,added]=installed.try_emplace({publication.product_id,sale.segment_id,publication.project_id});
            if (added) row->second={sale.product_id,sale.segment_id,publication.project_id,day.day,acquired};
            else row->second.tasks_per_day=std::max(row->second.tasks_per_day,acquired);
        }
    }
    for (auto& [identity,row]:installed) { (void)identity; result.push_back(std::move(row)); }
    return result;
}
}

const ProductCapacityBinding* market_binding(const MarketState& state,std::string_view product,OrdinalDay day) {
    for (auto row=state.capacity_bindings.rbegin();row!=state.capacity_bindings.rend();++row)
        if (row->product_id==product && row->starts_on<=day) return &*row;
    return nullptr;
}
bool market_detail::local_hardware_fits(const CapacityProject& work,const MarketSegment& group) {
    return !work.customer_inference || (work.minimum_memory_mib<=group.customer_memory_mib &&
        work.interconnect==group.customer_interconnect && work.inference_milli_CU_per_task.value()<=group.customer_compute.value());
}
bool market_detail::copy_offer(std::string_view id) {return id.starts_with("local.copy/");}
const WeightInstallation* market_detail::retained_installation(const MarketState& state,const CapacityState& capacity,
    const MarketProduct& product,const MarketSegment& group,const MarketInfluence* rule) {
    if (product.channel!=ProductChannel::open_weights || state.weight_installations.empty() ||
        (rule && !rule->eligible && !rule->blocked_weight_projects)) return nullptr;
    const auto group_key=std::pair<std::string_view,std::string_view>{product.id,group.id};
    auto row=std::ranges::lower_bound(state.weight_installations,WeightKey{product.id,group.id,{}},{},key);
    const WeightInstallation* best=nullptr;
    for (;row!=state.weight_installations.end() && std::pair<std::string_view,std::string_view>{row->product_id,row->segment_id}==group_key;++row) {
        if (rule && rule->blocked_weight_projects && std::ranges::find(*rule->blocked_weight_projects,row->project_id)!=rule->blocked_weight_projects->end()) continue;
        const auto* work=find(capacity.projects,row->project_id);
        if (!work || !local_hardware_fits(*work,group)) continue;
        if (!best || std::tie(row->tasks_per_day,row->received_on)>std::tie(best->tasks_per_day,best->received_on)) best=&*row;
    }
    return best;
}
void market_detail::record_weight_installations(MarketState& state,const MarketDay& day) {
    if (std::ranges::none_of(state.products,[](const auto& row){return row.channel==ProductChannel::open_weights;})) return;
    auto& installations=state.weight_installations;
    std::vector<WeightInstallation> acquired;
    for (const auto& sale:day.discretionary) {
        if (!sale.local_copy_project.empty() || sale.tasks<=sale.trial_tasks) continue;
        const auto* product=find(state.products,sale.product_id);
        if (!product || product->channel!=ProductChannel::open_weights) continue;
        const auto tasks=sale.tasks-sale.trial_tasks;
        const WeightKey identity{product->id,sale.segment_id,product->capacity_project_id};
        const auto row=std::ranges::lower_bound(installations,identity,{},key);
        if (row==installations.end() || key(*row)!=identity) {
            acquired.push_back({product->id,sale.segment_id,product->capacity_project_id,day.day,tasks});
        } else if (tasks>row->tasks_per_day) {
            auto updated=*row;
            updated.tasks_per_day=tasks;
            installations.replace(static_cast<std::size_t>(row-installations.begin()),std::move(updated));
        }
    }
    if (acquired.empty()) return;
    for (auto& row:acquired) installations.push_back(std::move(row));
    installations.sort([](const auto& a,const auto& b){return key(a)<key(b);});
}
void rebuild_weight_installations(MarketState& state) {state.weight_installations=audit_installations(state);}
void market_detail::validate_weight_installations(const MarketState& state) {
    if (state.weight_installations!=audit_installations(state))
        throw std::invalid_argument("Published weight installations disagree with settled distribution history");
}
} // namespace pgp::core
