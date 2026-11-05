#include <pgp/content/governance_catalog.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <set>
#include <stdexcept>
namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid() {throw std::invalid_argument("Corporate catalog has missing, unsupported or invalid fields");}
void fields(const Json& j,std::initializer_list<std::string_view> names) {if (!j.is_object() || j.size()!=names.size()) invalid();for (const auto n:names) if (!j.contains(n)) invalid();}
std::int64_t integer(const Json& j) {if (!j.is_number_integer() || (j.is_number_unsigned() && j.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) invalid();return j.get<std::int64_t>();}
std::string text(const Json& j) {if (!j.is_string()) invalid();auto s=j.get<std::string>();if (s.size()>512 || s.find('\0')!=std::string::npos) invalid();return s;}
bool boolean(const Json& j) {if (!j.is_boolean()) invalid();return j.get<bool>();}
void array(const Json& j,std::size_t max) {if (!j.is_array() || j.size()>max) invalid();}
std::uint32_t count(const Json& j,std::uint32_t max) {const auto n=integer(j);if (n<0 || n>max) invalid();return static_cast<std::uint32_t>(n);}
}
core::GovernanceCatalog load_governance_catalog(std::string_view bytes) {
    if (bytes.size()>16384) invalid();std::vector<std::set<std::string>> keys;std::size_t tokens=0;
    const auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>12 || ++tokens>4096) invalid();
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid();return true;
    });
    fields(j,{"financing","executives","board","vote_window_days","concern_period_days","response_window_days","maximum_pool_expansion_bp","acquisition_equity_bp","acquisition_premium_cents","subsidiary_cash_floor_cents","subsidiary_reserve_days"});
    core::GovernanceCatalog c;array(j.at("financing"),8);array(j.at("executives"),8);array(j.at("board"),8);
    for (const auto& f:j.at("financing")) {
        fields(f,{"id","name","negotiation","proceeds_cents","covenant_cash_floor_cents","waiver_fee_cents","acquired_bp","monthly_interest_bp","term_days","board_seat"});
        c.financing.push_back({text(f.at("id")),text(f.at("name")),text(f.at("negotiation")),core::Cents{integer(f.at("proceeds_cents"))},core::Cents{integer(f.at("covenant_cash_floor_cents"))},core::Cents{integer(f.at("waiver_fee_cents"))},
            core::BasisPoints{integer(f.at("acquired_bp"))},core::BasisPoints{integer(f.at("monthly_interest_bp"))},core::Days{integer(f.at("term_days"))},boolean(f.at("board_seat"))});
    }
    for (const auto& e:j.at("executives")) {
        fields(e,{"id","name","role","priorities","publication_research","signing_cost_cents","cash_salary_cents","hybrid_salary_cents","hybrid_grant_bp","cliff_days","vesting_days","promise_term_days","technical","commercial","risk_tolerance"});
        c.executives.push_back({text(e.at("id")),text(e.at("name")),text(e.at("role")),text(e.at("priorities")),text(e.at("publication_research")),core::Cents{integer(e.at("signing_cost_cents"))},core::Cents{integer(e.at("cash_salary_cents"))},core::Cents{integer(e.at("hybrid_salary_cents"))},
            core::BasisPoints{integer(e.at("hybrid_grant_bp"))},core::Days{integer(e.at("cliff_days"))},core::Days{integer(e.at("vesting_days"))},core::Days{integer(e.at("promise_term_days"))},count(e.at("technical"),100),count(e.at("commercial"),100),count(e.at("risk_tolerance"),100)});
    }
    for (const auto& b:j.at("board")) {
        fields(b,{"id","name","seats","player","cash_floor_cents","maximum_dilution_bp"});
        c.board.push_back({text(b.at("id")),text(b.at("name")),count(b.at("seats"),3),boolean(b.at("player")),core::Cents{integer(b.at("cash_floor_cents"))},core::BasisPoints{integer(b.at("maximum_dilution_bp"))}});
    }
    c.vote_window=core::Days{integer(j.at("vote_window_days"))};c.concern_period=core::Days{integer(j.at("concern_period_days"))};c.response_window=core::Days{integer(j.at("response_window_days"))};
    c.maximum_pool_expansion=core::BasisPoints{integer(j.at("maximum_pool_expansion_bp"))};c.acquisition_equity=core::BasisPoints{integer(j.at("acquisition_equity_bp"))};c.acquisition_premium=core::Cents{integer(j.at("acquisition_premium_cents"))};
    c.subsidiary_cash_floor=core::Cents{integer(j.at("subsidiary_cash_floor_cents"))};c.subsidiary_reserve_days=core::Days{integer(j.at("subsidiary_reserve_days"))};
    core::validate_governance_catalog(c);return c;
}
std::string canonical_governance_catalog(const core::GovernanceCatalog& c) {
    Json f=Json::array(),e=Json::array(),b=Json::array();
    for (const auto& r:c.financing) f.push_back({{"id",r.id},{"name",r.name},{"negotiation",r.negotiation},{"proceeds_cents",r.proceeds.value()},{"covenant_cash_floor_cents",r.covenant_cash_floor.value()},{"waiver_fee_cents",r.waiver_fee.value()},{"acquired_bp",r.acquired.value()},{"monthly_interest_bp",r.monthly_interest.value()},{"term_days",r.term.value()},{"board_seat",r.board_seat}});
    for (const auto& r:c.executives) e.push_back({{"id",r.id},{"name",r.name},{"role",r.role},{"priorities",r.priorities},{"publication_research",r.publication_research},{"signing_cost_cents",r.signing_cost.value()},{"cash_salary_cents",r.cash_salary.value()},{"hybrid_salary_cents",r.hybrid_salary.value()},{"hybrid_grant_bp",r.hybrid_grant.value()},{"cliff_days",r.cliff.value()},{"vesting_days",r.vesting.value()},{"promise_term_days",r.promise_term.value()},{"technical",r.technical},{"commercial",r.commercial},{"risk_tolerance",r.risk_tolerance}});
    for (const auto& r:c.board) b.push_back({{"id",r.id},{"name",r.name},{"seats",r.seats},{"player",r.player},{"cash_floor_cents",r.cash_floor.value()},{"maximum_dilution_bp",r.maximum_dilution.value()}});
    return Json{{"financing",std::move(f)},{"executives",std::move(e)},{"board",std::move(b)},{"vote_window_days",c.vote_window.value()},{"concern_period_days",c.concern_period.value()},{"response_window_days",c.response_window.value()},{"maximum_pool_expansion_bp",c.maximum_pool_expansion.value()},{"acquisition_equity_bp",c.acquisition_equity.value()},{"acquisition_premium_cents",c.acquisition_premium.value()},{"subsidiary_cash_floor_cents",c.subsidiary_cash_floor.value()},{"subsidiary_reserve_days",c.subsidiary_reserve_days.value()}}.dump();
}
}
