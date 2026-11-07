#include "market_json.hpp"
#include "history_blocks.hpp"

#include <algorithm>
#include <array>

namespace pgp::persistence::detail {
namespace {
constexpr std::array<std::string_view,5> status_names{"pending","active","completed","missed","cancelled"};
Json status_json(core::ContractStatus value) {
    const auto index=static_cast<std::size_t>(value);
    if (index>=status_names.size()) invalid("market/status","Unsupported contract status");
    return status_names[index];
}
core::ContractStatus parse_status(const Json& value) {
    const auto name=text(value,"market/status",32);
    const auto found=std::find(status_names.begin(),status_names.end(),name);
    if (found==status_names.end()) invalid("market/status","Unsupported contract status");
    return static_cast<core::ContractStatus>(found-status_names.begin());
}
Json optional_day(const std::optional<core::OrdinalDay>& value) {
    return value?Json(value->value()):Json(nullptr);
}
core::OrdinalDay day(const Json& value) { return core::OrdinalDay{i64(value,"market/day")}; }
std::optional<core::OrdinalDay> parse_optional_day(const Json& value) {
    return value.is_null()?std::nullopt:std::optional{day(value)};
}
std::string label(const Json& value) { return text(value,"market/text",256); }
core::Cents cents(const Json& value) { return core::Cents{i64(value,"market/cents")}; }
std::uint64_t tasks(const Json& value) { return u64(value,"market/tasks",core::market_task_limit); }
std::uint64_t utility(const Json& value) { return u64(value,"market/utility",core::market_utility_limit); }
void count_records(std::size_t& count,std::size_t added) {
    if (added>core::market_settlement_limit-count) invalid("market/history","Market history exceeds its combined record bound");
    count+=added;
}
}

Json market_json(const core::MarketState& state, bool packed) {
    Json products=Json::array(),segments=Json::array(),contracts=Json::array();
    HistoryRows days{packed}, adoptions{packed};
    for (const auto& product:state.products) {
        products.push_back({{"id",product.id},{"capacity_project_id",product.capacity_project_id},
            {"price_per_task_cents",product.price_per_task.value()},{"available",product.available}});
        if (product.channel != core::ProductChannel::legacy) products.back()["channel"] = core::channel_key(product.channel);
        if (product.monthly_migration.value() != 10000) products.back()["monthly_migration_bp"] = product.monthly_migration.value();
        if (!product.region_id.empty()) products.back()["region"] = product.region_id;
        if (const auto& notice=product.retirement; notice) products.back()["retirement"]={
            {"announced",notice->announced_on.value()},{"retires",notice->retires_on.value()}};
    }
    for (const auto& segment:state.segments) {
        Json preferences=Json::array();
        for (const auto& preference:segment.preferences) preferences.push_back({
            {"product_id",preference.product_id},{"utility",preference.utility},
            {"willingness_to_pay_cents",preference.willingness_to_pay.value()},
            {"switching_cost",preference.switching_cost},{"eligible",preference.eligible}});
        segments.push_back({{"id",segment.id},{"daily_tasks",segment.daily_tasks},
            {"daily_budget_cents",segment.daily_budget.value()},{"outside_utility",segment.outside_utility},
            {"reservation_threshold",segment.reservation_threshold},
            {"maximum_monthly_migration_basis_points",segment.maximum_monthly_migration.value()},
            {"preferences",std::move(preferences)}});
        if (!segment.region_id.empty()) {
            auto& encoded = segments.back();
            encoded["region"] = segment.region_id;
            encoded["customer_compute_milli_CU_per_day"] = segment.customer_compute.value();
            encoded["customer_memory_mib"] = segment.customer_memory_mib;
            encoded["customer_interconnect"] = segment.customer_interconnect;
            encoded["requires_local"] = segment.requires_local;
        }
    }
    for (const auto& contract:state.contracts) {
        contracts.push_back({
        {"id",contract.id},{"segment_id",contract.segment_id},{"product_id",contract.product_id},
        {"tasks_per_day",contract.tasks_per_day},{"monthly_fee_cents",contract.monthly_fee.value()},
        {"usage_price_cents",contract.usage_price.value()},
        {"service_credit_basis_points",contract.service_credit_basis_points.value()},
        {"missing_task_penalty_cents",contract.missing_task_penalty.value()},
        {"missed_start_penalty_cents",contract.missed_start_penalty.value()},
        {"cancellation_penalty_cents",contract.cancellation_penalty.value()},
        {"accepted_on",contract.accepted_on.value()},{"start_by",contract.start_by.value()},
        {"ends_before",contract.ends_before.value()},{"started_on",optional_day(contract.started_on)},
        {"stopped_on",optional_day(contract.stopped_on)},{"status",status_json(contract.status)},
        {"cause",contract.cause}});
        if (contract.customer_advance.value()>0) {
            contracts.back()["customer_advance_cents"]=contract.customer_advance.value();
            contracts.back()["advance_applied_cents"]=contract.advance_applied.value();
        }
    }
    const core::MarketDay* prior = nullptr;
    for (const auto& closed:state.days) {
        if (packed && prior && prior->contracts == closed.contracts && prior->discretionary == closed.discretionary &&
            prior->segments == closed.segments && prior->allocation_rounds == closed.allocation_rounds &&
            prior->total_demand == closed.total_demand) {
            days.repeat();
            continue;
        }
        prior = &closed;
        Json contract_results=Json::array(),discretionary=Json::array(),segment_results=Json::array();
        for (const auto& contract:closed.contracts) contract_results.push_back({
            {"contract_id",contract.contract_id},{"promised_tasks",contract.promised_tasks},
            {"delivered_tasks",contract.delivered_tasks},{"gross_fee_cents",contract.gross_fee.value()},
            {"service_credit_cents",contract.service_credit.value()},{"usage_charge_cents",contract.usage_charge.value()},
            {"penalty_cents",contract.penalty.value()}});
        for (const auto& delivery:closed.discretionary) {
            discretionary.push_back({{"segment_id",delivery.segment_id},{"product_id",delivery.product_id},
                {"tasks",delivery.tasks},{"charged_cents",delivery.charged.value()}});
            if (delivery.customer_compute.value() != 0)
                discretionary.back()["customer_compute_milli_CU"] = delivery.customer_compute.value();
            if (delivery.trial_tasks>0) discretionary.back()["trial_tasks"]=delivery.trial_tasks;
        }
        for (const auto& segment:closed.segments) segment_results.push_back({
            {"segment_id",segment.segment_id},{"demand",segment.demand},{"budget_cents",segment.budget.value()},
            {"contract_tasks",segment.contract_tasks},{"discretionary_tasks",segment.discretionary_tasks},
            {"outside_tasks",segment.outside_tasks},{"charged_cents",segment.charged.value()},
            {"unspent_cents",segment.unspent.value()},{"outside_reason",segment.outside_reason}});
        Json daily{{"contracts",std::move(contract_results)},
            {"discretionary",std::move(discretionary)},{"segments",std::move(segment_results)},
            {"allocation_rounds",closed.allocation_rounds}};
        if (!packed) daily["day"] = closed.day.value();
        if (closed.total_demand) daily["total_demand"] = *closed.total_demand;
        days.push(std::move(daily));
    }
    for (const auto& adoption:state.adoptions) {
        Json shares=Json::array();
        for (const auto& share:adoption.shares) shares.push_back({
            {"product_id",share.product_id},{"share_basis_points",share.share.value()}});
        adoptions.push({{"reviewed_on",adoption.reviewed_on.value()},{"segment_id",adoption.segment_id},
            {"observed_tasks",adoption.observed_tasks},{"shares",std::move(shares)}});
    }
    Json bindings=Json::array();
    for (const auto& row:state.capacity_bindings) bindings.push_back({{"product",row.product_id},{"project",row.project_id},{"starts",row.starts_on.value()}});
    Json changes=Json::array();
    for (const auto& row:state.demand_changes) changes.push_back({{"id",row.id},{"segment_id",row.segment_id},
        {"starts_on",row.starts_on.value()},{"expires_before",row.expires_before.value()},{"delta_bp",row.delta.value()}});
    Json prices=Json::array();
    for (const auto& row:state.prices) prices.push_back({{"product",row.product_id},{"starts",row.starts_on.value()},{"price",row.price.value()}});
    Json result{{"owner",state.owner},{"starts_on",state.starts_on.value()},
        {"closed_through",optional_day(state.closed_through)},{"products",std::move(products)},
        {"segments",std::move(segments)},{"contracts",std::move(contracts)},
        {"days",days.finish()},{"adoptions",adoptions.finish()},{"demand_changes",std::move(changes)},{"capacity_bindings",std::move(bindings)},{"prices",std::move(prices)}};
    if (!state.price_events.empty()) {
        Json events=Json::array();
        for (const auto& e:state.price_events) events.push_back({{"product",e.product_id},{"day",e.day.value()},
            {"before",e.before.value()},{"after",e.after.value()},{"actor",e.actor}});
        result["price_events"]=std::move(events);
    }
    if (!state.trials.empty()) {
        result["service_trials"]=Json::array();
        for (const auto& trial:state.trials) result["service_trials"].push_back({
            {"product",trial.product_id},{"project",trial.project_id},{"starts",trial.starts_on.value()},
            {"ends",trial.ends_before.value()},{"share_bp",trial.share.value()},{"stopped_on",optional_day(trial.stopped_on)}});
    }
    if (!state.compatibility.empty()) {
        result["compatibility"]=Json::array();
        for (const auto& term:state.compatibility) result["compatibility"].push_back({
            {"product",term.product_id},{"project",term.project_id},{"starts",term.starts_on.value()},{"ends",term.ends_before.value()}});
    }
    return result;
}

core::MarketState parse_market(const Json& value, bool packed) {
    fields(value,"market",{"owner","starts_on","closed_through","products","segments","contracts","days","adoptions"},
        {"demand_changes","capacity_bindings","prices","price_events","service_trials","compatibility"});
    core::MarketState result;
    if (value.contains("prices")) {
        array(value.at("prices"),"market/prices",core::market_price_history_limit);
        for (const auto& row:value.at("prices")) {
            fields(row,"market/price",{"product","starts","price"});
            result.prices.push_back({label(row.at("product")),day(row.at("starts")),cents(row.at("price"))});
        }
    }
    if (value.contains("price_events")) {
        array(value.at("price_events"),"market/price_events",core::market_price_history_limit);
        for (const auto& e:value.at("price_events")) {
            fields(e,"market/price_event",{"product","day","before","after","actor"});
            result.price_events.push_back({label(e.at("product")),day(e.at("day")),cents(e.at("before")),cents(e.at("after")),text(e.at("actor"),"market/actor",32,true)});
        }
    }
    if (value.contains("capacity_bindings")) {
        array(value.at("capacity_bindings"),"market/bindings",4096);
        for (const auto& row:value.at("capacity_bindings")) {
            fields(row,"market/binding",{"product","project","starts"});
            result.capacity_bindings.push_back({label(row.at("product")),label(row.at("project")),day(row.at("starts"))});
        }
    }
    if (value.contains("service_trials")) {
        const auto& trials=value.at("service_trials");
        array(trials,"market/service_trials",core::service_trial_limit);
        for (const auto& trial:trials) {
            fields(trial,"market/service_trial",{"product","project","starts","ends","share_bp","stopped_on"});
            result.trials.push_back({label(trial.at("product")),label(trial.at("project")),day(trial.at("starts")),
                day(trial.at("ends")),core::BasisPoints{i64(trial.at("share_bp"),"market/service_trial/share")},
                parse_optional_day(trial.at("stopped_on"))});
        }
    }
    if (value.contains("compatibility")) {
        const auto& terms=value.at("compatibility");
        array(terms,"market/compatibility",core::service_compatibility_limit);
        for (const auto& term:terms) {
            fields(term,"market/compatibility",{"product","project","starts","ends"});
            result.compatibility.push_back({label(term.at("product")),label(term.at("project")),day(term.at("starts")),day(term.at("ends"))});
        }
    }
    result.owner=label(value.at("owner"));
    result.starts_on=day(value.at("starts_on"));
    result.closed_through=parse_optional_day(value.at("closed_through"));
    if (value.contains("demand_changes")) {
        array(value.at("demand_changes"),"market/demand_changes",64);
        for (const auto& row:value.at("demand_changes")) {
            fields(row,"market/demand_change",{"id","segment_id","starts_on","expires_before","delta_bp"});
            result.demand_changes.push_back({label(row.at("id")),label(row.at("segment_id")),day(row.at("starts_on")),
                day(row.at("expires_before")),core::BasisPointDelta{i64(row.at("delta_bp"),"market/demand_change/delta_bp")}});
        }
    }
    const auto& products=value.at("products");array(products,"market/products",core::market_product_limit);
    for (const auto& product:products) {
        fields(product,"market/product",{"id","capacity_project_id","price_per_task_cents","available"},{"channel","monthly_migration_bp","region","retirement"});
        result.products.push_back({label(product.at("id")),label(product.at("capacity_project_id")),
            cents(product.at("price_per_task_cents")),boolean(product.at("available"),"market/product/available"),
            product.contains("channel") ? core::parse_channel(label(product.at("channel"))) : core::ProductChannel::legacy,
            core::BasisPoints{product.contains("monthly_migration_bp") ? i64(product.at("monthly_migration_bp"),"market/product/migration") : 10000},
            product.contains("region") ? label(product.at("region")) : std::string{}});
        if (product.contains("retirement")) {
            const auto& notice=product.at("retirement");
            fields(notice,"market/product/retirement",{"announced","retires"});
            result.products.back().retirement=core::ProductRetirement{day(notice.at("announced")),day(notice.at("retires"))};
        }
    }
    const auto& segments=value.at("segments");array(segments,"market/segments",core::market_segment_limit);
    for (const auto& segment:segments) {
        fields(segment,"market/segment",{"id","daily_tasks","daily_budget_cents","outside_utility","reservation_threshold",
            "maximum_monthly_migration_basis_points","preferences"},
            {"region","customer_compute_milli_CU_per_day","customer_memory_mib","customer_interconnect","requires_local"});
        core::MarketSegment row;
        row.id=label(segment.at("id"));row.daily_tasks=tasks(segment.at("daily_tasks"));
        row.daily_budget=cents(segment.at("daily_budget_cents"));row.outside_utility=utility(segment.at("outside_utility"));
        row.reservation_threshold=utility(segment.at("reservation_threshold"));
        row.maximum_monthly_migration=core::BasisPoints{i64(segment.at("maximum_monthly_migration_basis_points"),"market/migration")};
        if (segment.contains("region")) {
            row.region_id = label(segment.at("region"));
            row.customer_compute = core::MilliCUPerDay{i64(segment.at("customer_compute_milli_CU_per_day"),"market/customer_compute")};
            row.customer_memory_mib = u64(segment.at("customer_memory_mib"),"market/customer_memory");
            row.customer_interconnect = text(segment.at("customer_interconnect"),"market/customer_interconnect",64,true);
            row.requires_local = boolean(segment.at("requires_local"),"market/requires_local");
        } else if (segment.size() != 7) {
            invalid("market/segment", "Customer hardware fields require a region");
        }
        const auto& preferences=segment.at("preferences");array(preferences,"market/preferences",core::market_product_limit);
        for (const auto& preference:preferences) {
            fields(preference,"market/preference",{"product_id","utility","willingness_to_pay_cents","switching_cost","eligible"});
            row.preferences.push_back({label(preference.at("product_id")),utility(preference.at("utility")),
                cents(preference.at("willingness_to_pay_cents")),utility(preference.at("switching_cost")),
                boolean(preference.at("eligible"),"market/preference/eligible")});
        }
        result.segments.push_back(std::move(row));
    }
    const auto& contracts=value.at("contracts");array(contracts,"market/contracts",core::market_contract_limit);
    for (const auto& contract:contracts) {
        fields(contract,"market/contract",{"id","segment_id","product_id","tasks_per_day","monthly_fee_cents","usage_price_cents",
            "service_credit_basis_points","missing_task_penalty_cents","missed_start_penalty_cents","cancellation_penalty_cents",
            "accepted_on","start_by","ends_before","started_on","stopped_on","status","cause"},
            {"customer_advance_cents","advance_applied_cents"});
        result.contracts.push_back({label(contract.at("id")),label(contract.at("segment_id")),label(contract.at("product_id")),
            tasks(contract.at("tasks_per_day")),cents(contract.at("monthly_fee_cents")),cents(contract.at("usage_price_cents")),
            core::BasisPoints{i64(contract.at("service_credit_basis_points"),"market/service_credit_basis_points")},
            cents(contract.at("missing_task_penalty_cents")),cents(contract.at("missed_start_penalty_cents")),
            cents(contract.at("cancellation_penalty_cents")),day(contract.at("accepted_on")),day(contract.at("start_by")),
            day(contract.at("ends_before")),parse_optional_day(contract.at("started_on")),parse_optional_day(contract.at("stopped_on")),
            parse_status(contract.at("status")),label(contract.at("cause"))});
        if (contract.contains("customer_advance_cents")!=contract.contains("advance_applied_cents"))
            invalid("market/contract","Customer advance and application totals must be present together");
        if (contract.contains("customer_advance_cents")) {
            result.contracts.back().customer_advance=cents(contract.at("customer_advance_cents"));
            result.contracts.back().advance_applied=cents(contract.at("advance_applied_cents"));
        }
    }
    std::size_t records=0;
    read_history_rows(value.at("days"), "market/days", core::market_day_limit, packed, [&](const Json& closed, bool repeat) {
        const auto closed_day = packed ? core::advance_days(result.starts_on, core::Days{result.days.size()}) : day(closed.at("day"));
        if (repeat) {
            auto row = result.days.back();
            row.day = closed_day;
            count_records(records, row.contracts.size() + row.discretionary.size() + row.segments.size());
            result.days.push_back(std::move(row));
            return;
        }
        if (packed) fields(closed,"market/day",{"contracts","discretionary","segments","allocation_rounds"},{"total_demand"});
        else fields(closed,"market/day",{"day","contracts","discretionary","segments","allocation_rounds"},{"total_demand"});
        core::MarketDay row;
        row.day = closed_day;
        row.allocation_rounds=u64(closed.at("allocation_rounds"),"market/allocation_rounds");
        if (closed.contains("total_demand"))
            row.total_demand = u64(closed.at("total_demand"),"market/total_demand",core::market_segment_limit*core::market_task_limit);
        const auto& contract_results=closed.at("contracts");array(contract_results,"market/day/contracts",core::market_contract_limit);
        count_records(records,contract_results.size());
        for (const auto& contract:contract_results) {
            fields(contract,"market/contract_result",{"contract_id","promised_tasks","delivered_tasks","gross_fee_cents",
                "service_credit_cents","usage_charge_cents","penalty_cents"});
            row.contracts.push_back({label(contract.at("contract_id")),tasks(contract.at("promised_tasks")),
                tasks(contract.at("delivered_tasks")),cents(contract.at("gross_fee_cents")),cents(contract.at("service_credit_cents")),
                cents(contract.at("usage_charge_cents")),cents(contract.at("penalty_cents"))});
        }
        const auto& discretionary=closed.at("discretionary");array(discretionary,"market/day/discretionary",core::market_segment_limit*core::market_product_limit);
        count_records(records,discretionary.size());
        for (const auto& delivery:discretionary) {
            fields(delivery,"market/delivery",{"segment_id","product_id","tasks","charged_cents"},{"customer_compute_milli_CU","trial_tasks"});
            row.discretionary.push_back({label(delivery.at("segment_id")),label(delivery.at("product_id")),
                tasks(delivery.at("tasks")),cents(delivery.at("charged_cents")),
                core::MilliCU{delivery.contains("customer_compute_milli_CU") ? i64(delivery.at("customer_compute_milli_CU"),"market/delivery/compute") : 0},
                delivery.contains("trial_tasks")?tasks(delivery.at("trial_tasks")):0});
        }
        const auto& segment_results=closed.at("segments");array(segment_results,"market/day/segments",core::market_segment_limit);
        count_records(records,segment_results.size());
        for (const auto& segment:segment_results) {
            fields(segment,"market/segment_result",{"segment_id","demand","budget_cents","contract_tasks","discretionary_tasks",
                "outside_tasks","charged_cents","unspent_cents","outside_reason"});
            row.segments.push_back({label(segment.at("segment_id")),tasks(segment.at("demand")),cents(segment.at("budget_cents")),
                tasks(segment.at("contract_tasks")),tasks(segment.at("discretionary_tasks")),tasks(segment.at("outside_tasks")),
                cents(segment.at("charged_cents")),cents(segment.at("unspent_cents")),
                text(segment.at("outside_reason"),"market/outside_reason",256,true)});
        }
        if (!result.days.empty()) row.share_unchanged(result.days.back());
        result.days.push_back(std::move(row));
    });
    const auto& adoptions=value.at("adoptions");
    read_history_rows(adoptions, "market/adoptions", core::market_record_limit,
                      packed && !adoptions.is_array(), [&](const Json& adoption) {
        fields(adoption,"market/adoption",{"reviewed_on","segment_id","observed_tasks","shares"});
        core::MarketAdoption row;
        row.reviewed_on=day(adoption.at("reviewed_on"));row.segment_id=label(adoption.at("segment_id"));
        row.observed_tasks=u64(adoption.at("observed_tasks"),"market/adoption/observed_tasks");
        const auto& shares=adoption.at("shares");array(shares,"market/adoption/shares",core::market_product_limit+1);
        for (const auto& share:shares) {
            fields(share,"market/share",{"product_id","share_basis_points"});
            row.shares.push_back({label(share.at("product_id")),core::BasisPoints{i64(share.at("share_basis_points"),"market/share")}});
        }
        result.adoptions.push_back(std::move(row));
    });
    return result;
}
}
