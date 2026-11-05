#include "mergers_json.hpp"
namespace pgp::persistence::detail {
namespace {
std::string label(const Json& j) {return text(j,"merger/text",128);}
core::Cents cents(const Json& j) {return core::Cents{i64(j,"merger/cents")};}
core::BasisPoints bp(const Json& j) {return core::BasisPoints{i64(j,"merger/shares")};}
core::OrdinalDay day(const Json& j) {return core::OrdinalDay{i64(j,"merger/day")};}
}
Json acquisitions_json(const std::vector<runtime::AcquisitionRecord>& records) {
    Json result=Json::array();for (const auto& q:records) {
        Json sellers=Json::array(),settlements=Json::array(),transfers=Json::array();
        for (const auto& t:q.transfers) transfers.push_back({{"day",t.day.value()},{"amount",t.amount.value()},{"funding",t.funding}});
        for (const auto& h:q.sellers) sellers.push_back({{"holder",h.holder},{"shares",h.shares.value()},{"pool",h.reserved_pool}});
        for (const auto& p:q.settlements) settlements.push_back({{"holder",p.holder},{"cash",p.cash.value()},{"shares",p.shares.value()}});
        result.push_back({{"motion",q.motion_id},{"target",q.target},{"quoted_on",q.quoted_on.value()},{"completed_on",q.completed_on?Json(q.completed_on->value()):Json(nullptr)},
            {"value",q.value.value()},{"cash",q.cash.value()},{"shares",q.shares.value()},{"buyer_posting_cursor",q.buyer_posting_cursor},{"target_posting_cursor",q.target_posting_cursor},{"acquired_posting_cursor",q.acquired_posting_cursor},{"acquired_profit",q.acquired_profit.value()},{"sellers",std::move(sellers)},{"settlements",std::move(settlements)},{"transfers",std::move(transfers)}});
    }return result;
}
std::vector<runtime::AcquisitionRecord> parse_acquisitions(const Json& j) {
    array(j,"mergers",32);std::vector<runtime::AcquisitionRecord> result;
    for (const auto& q:j) {
        fields(q,"merger",{"motion","target","quoted_on","completed_on","value","cash","shares","buyer_posting_cursor","target_posting_cursor","acquired_posting_cursor","acquired_profit","sellers","settlements","transfers"});
        runtime::AcquisitionRecord r;r.motion_id=label(q.at("motion"));r.target=label(q.at("target"));r.quoted_on=day(q.at("quoted_on"));if (!q.at("completed_on").is_null()) r.completed_on=day(q.at("completed_on"));
        r.value=cents(q.at("value"));r.cash=cents(q.at("cash"));r.shares=bp(q.at("shares"));r.buyer_posting_cursor=u64(q.at("buyer_posting_cursor"),"merger/buyer",core::accounts_posting_limit);r.target_posting_cursor=u64(q.at("target_posting_cursor"),"merger/target",core::accounts_posting_limit);
        r.acquired_posting_cursor=u64(q.at("acquired_posting_cursor"),"merger/acquired-cursor",core::accounts_posting_limit);r.acquired_profit=cents(q.at("acquired_profit"));
        array(q.at("sellers"),"merger/sellers",core::accounts_terms_limit);array(q.at("settlements"),"merger/settlements",core::accounts_terms_limit);
        for (const auto& h:q.at("sellers")) {fields(h,"merger/seller",{"holder","shares","pool"});r.sellers.push_back({label(h.at("holder")),bp(h.at("shares")),boolean(h.at("pool"),"merger/pool")});}
        for (const auto& p:q.at("settlements")) {fields(p,"merger/settlement",{"holder","cash","shares"});r.settlements.push_back({label(p.at("holder")),cents(p.at("cash")),bp(p.at("shares"))});}
        array(q.at("transfers"),"merger/transfers",2048);for (const auto& t:q.at("transfers")) {fields(t,"merger/transfer",{"day","amount","funding"});r.transfers.push_back({day(t.at("day")),cents(t.at("amount")),boolean(t.at("funding"),"merger/funding")});}
        result.push_back(std::move(r));
    }return result;
}
}
