#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void check_founder_audiences() {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(
        persistence::read_bounded(PGP_AUDIENCE_SOURCE,content::maximum_document_bytes),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    check(scenario.management->people->founder_audiences.empty(),"No gender-based pay or mood rules may remain active.");
    const auto start=[&](std::string gender) {
        content::FoundingSelection founder;
        founder.family="rich";founder.advantages={"charisma"};founder.route="bootstrap";founder.office="home";founder.researcher="none";
        founder.gender=std::move(gender);founder.orientation="queer";
        for(const auto& scene:scenario.founding->scenes)founder.choices.emplace(scene.id,scene.choices.front().id);
        runtime::ScenarioOptions options;options.founding=founder;options.seed=718;
        options.identity=runtime::CampaignIdentity{"Identity test",founder.first_name+" "+founder.last_name,"they/them"};
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",std::nullopt,options);
        opening.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime game{program,opening};
        check(game.prepare_opening(opening.world.simulation.revision).accepted(),"Prepare the identity test.");
        return game;
    };
    auto woman=start("woman"),man=start("man"),nonbinary=start("nonbinary");
    const auto& candidates=woman.current_state().world.simulation.management->people->applicants;
    check(candidates==man.current_state().world.simulation.management->people->applicants &&
        candidates==nonbinary.current_state().world.simulation.management->people->applicants,
        "At equal company reputation, changing founder gender must leave all applicant traits and private salary preferences intact.");
    for(auto* game:{&woman,&man,&nonbinary}) {
        const auto& applicant=game->current_state().world.simulation.management->people->applicants.front();
        const auto result=game->submit({game->current_state().world.simulation.revision,
            core::ManageRecruitment{core::ProposeEmployment{applicant.profile.id,applicant.recruitment->minimum_salary,applicant.recruitment->ask_signing}}});
        check(result.accepted(),result.explanation.c_str());
        check(game->submit({game->current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(),"Resolve the offer.");
    }
    check(woman.current_state().world.simulation.management->people->applicants==man.current_state().world.simulation.management->people->applicants &&
        woman.current_state().world.simulation.management->people->applicants==nonbinary.current_state().world.simulation.management->people->applicants,
        "Identical offers must resolve identically for women, men and nonbinary founders.");
}
} // namespace

int main() {
    using namespace pgp;
    try {
        check_founder_audiences();
        const auto registry = content::foundation_registry();
        const auto program = runtime::compile_collection(
            content::load_collection(persistence::read_bounded(PGP_RECRUITING_SOURCE, content::maximum_document_bytes),
                                     registry),
            registry);
        const auto &scenario =
            std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection founder;
        founder.family = "rich";
        founder.route = "bootstrap";
        founder.office = "home";
        founder.advantages = {"charisma"};
        founder.researcher = "none";
        for (const auto &scene : scenario.founding->scenes)
            founder.choices.emplace(scene.id, scene.choices.front().id);
        runtime::ScenarioOptions options;
        options.founding = founder;
        options.identity = runtime::CampaignIdentity{"Test research", founder.first_name+" "+founder.last_name, "they/them"};
        auto initial = runtime::begin_scenario(program, "scenario.founder_campaign", "US-CA",
                                               core::ObjectiveSelection{core::ObjectiveKind::survival, {}}, options);
        initial.world.enabled_flows = std::vector<std::string>{};
        runtime::Runtime game{program, initial};
        const auto state = [&]() -> const auto & { return game.current_state().world.simulation; };
        const auto people = [&]() -> const auto & { return *state().management->people; };
        const auto submit = [&](core::CommandAction action) {
            return game.submit({state().revision, std::move(action)});
        };
        const auto apply = [&](core::CommandAction action) {
            const auto result = submit(std::move(action));
            if (!result.accepted())
                throw std::runtime_error(result.explanation);
        };
        const auto refuse = [&](core::CommandAction action) {
            const auto before = game.current_state();
            check(!submit(std::move(action)).accepted() && game.current_state() == before,
                  "A refused employment action must preserve the offer, desk and finances.");
        };
        const auto advance = [&] { apply(core::AdvanceDay{}); };
        const auto exact_save = [&] {
            const auto result =
                persistence::decode_save(program, persistence::encode_save(program, game.current_state()));
            check(result.checkpoint == game.current_state(), "Fresh negotiation and hiring data must reload exactly.");
        };

        check(game.prepare_opening(state().revision).accepted(), "Opening must prepare.");
        check(people().employees.empty() && people().applicants.size() >= 2,
              "A solo founder can compare multiple applicants.");
        const auto first = people().applicants[0].profile.id;
        const auto second = people().applicants[1].profile.id;
        const auto starting_cash = state().cash;
        const auto starting_payroll = state().accounts->recurring;
        refuse(core::HireEmployee{first, 1});
        apply(core::ManageRecruitment{core::ProposeEmployment{first, core::Cents{250000}, core::Cents{0}}});
        apply(core::ManageRecruitment{core::ProposeEmployment{second, core::Cents{250000}, core::Cents{0}}});
        check(core::recruitment_status(people().applicants[0], state().date) == "waiting",
              "An offer needs time for a reply.");
        check(state().cash == starting_cash && state().accounts->recurring == starting_payroll,
              "Sending an offer cannot create payroll.");
        exact_save();
        refuse(core::ManageRecruitment{core::AcceptEmploymentCounter{first}});
        advance();
        check(core::recruitment_status(people().applicants[0], state().date) == "counter" &&
                  core::recruitment_status(people().applicants[1], state().date) == "counter",
              "Low offers should receive actual candidate counteroffers.");
        apply(core::ManageRecruitment{core::DeclineApplicant{first}});
        check(core::recruitment_status(people().applicants[0], state().date) == "declined" &&
                  people().employees.empty(),
              "Declining one applicant must leave the other negotiation available.");
        apply(core::ManageRecruitment{core::AcceptEmploymentCounter{second}});
        const auto agreed = core::agreed_employment(people().applicants[1]);
        exact_save();
        refuse(core::HireEmployee{second, 1});
        apply(core::ManageHeadquarters{core::BuyHQEquipment{"desk"}});
        refuse(core::HireEmployee{second, 1});
        advance();
        apply(core::ManageHeadquarters{core::PlaceHQEquipment{1, 0, 0, 0, false}});
        apply(core::ManageHeadquarters{core::AssignHQDesk{1, "founder"}});
        refuse(core::HireEmployee{second, 1});
        apply(core::ManageHeadquarters{core::AssignHQDesk{1, ""}});
        const auto pre_hire_cash = state().cash;
        apply(core::HireEmployee{second, 1});
        const auto &hired = people().employees.front();
        check(people().employees.size() == 1 && hired.terms.profile.id == second &&
                  core::assigned_hq_desk(state(), second) == 1 && hired.terms.salary == agreed.first &&
                  hired.terms.signing == agreed.second,
              "Signing must hire the chosen person and assign the real desk at the agreed compensation.");
        const auto payroll = std::ranges::find(state().accounts->recurring, second, &core::RecurringObligation::id);
        check(payroll != state().accounts->recurring.end() && payroll->monthly_amount == agreed.first &&
                  state().cash == core::checked_unit_subtract(pre_hire_cash, agreed.second),
              "Payroll and signing cash must match the negotiation.");
        check(state().capacity->available_staff == 0, "A person in onboarding cannot provide work.");
        const auto ready = hired.ready_on;
        while (state().date < ready)
            advance();
        check(state().capacity->available_staff == 1, "Exactly one employee becomes available on the onboarding date.");
        exact_save();

        const auto another = people().applicants.back().profile.id;
        apply(core::ManageRecruitment{core::ProposeEmployment{another, core::Cents{1000000}, core::Cents{500000}}});
        advance();
        check(core::recruitment_status(people().applicants.back(), state().date) == "agreed",
              "A strong offer can be accepted directly.");
        const auto expiration = people().applicants.back().recruitment->offers.back().expires_on;
        while (state().date < expiration)
            advance();
        check(core::recruitment_status(people().applicants.back(), state().date) == "expired",
              "Unsigned terms expire.");
        refuse(core::HireEmployee{another, 1});
        std::cout << "Multiple applicants, dated replies, counteroffers, decline, agreed payroll, physical desk, "
                     "onboarding, expiry and fresh saves passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
