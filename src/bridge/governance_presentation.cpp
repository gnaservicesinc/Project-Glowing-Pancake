#include "governance_presentation.hpp"
#include "flow_presentation.hpp"
#include <QCoreApplication>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString tr(const char* s) {return QCoreApplication::translate("CompanyGovernance",s);}
QString text(std::string_view s) {return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QString money(core::Cents v) {
    const auto magnitude=v.value()<0?static_cast<std::uint64_t>(-(v.value()+1))+1:static_cast<std::uint64_t>(v.value());
    auto n=QString::number(static_cast<qulonglong>(magnitude/100));for (auto i=n.size()-3;i>0;i-=3) n.insert(i,QLatin1Char(','));
    return (v.value()<0?QStringLiteral("−$"):QStringLiteral("$"))+n+QStringLiteral(".%1").arg(magnitude%100,2,10,QLatin1Char('0'));
}
QString percent(core::BasisPoints v) {return QStringLiteral("%1.%2%").arg(v.value()/100).arg(v.value()%100,2,10,QLatin1Char('0'));}
QString date(core::OrdinalDay d) {return text(core::iso_date(d));}
QString motion_status(core::BoardMotionStatus s) {
    switch(s) {case core::BoardMotionStatus::pending:return tr("Awaiting vote");case core::BoardMotionStatus::approved:return tr("Approved");case core::BoardMotionStatus::rejected:return tr("Rejected");case core::BoardMotionStatus::expired:return tr("Expired");}return {};
}
QString dispute_status(core::CorporateDisputeStatus s) {
    switch(s) {case core::CorporateDisputeStatus::concern:return tr("Private concern");case core::CorporateDisputeStatus::formal:return tr("Formal recommendation");case core::CorporateDisputeStatus::resolved:return tr("Resolved");case core::CorporateDisputeStatus::departed:return tr("Executive departed");case core::CorporateDisputeStatus::accelerated:return tr("Debt accelerated");case core::CorporateDisputeStatus::waived:return tr("Covenant waived");}return {};
}
}
GovernanceModels governance_models(const runtime::Checkpoint& checkpoint,const runtime::CompiledProgram& program) {
    GovernanceModels result;const auto& s=checkpoint.world.simulation;
    if (!s.management || !s.management->governance || !s.management->catalog.governance) return result;
    const auto& g=*s.management->governance;const auto& c=*s.management->catalog.governance;
    const auto board=core::current_board(s);
    const auto subject_label=[&](const std::string& id) {
        if (id=="option_pool") return tr("Reserved option pool");
        for (const auto& f:c.financing) if (f.id==id) return text(f.name);
        for (const auto& e:c.executives) if (e.id==id) return text(e.name);
        if (checkpoint.world.competition) for (const auto& r:checkpoint.world.competition->companies) if (r.definition_id==id) return text(runtime::rival_definition(program,id).name);
        return readable(id);
    };
    result.ownership=tr("Fully diluted ownership totals 100.00%. Unallocated option pool: %1. Unvested grants are reserved inside the existing pool. Vesting moves earned ownership; it creates no cash or second allocation.\n").arg(percent(core::unallocated_option_pool(s)));
    for (const auto& h:s.accounts->holdings) {
        QString label=text(h.holder);
        if (h.holder=="founder") label=tr("Founder");else if (h.holder=="cofounder") label=tr("Cofounder");else if (h.holder=="option_pool") label=tr("Reserved option pool");else if (h.holder=="seed_investor") label=tr("Seed investor");
        for (const auto& e:c.executives) if (h.holder=="executive.grant/"+e.id) label=tr("%1 · vested grant").arg(text(e.name));
        for (const auto& f:c.financing) if (h.holder=="investor/"+f.id) label=text(f.name);
        for (const auto& f:s.management->catalog.financing) if (h.holder=="investor/"+f.id) label=text(f.name);
        if (checkpoint.world.competition) for (const auto& q:checkpoint.world.competition->acquisitions) if (h.holder=="merger.sellers/"+q.target) label=tr("%1 · seller trust").arg(text(runtime::rival_definition(program,q.target).name));
        result.ownership+=tr("%1: %2%3\n").arg(label,percent(h.shares),h.reserved_pool?tr(" (reserved pool)"):QString{});
    }
    if (checkpoint.world.competition && !checkpoint.world.competition->acquisitions.empty()) {
        const auto group=runtime::owned_group_report(checkpoint.world);
        result.ownership+=tr("Owned group: cash %1 · consolidated book net assets %2 · operating profit while owned %3. Investment assets and internal dividend income are eliminated from these group totals.\n").arg(money(group.cash),money(group.net_assets),money(group.operating_profit));
    }
    result.board=tr("Board motions have a %1-day response window. A majority of seats decides; financing dilution does not itself change seats. Each nonplayer position uses cash after the proposed action and its maximum dilution. Review the recorded votes after deciding.\n").arg(c.vote_window.value());
    for (const auto& member:board) result.board+=tr("%1: %2 seats · %3\n").arg(text(member.name)).arg(member.seats).arg(member.player?tr("your vote"):tr("supports cash ≥ %1 and dilution ≤ %2").arg(money(member.cash_floor),percent(member.maximum_dilution)));
    for (const auto& f:c.financing) {
        const bool used=std::any_of(g.motions.begin(),g.motions.end(),[&](const auto& m){return m.kind==core::BoardMotionKind::financing && (m.status==core::BoardMotionStatus::pending || m.executed) && std::any_of(c.financing.begin(),c.financing.end(),[&](const auto& prior){return prior.id==m.subject && prior.negotiation==f.negotiation;});});
        auto detail=tr("Negotiate this fixed counteroffer, then submit it to the board. Proposing terms pays nothing. Approval rechecks and commits the exact terms once.\nCash proceeds: %1. New fully diluted ownership: %2. Board representative: %3.\n").arg(money(f.proceeds),percent(f.acquired),f.board_seat?tr("one seat"):tr("none"));
        if (f.term.value()>0) detail+=tr("Loan term %1 days · interest %2 per month · principal remains due. Cash covenant: at least %3. A breach first raises a concern, then a formal recommendation after %4 days and a %5-day response window. Restore cash, repay the debt, or buy the one permanent covenant waiver for %6; maturity and interest remain. Unresolved breach accelerates maturity to no later than 30 days after the response deadline.\n").arg(f.term.value()).arg(percent(f.monthly_interest),money(f.covenant_cash_floor)).arg(c.concern_period.value()).arg(c.response_window.value()).arg(money(f.waiver_fee));
        else detail+=tr("Equity proceeds are contributed capital, not product revenue. All existing holdings and unvested claims dilute once.\n");
        result.financing.push_back({text(f.id),text(f.name),money(f.proceeds),detail,used?tr("Pending or completed"):tr("Counteroffer available"),!used,{}});
    }
    for (const auto& e:c.executives) {
        const auto a=std::find_if(g.appointments.begin(),g.appointments.end(),[&](const auto& v){return v.candidate==e.id;});
        auto detail=tr("Role: %1. Priorities: %2\nTechnical %3 / commercial %4 / risk tolerance %5 (scenario descriptors; no instant model-quality bonus).\nSigning cash %6. Cash-only salary %7/month, or agreed hybrid salary %8/month plus %9 from the existing option pool. One paid staff member is added.\nCliff %10 days; cumulative vesting over %11 days. Departure keeps vested claims and returns only the unvested remainder. An accepted appointment cannot be signed again.\n").arg(text(e.role),text(e.priorities)).arg(e.technical).arg(e.commercial).arg(e.risk_tolerance).arg(money(e.signing_cost),money(e.cash_salary),money(e.hybrid_salary),percent(e.hybrid_grant)).arg(e.cliff.value()).arg(e.vesting.value());
        if (!e.publication_research.empty()) detail+=tr("Promised publication: %1 within %2 days of appointment. A missed promise has a concern, formal recommendation and %3-day response window. Publish the actual completed research to resolve it; unresolved commitment leads to departure.\n").arg(readable(e.publication_research)).arg(e.promise_term.value()).arg(c.response_window.value());
        if (a!=g.appointments.end()) {
            detail+=tr("Hired %1 under %2 terms.\n").arg(date(a->hired_on),a->hybrid?tr("cash plus equity"):tr("cash-only"));
            if (a->departed_on) detail+=tr("Departed %1. Historical payroll, work and earned ownership remain.\n").arg(date(*a->departed_on));
            for (const auto& grant:g.grants) if (grant.candidate==e.id) detail+=tr("Current unvested claim: %1.\n").arg(percent(grant.unvested));
        }
        result.executives.push_back({text(e.id),text(e.name),text(e.role),detail,a==g.appointments.end()?tr("Negotiable"):a->departed_on?tr("Departed"):tr("Appointed"),a==g.appointments.end(),{{QStringLiteral("hybrid"),e.hybrid_grant.value()>0 && e.hybrid_grant<=core::unallocated_option_pool(s)}}});
    }
    for (const auto& m:g.motions) {
        auto detail=tr("Subject: %1. Opened %2; vote before %3.\nCash %4; explicit dilution %5. Approval commits financing/pool changes or the reviewed acquisition atomically. No unvoted offer is accepted automatically.\n").arg(subject_label(m.subject),date(m.opened_on),date(m.deadline),money(m.cash),percent(m.shares));
        if (m.resolved_on) detail+=tr("%1 on %2.\n").arg(motion_status(m.status),date(*m.resolved_on));
        for (const auto& b:m.ballots) {QString member=readable(b.member);for (const auto& seat:board) if (seat.id==b.member) member=text(seat.name);detail+=tr("%1: %2 seats · %3\n").arg(member).arg(b.seats).arg(b.support?tr("support"):tr("oppose"));}
        result.motions.push_back({text(m.id),subject_label(m.subject),percent(m.shares),detail,motion_status(m.status),m.status==core::BoardMotionStatus::pending,{}});
    }
    for (const auto& d:g.disputes) {
        auto detail=tr("Cause: %1 · %2. First concern %3; formal recommendation %4; respond by %5.\n").arg(d.kind=="publication"?tr("Publication commitment"):tr("Cash covenant"),subject_label(d.subject),date(d.opened_on),date(core::advance_days(d.opened_on,c.concern_period)),date(d.deadline));
        detail+=d.kind=="publication"?tr("Publish the promised completed research. Paying a bonus does not satisfy this commitment. Unresolved terms lead to departure; earned shares and past costs remain."):tr("Restore the agreed cash floor, repay outstanding principal, or negotiate the disclosed one-time covenant waiver. Acknowledgment preserves the obligation and original response date.");
        if (d.kind=="covenant") for (const auto& f:c.financing) if (f.id==d.subject) detail+=tr("\nPermanent covenant waiver: %1 cash. Principal, interest and maturity remain due.").arg(money(f.waiver_fee));
        if (d.resolved_on) detail+=tr("\n%1 on %2.").arg(dispute_status(d.status),date(*d.resolved_on));
        const bool open=d.status==core::CorporateDisputeStatus::concern || d.status==core::CorporateDisputeStatus::formal;
        result.disputes.push_back({text(d.id),subject_label(d.subject),date(d.deadline),detail,dispute_status(d.status),!d.acknowledged,{{QStringLiteral("waiver"),open && d.kind=="covenant"}}});
    }
    if (checkpoint.world.competition) for (const auto& rival:checkpoint.world.competition->companies) if (rival.planning && rival.planning->entered_on && !rival.response) {
        const auto& definition=runtime::rival_definition(program,rival.definition_id);
        auto detail=tr("Review due diligence for a full-ownership acquisition. The seller quotes net book assets plus the disclosed %1 continuity premium. Choose cash or half cash plus a conserving equity counteroffer, within the %2 ceiling. Submit reviewed terms to the board. The target continues as an operating subsidiary, retaining all obligations and qualified services; its cash is not paid to the buyer.\n").arg(money(c.acquisition_premium),percent(c.acquisition_equity));
        if (!rival.parent_company.empty()) {
            const auto report=core::accounts_report(*rival.simulation.accounts);
            detail+=tr("Dividends are limited to undistributed operating profit earned after acquisition and must retain %1 plus projected bills for %2 days. Capital funding and dividends conserve cash across the group.\n").arg(money(c.subsidiary_cash_floor)).arg(c.subsidiary_reserve_days.value());
            detail+=tr("Owned by %1. Subsidiary cash %2; debt %3; payables %4. These remain separate accounts; the parent's investment must be eliminated when consolidating group value. No new external subsidiary financing is authorized.\n").arg(tr("Your company"),money(report.cash),money(report.debt),money(report.payables));
            for (const auto& q:checkpoint.world.competition->acquisitions) if (q.target==rival.definition_id && q.completed_on) {
                core::Cents funded{0},dividends{0};for (const auto& transfer:q.transfers) {if (transfer.funding) funded=core::checked_unit_add(funded,transfer.amount);else dividends=core::checked_unit_add(dividends,transfer.amount);}
                detail+=tr("Capital funded %1; dividends paid %2. Operating profit earned since acquisition %3.\n").arg(money(funded),money(dividends),money(core::checked_unit_subtract(report.profit,q.acquired_profit)));
            }
        }
        result.targets.push_back({text(rival.definition_id),text(definition.name),(rival.parent_company.empty()?QString{}:tr("Your company")),detail,rival.parent_company.empty()?tr("Independent"):tr("Owned subsidiary"),rival.parent_company.empty() && rival.simulation.phase==core::DayPhase::work_ready,{{QStringLiteral("owned"),!rival.parent_company.empty()}}});
    }
    return result;
}
QString acquisition_terms(const runtime::Checkpoint& checkpoint,const runtime::CompiledProgram& program,std::string_view id,bool equity) {
    const auto q=runtime::quote_acquisition(checkpoint.world,program,id,equity);
    return tr("Seller counteroffer for %1, dated %2:\nTotal consideration %3: %4 cash and %5 new fully diluted parent equity. The equity consideration is recorded as assets, not cash or product revenue.\nAll existing seller holdings transfer to the parent; actual cash/equity goes to those sellers by their outstanding shares. The unallocated target pool is dissolved; no target executive grant is outstanding. Assets, debt, bills, licenses, staff, customer contracts and product qualifications stay with the operating subsidiary.\nThe board must approve before its deadline; seller control/ownership and buyer cash are rechecked. Declining or expiration spends nothing. A merger does not cure subsidiary insolvency or create compatible products instantly.").arg(text(runtime::rival_definition(program,q.target).name),date(q.quoted_on),money(q.value),money(q.cash),percent(q.shares));
}
}
