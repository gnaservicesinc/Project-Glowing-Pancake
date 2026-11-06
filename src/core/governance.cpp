#include "governance_internal.hpp"
#include <set>

namespace pgp::core {
using namespace governance_detail;
namespace {
void pay(SimulationState& s,Cents amount,Cents protected_cash,std::string key,std::string cause) {
    if (amount.value()<0 || amount>checked_unit_subtract(s.cash,protected_cash)) invalid("The agreed cash payment exceeds unreserved cash");
    if (amount.value()>0) record_cash(*s.accounts,s.date,Cents{-amount.value()},Account::expenses,std::move(cause),std::move(key));
    s.cash=accounts_report(*s.accounts).cash;
}
bool published(const SimulationState& s,std::string_view id) {
    return s.lab && std::any_of(s.lab->publications.begin(),s.lab->publications.end(),[&](const auto& p){return p.research_id==id;});
}
bool finance_used(const SimulationState& s,std::string_view id) {
    const auto& proposed=find(catalog(s).financing,id);
    for (const auto& m:s.management->governance->motions)
        if (m.kind==BoardMotionKind::financing && find(catalog(s).financing,m.subject).negotiation==proposed.negotiation && (m.status==BoardMotionStatus::pending || m.executed)) return true;
    return false;
}
void require_listing_qualification(const SimulationState& state,std::string_view offer) {
    const auto& rules=catalog(state).public_listing;
    if (!rules || rules->offer_id!=offer) return;
    const auto qualification=listing_qualification(state,state.date);
    if (!qualification.old_enough)
        throw std::invalid_argument("The listing can be proposed from "+iso_date(qualification.eligible_on));
    if (!qualification.qualified)
        invalid("The IPO requires the disclosed revenue and profit over its settled qualification window");
}
BoardMember financing_director(const GovernanceCatalog& rules,const CorporateFinanceTerms& finance) {
    if (rules.public_listing && rules.public_listing->offer_id==finance.id) {
        const auto& listing=*rules.public_listing;
        return {"seat/"+finance.id,"Public shareholder director",1,false,
            listing.director_cash_floor,listing.director_maximum_dilution};
    }
    return {"seat/"+finance.id,finance.name+" representative",1,false,finance.covenant_cash_floor,BasisPoints{1500}};
}
void depart(SimulationState& s,std::string_view id) {
    auto& g=governance(s);
    auto appointment=std::find_if(g.appointments.begin(),g.appointments.end(),[&](const auto& a){return a.candidate==id;});
    if (appointment==g.appointments.end() || appointment->departed_on) invalid("The executive has already departed");
    appointment->departed_on=s.date;
    if (appointment->hybrid) {
        const auto& grant=find(g.grants,id);
        record_capital(s,{"departure",std::string(id),"executive/"+std::string(id),s.date,grant.unvested,Cents{0}});
    }
    end_recurring(*s.accounts,"executive/"+std::string(id),s.date);
    // Optional work can be stopped; protected customer service keeps its actual
    // history and receives the normal bounded daily allocation after departure.
    std::vector<std::string> optional;
    if (s.resilience) {
        std::vector<std::string> replicas;
        for (const auto& replica:s.resilience->replicas) if (!replica.stopped_on) replicas.push_back(replica.id);
        for (const auto& replica:replicas) stop_service_replica(s,replica);
    }
    for (const auto& p:s.capacity->projects) if (p.status==ProjectStatus::active && p.kind!=ProjectKind::serving) optional.push_back(p.id);
    for (const auto& project:optional) cancel_project(*s.capacity,s.date,project);
    remove_staff_capacity(*s.capacity,s.date,"executive.departure/"+std::string(id),1);
}
}
namespace governance_detail {
BasisPoints vesting_due(const std::vector<EquityHolding>& h,const OptionGrant& g,const ExecutiveAppointment& a,const ExecutiveCandidate& c,OrdinalDay day) {
    if (g.departed || day<a.hired_on) return BasisPoints{0};
    const auto elapsed=checked_subtract(day.value(),a.hired_on.value());
    if (elapsed<c.cliff.value()) return BasisPoints{0};
    if (elapsed>=c.vesting.value()) return g.unvested;
    const auto vested=shares_of(h,grant_holder(g.candidate));
    const auto total=checked_add(vested.value(),g.unvested.value());
    const auto cumulative=mul_div_floor(static_cast<std::uint64_t>(total),static_cast<std::uint64_t>(elapsed),static_cast<std::uint64_t>(c.vesting.value())).quotient;
    // Hamilton dilution can leave a previously vested claim one unit above the
    // next rounded cumulative target; ownership already earned is never revoked.
    const auto earned=static_cast<std::int64_t>(cumulative);
    return BasisPoints{earned>vested.value()?std::min(g.unvested.value(),earned-vested.value()):0};
}
void apply_capital(std::vector<EquityHolding>& h,std::vector<OptionGrant>& grants,const std::vector<ExecutiveAppointment>& appointments,const GovernanceCatalog& c,const CapitalEvent& e) {
    if (e.kind=="investment" || e.kind=="asset_equity" || e.kind=="pool") {
        if (e.shares.value()==0 || (e.kind=="pool" && e.shares>c.maximum_pool_expansion)) invalid("Corporate dilution has invalid terms");
        for (auto& g:grants) g.unvested=BasisPoints{mul_div_floor(static_cast<std::uint64_t>(g.unvested.value()),static_cast<std::uint64_t>(10000-e.shares.value()),10000).quotient};
        dilute_holdings(h,e.kind=="pool"?pool_id(h):e.subject,e.shares);
    } else if (e.kind=="grant") {
        const auto& candidate=find(c.executives,e.subject);const auto& appointment=find(appointments,e.subject);
        if (!appointment.hybrid || appointment.hired_on!=e.day || e.shares!=candidate.hybrid_grant ||
            std::any_of(grants.begin(),grants.end(),[&](const auto& g){return g.candidate==e.subject;})) invalid("Grant repeats or differs from agreed compensation");
        std::int64_t reserved=0;for (const auto& g:grants) reserved=checked_add(reserved,g.unvested.value());
        if (checked_add(reserved,e.shares.value())>shares_of(h,pool_id(h)).value()) invalid("Grant exceeds the existing unallocated option pool");
        grants.push_back({e.subject,e.shares,false});
    } else if (e.kind=="vest" || e.kind=="departure") {
        auto at=std::find_if(grants.begin(),grants.end(),[&](const auto& g){return g.candidate==e.subject;});
        if (at==grants.end() || at->departed) invalid("Vesting/departure requires an outstanding grant");
        const auto& a=find(appointments,e.subject);const auto& candidate=find(c.executives,e.subject);
        if (e.kind=="departure") {
            if (e.shares!=at->unvested || a.departed_on!=e.day) invalid("Departure did not return its actual unvested claim");
            at->unvested=BasisPoints{0};at->departed=true;
        } else {
            const auto due=vesting_due(h,*at,a,candidate,e.day);
            if (due.value()==0 || due!=e.shares || (a.departed_on && e.day>*a.departed_on)) invalid("Vesting differs from cumulative earned entitlement");
            const auto pool=pool_id(h);
            for (auto& row:h) if (row.holder==pool) row.shares=BasisPoints{checked_subtract(row.shares.value(),due.value())};
            const auto holder=grant_holder(e.subject);bool exists=false;
            for (auto& row:h) if (row.holder==holder) {row.shares=BasisPoints{checked_add(row.shares.value(),due.value())};exists=true;}
            if (!exists) h.push_back({holder,due,false});
            at->unvested=BasisPoints{checked_subtract(at->unvested.value(),due.value())};
        }
    } else invalid("Unsupported capital event");
}
void record_capital(SimulationState& s,CapitalEvent e) {
    auto& g=governance(s);if (g.capital.size()>=8192) invalid("Corporate capital history is full");
    apply_capital(s.accounts->holdings,g.grants,g.appointments,catalog(s),e);g.capital.push_back(std::move(e));
}
BoardMotion& new_motion(SimulationState& s,BoardMotionKind kind,std::string subject,Cents cash,Cents asset,BasisPoints shares) {
    auto& g=governance(s);const auto& c=catalog(s);
    if (g.motions.size()>=128 || advance_days(s.date,c.vote_window)>=s.end_date) invalid("No bounded board response window remains");
    const auto id="board."+std::to_string(g.motions.size()+1);
    BoardMotion m;m.id=id;m.subject=std::move(subject);m.kind=kind;m.opened_on=s.date;m.deadline=advance_days(s.date,c.vote_window);
    m.cash=cash;m.asset_value=asset;m.shares=shares;g.motions.push_back(std::move(m));s.paused=true;
    return g.motions.back();
}
std::vector<BoardMember> charter_board(const SimulationState& s,std::vector<BoardMember> members,std::uint64_t cursor) {
    const auto& history=s.management->governance->charters;
    if(cursor>history.size()) invalid("Board charter cursor exceeds its actual amendment history");
    if(cursor) {
        const auto& charter=find(catalog(s).charters,history[cursor-1].charter);
        for(auto& member:members) if(!member.player) {
            member.cash_floor=std::max(member.cash_floor,charter.cash_floor);
            member.maximum_dilution=std::min(member.maximum_dilution,charter.maximum_dilution);
        }
    }
    return members;
}
void apply_charter(SimulationState& s,std::string_view id,std::string motion) {
    auto& g=governance(s);const auto& charter=find(catalog(s).charters,id);
    if(g.charters.size()>=129) invalid("Charter amendment history is full");
    if(!g.charters.empty()) end_recurring(*s.accounts,"charter/employees/"+std::to_string(g.charters.size()),s.date);
    const auto key="charter/employees/"+std::to_string(g.charters.size()+1);
    add_recurring(*s.accounts,{key,charter.monthly_employee_budget,s.date,{},"Employee program under "+charter.name});
    g.charters.push_back({std::string(id),std::move(motion),s.date});
}
std::vector<BoardBallot> ballots_for(const SimulationState& s,const BoardMotion& motion,bool player,Cents cash) {
    const auto after=motion.kind==BoardMotionKind::financing?checked_unit_add(cash,motion.cash):checked_unit_subtract(cash,motion.cash);
    const auto& rules=catalog(s);
    auto members=rules.board;
    for (const auto& prior:s.management->governance->motions) if (prior.kind==BoardMotionKind::financing && prior.executed && prior.id!=motion.id) {
        const auto& f=find(rules.financing,prior.subject);bool preceded=false;
        for (std::size_t i=0;i<std::min<std::uint64_t>(motion.posting_cursor,s.accounts->postings.size());++i) {
            const auto& posting=s.accounts->postings[i];
            if (posting.key==(f.term.value()==0?"corporate/":"debt/corporate/")+f.id) preceded=true;
        }
        if (f.board_seat && preceded) members.push_back(financing_director(rules,f));
    }
    members=succession_board(s,std::move(members),motion.leadership_cursor);
    members=charter_board(s,std::move(members),motion.charter_cursor);
    if(motion.kind==BoardMotionKind::charter) {
        const auto& proposed=find(catalog(s).charters,motion.subject);
        for(auto& member:members) if(!member.player) member.cash_floor=std::max(member.cash_floor,proposed.cash_floor);
    }
    const auto* filing=rules.public_listing?latest_public_filing(s,motion.resolved_on.value_or(s.date)):nullptr;
    const bool restrict_expansion=filing && filing->profit().value()<0 &&
        (motion.kind==BoardMotionKind::acquisition || motion.kind==BoardMotionKind::pool_expansion);
    const auto public_seat=restrict_expansion?"seat/"+rules.public_listing->offer_id:std::string{};
    std::vector<BoardBallot> votes;
    for (const auto& member:members) votes.push_back({member.id,member.seats,member.player?player:
        member.id!=public_seat && after>=member.cash_floor && motion.shares<=member.maximum_dilution});
    return votes;
}
void execute_finance(SimulationState& s,BoardMotion& m) {
    const auto& terms=find(catalog(s).financing,m.subject);const auto key="corporate/"+m.subject;
    if (terms.term.value()==0) company_equity(s,"investor/"+terms.id,terms.proceeds,terms.acquired,key);
    else draw_debt(*s.accounts,s.date,{key,terms.proceeds,terms.monthly_interest,s.date,advance_days(s.date,terms.term),0,{}},"Negotiated finance: "+terms.name);
    s.cash=accounts_report(*s.accounts).cash;m.executed=true;
}
}
BasisPoints unallocated_option_pool(const SimulationState& s) {
    (void)catalog(s);std::int64_t used=0;
    for (const auto& grant:s.management->governance->grants) used=checked_add(used,grant.unvested.value());
    return BasisPoints{checked_subtract(shares_of(s.accounts->holdings,pool_id(s.accounts->holdings)).value(),used)};
}
std::vector<BoardMember> current_board(const SimulationState& s) {
    const auto& rules=catalog(s);
    auto rows=rules.board;
    for (const auto& m:s.management->governance->motions) if (m.kind==BoardMotionKind::financing && m.executed) {
        const auto& f=find(rules.financing,m.subject);
        if (f.board_seat) rows.push_back(financing_director(rules,f));
    }
    return charter_board(s,succession_board(s,std::move(rows),s.succession?s.succession->controls.size():0),s.management->governance->charters.size());
}
void company_equity(SimulationState& s,std::string investor,Cents proceeds,BasisPoints acquired,std::string key,bool assets) {
    if (!s.accounts) invalid("Equity requires company accounts");
    const auto before=s.accounts->holdings;
    if (assets) issue_asset_equity(*s.accounts,s.date,investor,proceeds,acquired,key);
    else invest_equity(*s.accounts,s.date,investor,proceeds,acquired,key);
    if (s.management && s.management->governance) {
        auto& g=governance(s);if (g.capital.size()>=8192) invalid("Corporate capital history is full");
        auto expected=before;CapitalEvent e{assets?"asset_equity":"investment",std::move(investor),std::move(key),s.date,acquired,proceeds};
        apply_capital(expected,g.grants,g.appointments,catalog(s),e);
        if (expected!=s.accounts->holdings) invalid("Corporate dilution differs from the actual cap table");
        g.capital.push_back(std::move(e));
    }
    s.cash=accounts_report(*s.accounts).cash;
}
void negotiate_corporate_finance(SimulationState& s,std::string_view id) {
    const auto& terms=find(catalog(s).financing,id);
    if (finance_used(s,id)) invalid("This negotiated financing is already pending or completed");
    require_listing_qualification(s,id);
    if (terms.term.value()>0 && advance_days(s.date,terms.term)>=s.end_date) invalid("The loan maturity must fall inside this campaign");
    (void)new_motion(s,BoardMotionKind::financing,std::string(id),terms.proceeds,Cents{0},terms.acquired);
}
void appoint_executive(SimulationState& s,std::string_view id,bool hybrid,Cents protected_cash) {
    auto& g=governance(s);const auto& c=find(catalog(s).executives,id);
    if (g.appointments.size()>=8 || std::any_of(g.appointments.begin(),g.appointments.end(),[&](const auto& a){return a.candidate==id;}))
        invalid("An executive can agree one engagement; repeating or rehiring cannot create another grant");
    for (const auto& a:g.appointments) if (!a.departed_on && find(catalog(s).executives,a.candidate).role==c.role) invalid("That key role is already filled");
    if (hybrid && (c.hybrid_grant.value()==0 || c.hybrid_grant>unallocated_option_pool(s))) invalid("The negotiated grant exceeds the existing unallocated pool");
    if (!c.publication_research.empty() && advance_days(s.date,c.promise_term)>=s.end_date) invalid("The promised response window must fit inside this campaign");
    pay(s,c.signing_cost,protected_cash,"executive/"+c.id,"Executive signing: "+c.name);
    add_staff_capacity(*s.capacity,s.date,"executive/"+c.id,1);
    add_recurring(*s.accounts,{"executive/"+c.id,hybrid?c.hybrid_salary:c.cash_salary,s.date,{},"Agreed executive salary: "+c.name});
    g.appointments.push_back({c.id,hybrid,s.date,{}});
    if (hybrid) record_capital(s,{"grant",c.id,"executive/"+c.id,s.date,c.hybrid_grant,Cents{0}});
}
void propose_pool_expansion(SimulationState& s,BasisPoints shares) {
    if (shares.value()==0 || shares>catalog(s).maximum_pool_expansion) invalid("Pool expansion exceeds the disclosed dilution ceiling");
    (void)new_motion(s,BoardMotionKind::pool_expansion,"option_pool",Cents{0},Cents{0},shares);
}
const OrganizationalCharter* active_charter(const SimulationState& s) {
    if(!s.management || !s.management->governance || s.management->governance->charters.empty()) return nullptr;
    return &find(catalog(s).charters,s.management->governance->charters.back().charter);
}
void initialize_charter(SimulationState& s,std::string_view id) {
    const auto& g=governance(s);
    if(s.date!=s.accounts->starts_on || !g.charters.empty() || !g.motions.empty()) invalid("Starting charter can only be selected at company formation");
    apply_charter(s,id,{});
}
void propose_charter(SimulationState& s,std::string_view id) {
    const auto& proposed=find(catalog(s).charters,id);
    if(const auto* current=active_charter(s);current && current->id==proposed.id) invalid("That charter is already in force");
    for(const auto& m:governance(s).motions) if(m.kind==BoardMotionKind::charter && m.status==BoardMotionStatus::pending) invalid("Resolve the existing charter motion first");
    (void)new_motion(s,BoardMotionKind::charter,std::string(id),Cents{0},Cents{0},BasisPoints{0});
}
void propose_acquisition_board(SimulationState& s,std::string target,Cents cash,Cents asset,BasisPoints shares) {
    if (target.empty() || cash.value()<=0 || asset<cash || shares>catalog(s).acquisition_equity) invalid("Acquisition consideration differs from negotiated terms");
    (void)new_motion(s,BoardMotionKind::acquisition,std::move(target),cash,asset,shares);
}
void vote_board_motion(SimulationState& s,std::string_view id,bool support,Cents protected_cash) {
    auto& g=governance(s);const auto it=std::find_if(g.motions.begin(),g.motions.end(),[&](const auto& m){return m.id==id;});
    if (it==g.motions.end() || it->status!=BoardMotionStatus::pending || s.date>=it->deadline) invalid("The board motion is resolved or its response window has ended");
    auto& m=*it;
    if (m.kind==BoardMotionKind::financing) require_listing_qualification(s,m.subject);
    if (m.kind==BoardMotionKind::acquisition && m.cash>checked_unit_subtract(s.cash,protected_cash)) invalid("The acquisition exceeds unreserved cash");
    m.reviewed_cash=s.cash;m.posting_cursor=s.accounts->postings.size();m.leadership_cursor=s.succession?s.succession->controls.size():0;
    m.charter_cursor=g.charters.size();
    m.player_vote=support;m.ballots=ballots_for(s,m,support,s.cash);
    std::uint32_t yes=0,total=0;for (const auto& b:m.ballots) {total+=b.seats;if (b.support) yes+=b.seats;}
    m.resolved_on=s.date;m.status=yes>total/2?BoardMotionStatus::approved:BoardMotionStatus::rejected;
    if (m.status==BoardMotionStatus::approved) {
        if (m.kind==BoardMotionKind::financing) execute_finance(s,m);
        else if (m.kind==BoardMotionKind::pool_expansion) {record_capital(s,{"pool",m.id,m.id,s.date,m.shares,Cents{0}});m.executed=true;}
        else if(m.kind==BoardMotionKind::charter) {apply_charter(s,m.subject,m.id);m.executed=true;}
        // Acquisition is completed by the owning cross-company runtime, inside
        // the same transaction. It cannot be faked by a core-only transfer.
    }
}
void respond_corporate_dispute(SimulationState& s,std::string_view id,bool waiver,Cents protected_cash) {
    auto& g=governance(s);auto it=std::find_if(g.disputes.begin(),g.disputes.end(),[&](const auto& d){return d.id==id;});
    if (it==g.disputes.end() || (it->acknowledged && !waiver)) invalid("The corporate notice is absent or already acknowledged");
    if (waiver) {
        if (it->status!=CorporateDisputeStatus::concern && it->status!=CorporateDisputeStatus::formal) invalid("The dispute no longer has an open response window");
        if (it->kind!="covenant") invalid("A publication commitment cannot be erased with a cash waiver");
        const auto& terms=find(catalog(s).financing,it->subject);
        pay(s,terms.waiver_fee,protected_cash,"covenant.waiver/"+it->id,"One negotiated covenant waiver");
        it->status=CorporateDisputeStatus::waived;it->resolved_on=s.date;
    }
    it->acknowledged=true;
}
std::vector<EquityHolding> corporate_holdings_before(const SimulationState& s,OrdinalDay before) {
    const auto& rules=catalog(s);
    const auto& g=*s.management->governance;
    auto result=g.opening_holdings;
    std::vector<OptionGrant> grants;
    for(const auto& event:g.capital) {
        if(event.day>=before) break;
        apply_capital(result,grants,g.appointments,rules,event);
    }
    return result;
}
void prepare_governance(SimulationState& s) {
    if (!s.management || !s.management->governance || s.phase==DayPhase::closed || s.phase==DayPhase::finished) return;
    auto& g=governance(s);const auto& c=catalog(s);
    for (const auto& a:g.appointments) if (a.hybrid && !a.departed_on) {
        const auto& grant=find(g.grants,a.candidate);const auto due=vesting_due(s.accounts->holdings,grant,a,find(c.executives,a.candidate),s.date);
        if (due.value()>0) record_capital(s,{"vest",a.candidate,"executive/"+a.candidate,s.date,due,Cents{0}});
    }
    for (auto& m:g.motions) if (m.status==BoardMotionStatus::pending && s.date>=m.deadline) {m.status=BoardMotionStatus::expired;m.resolved_on=m.deadline;}
    const auto open=[&](std::string kind,std::string subject) {
        // One cause gets one warned response path. A paid covenant waiver ends
        // that covenant; it is not a repeatedly purchasable maturity reset.
        std::size_t episode=1;
        for (const auto& d:g.disputes) if (d.kind==kind && d.subject==subject) {
            ++episode;
            if (d.status!=CorporateDisputeStatus::resolved || d.resolved_on==s.date) return;
        }
        const auto id=kind+"/"+subject+"/"+std::to_string(episode);
        if (g.disputes.size()>=32) invalid("Corporate dispute history is full");
        CorporateDispute d;d.id=id;d.subject=std::move(subject);d.kind=std::move(kind);d.opened_on=s.date;d.deadline=advance_days(s.date,c.response_window);
        d.opening_cash=s.cash;d.posting_cursor=s.accounts->postings.size();d.publication_cursor=s.lab?s.lab->publications.size():0;
        g.disputes.push_back(std::move(d));s.paused=true;
    };
    for (const auto& a:g.appointments) if (!a.departed_on) {
        const auto& candidate=find(c.executives,a.candidate);
        if (!candidate.publication_research.empty() && s.date>=advance_days(a.hired_on,candidate.promise_term) && !published(s,candidate.publication_research)) open("publication",a.candidate);
    }
    for (const auto& m:g.motions) if (m.kind==BoardMotionKind::financing && m.executed) {
        const auto& terms=find(c.financing,m.subject);
        if (terms.term.value()>0 && terms.covenant_cash_floor.value()>0 && find(s.accounts->debts,"corporate/"+terms.id).principal.value()>0 && s.cash<terms.covenant_cash_floor) open("covenant",terms.id);
    }
    for (auto& d:g.disputes) {
        if (d.status!=CorporateDisputeStatus::concern && d.status!=CorporateDisputeStatus::formal) continue;
        bool cured=false;
        if (d.kind=="publication") cured=published(s,find(c.executives,d.subject).publication_research);
        else {
            const auto& terms=find(c.financing,d.subject);
            cured=s.cash>=terms.covenant_cash_floor || find(s.accounts->debts,"corporate/"+d.subject).principal.value()==0;
        }
        if (cured) {d.status=CorporateDisputeStatus::resolved;d.resolved_on=s.date;continue;}
        if (s.date>=d.deadline) {
            if (d.kind=="publication") {depart(s,d.subject);d.status=CorporateDisputeStatus::departed;}
            else {
                auto at=std::find_if(s.accounts->debts.begin(),s.accounts->debts.end(),[&](const auto& debt){return debt.id=="corporate/"+d.subject;});
                at->due=std::min(at->due,advance_days(s.date,Days{30}));d.status=CorporateDisputeStatus::accelerated;
            }
            d.resolved_on=s.date;d.acknowledged=false;s.paused=true;
        } else if (s.date>=advance_days(d.opened_on,c.concern_period) && d.status==CorporateDisputeStatus::concern) {d.status=CorporateDisputeStatus::formal;d.acknowledged=false;s.paused=true;}
    }
}
void extend_executive_response(SimulationState& s,std::string_view id,std::string project) {
    auto& g=governance(s);
    const auto it=std::find_if(g.disputes.begin(),g.disputes.end(),[&](const auto& row){return row.id==id;});
    if(it==g.disputes.end() || it->kind!="publication" || it->status!=CorporateDisputeStatus::formal ||
        it->negotiated_on || s.date>=it->deadline) invalid("Negotiate one formal publication dispute before its response deadline.");
    const auto& work=find(s.capacity->projects,project);
    const auto due=advance_days(it->deadline,catalog(s).response_window);
    if(work.kind!=ProjectKind::research || work.status!=ProjectStatus::active || work.starts_on!=s.date ||
        work.total_work!=integrate(work.requested,Days{due.value()-s.date.value()}) || work.staff==0)
        invalid("The executive extension needs a funded review team through the extended deadline.");
    it->negotiated_on=s.date;it->review_project=std::move(project);it->deadline=due;it->acknowledged=true;
}
bool required_governance_notice(const SimulationState& s) {
    if (!s.management || !s.management->governance) return false;
    return std::any_of(s.management->governance->disputes.begin(),s.management->governance->disputes.end(),[](const auto& d){return !d.acknowledged;});
}
}
