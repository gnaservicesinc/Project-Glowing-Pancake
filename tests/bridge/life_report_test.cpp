#include "life_report.hpp"
#include "objectives_controller.hpp"
#include "../../src/bridge/founding_fixture.hpp"
#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/current_campaign.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/career.hpp>
#include <pgp/runtime/legacy.hpp>
#include <nlohmann/json.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <fstream>
#include <iostream>

namespace {
void check(bool okay,const char* message) {if(!okay) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        using namespace pgp;
        auto document=nlohmann::json::parse(persistence::read_bounded(PGP_LIFE_SOURCE,content::maximum_document_bytes));
        const auto registry=content::foundation_registry();
        const auto installed=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        const auto& founder=std::get<content::ScenarioDefinition>(installed.collection.find("scenario.founder_campaign")->data);
        check(founder.ends_before==core::to_ordinal({2097,1,1}) && founder.objectives->retrospective,
            "The founder campaign must end at age 100 with a retrospective.");
        QTemporaryDir temporary;
        bridge::FoundingController form{installed.collection,std::filesystem::path{temporary.path().toStdString()}/"founding.json"};
        form.setField("family","rich");
        form.setField("route","bootstrap");
        bridge::complete_formative_fixture(form);
        const auto selection=form.selection();
        runtime::ScenarioOptions options;
        options.founding=selection;
        options.identity=runtime::CampaignIdentity{"Cedar Works","Alex Vale","they/them"};
        auto fresh=runtime::begin_scenario(installed,"scenario.founder_campaign","US-CA",{},options);
        const auto untouched=fresh;
        const auto live=bridge::lifeReport(fresh,installed);
        check(!live.value("obituary").toBool() && live.value("headline").toString().contains("life in progress") && fresh==untouched,
            "A live report must preserve the simulation and describe the founder as alive.");

        runtime::Runtime career{installed,fresh};
        const auto& state=[&]() -> const core::SimulationState& {return career.current_state().world.simulation;};
        const auto submit=[&](core::CommandAction action) {return career.submit({state().revision,std::move(action)});};
        const auto commit=[&](core::CommandAction action) {
            const auto result=submit(std::move(action));
            if(!result.accepted()) throw std::runtime_error(result.explanation);
        };
        check(career.prepare_opening(state().revision).accepted(),"Career fixture morning must prepare.");
        check(!submit(core::AcceptResearchAppointment{"company.century.roster.domestic.1"}).accepted(),
            "A going concern must not trigger the forced departure route.");
        commit(core::IssueInvoice{{"fixture.career-closure",core::InvoiceKind::payable,state().date,
            core::Cents{1000000000},core::Cents{0},"Fixture overdue bill"}});
        for(int days=0;days<80 && !runtime::objective_time_stopped(career.current_state().world);++days) {
            const auto& rows=career.current_state().runtime.presentations;
            const auto pending=std::ranges::find_if(rows,[](const auto& row){return !row.resolved && !row.choices.empty();});
            if(pending!=rows.end()) {
                check(career.choose(state().revision,pending->id,pending->choices.back().id).accepted(),"Resolve the pending founder choice.");
                continue;
            }
            if(const auto warning=core::liquidity_report(*state().liquidity).pending_warning) commit(core::AcknowledgeLiquidityWarning{*warning});
            commit(core::AdvanceDay{});
        }
        const auto closure_day=state().date;
        const auto closed_accounts=*state().accounts;
        const auto closed_family=career.current_state().world.family;
        const auto closed_identity=career.current_state().world.identity;
        check(state().phase==core::DayPhase::closed && runtime::career_open(career.current_state().world),
            "A real insolvency closure must open the founder's employment route.");
        if(argc==3 && std::string_view(argv[1])=="--career-smoke-fixture") {
            const auto folder=std::filesystem::path(argv[2])/"saves"/"continuing";
            persistence::CurrentCampaign{folder}.commit(installed,career.checkpoint());
        }
        const auto jobs=runtime::research_job_offers(career.current_state().world,installed);
        check(!jobs.empty() && !jobs.front().fellowship && jobs.front().salary.value()>0,
            "An operating lab with runway must fund an appointment at this closure.");
        const auto employer=jobs.front().employer;
        commit(core::AcceptResearchAppointment{employer});
        const auto job_world=career.current_state().world;
        const auto* lab=runtime::career_employer(job_world);
        check(lab && lab->definition_id==employer && runtime::career_payroll(*lab)->monthly_amount==jobs.front().salary &&
            !runtime::objective_time_stopped(job_world) && job_world.family==closed_family && job_world.identity==closed_identity,
            "Acceptance must use the existing lab, create its real payroll and preserve the founder's identity and family.");
        check(!submit(core::AcceptResearchAppointment{employer}).accepted(),"A founder must not take the same salary twice.");
        std::vector<runtime::CareerResearchOption> research;
        for(int days=0;days<35;++days) {
            research=runtime::career_research_options(career.current_state().world,installed);
            if(!research.empty()) break;
            commit(core::AdvanceDay{});
        }
        check(!research.empty(),"The research role must give the player a feasible paid project.");
        const auto topic=research.front().topic;
        const auto& hiring_lab=runtime::career_employer(career.current_state().world)->simulation;
        const auto before_work=hiring_lab.cash;
        const auto occupied=hiring_lab.capacity->projects;
        commit(core::DirectCareerResearch{topic});
        const auto& commissioned=runtime::career_employer(career.current_state().world)->simulation;
        const auto project_id=commissioned.lab->research.back().project_id;
        check(commissioned.cash==
            core::checked_unit_subtract(before_work,research.front().fee),"Research must spend the actual employer's money.");
        for(const auto& prior:occupied) if(prior.kind==core::ProjectKind::training && prior.status==core::ProjectStatus::active) {
            const auto& projects=commissioned.capacity->projects;
            const auto paused=std::ranges::find(projects,prior.id,&core::CapacityProject::id);
            check(paused!=projects.end() && paused->status==core::ProjectStatus::paused && paused->completed_work==prior.completed_work,
                "Deferring training must preserve its earlier work.");
        }
        check(!submit(core::PublishCareerResearch{topic}).accepted(),"Unfinished work cannot be published.");
        check(!submit(core::CancelProject{"a-project-outside-the-appointment"}).accepted(),
            "The founder may manage only projects from their appointment.");
        bool published=false;
        std::string publication_error;
        for(int days=0;days<75 && !published;++days) {
            commit(core::AdvanceDay{});
            const auto result=submit(core::PublishCareerResearch{topic});
            published=result.accepted();publication_error=result.explanation;
        }
        if(!published) throw std::runtime_error(topic+" ("+project_id+"): "+publication_error);
        for(int days=0;days<32 && runtime::career_salary_received(career.current_state().world).value()==0;++days)
            commit(core::AdvanceDay{});
        const auto& continued=career.current_state();
        const auto paid=runtime::career_salary_received(continued.world);
        if(paid.value()<=0) {
            const auto* lab=runtime::career_employer(continued.world);
            if(!lab) throw std::runtime_error("The first employer closed before it made a salary payment.");
            std::string invoices;
            for(const auto& invoice:lab->simulation.accounts->invoices)
                if(invoice.id.find("career/")!=std::string::npos)
                    invoices+=" "+invoice.id+" "+std::to_string(invoice.settled.value())+"/"+std::to_string(invoice.amount.value());
            throw std::runtime_error("Employer salary at "+core::iso_date(lab->simulation.date)+": "+invoices);
        }
        check(runtime::campaign_day(continued.world)>closure_day && *state().accounts==closed_accounts &&
            runtime::founder_legacy(continued.world,continued.world.company_id).cash==
                core::checked_unit_add(state().succession->personal_cash,paid),
            "The later career must publish actual completed work and pay the founder while preserving the closed company.");
        check(persistence::decode_save(installed,persistence::encode_save(installed,continued)).checkpoint==continued,
            "The continued career and employer work must survive an exact reload.");
        auto tampered=continued;
        tampered.world.objectives->career.reset();
        bool rejected=false;
        try {runtime::validate_checkpoint(installed,tampered);} catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"An employer salary cannot survive without its recorded appointment.");
        const auto continued_report=bridge::lifeReport(continued,installed);
        const auto articles=continued_report.value("articles").toList();
        check(std::ranges::any_of(articles,[](const auto& row){return row.toMap().value("id")=="career";}),
            "The newspaper must include the later appointment and its research.");
        while(core::from_ordinal(runtime::campaign_day(career.current_state().world)).day!=1) commit(core::AdvanceDay{});
        persistence::AutosaveRing ring{std::filesystem::path(temporary.path().toStdString())/"career-autosaves"};
        const auto saved=ring.write(installed,career.checkpoint(),persistence::AutosaveReason::monthly_boundary);
        check(saved && persistence::load_file(installed,*saved).checkpoint==career.checkpoint(),
            "The continued career's monthly autosave must use the new calendar.");

        auto federal=document;
        auto& federal_scenario=pgp_test::definition(federal,"scenario.founder_campaign");
        for(const auto* group:{"domestic","international","heavyweights"}) federal_scenario["competition"][group]=0;
        auto federal_civic=pgp_test::definition(federal,federal_scenario.at("civic").get<std::string>());
        federal_civic["id"]="civic.fellowship_fixture";
        federal_scenario["civic"]=federal_civic["id"];
        auto order=federal_civic["catalog"]["policies"].front();
        order["id"]="policy.fixture_federal_closure";
        order["title"]="Reviewed federal closure order";
        order["jurisdiction"]="US";order["rule"]="final_closure";order["channel"]="legacy";
        order["evidence_axis"]=0;order["evidence_floor_bp"]=0;order["exempt_reviewed_service"]=false;
        order["permit_watts"]=0;order["first_agenda_day"]=core::to_ordinal({2015,1,1}).value();
        order["repeat_years"]=0;order["case_strength_bp"]=10000;
        order["hearing_days"]=7;order["vote_spacing_days"]=2;order["review_days"]=2;order["review_floor_bp"]=1;
        order["implementation_days"]=3;order["term_days"]=365;
        federal_civic["catalog"]["policies"]=nlohmann::json::array({order});
        federal["definitions"].push_back(std::move(federal_civic));
        const auto federal_program=runtime::compile_collection(content::load_collection(federal.dump(),registry),registry);
        runtime::Runtime fellow{federal_program,runtime::begin_scenario(federal_program,"scenario.founder_campaign","US-CA",{},options)};
        const auto& fellowship_state=[&]() -> const core::SimulationState& {return fellow.current_state().world.simulation;};
        const auto fellowship_commit=[&](core::CommandAction action) {
            const auto result=fellow.submit({fellowship_state().revision,std::move(action)});
            if(!result.accepted()) throw std::runtime_error(result.explanation);
        };
        check(fellow.prepare_opening(fellowship_state().revision).accepted(),"The federal review fixture must prepare.");
        for(int days=0;days<45 && !runtime::objective_time_stopped(fellow.current_state().world);++days) {
            const auto& rows=fellow.current_state().runtime.presentations;
            const auto pending=std::ranges::find_if(rows,[](const auto& row){return !row.resolved && !row.choices.empty();});
            if(pending!=rows.end()) {
                check(fellow.choose(fellowship_state().revision,pending->id,pending->choices.back().id).accepted(),
                    "Resolve the pending decision in the federal review fixture.");
                continue;
            }
            fellowship_commit(core::AdvanceDay{});
        }
        const auto& departure=fellow.current_state().world;
        check(departure.objectives->end && departure.objectives->end->cause==runtime::CampaignEndCause::final_order &&
            runtime::career_open(departure) && departure.competition->companies.empty(),
            "An enforceable closure must open re-entry even when no rival has entered.");
        const auto fellowship_offer=runtime::research_job_offers(departure,federal_program);
        check(fellowship_offer.size()==1 && fellowship_offer.front().fellowship,"The endowment must provide a funded fallback immediately.");
        if(argc==3 && std::string_view(argv[1])=="--fellowship-smoke-fixture") {
            const auto folder=std::filesystem::path(argv[2]);
            std::ofstream content{folder/"fellowship-content.json"};
            content<<federal_program.canonical_content_bytes;
            check(content.good(),"Write the isolated fellowship content for the native walkthrough.");
            persistence::CurrentCampaign{folder/"saves"/"continuing"}.commit(federal_program,fellow.checkpoint());
        }
        const auto prior_founder_money=fellowship_state().succession->personal_cash;
        const auto prior_company_accounts=*fellowship_state().accounts;
        const auto prior_family=departure.family;
        const auto fellowship_day=runtime::campaign_day(departure);
        fellowship_commit(core::AcceptResearchAppointment{fellowship_offer.front().employer});
        const auto& employed=fellow.current_state().world;
        const auto* fellowship_lab=runtime::career_employer(employed);
        check(fellowship_lab && fellowship_lab->planning->entered_on==fellowship_day &&
            runtime::career_payroll(*fellowship_lab)->monthly_amount==fellowship_offer.front().salary &&
            fellowship_state().succession->personal_cash==prior_founder_money && *fellowship_state().accounts==prior_company_accounts,
            "The fallback must establish an employer with its own resources and payroll.");
        const auto fellowship_topics=runtime::career_research_options(employed,federal_program);
        check(!fellowship_topics.empty() && !fellowship_topics.front().allocation.researchers.empty(),
            "The funded fallback must offer work with named researchers immediately.");
        const auto fellowship_topic=fellowship_topics.front().topic;
        fellowship_commit(core::DirectCareerResearch{fellowship_topic});
        const auto cancelled_id=runtime::career_employer(fellow.current_state().world)->simulation.lab->research.back().project_id;
        fellowship_commit(core::CancelProject{cancelled_id});
        fellowship_commit(core::DirectCareerResearch{fellowship_topic});
        for(int days=0;days<60;++days) {
            fellowship_commit(core::AdvanceDay{});
            const auto& world=fellow.current_state().world;
            const auto* employer=runtime::career_employer(world);
            check(employer!=nullptr,"The fellowship must remain open during the funded project.");
            if(employer->simulation.lab->research.back().completed && runtime::career_salary_received(world).value()>0) break;
        }
        fellowship_commit(core::PublishCareerResearch{fellowship_topic});
        const auto& fellowship_finished=fellow.current_state();
        check(runtime::career_salary_received(fellowship_finished.world).value()>0 &&
            fellowship_finished.world.family->members==prior_family->members &&
            *fellowship_state().accounts==prior_company_accounts &&
            persistence::decode_save(federal_program,persistence::encode_save(federal_program,fellowship_finished)).checkpoint==fellowship_finished,
            "The fallback must settle actual salary, publish, preserve the original company and reload exactly.");

        auto& scenario=pgp_test::definition(document,"scenario.century_frontier");
        scenario["starts_on"]="2113-12-29";
        scenario.erase("backgrounds");
        scenario.erase("resilience");
        scenario.erase("frontier");
        scenario["flows"]=nlohmann::json::array();
        scenario["business_offers"]=nlohmann::json::array();
        scenario["management"]["setbacks"]=nlohmann::json::array();
        scenario["management"]["succession"]["founder_birth_year"]=2075;
        scenario["objectives"]["retrospective"]=true;
        auto society=pgp_test::definition(document,"society.century");
        society["id"]="society.fixture_life";
        society["catalog"]["forum"]["first_agenda_day"]=core::to_ordinal({2113,12,29}).value();
        scenario["society"]="society.fixture_life";
        scenario["competition"]["domestic"]=1;
        scenario["competition"]["international"]=0;
        scenario["competition"]["heavyweights"]=0;
        auto& rivals=scenario["rivals"];
        rivals.erase(std::remove_if(rivals.begin(),rivals.end(),[](const auto& row){
            return row["id"]!="company.century.frontier";
        }),rivals.end());
        rivals.front()["start_after_days"]=0;
        rivals.front()["planning"]["slot"]=1;
        document["definitions"].push_back(std::move(society));
        auto& definitions=document["definitions"];
        definitions.erase(std::remove_if(definitions.begin(),definitions.end(),[](const auto& row){
            return row["kind"]=="scenario" && row["id"]!="scenario.century_frontier";
        }),definitions.end());
        pgp_test::prune_unused_flows(document);
        const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.century_frontier","US-WA",
            core::ObjectiveSelection{core::ObjectiveKind::utopia,{}})};
        auto result=game.prepare_opening(game.current_state().world.simulation.revision);
        check(result.accepted(),"The retrospective fixture must prepare.");
        const auto act=[&](core::CommandAction action) {
            const auto receipt=game.submit({game.current_state().world.simulation.revision,std::move(action)});
            if(!receipt.accepted()) throw std::runtime_error(receipt.explanation);
        };
        for(int day=0;day<3;++day) {
            const auto& state=game.current_state().world.simulation;
            if(const auto warning=core::liquidity_report(*state.liquidity).pending_warning) act(core::AcknowledgeLiquidityWarning{*warning});
            while(const auto* notice=core::required_succession_notice(game.current_state().world.simulation))
                act(core::AcknowledgeSuccession{notice->id});
            act(core::AdvanceDay{});
        }
        const auto& ended=game.current_state();
        check(ended.world.objectives->end && ended.world.objectives->end->cause==runtime::CampaignEndCause::deadline &&
            !runtime::required_objective_review(ended.world) && !runtime::objective_postmortem_candidate(ended.world),
            "World conditions must reach the retrospective without a failed-objective review.");
        for(const auto kind:core::objective_kinds)
            check(runtime::objective_result(ended.world,kind)==runtime::ObjectiveResult::unavailable,
                "The retrospective must never award a player victory or defeat.");
        check(persistence::decode_save(program,persistence::encode_save(program,ended)).checkpoint==ended,
            "The completed record must reload exactly.");
        auto snapshot=std::make_shared<bridge::CampaignSnapshot>();
        snapshot->checkpoint=ended;
        bridge::ObjectivesController controller;
        controller.update(snapshot,program,true);
        check(controller.retrospective() && controller.rows().empty() && !controller.editable() &&
            !controller.canContinue() && controller.breachText().isEmpty() && !controller.report().isEmpty(),
            "The public controller must expose a newspaper instead of score choices and results.");

        // Presentation checks use an explicitly constructed terminal biography.
        auto obituary=fresh;
        auto& end=obituary.world.objectives->end.emplace();
        end.day=founder.ends_before;
        end.cause=runtime::CampaignEndCause::deadline;
        const auto newspaper=bridge::lifeReport(obituary,installed);
        check(newspaper.value("obituary").toBool() && newspaper.value("headline").toString()=="Alex Vale dies at 100",
            "The obituary must use the founder's actual birth date and name.");
        end.day=core::to_ordinal({2016,1,1});
        end.cause=runtime::CampaignEndCause::insolvency;
        const auto closure=bridge::lifeReport(obituary,installed);
        check(!closure.value("obituary").toBool() && closure.value("headline").toString().contains("Cedar Works") &&
            closure.value("headline").toString().endsWith("closes"),
            "A closure must name the company chapter without reporting the founder's death.");
        end.cause=runtime::CampaignEndCause::retirement;
        const auto voluntary=bridge::lifeReport(obituary,installed);
        check(!voluntary.value("obituary").toBool() && voluntary.value("headline").toString()=="Alex Vale looks back at 19",
            "Voluntarily ending after resignation must report the current age without an obituary.");
        std::cout<<"Paid lab and fellowship careers, federal closure, exact reload and dated newspaper checks passed.\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
