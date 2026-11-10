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
    content::FoundingSelection founder;
    founder.family="rich";founder.leg="charisma";founder.route="bootstrap";founder.office="home";founder.researcher="none";
    for(const auto& scene:scenario.founding->scenes)founder.choices.emplace(scene.id,scene.choices.front().id);
    const auto start=[&](std::string gender,bool partnership=false) {
        runtime::ScenarioOptions options;options.founding=founder;options.seed=718;
        options.founding->gender=std::move(gender);
        if(partnership) {
            options.founding->route="partnership";options.founding->office="shared";
            options.founding->partner="partner_okafor";
        }
        options.identity=runtime::CampaignIdentity{"Audience test",founder.first_name+" "+founder.last_name,"they/them"};
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",std::nullopt,options);
        opening.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime game{program,opening};
        check(game.prepare_opening(opening.world.simulation.revision).accepted(),"Prepare the founder audience test.");
        return game;
    };
    auto woman=start("woman"), man=start("man"), neutral=start("unspecified"), nonbinary=start("nonbinary");
    const auto state=[](const auto& game)->const auto&{return game.current_state().world.simulation;};
    const auto& candidates=state(neutral).management->people->applicants;
    check(candidates==state(woman).management->people->applicants && candidates==state(man).management->people->applicants &&
        candidates==state(nonbinary).management->people->applicants,"Founder identity must leave the applicant pool, skills, preferences and traits intact.");
    const auto applicant=std::ranges::find_if(candidates,[](const auto& row) {
        return row.profile.gender=="man" && row.recruitment->minimum_salary<row.recruitment->ask_salary;
    });
    check(applicant!=candidates.end(),"The seeded pool must have a male applicant with room for a higher salary floor.");
    const auto id=applicant->profile.id;
    const auto salary=applicant->recruitment->minimum_salary;
    const auto lower_salary=core::Cents{salary.value()*99/100};
    const auto signing=applicant->recruitment->ask_signing;
    const auto submit=[&](runtime::Runtime& game,core::CommandAction action) {
        const auto result=game.submit({state(game).revision,std::move(action)});
        check(result.accepted(),result.explanation.c_str());
    };
    for(auto* game:{&woman,&man,&neutral,&nonbinary}) {
        submit(*game,core::ManageRecruitment{core::ProposeEmployment{id,game==&woman?salary:lower_salary,signing}});
        submit(*game,core::AdvanceDay{});
    }
    const auto reply=[&](const auto& game) {
        const auto& applicants=state(game).management->people->applicants;
        return std::ranges::find_if(applicants,[&](const auto& row){return row.profile.id==id;})->recruitment->offers.back().reply;
    };
    check(reply(woman)=="counter" && reply(man)=="accept" && reply(neutral)=="counter" && reply(nonbinary)=="counter",
        "Audience adjustments must raise or lower the salary boundary while unmatched groups use the base terms.");
    check(persistence::decode_save(program,persistence::encode_save(program,woman.current_state())).checkpoint==woman.current_state(),
        "The offer and audience binding must survive exact save reload.");
    auto forged=woman.current_state();
    forged.world.simulation.management->people->founder_gender="man";
    bool refused=false;
    try {runtime::validate_checkpoint(program,forged);} catch(const std::invalid_argument&) {refused=true;}
    check(refused,"A forged founder identity must be refused.");

    const auto partnered=start("woman",true), partnered_neutral=start("unspecified",true);
    const auto mood=core::workplace_moods(state(partnered)).begin()->second;
    const auto neutral_mood=core::workplace_moods(state(partnered_neutral)).begin()->second;
    check(mood.founder_audience==3 && neutral_mood.founder_audience==0 && mood.score==neutral_mood.score+3,
        "A working cofounder's mood must show the positive audience response as a separate factor.");
    auto changed_leadership=state(partnered);
    auto successor=changed_leadership.succession->terms.front();
    successor.starts_on=core::advance_days(changed_leadership.date,core::Days{1});successor.person="successor";
    changed_leadership.succession->terms.front().ended_on=successor.starts_on;
    changed_leadership.succession->terms.push_back(successor);
    check(core::founder_audience_on(changed_leadership,"woman",changed_leadership.date) &&
        !core::founder_audience_on(changed_leadership,"woman",successor.starts_on),
        "A later CEO's appointment ends the current effect and preserves dated offers.");
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
        founder.leg = "charisma";
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
