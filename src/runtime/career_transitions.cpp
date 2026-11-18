#include <pgp/runtime/career.hpp>
#include <pgp/runtime/authority.hpp>
#include "objectives_internal.hpp"
#include "competition_internal.hpp"

#include <algorithm>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;

const CareerTransition* pending(const WorldState& world) {
    if(!world.objectives || world.objectives->end || world.objectives->transitions.empty()) return nullptr;
    const auto& last=world.objectives->transitions.back();
    return last.decided_on?nullptr:&last;
}
bool founding_executive(const WorldState& world) {
    return world.founding && world.objectives && world.objectives->mode==ObjectiveMode::retrospective &&
        !world.objectives->end && world.simulation.phase==DayPhase::work_ready &&
        world.simulation.succession && founder_role(*world.simulation.succession)==FounderRole::executive &&
        world.simulation.succession->terms.front().starts_on<world.simulation.date;
}
bool already_reviewed(const WorldState& world,std::string_view kind,OrdinalDay after) {
    return std::ranges::any_of(world.objectives->transitions,[&](const auto& row){return row.kind==kind && row.notice>=after;});
}
BasisPoints founder_shares(const SimulationState& state) {
    const auto row=std::ranges::find(state.accounts->holdings,state.succession->founder_holder,&EquityHolding::holder);
    return row==state.accounts->holdings.end()?BasisPoints{0}:row->shares;
}
bool has_board_majority(const SimulationState& state) {
    int seat_margin=0;
    for(const auto& director:current_board(state))
        seat_margin+=(director.player?1:-1)*static_cast<int>(director.seats);
    return seat_margin<0;
}
std::size_t missed_expectations(const SimulationState& state,OrdinalDay since) {
    if(!state.management || !state.management->governance) return 0;
    return static_cast<std::size_t>(std::ranges::count_if(state.management->governance->disputes,[&](const auto& row) {
        return row.opened_on>=since && (row.status==CorporateDisputeStatus::departed || row.status==CorporateDisputeStatus::accelerated);
    }));
}
bool workforce_support(const SimulationState& state) {
    if(!social_rules(state)) return false;
    const auto& people=*state.management->people;
    std::size_t active=0, allies=0;
    for(const auto& employee:people.employees) {
        if(employee.departed_on || employee.ready_on>state.date || employee.terms.profile.role=="Founder / researcher") continue;
        ++active;
        const auto* link=social_link(state,"founder",employee.terms.profile.id);
        if(link && link->affinity>=600) ++allies;
    }
    return active>0 && allies*2>=active;
}
bool continued_mistrust(const WorldState& world,OrdinalDay since) {
    const auto& authority=*world.authority;
    const auto day=world.simulation.date;
    const auto earliest=OrdinalDay{day.value()-60};
    return since<=earliest &&
        std::ranges::any_of(authority.cases,[&](const auto& row){
            return row.region=="region.us" && row.disclosed_on && *row.disclosed_on<=earliest &&
                (!row.resolved_on || *row.resolved_on>day);
        }) && public_identity(world,day)[4]<5000 && public_identity(world,earliest)[4]<5000;
}
std::optional<OrdinalDay> private_agi(const WorldState& world) {
    const auto& state=world.simulation;
    if(!state.frontier || earliest_frontier_qualification(*state.frontier,true)) return {};
    const auto qualified=earliest_frontier_qualification(*state.frontier,false);
    if(!qualified || !world.authority) return {};
    if(std::ranges::any_of(world.authority->testimonies,[&](const auto& record){return record.day>=*qualified;})) return {};
    return qualified;
}
void add_notice(WorldState& world,std::string kind,std::string source,Cents quote=Cents{0}) {
    auto& rows=world.objectives->transitions;
    if(rows.size()>=32) return;
    const auto day=world.simulation.date;
    rows.push_back({std::move(kind),std::move(source),{},day,advance_days(day,Days{30}),quote});
    world.simulation.paused=true;
}
const RivalCompany* buyer(const WorldState& world) {
    if(!world.competition) return nullptr;
    const RivalCompany* best=nullptr;
    for(const auto& rival:world.competition->companies) if(rival.planning && rival.planning->entered_on &&
        rival.parent_company.empty() && rival.simulation.date==world.simulation.date &&
        rival.simulation.phase==DayPhase::work_ready && rival.simulation.accounts &&
        rival.simulation.liquidity->status==LiquidityStatus::operating &&
        (!best || discretionary_cash(rival.simulation)>discretionary_cash(best->simulation))) best=&rival;
    return best;
}
Cents bid(const WorldState& world,const CompiledProgram& program,const RivalCompany& contender) {
    const auto& rules=*rival_definition(program,contender.definition_id).planning;
    const auto& state=world.simulation;
    const auto amount=checked_unit_add(std::max(Cents{0},book_net_assets(accounts_report(*state.accounts))),
        state.management->catalog.governance->acquisition_premium);
    return amount.value()>0 && discretionary_cash(contender.simulation)>=checked_unit_add(amount,rules.reserve_floor)?
        amount:Cents{0};
}
void finish_transition(WorldState& world,CareerTransition& row,CampaignEndCause cause) {
    replace_founder_executive(world.simulation,row.kind);
    objective_detail::finish(world,cause,row.source);
    world.objectives->observer_day=world.simulation.date;
    world.objectives->observer_paused=true;
}
}

bool pending_career_transition(const WorldState& world) {return pending(world)!=nullptr;}
bool acquisition_interest(const WorldState& world,const CompiledProgram& program) {
    if(!founding_executive(world) || pending(world) || nonprofit_organization(world.simulation) ||
       !world.simulation.management->governance || world.objectives->transitions.size()>=32 ||
       world.simulation.liquidity->status==LiquidityStatus::closed) return false;
    const auto* bidder=buyer(world);
    return bidder && bid(world,program,*bidder).value()>0;
}

void review_career_transition(WorldState& world,const CompiledProgram&,std::optional<OrdinalDay> settled) {
    if(!settled || !founding_executive(world) || pending(world) || !world.authority) return;
    auto& state=world.simulation;
    const auto day=state.date;
    if(const auto agi=private_agi(world);agi && continued_mistrust(world,*agi) &&
        !already_reviewed(world,"uscif",OrdinalDay{day.value()-365})) {
        add_notice(world,"uscif",iso_date(*agi));
        return;
    }
    if(!state.management->governance || founder_shares(state)>BasisPoints{5000} || !has_board_majority(state) ||
       leadership_candidates(succession_catalog(state),day).empty()) return;
    const auto since=OrdinalDay{day.value()-730};
    if(discretionary_cash(state)<succession_catalog(state).cash_floor && missed_expectations(state,since)>=2 &&
        !already_reviewed(world,"dismissal",since))
        add_notice(world,"dismissal","Board confidence review");
}

void decide_career_transition(WorldState& world,const CompiledProgram& program,const DecideCareerTransition& command) {
    auto& state=world.simulation;
    if(command.choice=="request_acquisition" && acquisition_interest(world,program)) {
        const auto& contender=*buyer(world);
        add_notice(world,"acquisition",contender.definition_id,bid(world,program,contender));
        return;
    }
    if(!pending(world) || !founding_executive(world)) throw std::invalid_argument("There is no pending career decision.");
    auto& row=world.objectives->transitions.back();
    if(state.date>row.deadline) throw std::invalid_argument("The response window has expired.");
    row.response=command.choice;
    row.decided_on=state.date;
    if(row.kind=="dismissal") {
        if(command.choice=="remedy" && (workforce_support(state) || discretionary_cash(state)>=succession_catalog(state).cash_floor)) {
            row.response="retained";
            return;
        }
        if(command.choice=="remedy" || command.choice=="accept") {finish_transition(world,row,CampaignEndCause::dismissal);return;}
    } else if(row.kind=="uscif") {
        if(command.choice=="disclose" && !private_agi(world)) return;
        if(command.choice=="negotiate" && public_identity(world,state.date)[4]>=5000) return;
        if(command.choice=="assurance" && world.authority && std::ranges::any_of(world.authority->promises,[&](const auto& pledge){
            return pledge.met_on && *pledge.met_on>=row.notice;
        })) return;
        if(command.choice=="accept" && leadership_candidates(succession_catalog(state),state.date).size()>0) {
            finish_transition(world,row,CampaignEndCause::uscif);
            return;
        }
    } else if(row.kind=="acquisition") {
        if(command.choice=="decline") return;
        if(command.choice=="accept") {
            auto acquire=std::ranges::find(world.competition->companies,row.source,&RivalCompany::definition_id);
            if(acquire==world.competition->companies.end() || acquire->simulation.phase!=DayPhase::work_ready ||
                bid(world,program,*acquire)!=row.consideration || leadership_candidates(succession_catalog(state),state.date).empty())
                throw std::invalid_argument("The buyer’s funds or current consideration changed. Decline and request updated terms.");
            const auto old_shares=founder_shares(state);
            row.personal_proceeds=Cents{mul_div_floor(static_cast<std::uint64_t>(row.consideration.value()),
                static_cast<std::uint64_t>(old_shares.value()),10000).quotient};
            const auto marker="career.sale/"+std::to_string(state.date.value());
            record_cash(*acquire->simulation.accounts,state.date,Cents{-row.consideration.value()},Account::assets,
                "Acquisition of "+world.identity->company_name,marker);
            acquire->simulation.cash=accounts_report(*acquire->simulation.accounts).cash;
            transfer_to_acquirer(state,row.source,row.consideration,row.personal_proceeds,marker);
            finish_transition(world,row,CampaignEndCause::acquisition);
            return;
        }
    }
    throw std::invalid_argument("The proposed response needs the stated evidence or authority.");
}

void validate_career_transition(const WorldState& world,const CompiledProgram& program,bool deep) {
    const auto& history=world.objectives->transitions;
    const auto& state=world.simulation;
    if(history.size()>32 || (!history.empty() && world.objectives->mode!=ObjectiveMode::retrospective))
        throw std::invalid_argument("Career decisions require the bounded life campaign.");
    for(const auto& row:history) {
        if(row.notice<world.campaign_start || row.notice>state.date || row.deadline!=advance_days(row.notice,Days{30}) ||
           (row.kind!="dismissal" && row.kind!="acquisition" && row.kind!="uscif") || row.source.empty() ||
           (row.decided_on && (*row.decided_on<row.notice || *row.decided_on>state.date || row.response.empty())) ||
           (!row.decided_on && (!row.response.empty() || &row!=&history.back() || world.objectives->end || !state.paused)) ||
           (row.kind=="acquisition"?row.consideration.value()<=0:row.consideration.value()!=0) ||
           row.personal_proceeds.value()<0 || row.personal_proceeds>row.consideration)
            throw std::invalid_argument("A career decision has invalid terms, dates or response history.");
        if(row.kind=="acquisition") (void)rival_definition(program,row.source);
    }
    if(!deep || !state.succession) return;
    for(const auto& term:state.succession->terms) if(!term.extraordinary_handover.empty()) {
        const auto& end=world.objectives->end;
        if(!end || campaign_end_key(end->cause)!=term.extraordinary_handover || end->day!=term.starts_on ||
            history.empty() || history.back().kind!=term.extraordinary_handover || history.back().decided_on!=term.starts_on ||
            end->source!=history.back().source)
            throw std::invalid_argument("A forced handover needs its actual career decision and successor.");
        const auto& row=history.back();
        if(row.kind=="acquisition") {
            const auto acquire=std::ranges::find(world.competition->companies,row.source,&RivalCompany::definition_id);
            const auto marker="career.sale/"+std::to_string(end->day.value());
            if(acquire==world.competition->companies.end()) throw std::invalid_argument("Acquisition lost its buyer.");
            const auto payment=std::ranges::find(acquire->simulation.accounts->postings,marker,&Posting::key);
            if(payment==acquire->simulation.accounts->postings.end() || payment->day!=end->day ||
                payment->amount!=row.consideration || payment->credit!=Account::cash || payment->debit!=Account::assets ||
                state.succession->sale_proceeds!=row.personal_proceeds)
                throw std::invalid_argument("Acquisition lost its settled payment or founder proceeds.");
        }
    }
}
}
