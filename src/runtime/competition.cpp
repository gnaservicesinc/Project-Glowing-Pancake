#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/runtime.hpp>
#include "competition_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
namespace {
using namespace core;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void command(RivalCompany& rival,CommandAction action) {
    Simulation simulation{std::move(rival.simulation),TrustedSimulationState{}};
    const auto outcome=simulation.submit_in_transaction({simulation.checkpoint().revision,std::move(action)});
    if (!outcome.accepted()) throw std::invalid_argument("Rival command refused: "+outcome.explanation);
    rival.simulation=std::move(simulation).release_checkpoint();
}
std::size_t public_deployments_on(const WorldState& world,OrdinalDay day) {
    if (!world.simulation.market) return 0;
    const auto& market=*world.simulation.market;
    std::size_t count=0;
    for (const auto& product:market.products) {
        bool observed=false;
        for (const auto& settled:market.days) {
            if (settled.day>=day || observed) break;
            observed=std::any_of(settled.discretionary.begin(),settled.discretionary.end(),[&](const auto& row) {
                return row.product_id==product.id && row.tasks>0;
            });
            if (observed) break;
            for (const auto& delivery:settled.contracts) {
                if (delivery.delivered_tasks==0) continue;
                const auto contract=std::find_if(market.contracts.begin(),market.contracts.end(),[&](const auto& row){return row.id==delivery.contract_id;});
                if (contract!=market.contracts.end() && contract->product_id==product.id) { observed=true; break; }
            }
        }
        if (observed) ++count;
    }
    return count;
}
void plan(WorldState& world,const CompiledProgram& program,RivalCompany& rival) {
    const auto& definition=rival_definition(program,rival.definition_id);
    auto& simulation=rival.simulation;
    if (simulation.phase==DayPhase::closed || simulation.phase==DayPhase::finished) return;
    const auto day=simulation.date;
    const auto earliest=advance_days(world.campaign_start,definition.start_after);
    if (definition.player_deployment_threshold>0 && !rival.warned_on && day>=earliest &&
        public_deployments_on(world,day)>=definition.player_deployment_threshold) {
        rival.warned_on=day;
        world.competition->notices.push_back({definition.id,"warning",day,advance_days(day,definition.warning_lead)});
    }
    const auto due=definition.player_deployment_threshold>0?
        (rival.warned_on?std::optional{advance_days(*rival.warned_on,definition.warning_lead)}:std::nullopt):std::optional{earliest};
    if (rival.attempted || !due || day<*due) return;
    const auto& catalog=simulation.lab->catalog;
    const auto data=std::find_if(catalog.data.begin(),catalog.data.end(),[&](const auto& row){return row.id==definition.data_id;});
    if (data==catalog.data.end()) invalid("Rival data source does not exist");
    // No hidden subsidy or arbitrary catch-up. Pay data now and retain at least
    // two months of the authored payroll/lease budget before starting work.
    const auto buffer=checked_unit_add(data->price,Cents{checked_add(
        checked_add(definition.monthly_payroll.value(),definition.monthly_lease.value()),
        checked_add(definition.monthly_payroll.value(),definition.monthly_lease.value()))});
    rival.attempted=true;
    if (simulation.cash<buffer) {
        world.competition->notices.push_back({definition.id,"budget_declined",day,std::nullopt}); return;
    }
    command(rival,BuyData{definition.data_id});
    TrainingRequest request{"model.rival",definition.recipe_id,"",{definition.data_id},"lease.initial",definition.training_rate,
        definition.staff,definition.training_work};
    command(rival,StartTraining{std::move(request)});
}
}
const content::RivalDefinition& rival_definition(const CompiledProgram& program,std::string_view id) {
    const content::RivalDefinition* result=nullptr;
    for (const auto& [name,definition]:program.collection.definitions) {
        (void)name;
        const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);
        if (!scenario) continue;
        for (const auto& rival:scenario->rivals) if (rival.id==id) {
            if (result && *result!=rival) invalid("Rival definition ID is ambiguous across scenarios");
            result=&rival;
        }
    }
    if (!result) invalid("Rival definition is missing");
    return *result;
}
void initialize_competition(WorldState& world,const CompiledProgram& program,const std::vector<content::RivalDefinition>& definitions,const std::optional<content::CompetitionSetup>& setup) {
    if (setup) {competition_detail::initialize_planning(world,program,definitions,*setup);return;}
    if (definitions.empty()) return;
    if (!world.simulation.lab) invalid("Competition requires the shared lab catalog");
    CompetitionState competition;
    for (const auto& definition:definitions) {
        SimulationState simulation;
        simulation.date=world.campaign_start; simulation.end_date=world.simulation.end_date; simulation.cash=definition.cash;
        simulation.random=RandomStreams{definition.seed}.snapshot();
        simulation.accounts=opening_accounts(definition.cash,simulation.date,definition.id);
        add_recurring(*simulation.accounts,{"payroll",definition.monthly_payroll,simulation.date,std::nullopt,"Rival payroll"});
        add_recurring(*simulation.accounts,{"compute_lease",definition.monthly_lease,simulation.date,std::nullopt,"Rival compute lease"});
        simulation.capacity=opening_capacity({{"lease.initial","cloud/"+definition.id,definition.id,"supplier.rival",CapacitySourceKind::leased,
            definition.capacity,definition.capacity,0,"general",true}},definition.staff,simulation.date,definition.id);
        simulation.liquidity=opening_liquidity(simulation.date,definition.id);
        LabState laboratory; laboratory.owner=definition.id; laboratory.starts_on=simulation.date;
        laboratory.catalog=world.simulation.lab->catalog;
        laboratory.research_stream_origin=simulation.random[static_cast<std::size_t>(RandomStream::research)]; simulation.lab=std::move(laboratory);
        RivalCompany rival; rival.definition_id=definition.id; rival.simulation=Simulation{std::move(simulation)}.checkpoint();
        competition.companies.push_back(std::move(rival));
    }
    world.competition=std::move(competition);
    for (auto& rival:world.competition->companies) plan(world,program,rival);
}
void advance_competition(WorldState& world,const CompiledProgram& program,OrdinalDay prior_day,const IndustryAllocations& assignments) {
    if (!world.competition) return;
    if (world.competition->setup) {competition_detail::advance_planning(world,program,prior_day,assignments);return;}
    for (auto& rival:world.competition->companies) {
        if (rival.simulation.phase==DayPhase::closed || rival.simulation.phase==DayPhase::finished) continue;
        if (rival.simulation.date!=prior_day) invalid("Rival daily cursor differs from the player settlement");
        const auto warning=liquidity_report(*rival.simulation.liquidity).pending_warning;
        if (warning) command(rival,AcknowledgeLiquidityWarning{*warning});
        command(rival,AdvanceDay{});
        if (rival.simulation.date!=world.simulation.date && rival.simulation.phase!=DayPhase::closed)
            invalid("Rival advancement differs from the settled world date");
        if (!rival.published_on && !rival.simulation.lab->models.empty()) {
            const auto& project=rival.simulation.capacity->projects.front();
            if (project.status==ProjectStatus::completed) {
                rival.published_on=rival.simulation.date;
                world.competition->notices.push_back({rival.definition_id,"publication",*rival.published_on,std::nullopt});
            }
        }
        if (rival.simulation.phase==DayPhase::closed)
            world.competition->notices.push_back({rival.definition_id,"closure",rival.simulation.date,std::nullopt});
        else plan(world,program,rival);
    }
}
void validate_competition(const WorldState& world,const CompiledProgram& program,bool deep) {
    if (!world.competition) return;
    if (world.competition->setup) {competition_detail::validate_planning(world,program,deep);return;}
    const auto& competition=*world.competition;
    if (!competition.releases.empty() || !competition.adoptions.empty() || std::any_of(competition.companies.begin(),competition.companies.end(),[](const auto& row){return row.planning.has_value();}))
        invalid("Legacy bounded competition cannot adopt an undeclared planner or industry state");
    if (competition.companies.size()>32 || competition.notices.size()>128 || !world.simulation.lab)
        invalid("Competition state exceeds its bounds or has no lab catalog");
    std::set<std::string_view> ids;
    for (const auto& rival:competition.companies) {
        const auto& definition=rival_definition(program,rival.definition_id);
        const auto& simulation=rival.simulation;
        if (!ids.insert(rival.definition_id).second || !simulation.lab || !simulation.accounts || !simulation.capacity || !simulation.liquidity ||
            simulation.market || simulation.business_offers || simulation.accounts->owner!=definition.id ||
            simulation.date>world.simulation.date || (simulation.date!=world.simulation.date && simulation.phase!=DayPhase::closed) ||
            simulation.end_date!=world.simulation.end_date || simulation.lab->catalog!=world.simulation.lab->catalog ||
            simulation.capacity->available_staff!=definition.staff || simulation.capacity->sources.size()!=1 ||
            simulation.capacity->sources[0].nominal!=definition.capacity || simulation.capacity->sources[0].effective!=definition.capacity ||
            simulation.capacity->projects.size()>1 || simulation.lab->models.size()>1 || !simulation.lab->research.empty() || !simulation.lab->evaluations.empty())
            invalid("Rival state invented resources, private research or an invalid daily cursor");
        if (deep) {
            validate_simulation(simulation);
            const auto& ledger=*simulation.accounts;
            if (ledger.starts_on!=world.campaign_start || ledger.postings.empty() || ledger.postings.front().key!="opening" ||
                ledger.postings.front().amount!=definition.cash || !ledger.debts.empty() || ledger.recurring.size()!=2 ||
                std::any_of(ledger.postings.begin(),ledger.postings.end(),[](const auto& row){return row.debit==Account::cash && row.key!="opening";}))
                invalid("Minimal rival routine invented capital, revenue or financing");
            for (const auto& obligation:ledger.recurring) {
                const auto amount=obligation.id=="payroll"?definition.monthly_payroll:definition.monthly_lease;
                if ((obligation.id!="payroll" && obligation.id!="compute_lease") || obligation.monthly_amount!=amount ||
                    obligation.starts_on!=world.campaign_start || obligation.ends_before) invalid("Rival recurring budget differs from its authored terms");
            }
            const auto& source=simulation.capacity->sources.front();
            if (source.id!="lease.initial" || source.physical_id!="cloud/"+definition.id || source.provider!="supplier.rival" ||
                source.kind!=CapacitySourceKind::leased || source.memory_mib!=0 || source.interconnect!="general" ||
                !source.energy_included || source.available_from) invalid("Minimal rival routine invented hardware or favorable lease rights");
        }
        if (simulation.lab->research_stream_origin!=RandomStreams{definition.seed}.snapshot()[static_cast<std::size_t>(RandomStream::research)])
            invalid("Rival random stream origin differs from its authored seed");
        if (rival.warned_on) {
            if (definition.player_deployment_threshold==0 || *rival.warned_on>world.simulation.date ||
                *rival.warned_on<advance_days(world.campaign_start,definition.start_after) ||
                public_deployments_on(world,*rival.warned_on)<definition.player_deployment_threshold) invalid("Heavyweight warning lacks its actual visible customer-delivery trigger");
        }
        if (!simulation.lab->models.empty()) {
            if (simulation.capacity->projects.size()!=1) invalid("Rival model has no unique actual training project");
            const auto& model=simulation.lab->models.front(); const auto& work=simulation.capacity->projects.front();
            const auto due=definition.player_deployment_threshold>0?
                (rival.warned_on?advance_days(*rival.warned_on,definition.warning_lead):simulation.end_date):advance_days(world.campaign_start,definition.start_after);
            if (!rival.attempted || model.id!="model.rival" || model.recipe_id!=definition.recipe_id || model.data_ids!=std::vector<std::string>{definition.data_id} ||
                model.created_on<due || work.total_work!=definition.training_work || work.requested!=definition.training_rate || work.staff!=definition.staff)
                invalid("Rival training bypassed its terms or warning lead time");
            if (rival.published_on.has_value()!=(work.status==ProjectStatus::completed) ||
                (rival.published_on && *rival.published_on!=advance_days(*work.stopped_on,Days{1}))) invalid("Rival publication differs from actual completed work");
        } else if (!simulation.capacity->projects.empty() || rival.published_on) invalid("Rival publication has no trained model");
    }
    std::set<std::pair<std::string_view,std::string_view>> notices;
    for (const auto& notice:competition.notices) {
        const auto rival=std::find_if(competition.companies.begin(),competition.companies.end(),[&](const auto& row){return row.definition_id==notice.company_id;});
        if (rival==competition.companies.end() || !notices.emplace(notice.company_id,notice.kind).second ||
            notice.day<world.campaign_start || notice.day>world.simulation.date) invalid("Competitive notice has invalid source, date or duplicate identity");
        const auto& definition=rival_definition(program,notice.company_id);
        if (notice.kind=="warning") {
            if (rival->warned_on!=notice.day || notice.response_due!=advance_days(notice.day,definition.warning_lead)) invalid("Competitive warning lost its response window");
        } else if (notice.kind=="publication") {
            if (rival->published_on!=notice.day || notice.response_due) invalid("Competitive publication is not an actual public result");
        } else if (notice.kind=="closure") {
            if (rival->simulation.phase!=DayPhase::closed || rival->simulation.date!=notice.day || notice.response_due) invalid("Competitive closure is not a real company closure");
        } else if (notice.kind=="budget_declined") {
            const auto due=definition.player_deployment_threshold>0?
                (rival->warned_on?advance_days(*rival->warned_on,definition.warning_lead):world.simulation.end_date):advance_days(world.campaign_start,definition.start_after);
            if (!rival->attempted || !rival->simulation.lab->models.empty() || notice.response_due || notice.day<due)
                invalid("Competitive budget refusal invented training or skipped its warning");
        } else invalid("Unknown competitive notice kind");
    }
    for (const auto& rival:competition.companies)
        if ((rival.warned_on && !notices.contains({rival.definition_id,"warning"})) ||
            (rival.published_on && !notices.contains({rival.definition_id,"publication"})) ||
            (rival.simulation.phase==DayPhase::closed && !notices.contains({rival.definition_id,"closure"})) ||
            (rival.attempted && rival.simulation.lab->models.empty() && !notices.contains({rival.definition_id,"budget_declined"})))
            invalid("Competitive state lost its public notice");
}
}
