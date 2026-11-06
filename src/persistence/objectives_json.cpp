#include "objectives_json.hpp"
#include "../content/civic_codec.hpp"

namespace pgp::content::civic_codec {
template<> struct Extension<core::ObjectiveKind> {
    static constexpr auto key=core::objective_key;
    static constexpr auto count=core::objective_kinds.size(), maximum_rows=count-1;
};
template<> struct Extension<runtime::CampaignEndCause> {
    static constexpr auto key=runtime::campaign_end_key;
    static constexpr std::size_t count=7;
};
template<> struct Extension<runtime::ObjectiveMode> {
    static constexpr auto key=runtime::objective_mode_key;
    static constexpr std::size_t count=3;
};
template<> struct Extension<core::LiquidityCauseKind> {
    static constexpr std::size_t count=2;
    static std::string_view key(core::LiquidityCauseKind kind) {
        return kind==core::LiquidityCauseKind::payable?"payable":"matured_debt";
    }
};
template<> struct Extension<core::ObjectiveSelection> {
    static void fields(auto& r,auto f) {f("primary",r.primary);f("secondary",r.secondary);}
};
template<> struct Extension<core::DeliveredWork> {
    static void fields(auto& r,auto f) {f("controlled",r.controlled);f("total",r.total);}
};
template<> struct Extension<core::ObjectiveMonth> {
    static constexpr auto maximum_rows=core::utopia_months;
    static void fields(auto& r,auto f) {
        f("month_end",r.month_end);f("delivery",r.delivery);f("population_coverage_bp",r.population_coverage);
        f("output_coverage_bp",r.output_coverage);f("global_welfare_bp",r.global_welfare);f("regional_welfare_bp",r.regional_welfare);
    }
};
template<> struct Extension<runtime::ObjectiveSelectionEvent> {
    static constexpr auto maximum_rows=runtime::objective_selection_limit;
    static void fields(auto& r,auto f) {f("day",r.day);f("selection",r.selection);}
};
template<> struct Extension<runtime::ObjectiveBreach> {
    static void fields(auto& r,auto f) {f("first",r.first);f("failed_checks",r.failed_checks);f("acknowledged_on",r.acknowledged_on);}
};
template<> struct Extension<runtime::AsiRaceResult> {
    static void fields(auto& r,auto f) {f("day",r.day);f("winners",r.winners);}
};
template<> struct Extension<runtime::LegacyValuation> {
    static constexpr std::size_t maximum_rows=33;
    static void fields(auto& r,auto f) {
        f("institution",r.institution);f("realized_assets_cents",r.realized_assets);f("payables_cents",r.payables);f("debt_cents",r.debt);
        f("audited_profit_cents",r.audited_profit);f("company_equity_cents",r.company_equity);f("founder_equity_cents",r.founder_equity);
        f("personal_cash_cents",r.personal_cash);f("personal_liabilities_cents",r.personal_liabilities);f("legacy_wealth_cents",r.legacy_wealth);
    }
};
template<> struct Extension<runtime::SurvivalEvidence> {
    static void fields(auto& r,auto f) {
        f("operating",r.operating);f("serviced_liabilities",r.serviced_liabilities);f("active_team",r.active_team);
        f("paid_tasks",r.paid_tasks);f("paid_wholesale_milli_CU",r.paid_wholesale);f("funded_research_milli_CU",r.funded_research);
    }
};
template<> struct Extension<core::LiquidityCause> {
    static void fields(auto& r,auto f) {f("kind",r.kind);f("reference",r.reference);f("due",r.due);f("unpaid_cents",r.unpaid);f("cause",r.cause);}
};
template<> struct Extension<core::LiquidityExposure> {
    static void fields(auto& r,auto f) {f("unpaid_payables_cents",r.unpaid_payables);f("matured_principal_cents",r.matured_principal);f("oldest",r.oldest);}
};
template<> struct Extension<runtime::CampaignEnd> {
    static void fields(auto& r,auto f) {
        f("day",r.day);f("cause",r.cause);f("source",r.source);f("profit",r.profit);f("survival",r.survival);f("obligations",r.obligations);
        f("posting_cursor",r.posting_cursor);
    }
};
template<> struct Extension<runtime::ObjectivesState> {
    static void fields(auto& r,auto f) {
        f("deadline",r.deadline);f("selections",r.selections);f("months",r.months);f("breaches",r.breaches);f("race",r.race);
        f("end",r.end);f("mode",r.mode);f("observer_day",r.observer_day);f("observer_paused",r.observer_paused);
    }
};
}
namespace pgp::persistence::detail {
Json objectives_json(const runtime::ObjectivesState& value) {return content::civic_codec::encode(value);}
runtime::ObjectivesState parse_objectives(const Json& value) {return content::civic_codec::decode<runtime::ObjectivesState>(value);}
}
