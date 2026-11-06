#include "endurance_strategy.hpp"

#include <pgp/runtime/scenario.hpp>
#include <pgp/session/slice_run.hpp>
#include <algorithm>
#include <ostream>
#include <stdexcept>

namespace {
using namespace pgp;
using namespace core;
using runtime::Runtime;

const SimulationState& state(const Runtime& game) { return game.current_state().world.simulation; }
void apply(Runtime& game, CommandAction action) {
    const auto result = game.submit({state(game).revision, std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
template<class Rows> const auto* find(const Rows& rows, std::string_view id) {
    const auto found = std::ranges::find_if(rows, [&](const auto& row) { return row.id == id; });
    return found == rows.end() ? nullptr : &*found;
}
bool research_done(const Runtime& game, std::string_view id) {
    return std::ranges::any_of(state(game).lab->research, [&](const auto& row) {
        return row.research_id == id && row.completed;
    });
}
bool project_done(const Runtime& game, std::string_view id) {
    const auto* project = find(state(game).capacity->projects, id);
    return project && project->status == ProjectStatus::completed;
}
bool licensed(const Runtime& game, std::string_view id) {
    return std::ranges::any_of(state(game).lab->licenses, [&](const auto& row) { return row.offer_id == id; });
}
void fund_opening(Runtime& game) {
    if (std::ranges::none_of(state(game).management->financing, [](const auto& row) {
        return row.offer_id == "finance.bridge_equity";
    })) apply(game, TakeFinancing{"finance.bridge_equity"});
}
void fund_site(Runtime& game, std::string_view id, std::string_view name,
               std::uint64_t watts, const std::vector<std::string>& orders) {
    if (!find(state(game).infrastructure->sites, id))
        apply(game, BuildOwnedSite{std::string(id), std::string(name), "US-WA", watts, CoolingMode::air});
    for (const auto& order : orders)
        if (!find(state(game).infrastructure->hardware, order))
            apply(game, OrderOwnedHardware{order, std::string(id), "rack.reference"});
    const auto operators = std::string(id) + ".operators";
    std::size_t hired = 0;
    for (const auto& hire : state(game).infrastructure->hires)
        if (hire.site_id == id) hired += hire.people;
    if (hired < orders.size()) {
        auto hire_id = operators;
        if (find(state(game).infrastructure->hires, hire_id))
            hire_id += '.' + std::to_string(state(game).infrastructure->hires.size() + 1);
        apply(game, HireSiteOperators{hire_id, std::string(id), static_cast<std::uint32_t>(orders.size() - hired)});
    }
    const auto appointment = std::string(id) + ".inspect";
    const auto* site = find(state(game).infrastructure->sites, id);
    const bool booked = std::ranges::any_of(state(game).infrastructure->appointments, [&](const auto& row) {
        return row.site_id == id && !row.cancelled_on && row.visit_on >= state(game).date;
    });
    if (!site->commissioned_on && !booked) {
        auto visit = std::max(site->built_on, site->grid_on);
        if (visit <= state(game).date) visit = advance_days(state(game).date, Days{30});
        apply(game, ScheduleSiteAppointment{appointment, std::string(id), visit, AppointmentMode::remote});
    }
}
void open_ecosystem(Runtime& game) {
    fund_opening(game);
    if (!licensed(game, "data.open_documents")) apply(game, BuyData{"data.open_documents"});
    for (const auto& topic : {"research.held_out_evaluation", "research.reproducible_tooling"}) {
        if (std::ranges::any_of(state(game).lab->research, [&](const auto& row) { return row.research_id == topic; })) continue;
        const bool held_out = std::string_view{topic} == "research.held_out_evaluation";
        apply(game, StartResearch{topic, {"lease.initial", MilliCUPerDay{held_out ? 20000 : 10000}, held_out ? 2U : 1U}});
    }
    if (!find(state(game).lab->models, "model.open"))
        apply(game, StartTraining{{"model.open", "recipe.document_baseline", "", {"data.open_documents"},
            "lease.initial", MilliCUPerDay{60000}, 3, MilliCU{3600000}}});
    if (state(game).date < advance_days(game.current_state().world.campaign_start, Days{61})) return;
    if (research_done(game, "research.reproducible_tooling") &&
        std::ranges::none_of(state(game).lab->publications, [](const auto& row) {
            return row.research_id == "research.reproducible_tooling";
        })) apply(game, PublishResearch{"research.reproducible_tooling"});
    const auto* model = find(state(game).lab->models, "model.open");
    if (!project_done(game, model->project_id) || !research_done(game, "research.held_out_evaluation")) return;
    const auto* evidence = find(state(game).lab->evaluations, "evidence.open");
    if (!evidence) {
        apply(game, StartEvaluation{"evidence.open", "model.open", "evaluation.held_out", {"lease.initial", MilliCUPerDay{20000}, 1}});
        return;
    }
    if (!evidence->completed || state(game).date < advance_days(game.current_state().world.campaign_start, Days{65})) return;
    for (const auto& service : {"product.education_model", "product.private_organization"})
        if (!find(state(game).market->products, service))
            apply(game, DeployModel{service, "model.open", "evidence.open", "", false, "region.europe"});
    if (!find(state(game).market->products, "product.documents"))
        apply(game, DeployModel{"product.documents", "model.open", "evidence.open", "lease.initial", false});
    if (std::ranges::none_of(state(game).business_offers->accepted, [](const auto& row) {
        return row.offer_id == "offer.enterprise_service";
    })) apply(game, AcceptBusinessOffer{"offer.enterprise_service"});
}
void infrastructure(Runtime& game) {
    fund_opening(game);
    fund_site(game, "washington", "Washington wholesale", 32000, {"provider.a", "provider.b"});
    const auto* site = find(state(game).infrastructure->sites, "washington");
    if (!site->commissioned_on) return;
    const auto commissioned_on = *site->commissioned_on;
    if (!state(game).infrastructure->initial_lease_ends_before) apply(game, NoticeInitialLeaseTermination{});
    if (from_ordinal(state(game).date).day != 1 && commissioned_on != state(game).date) return;
    std::vector<std::string> buyers;
    for (const auto& rival : game.current_state().world.competition->companies)
        if (rival.planning->entered_on) buyers.push_back(rival.definition_id);
    for (const auto& source : {"owned/provider.a", "owned/provider.b"}) for (const auto& buyer : buyers) {
        // A restored morning may already contain this month's signed leg.
        if (std::ranges::any_of(game.current_state().world.competition->wholesale, [&](const auto& contract) {
            return contract.source_id == source && contract.buyer_id == buyer && contract.starts_on == state(game).date;
        })) continue;
        try { (void)runtime::wholesale_quote(game.current_state().world, game.program(), "wholesale.power_included", source, buyer); }
        catch (const std::invalid_argument&) { continue; }
        const auto result = game.signWholesale(state(game).revision, "wholesale.power_included", source, buyer);
        if (!result.accepted()) throw std::runtime_error(result.explanation);
    }
}
bool trial_done(const Runtime& game, FrontierTrialKind kind, std::string_view protocol) {
    return std::ranges::any_of(state(game).frontier->trials, [&](const auto& trial) {
        return trial.kind == kind && trial.protocol == protocol && trial.result && trial.result->valid;
    });
}
void frontier(Runtime& game, std::ostream& output) {
    if (from_ordinal(state(game).date).day != 1) return;
    if (state(game).frontier->research_programs.empty()) {
        FrontierResearchPlan plan;
        plan.topics = {"research.sequence_modeling", "research.attention_systems", "research.multimodal_systems",
            "research.workflow_verification", "research.integrated_science", "research.energy_prototype",
            "research.materials_prototype", "research.methods_prototype"};
        plan.budget = {{"lease.initial", MilliCUPerDay{100000}, 3}, Cents{300000000}, Cents{100000000}, Days{14}, 8};
        apply(game, AuthorizeFrontierResearch{plan});
    }
    if (!research_done(game, "research.methods_prototype")) return;
    if (!licensed(game, "data.frontier_corpora")) apply(game, BuyData{"data.frontier_corpora"});
    if (!find(state(game).lab->models, "model.frontier.policy")) {
        apply(game, StartTraining{{"model.frontier.policy", "recipe.frontier_general", "", {"data.frontier_corpora"},
            "lease.initial", MilliCUPerDay{180000}, 4, MilliCU{36000000}}});
        return;
    }
    const auto* model = find(state(game).lab->models, "model.frontier.policy");
    if (!project_done(game, model->project_id)) return;
    const auto* evidence = find(state(game).lab->evaluations, "evidence.frontier.policy");
    if (!evidence) {
        apply(game, StartEvaluation{"evidence.frontier.policy", "model.frontier.policy", "evaluation.held_out", {"lease.initial", MilliCUPerDay{30000}, 1}});
        return;
    }
    if (!evidence->completed) return;
    if (!find(state(game).market->products, "product.document_classification"))
        apply(game, DeployModel{"product.document_classification", "model.frontier.policy", "evidence.frontier.policy", "lease.initial", false, "region.us"});
    if (state(game).frontier->systems.empty())
        apply(game, RegisterFrontierSystem{"Century research service", {"product.document_classification"}});
    fund_site(game, "replication", "Independent replication site", 16000, {"replication.rack"});
    const auto qualification = frontier_qualification(*state(game).frontier, 1);
    if (qualification.asi_on || (!state(game).frontier->catalog.asi_route && qualification.agi_on)) return;
    if (std::ranges::any_of(state(game).frontier->trials, [&](const auto& trial) {
        const auto* project = find(state(game).capacity->projects, trial.project);
        return project && project->status == ProjectStatus::active;
    })) return;
    StartFrontierTrial request;
    request.system = 1;
    request.allocation = {"lease.initial", MilliCUPerDay{100000}, 2};
    for (const auto& panel : state(game).frontier->catalog.panels)
        if (!trial_done(game, FrontierTrialKind::assessment, panel.id)) {
            request.kind = FrontierTrialKind::assessment;
            request.protocol = panel.id;
            break;
        }
    if (request.protocol.empty()) for (const auto& experiment : state(game).frontier->catalog.experiments) {
        if (!trial_done(game, FrontierTrialKind::experiment, experiment.id)) {
            request.kind = FrontierTrialKind::experiment;
            request.protocol = experiment.id;
            break;
        }
        if (!trial_done(game, FrontierTrialKind::replication, experiment.id)) {
            request.kind = FrontierTrialKind::replication;
            request.protocol = experiment.id;
            request.allocation = {"owned/replication.rack", MilliCUPerDay{50000}, 2};
            break;
        }
    }
    if (request.protocol.empty()) return;
    Cents protected_cash{0};
    for (const auto& reservation : state(game).reservations)
        protected_cash = checked_unit_add(protected_cash, reservation.reserved);
    try { (void)quote_frontier_trial(state(game), request, protected_cash); }
    catch (const std::invalid_argument& error) {
        output << iso_date(state(game).date) << " frontier_waiting=" << error.what() << '\n';
        return;
    }
    apply(game, request);
    output << iso_date(state(game).date) << " funded_frontier_trial=" << frontier_key(request.kind) << '/' << request.protocol << '\n';
}
} // namespace

EnduranceStrategy parse_endurance_strategy(std::string_view name) {
    if (name == "specialist") return EnduranceStrategy::specialist;
    if (name == "open") return EnduranceStrategy::open;
    if (name == "frontier") return EnduranceStrategy::frontier;
    if (name == "infrastructure") return EnduranceStrategy::infrastructure;
    throw std::invalid_argument("Strategy must be specialist, open, frontier or infrastructure");
}
pgp::runtime::Checkpoint begin_endurance_strategy(const pgp::runtime::CompiledProgram& program,
    std::string_view scenario_id, pgp::core::OrdinalDay through, EnduranceStrategy strategy) {
    const auto& scenario = std::get<pgp::content::ScenarioDefinition>(program.collection.find(scenario_id)->data);
    if (strategy == EnduranceStrategy::specialist || strategy == EnduranceStrategy::frontier) {
        const auto opening_day = std::min(through, pgp::core::advance_days(scenario.starts_on, pgp::core::Days{181}));
        auto opening = pgp::session::run_competition_specialist(program, scenario_id, opening_day);
        if (opening.stop != pgp::session::SliceStop::requested_boundary)
            throw std::runtime_error("The specialist opening ended before its requested boundary");
        return std::move(opening.checkpoint);
    }
    auto opening = pgp::runtime::begin_scenario(program, scenario_id, "US-CA");
    if (opening.world.enabled_flows) std::erase_if(*opening.world.enabled_flows, [&](const auto& id) {
        const auto& tags = std::get<pgp::content::FlowDefinition>(program.collection.find(id)->data).tags;
        return std::ranges::find(tags, "story") != tags.end();
    });
    return opening;
}
void review_endurance_strategy(pgp::runtime::Runtime& game, EnduranceStrategy strategy, std::ostream& output) {
    const auto& company = state(game);
    if (strategy != EnduranceStrategy::specialist &&
        (!company.lab || !company.capacity || !company.market || !company.management || !company.accounts ||
         !company.business_offers || !company.deployments))
        throw std::invalid_argument("This strategy requires the authored company, Lab and service catalogs");
    if ((strategy == EnduranceStrategy::frontier || strategy == EnduranceStrategy::infrastructure) && !company.infrastructure)
        throw std::invalid_argument("This strategy requires the authored infrastructure catalog");
    if (strategy == EnduranceStrategy::frontier && !company.frontier)
        throw std::invalid_argument("The frontier strategy requires the authored future research catalog");
    switch (strategy) {
    case EnduranceStrategy::specialist: return;
    case EnduranceStrategy::open: return open_ecosystem(game);
    case EnduranceStrategy::frontier: return frontier(game, output);
    case EnduranceStrategy::infrastructure: return infrastructure(game);
    }
}
