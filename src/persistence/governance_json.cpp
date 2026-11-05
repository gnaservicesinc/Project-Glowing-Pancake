#include "governance_json.hpp"
namespace pgp::persistence::detail {
namespace {
std::string label(const Json& j) {return text(j,"governance/text",256);}
core::OrdinalDay day(const Json& j) {return core::OrdinalDay{i64(j,"governance/day")};}
core::Cents cents(const Json& j) {return core::Cents{i64(j,"governance/cents")};}
core::BasisPoints bp(const Json& j) {return core::BasisPoints{i64(j,"governance/bp")};}
Json optional_day(std::optional<core::OrdinalDay> d) {return d?Json(d->value()):Json(nullptr);}
std::optional<core::OrdinalDay> optional_day(const Json& j) {return j.is_null()?std::nullopt:std::optional{day(j)};}
}
Json governance_json(const core::GovernanceState& g) {
    Json opening=Json::array(),appointments=Json::array(),grants=Json::array(),capital=Json::array(),motions=Json::array(),disputes=Json::array();
    for (const auto& h:g.opening_holdings) opening.push_back({{"holder",h.holder},{"shares",h.shares.value()},{"pool",h.reserved_pool}});
    for (const auto& a:g.appointments) appointments.push_back({{"candidate",a.candidate},{"hybrid",a.hybrid},{"hired_on",a.hired_on.value()},{"departed_on",optional_day(a.departed_on)}});
    for (const auto& r:g.grants) grants.push_back({{"candidate",r.candidate},{"unvested",r.unvested.value()},{"departed",r.departed}});
    for (const auto& e:g.capital) capital.push_back({{"kind",e.kind},{"subject",e.subject},{"key",e.key},{"day",e.day.value()},{"shares",e.shares.value()},{"consideration",e.consideration.value()}});
    for (const auto& m:g.motions) {
        Json ballots=Json::array();for (const auto& b:m.ballots) ballots.push_back({{"member",b.member},{"seats",b.seats},{"support",b.support}});
        motions.push_back({{"id",m.id},{"subject",m.subject},{"kind",static_cast<unsigned>(m.kind)},{"status",static_cast<unsigned>(m.status)},
            {"opened_on",m.opened_on.value()},{"deadline",m.deadline.value()},{"resolved_on",optional_day(m.resolved_on)},{"cash",m.cash.value()},{"asset_value",m.asset_value.value()},
            {"reviewed_cash",m.reviewed_cash.value()},{"shares",m.shares.value()},{"posting_cursor",m.posting_cursor},{"player_vote",m.player_vote?Json(*m.player_vote):Json(nullptr)},{"ballots",std::move(ballots)},{"executed",m.executed}});
    }
    for (const auto& d:g.disputes) disputes.push_back({{"id",d.id},{"subject",d.subject},{"kind",d.kind},{"opened_on",d.opened_on.value()},{"deadline",d.deadline.value()},
        {"status",static_cast<unsigned>(d.status)},{"resolved_on",optional_day(d.resolved_on)},{"acknowledged",d.acknowledged},{"opening_cash",d.opening_cash.value()},{"posting_cursor",d.posting_cursor},{"publication_cursor",d.publication_cursor}});
    return {{"opening_holdings",std::move(opening)},{"appointments",std::move(appointments)},{"grants",std::move(grants)},{"capital",std::move(capital)},{"motions",std::move(motions)},{"disputes",std::move(disputes)}};
}
core::GovernanceState parse_governance(const Json& j) {
    fields(j,"governance",{"opening_holdings","appointments","grants","capital","motions","disputes"});
    array(j.at("opening_holdings"),"governance/opening",core::accounts_terms_limit);array(j.at("appointments"),"governance/appointments",8);array(j.at("grants"),"governance/grants",8);
    array(j.at("capital"),"governance/capital",8192);array(j.at("motions"),"governance/motions",128);array(j.at("disputes"),"governance/disputes",32);
    core::GovernanceState g;
    for (const auto& h:j.at("opening_holdings")) {fields(h,"governance/holding",{"holder","shares","pool"});g.opening_holdings.push_back({label(h.at("holder")),bp(h.at("shares")),boolean(h.at("pool"),"governance/pool")});}
    for (const auto& a:j.at("appointments")) {fields(a,"governance/appointment",{"candidate","hybrid","hired_on","departed_on"});g.appointments.push_back({label(a.at("candidate")),boolean(a.at("hybrid"),"governance/hybrid"),day(a.at("hired_on")),optional_day(a.at("departed_on"))});}
    for (const auto& r:j.at("grants")) {fields(r,"governance/grant",{"candidate","unvested","departed"});g.grants.push_back({label(r.at("candidate")),bp(r.at("unvested")),boolean(r.at("departed"),"governance/departed")});}
    for (const auto& e:j.at("capital")) {fields(e,"governance/capital-event",{"kind","subject","key","day","shares","consideration"});g.capital.push_back({label(e.at("kind")),label(e.at("subject")),label(e.at("key")),day(e.at("day")),bp(e.at("shares")),cents(e.at("consideration"))});}
    for (const auto& m:j.at("motions")) {
        fields(m,"governance/motion",{"id","subject","kind","status","opened_on","deadline","resolved_on","cash","asset_value","reviewed_cash","shares","posting_cursor","player_vote","ballots","executed"});
        core::BoardMotion r;r.id=label(m.at("id"));r.subject=label(m.at("subject"));r.kind=static_cast<core::BoardMotionKind>(u64(m.at("kind"),"governance/motion-kind",2));r.status=static_cast<core::BoardMotionStatus>(u64(m.at("status"),"governance/motion-status",3));
        r.opened_on=day(m.at("opened_on"));r.deadline=day(m.at("deadline"));r.resolved_on=optional_day(m.at("resolved_on"));r.cash=cents(m.at("cash"));r.asset_value=cents(m.at("asset_value"));r.reviewed_cash=cents(m.at("reviewed_cash"));r.shares=bp(m.at("shares"));r.posting_cursor=u64(m.at("posting_cursor"),"governance/posting-cursor",core::accounts_posting_limit);
        if (!m.at("player_vote").is_null()) r.player_vote=boolean(m.at("player_vote"),"governance/vote");
        array(m.at("ballots"),"governance/ballots",16);for (const auto& b:m.at("ballots")) {fields(b,"governance/ballot",{"member","seats","support"});r.ballots.push_back({label(b.at("member")),static_cast<std::uint32_t>(u64(b.at("seats"),"governance/seats",3)),boolean(b.at("support"),"governance/support")});}
        r.executed=boolean(m.at("executed"),"governance/executed");g.motions.push_back(std::move(r));
    }
    for (const auto& d:j.at("disputes")) {fields(d,"governance/dispute",{"id","subject","kind","opened_on","deadline","status","resolved_on","acknowledged","opening_cash","posting_cursor","publication_cursor"});g.disputes.push_back({label(d.at("id")),label(d.at("subject")),label(d.at("kind")),day(d.at("opened_on")),day(d.at("deadline")),static_cast<core::CorporateDisputeStatus>(u64(d.at("status"),"governance/dispute-status",5)),optional_day(d.at("resolved_on")),boolean(d.at("acknowledged"),"governance/acknowledged"),cents(d.at("opening_cash")),u64(d.at("posting_cursor"),"governance/posting-cursor",core::accounts_posting_limit),u64(d.at("publication_cursor"),"governance/publication-cursor",32)});}
    return g;
}
}
