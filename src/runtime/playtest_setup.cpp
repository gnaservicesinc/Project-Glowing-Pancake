#include <pgp/runtime/playtest_setup.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/objectives.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/competition.hpp>
#include <pgp/runtime/heavyweight.hpp>
#include <pgp/runtime/society.hpp>
#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>
#include <pgp/core/devices.hpp>
#include <pgp/core/governance.hpp>
#include <pgp/core/headquarters.hpp>
#include <pgp/core/lab.hpp>
#include <pgp/core/people.hpp>
#include <pgp/core/research_data.hpp>
#include <pgp/core/succession.hpp>
#include <pgp/core/workplace_research.hpp>

#include <algorithm>
#include <ranges>
#include <set>
#include <stdexcept>

namespace pgp::runtime {
#if !PGP_DISTRIBUTION
namespace {
const content::ScenarioDefinition* find_scenario(const CompiledProgram& program, std::string_view id) {
    const auto* def = program.collection.find(id);
    if (!def) return nullptr;
    return std::get_if<content::ScenarioDefinition>(&def->data);
}

bool valid_text(std::string_view s, std::size_t max_len) {
    return !s.empty() && s.size() <= max_len && s.find('\0') == std::string_view::npos &&
           s.find_first_of("\r\n\t") == std::string_view::npos;
}

std::string topic_title(const CompiledProgram& program, const std::string& id) {
    if (const auto* msg = program.collection.message(id + ".title")) return *msg;
    return id;
}
} // namespace

PlaytestSetupConfig default_playtest_setup(const CompiledProgram& program, std::string_view scenario_id) {
    PlaytestSetupConfig cfg;
    cfg.scenario_id = std::string(scenario_id);
    cfg.appearance = core::make_portrait(20260402ULL, "playtest/protagonist");
    if (const auto* scenario = find_scenario(program, scenario_id)) {
        cfg.seed = scenario->seed;
        if (!scenario->headquarters_choices.empty()) cfg.headquarters = scenario->headquarters_choices.front();
        const auto start_year = core::from_ordinal(scenario->starts_on).year;
        cfg.launch_year = start_year;
        cfg.birth_year = cfg.launch_year - 26;
        if (scenario->management && scenario->management->governance && !scenario->management->governance->charters.empty()) {
            cfg.charter = scenario->management->governance->charters.front().id;
        }
        if (scenario->lab) {
            const auto lab = content::scenario_lab_catalog(program.collection, *scenario);
            if (!lab.data.empty()) cfg.owned_dataset_ids.push_back(lab.data.front().id);
        }
    }
    return cfg;
}

std::vector<std::string> validate_playtest_setup(const CompiledProgram& program, const PlaytestSetupConfig& config) {
    std::vector<std::string> errors;
    const auto* scenario = find_scenario(program, config.scenario_id);
    if (!scenario) {
        errors.push_back("Selected scenario definition does not exist.");
        return errors;
    }
    if (!valid_text(config.first_name, 48)) errors.push_back("Enter a protagonist first name (1-48 characters).");
    if (!valid_text(config.last_name, 48)) errors.push_back("Enter a protagonist last name (1-48 characters).");
    if (!valid_text(config.company_name, 96)) errors.push_back("Enter a company name (1-96 characters).");
    if (!valid_text(config.pronouns, 48)) errors.push_back("Enter protagonist pronouns (1-48 characters).");
    if (config.gender != "woman" && config.gender != "man" && config.gender != "nonbinary")
        errors.push_back("Choose woman, man, or nonbinary for gender.");

    if (!scenario->founding || std::ranges::none_of(scenario->founding->families, [&](const auto& f) { return f.id == config.family; }))
        errors.push_back("Choose a family background from the founding catalog.");
    if (config.industry_stage < 0 || config.industry_stage > 3) errors.push_back("Choose a starting industry stage from 0 to 3.");
    if (!valid_text(config.education, 128)) errors.push_back("Enter a bounded education description.");
    const int min_year = core::from_ordinal(scenario->starts_on).year;
    const int max_year = std::min(min_year + 5, std::max(min_year, core::from_ordinal(scenario->ends_before).year - 2));
    if (config.launch_year < min_year || config.launch_year > max_year)
        errors.push_back("Starting year must be between " + std::to_string(min_year) + " and " + std::to_string(max_year) + ".");
    const auto age = static_cast<std::int64_t>(config.launch_year) - config.birth_year;
    if (age < 18 || age > 70)
        errors.push_back("Protagonist age at start must be between 18 and 70 (adjust birth year or start year).");
    if (config.experience > static_cast<std::uint32_t>(std::max<std::int64_t>(0, age - 18)))
        errors.push_back("Years of experience cannot exceed working years since age 18.");

    if (config.research > 100 || config.engineering > 100 || config.evaluation > 100 ||
        config.mission_fit > 100 || config.prestige > 100 || config.buzz > 100)
        errors.push_back("Skill, fit, prestige, and buzz stats must be between 0 and 100.");

    if (config.personal_cash.value() < 0 || config.personal_cash.value() > 1000000000000LL)
        errors.push_back("Personal cash must be between $0 and $10,000,000,000.");
    if (config.personal_debt.value() < 0 || config.personal_debt.value() > 100000000000LL)
        errors.push_back("Personal debt must be between $0 and $1,000,000,000.");
    if (config.company_cash.value() < 0 || config.company_cash.value() > 1000000000000LL)
        errors.push_back("Company cash must be between $0 and $10,000,000,000.");
    if (config.company_debt.value() < 0 || config.company_debt.value() > 100000000000LL)
        errors.push_back("Company debt must be between $0 and $1,000,000,000.");

    if (std::ranges::find(scenario->headquarters_choices, config.headquarters) == scenario->headquarters_choices.end())
        errors.push_back("Selected headquarters state is not offered by this scenario.");
    if (config.legal_form != "for_profit" && config.legal_form != "nonprofit")
        errors.push_back("Legal form must be for_profit or nonprofit.");
    if (config.route != "self_funded" && config.route != "hired_ceo" && config.route != "partnership" && config.route != "backed")
        errors.push_back("Starting route must be self_funded, hired_ceo, partnership, or backed.");
    if (config.mission != "research" && config.mission != "public benefit" && config.mission != "commercial")
        errors.push_back("Company mission must be research, public benefit, or commercial.");
    if (!core::objective_kind(config.objective))
        errors.push_back("Choose a valid primary objective (survival, utopia, profit, monopoly, domination, or asi).");
    if (config.legal_form == "nonprofit" && config.objective == "profit")
        errors.push_back("A nonprofit organization cannot use founder wealth (profit) as its primary objective.");

    if (config.founder_equity_bp > 10000)
        errors.push_back("Founder equity must be between 0% and 100%.");
    if (config.founder_board_seats < 1 || config.founder_board_seats > 3)
        errors.push_back("Protagonist board seats must be between 1 and 3.");
    if (config.outside_board_seats > 6 || config.founder_board_seats + config.outside_board_seats > 12)
        errors.push_back("Total board seats cannot exceed 12 (outside seats 0-6).");

    if (config.extra_staff_count > 8)
        errors.push_back("Starting extra staff must be between 0 and 8.");
    if (config.staff_skill_level < 1 || config.staff_skill_level > 100)
        errors.push_back("Staff skill level must be between 1 and 100.");
    if (config.best_friends_count > 6)
        errors.push_back("Starting close industry contacts must be between 0 and 6.");
    if (config.office != "home" && config.office != "leased")
        errors.push_back("Office mode must be home or leased.");
    if (config.extra_laptops > 3 || config.extra_desktops > 4)
        errors.push_back("Starting extra laptops (0-3) or desktops (0-4) exceed bounds.");
    if (config.starting_compute.value() < 0 || config.starting_compute.value() > 1000000000LL)
        errors.push_back("Starting compute must be between 0 and 1,000,000 CU/day.");

    if (scenario->management && scenario->management->governance && !config.charter.empty()) {
        const auto& charters = scenario->management->governance->charters;
        if (std::ranges::none_of(charters, [&](const auto& ch) { return ch.id == config.charter; }))
            errors.push_back("Selected bylaws/charter ID is not in this scenario.");
    }

    if (scenario->management && scenario->management->people && scenario->management->people->talent) {
        const auto& catalog_skills = scenario->management->people->talent->skills;
        for (const auto& sk : config.skills) {
            if (std::ranges::none_of(catalog_skills, [&](const auto& def) { return def.id == sk; }))
                errors.push_back("Unknown skill ID: " + sk);
        }
    }

    if (config.traits.size() > core::person_trait_limit)
        errors.push_back("Too many protagonist traits selected.");

    if (scenario->lab) {
        const auto lab = content::scenario_lab_catalog(program.collection, *scenario);
        std::set<std::string> chosen_research(config.completed_research_ids.begin(), config.completed_research_ids.end());
        for (const auto& rid : config.completed_research_ids) {
            const auto it = std::ranges::find(lab.research, rid, &core::LabResearch::id);
            if (it == lab.research.end()) {
                errors.push_back("Unknown research topic ID: " + rid);
                continue;
            }
            for (const auto& req : it->all_of) {
                if (!chosen_research.contains(req)) {
                    errors.push_back("Completed research '" + topic_title(program, it->id) + "' also requires prerequisite '" + topic_title(program, req) + "'.");
                }
            }
            for (const auto& alternatives : it->any_of_groups) {
                if (std::ranges::none_of(alternatives, [&](const auto& id) { return chosen_research.contains(id); }))
                    errors.push_back("Completed research '" + topic_title(program, rid) + "' needs one of: " +
                        [&] { std::string names; for (const auto& id : alternatives) {
                            if (!names.empty()) names += ", ";
                            names += topic_title(program, id);
                        } return names; }());
            }
            if (!it->any_of.empty() && std::ranges::none_of(it->any_of, [&](const auto& req) { return chosen_research.contains(req); })) {
                errors.push_back("Completed research '" + topic_title(program, it->id) + "' requires at least one prerequisite from its branch.");
            }
        }
        for (const auto& did : config.owned_dataset_ids) {
            if (std::ranges::none_of(lab.data, [&](const auto& d) { return d.id == did; }))
                errors.push_back("Unknown dataset ID: " + did);
        }
    }
    if (config.include_starter_product && scenario->lab) {
        const auto catalog = content::scenario_lab_catalog(program.collection, *scenario);
        const bool evaluated = std::ranges::any_of(catalog.evaluations, [&](const auto& plan) {
            return plan.independent && plan.coverage.front().value() > 0 && std::ranges::all_of(plan.requires_research,
                [&](const auto& id) { return std::ranges::find(config.completed_research_ids, id) != config.completed_research_ids.end(); });
        });
        if (!evaluated) errors.push_back("A starting service needs completed research for an independent document evaluation.");
        for (const auto& id : config.owned_dataset_ids) {
            const auto offer = std::ranges::find(catalog.data, id, &core::DataOffer::id);
            if (offer != catalog.data.end() && !offer->commercial_use) errors.push_back(offer->name + " does not permit commercial training.");
        }
    }
    if (config.include_starter_product && !config.include_starter_model)
        errors.push_back("Starting with a deployed product requires enabling the starter model.");
    if (config.include_starter_model && config.owned_dataset_ids.empty())
        errors.push_back("Starting with a trained model requires selecting at least one owned dataset.");
    return errors;
}

Checkpoint begin_playtest_scenario(const CompiledProgram& program, const PlaytestSetupConfig& config) {
    const auto errors = validate_playtest_setup(program, config);
    if (!errors.empty()) throw std::invalid_argument(errors.front());

    const auto* scenario = find_scenario(program, config.scenario_id);
    content::FoundingSelection founding;
    founding.mode = "playtest";
    founding.first_name = config.first_name;
    founding.last_name = config.last_name;
    founding.gender = config.gender;
    founding.orientation = config.orientation;
    founding.appearance = config.appearance;
    founding.family = config.family;
    founding.sociability = config.sociability;
    founding.activity = config.activity;
    founding.disposition = config.disposition;
    founding.advantages = config.advantages;
    founding.legal_form = config.legal_form == "nonprofit" ? "nonprofit" : "corporation";
    founding.route = config.route == "self_funded" ? "bootstrap" :
                    config.route == "backed" ? (config.legal_form == "nonprofit" ? "mission_director" : "investment") : config.route;
    founding.office = config.office == "home" ? "home" : "shared";
    founding.researcher = "founder";

    ScenarioOptions opts;
    opts.identity = CampaignIdentity{config.company_name, config.first_name + " " + config.last_name, config.pronouns};
    opts.seed = config.seed;
    opts.charter = config.charter;
    opts.founding = founding;

    core::ObjectiveSelection target_objective{*core::objective_kind(config.objective), {}};
    const auto first_day = core::from_ordinal(scenario->starts_on);
    opts.playtest_start = core::to_ordinal({config.launch_year, first_day.month, first_day.day});
    Checkpoint checkpoint = begin_scenario(program, config.scenario_id, config.headquarters, target_objective, std::move(opts));
    auto& world = checkpoint.world;
    auto& sim = world.simulation;

    world.developer_playtest = true;
    world.playtest_custom = true;
    world.starting_industry_stage = config.industry_stage;
    world.playtest_unlimited = config.playtest_unlimited;

    // 1. Pre-fund temporary working capital so real device/equipment/data purchases post cleanly,
    // then reconcile exact requested company cash at the end.
    if (sim.accounts) {
        constexpr core::Cents prefund{500000000LL};
        if (core::accounts_report(*sim.accounts).cash < prefund) {
            core::record_cash(*sim.accounts, sim.date, prefund, core::Account::equity,
                              "Playtest setup pre-funding", "playtest.capital/prefund");
            sim.cash = core::accounts_report(*sim.accounts).cash;
        }
    }

    // 2. Extra compute source & real devices/equipment through core transactions
    if (sim.capacity) {
        const auto compute_rate = config.starting_compute.value() > 0
            ? config.starting_compute
            : ((!config.completed_research_ids.empty() || config.include_starter_model) && sim.capacity->sources.empty()
                   ? core::MilliCUPerDay{10000}
                   : core::MilliCUPerDay{0});
        if (compute_rate.value() > 0) {
            const auto existing = std::ranges::find(sim.capacity->sources, "playtest.compute", &core::CapacitySource::id);
            if (existing == sim.capacity->sources.end()) {
                core::add_capacity_source(*sim.capacity, sim.date,
                    {"playtest.compute", "physical/" + world.company_id + "/playtest.compute", world.company_id,
                     "playtest.provider", core::CapacitySourceKind::leased, compute_rate, compute_rate,
                     std::max<std::uint64_t>(1048576, static_cast<std::uint64_t>(compute_rate.value())), "general", true, sim.date});
            }
        }
    }
    if (sim.infrastructure && sim.infrastructure->workplace && sim.infrastructure->workplace->devices) {
        for (std::uint32_t i = 0; i < config.extra_laptops; ++i) {
            core::buy_personal_device(sim, "laptop", core::Cents{0});
            auto& dev = sim.infrastructure->workplace->devices->records.back();
            if (dev.ready_on > sim.date) {
                dev.ready_on = sim.date;
                dev.playtest_delivery = true;
            }
        }
    }
    if (sim.infrastructure && sim.infrastructure->workplace && sim.infrastructure->workplace->headquarters &&
        sim.infrastructure->catalog.workplace && sim.infrastructure->catalog.workplace->headquarters) {
        const auto& hq_rules = *sim.infrastructure->catalog.workplace->headquarters;
        if (config.office == "leased" && core::current_headquarters(sim).kind == "home") {
            const auto market_it = std::ranges::find(hq_rules.markets, config.headquarters, &core::HQMarket::state);
            if (market_it != hq_rules.markets.end() && !hq_rules.plans.empty()) {
                core::manage_headquarters(sim, core::LeaseHQ{market_it->id, hq_rules.plans.front().id}, core::Cents{0});
            }
        }
        const auto& eq_catalog = hq_rules.equipment;
        const auto desk_it = std::ranges::find(eq_catalog, std::string_view{"desk"}, &core::HQEquipmentOffer::kind);
        const auto pc_it = std::ranges::find(eq_catalog, std::string_view{"desktop"}, &core::HQEquipmentOffer::kind);
        for (std::uint32_t i = 0; i < config.extra_desktops; ++i) {
            if (desk_it != eq_catalog.end()) {
                core::manage_headquarters(sim, core::BuyHQEquipment{desk_it->id}, core::Cents{0});
                auto& desk = sim.infrastructure->workplace->headquarters->items.back();
                if (desk.delivered_on > sim.date) {
                    desk.delivered_on = sim.date;
                    desk.playtest_delivery = true;
                }
            }
            if (pc_it != eq_catalog.end()) {
                core::manage_headquarters(sim, core::BuyHQEquipment{pc_it->id}, core::Cents{0});
                auto& pc = sim.infrastructure->workplace->headquarters->items.back();
                if (pc.delivered_on > sim.date) {
                    pc.delivered_on = sim.date;
                    pc.playtest_delivery = true;
                }
            }
        }
        core::prepare_infrastructure(sim);
    }

    // 3. Ownership & Board seats
    if (sim.accounts && sim.management && sim.management->governance && founding.legal_form != "nonprofit") {
        const std::int64_t total_shares = 10000;
        const std::int64_t founder_shares = static_cast<std::int64_t>(config.founder_equity_bp);
        const std::int64_t option_shares = std::min<std::int64_t>(1000, total_shares - founder_shares);
        const std::int64_t outside_shares = total_shares - founder_shares - option_shares;
        sim.accounts->holdings.clear();
        if (founder_shares > 0) sim.accounts->holdings.push_back({sim.management->catalog.scoring.founder, core::BasisPoints{founder_shares}, false});
        if (outside_shares > 0) sim.accounts->holdings.push_back({"investors/playtest", core::BasisPoints{outside_shares}, false});
        if (option_shares > 0) sim.accounts->holdings.push_back({"option_pool", core::BasisPoints{option_shares}, true});
        if (sim.accounts->holdings.empty()) sim.accounts->holdings.push_back({sim.management->catalog.scoring.founder, core::BasisPoints{total_shares}, false});
        sim.management->governance->opening_holdings = sim.accounts->holdings;

        const auto founder_board_id = sim.management->catalog.succession
            ? sim.management->catalog.succession->founder_board
            : std::string{"board.founder"};
        auto& board = sim.management->catalog.governance->board;
        board.clear();
        board.push_back({founder_board_id, "Protagonist", config.founder_board_seats, true, core::Cents{0}, core::BasisPoints{3000}});
        if (config.outside_board_seats > 0) {
            std::uint32_t remaining = config.outside_board_seats;
            std::uint32_t idx = 1;
            while (remaining > 0) {
                const std::uint32_t chunk = std::min<std::uint32_t>(3, remaining);
                board.push_back({"outside_" + std::to_string(idx),
                                 idx == 1 ? "Outside Board Seat" : "Board Director " + std::to_string(idx),
                                 chunk, false, core::Cents{5000000}, core::BasisPoints{2000}});
                remaining -= chunk;
                ++idx;
            }
        }
    }

    // 4. Personal finances, birth year, and term reconciliation
    if (sim.succession && sim.management && sim.management->catalog.succession) {
        auto& sc = *sim.management->catalog.succession;
        sc.founder_birth_year = config.birth_year;
        const auto current_personal = sim.succession->personal_cash;
        const auto delta = core::checked_unit_subtract(config.personal_cash, current_personal);
        if (delta.value() >= 0) {
            sim.succession->playtest_cash = core::checked_unit_add(sim.succession->playtest_cash, delta);
        } else {
            sc.personal_cash = config.personal_cash;
            sim.succession->playtest_cash = core::Cents{0};
        }
        sim.succession->personal_cash = config.personal_cash;
        if (config.personal_debt.value() > 0 && sim.succession->personal_life) {
            sim.succession->personal_life->student_loan = core::PersonalStudentLoan{
                config.personal_debt, core::BasisPoints{45}, sim.date,
                core::advance_days(sim.date, core::Days{365 * 5}), {}};
        }
        if (!sim.succession->terms.empty()) {
            auto& term = sim.succession->terms.front();
            term.birth_year = config.birth_year;
            const int retirement_year = config.birth_year + static_cast<int>(sc.executive_retirement_age);
            const auto retirement_day = core::to_ordinal({retirement_year, 1, 1});
            const auto start_cal = core::from_ordinal(term.starts_on);
            const auto max_term_day = core::to_ordinal({start_cal.year + static_cast<int>(sc.maximum_term_years),
                                                        start_cal.month,
                                                        std::min<unsigned>(start_cal.day, 28)});
            term.due_on = std::min(retirement_day, max_term_day);
            if (term.due_on <= term.starts_on) term.due_on = core::advance_days(term.starts_on, core::Days{365});
        }
    }

    // 5. Protagonist stats, traits, skills, mission, and inherited purpose
    std::string founder_emp_id;
    if (sim.management && sim.management->people) {
        auto& people = *sim.management->people;
        people.mission = config.mission;
        people.founding_prestige = std::min(100U, config.prestige);
        people.community_buzz = std::min(100U, config.buzz);
        if (people.social && people.social->mission_conflict) {
            people.social->mission_conflict->mission = config.mission;
            people.social->mission_conflict->committed_on = sim.date;
            people.social->mission_conflict->inherited_purpose = config.inherited_purpose || config.route == "hired_ceo";
        }
        if (people.talent) {
            people.talent->founder_skills = config.skills;
            people.talent->founder_genius = std::ranges::find(config.advantages, "genius") != config.advantages.end();
        }
        for (auto& emp : people.employees) {
            if (emp.terms.profile.role == "Founder / researcher") {
                founder_emp_id = emp.terms.profile.id;
                auto& p = emp.terms.profile;
                p.birth_year = config.birth_year;
                if (world.romance) {
                    world.romance->founder.born_on = core::to_ordinal({config.birth_year, 1, 1});
                    p.romantic = world.romance->founder;
                } else if (p.romantic) {
                    p.romantic->born_on = core::to_ordinal({config.birth_year, 1, 1});
                }
                if (world.family) {
                    const auto founder_born = core::to_ordinal({config.birth_year, 1, 1});
                    const int parent_year = config.birth_year - 27;
                    for (std::size_t idx = 0; idx < std::min<std::size_t>(2, world.family->members.size()); ++idx) {
                        auto member = world.family->members[idx];
                        if (member.profile.birth_year > parent_year) {
                            member.profile.birth_year = parent_year;
                            if (member.profile.romantic) {
                                member.profile.romantic->born_on = core::to_ordinal({parent_year, 1, 1});
                            }
                            world.family->members.replace(idx, std::move(member));
                        }
                    }
                    for (std::size_t idx = 0; idx < world.family->edges.size(); ++idx) {
                        auto edge = world.family->edges[idx];
                        if (edge.child == "founder" && edge.kind == "biological") {
                            edge.started_on = founder_born;
                            world.family->edges.replace(idx, std::move(edge));
                        }
                    }
                }
                p.experience = config.experience;
                p.research = config.research;
                p.engineering = config.engineering;
                p.evaluation = config.evaluation;
                emp.terms.evaluation_credit = core::BasisPoints{
                    sim.management->catalog.people->maximum_evaluation_credit.value() * p.evaluation / 100};
                p.mission_fit = config.mission_fit;
                p.education = config.education;
                p.preferred_mission = config.mission;
                p.skills = config.skills;
                p.portrait = founding.appearance;
                p.traits = config.traits;
                if (people.social) people.social->founder_traits = p.traits;
            }
        }
        if (founder_emp_id.empty() && !people.employees.empty()) {
            founder_emp_id = people.employees.front().terms.profile.id;
        }
        const std::array<std::string_view, 3> roles{"Researcher", "Software engineer", "Evaluation researcher"};
        const std::array<std::string_view, 3> role_skills{"ml_research", "programming", "evaluation"};
        const int cur_year = core::from_ordinal(sim.date).year;
        for (std::uint32_t i = 0; i < config.extra_staff_count; ++i) {
            core::PersonProfile prof;
            prof.id = "employee/" + std::to_string(people.next_id++);
            prof.name = "Team Member " + std::to_string(i + 1);
            prof.gender = (i % 3 == 0) ? "woman" : (i % 3 == 1) ? "man" : "nonbinary";
            prof.pronouns = (i % 3 == 0) ? "she/her" : (i % 3 == 1) ? "he/him" : "they/them";
            prof.birth_year = cur_year - 28 - static_cast<int>(i % 8);
            prof.experience = 4;
            prof.education = "Master's degree";
            prof.role = std::string(roles[i % roles.size()]);
            prof.applied_on = sim.date;
            prof.portrait = core::make_portrait(config.seed ^ (i + 77ULL), prof.id);
            prof.preferred_mission = config.mission;
            prof.mission_fit = 82;
            prof.research = config.staff_skill_level;
            prof.engineering = config.staff_skill_level;
            prof.evaluation = config.staff_skill_level;
            prof.orientation = "straight";
            if (sim.management->catalog.people->romance) {
                core::initialize_romantic_profile(prof, config.seed ^ (i + 77ULL), *sim.management->catalog.people->romance);
            }
            if (people.talent) {
                const auto sk = std::string(role_skills[i % role_skills.size()]);
                if (std::ranges::any_of(sim.management->catalog.people->talent->skills, [&](const auto& s) { return s.id == sk; }))
                    prof.skills.push_back(sk);
            }
            core::Applicant app{prof, core::Cents{0}, core::Cents{0},
                                core::BasisPoints{sim.management->catalog.people->maximum_evaluation_credit.value() * prof.evaluation / 100}, {}};
            people.employees.push_back({std::move(app), sim.date, sim.date, true, {}});
            if (sim.capacity) {
                sim.capacity->available_staff += 1;
            }
        }
        if (config.extra_staff_count > 0 && sim.management->catalog.people->dismissal && sim.accounts) {
            std::vector<const core::Employee*> non_founder_opening;
            for (const auto& w : people.employees) {
                if (w.opening && w.terms.profile.role != "Founder / researcher") non_founder_opening.push_back(&w);
            }
            if (!non_founder_opening.empty()) {
                const auto per_person = sim.management->catalog.people->monthly_salary;
                const auto total = core::Cents{per_person.value() * static_cast<std::int64_t>(non_founder_opening.size())};
                auto grouped = std::ranges::find(sim.accounts->recurring, std::string_view{"payroll_with_benefits"}, &core::RecurringObligation::id);
                if (grouped != sim.accounts->recurring.end()) {
                    grouped->monthly_amount = total;
                    grouped->ends_before = people.starts_on;
                } else {
                    sim.accounts->recurring.push_back({"payroll_with_benefits", total, people.starts_on, people.starts_on, "Opening team payroll"});
                }
                for (const auto* w : non_founder_opening) {
                    const auto& pid = w->terms.profile.id;
                    auto rec = std::ranges::find(sim.accounts->recurring, pid, &core::RecurringObligation::id);
                    if (rec != sim.accounts->recurring.end()) rec->monthly_amount = per_person;
                    else core::add_recurring(*sim.accounts, {pid, per_person, people.starts_on, std::nullopt, "Employee salary: " + w->terms.profile.name});
                }
            }
        }
        core::reconcile_employee_study(sim);
        if (people.social && config.best_friends_count > 0) {
            std::uint32_t added = 0;
            for (const auto& [peer_id, peer] : people.industry) {
                if (added >= config.best_friends_count) break;
                if (!peer.active) continue;
                const std::string first = std::string("founder") < peer_id ? "founder" : peer_id;
                const std::string second = std::string("founder") < peer_id ? peer_id : "founder";
                if (std::ranges::any_of(people.social->links, [&](const auto& l) { return l.first == first && l.second == second; }))
                    continue;
                people.social->links.push_back({first, second, 880, 8, people.starts_on, sim.date, std::nullopt, "Playtest starting close contact", true});
                ++added;
            }
        }
    }

    // 6. Objectives override
    if (world.objectives && !world.objectives->selections.empty()) {
        world.objectives->selections.front().selection = target_objective;
    }

    // 7. Pre-completed research, owned datasets & starter model
    if (sim.lab && sim.capacity && sim.accounts) {
        sim.lab->research_work_scale_bp = core::BasisPoints{10000};
        for (const auto& did : config.owned_dataset_ids) {
            const auto it = std::ranges::find(sim.lab->catalog.data, did, &core::DataOffer::id);
            if (it == sim.lab->catalog.data.end()) continue;
            if (std::ranges::any_of(sim.lab->licenses, [&](const auto& l) { return l.offer_id == did; })) continue;
            if (it->price.value() > 0) {
                core::record_cash(*sim.accounts, sim.date, core::Cents{-it->price.value()}, core::Account::expenses,
                                  "Data license: " + it->name, "lab/data/" + it->id);
            }
            sim.lab->licenses.push_back({did, sim.date});
        }
        const bool grounded = sim.lab->catalog.research_workplaces.has_value();
        std::string default_src;
        core::MilliCUPerDay default_rate{1};
        for (const auto& src : sim.capacity->sources) {
            if (src.id == "playtest.compute") { default_src = src.id; default_rate = src.effective; break; }
        }
        if (default_src.empty() && !sim.capacity->sources.empty()) {
            default_src = sim.capacity->sources.front().id;
            default_rate = sim.capacity->sources.front().effective;
        }
        std::vector<std::string> pending = config.completed_research_ids;
        while (!pending.empty()) {
            auto ready = std::ranges::find_if(pending,[&](const auto& id) {
                return core::research_pacing(*sim.lab,*sim.capacity,id,sim.date).missing_groups == 0;
            });
            if (ready == pending.end() || default_src.empty())
                throw std::invalid_argument("Prepared research requires its prerequisite results and a compute source.");
            const auto rid = *ready;
            pending.erase(ready);
            const auto it = std::ranges::find(sim.lab->catalog.research, rid, &core::LabResearch::id);
            if (std::ranges::any_of(sim.lab->research, [&](const auto& r) { return r.research_id == rid && r.completed; })) continue;
            const std::string proj_id = "lab/research/" + it->id + "/1";
            const auto min_work = it->experiment_work;
            core::ResearchRun run;
            if (grounded) run.researchers = {founder_emp_id};
            if (sim.lab->catalog.creative_research.contains(rid)) {
                const auto& employees = sim.management->people->employees;
                const auto founder = std::ranges::find_if(employees, [&](const auto& e) { return e.terms.profile.id == founder_emp_id; });
                if (founder == employees.end()) throw std::invalid_argument("Prepared research needs an onboarded founder.");
                run.original_traits[founder_emp_id] = core::distinct_person_traits(founder->terms.profile);
                const auto team = core::research_team(sim, run.researchers, run.original_traits);
                run.creativity = team.creativity;
                run.compatibility_bp = team.compatibility_bp;
            }
            const auto req_staff_days = grounded ? core::coordinated_staff_days(sim.lab->catalog, *it, run.compatibility_bp, core::BasisPoints{0}) : 0U;
            core::CapacityProject proj;
            proj.id = proj_id;
            proj.source_id = default_src;
            proj.kind = core::ProjectKind::research;
            proj.status = core::ProjectStatus::completed;
            proj.starts_on = sim.date;
            proj.stopped_on = sim.date;
            proj.requested = core::MilliCUPerDay{std::max<std::int64_t>(1, default_rate.value())};
            proj.staff = 1;
            proj.total_work = min_work;
            proj.completed_work = min_work;
            proj.required_staff_days = req_staff_days;
            proj.completed_staff_days = req_staff_days;
            proj.cause = "Playtest completed research: " + topic_title(program, it->id);
            proj.playtest_completed = true;
            sim.capacity->projects.push_back(std::move(proj));
            run.research_id = it->id;
            run.project_id = proj_id;
            run.completed = true;
            run.sponsored_fee = it->cash;
            if (it->data_requirement.required) {
                for (const auto& id : config.owned_dataset_ids) {
                    if (core::research_data_fits(it->data_requirement, core::lab_data(*sim.lab,id))) run.data_ids.push_back(id);
                    if (core::research_data_blocker(*sim.lab,*sim.capacity,sim.date,*it,run.data_ids).empty()) break;
                }
                if (const auto why=core::research_data_blocker(*sim.lab,*sim.capacity,sim.date,*it,run.data_ids); !why.empty())
                    throw std::invalid_argument(topic_title(program,it->id) + ": " + why);
            }
            core::record_research_prototype(*sim.lab,*it,run,sim.capacity->projects.back(),sim.date);
            sim.lab->research.push_back(std::move(run));
        }
        if (config.include_starter_model && !sim.lab->catalog.recipes.empty() && !sim.lab->licenses.empty() && !default_src.empty()) {
            const core::TrainingRecipe* chosen = nullptr;
            for (const auto& r : sim.lab->catalog.recipes) {
                bool ok = true;
                for (const auto& req : r.requires_research) {
                    const bool known = std::ranges::any_of(sim.lab->research, [&](const auto& run) {
                        return run.research_id == req && run.completed;
                    });
                    if (!known) { ok = false; break; }
                }
                if (ok) { chosen = &r; break; }
            }
            if (chosen) {
                core::CapacityProject proj;
                proj.id = "lab/train/playtest_starter";
                proj.source_id = default_src;
                proj.kind = core::ProjectKind::training;
                proj.status = core::ProjectStatus::completed;
                proj.starts_on = sim.date;
                proj.stopped_on = sim.date;
                proj.requested = core::MilliCUPerDay{std::max<std::int64_t>(1, default_rate.value())};
                proj.staff = 1;
                proj.minimum_memory_mib = chosen->minimum_memory_mib;
                proj.interconnect = chosen->interconnect;
                const auto training_work = config.include_starter_product ? chosen->maximum_work : chosen->minimum_work;
                proj.total_work = training_work;
                proj.completed_work = training_work;
                proj.cause = "Playtest starter model training";
                proj.playtest_completed = true;
                sim.capacity->projects.push_back(std::move(proj));
                core::LabModel model;
                model.id = "playtest_starter";
                model.recipe_id = chosen->id;
                model.project_id = "lab/train/playtest_starter";
                model.data_ids = config.owned_dataset_ids;
                std::ranges::sort(model.data_ids);
                model.created_on = sim.date;
                model.consumed_work = training_work;
                model.capability = core::model_capability(sim.lab->catalog, model, sim.lab->generated, sim.lab->acquisitions);
                sim.lab->models.push_back(std::move(model));
            }
        }
    }

    if (config.include_starter_product) {
        if (!sim.lab || !sim.capacity || !sim.accounts || !sim.market || !sim.deployments || sim.lab->models.empty())
            throw std::invalid_argument("A starting service needs a trained model, a serving source and a customer market.");
        const auto& model = sim.lab->models.back();
        const auto& source = sim.capacity->projects.back().source_id;
        const auto plan = std::ranges::find_if(sim.lab->catalog.evaluations, [&](const auto& evaluation) {
            return evaluation.independent && evaluation.coverage.front().value() > 0 &&
                std::ranges::all_of(evaluation.requires_research, [&](const auto& topic) {
                    return core::has_research_access(*sim.lab, *sim.capacity, topic, sim.date);
                });
        });
        if (plan == sim.lab->catalog.evaluations.end()) throw std::invalid_argument("Complete held-out evaluation research to launch the starting service.");
        core::DeployModel release{"product.documents", model.id, "playtest_eval", source, false, {}};
        core::start_evaluation(*sim.lab, *sim.accounts, *sim.capacity, sim.date, release.evidence_id,
            release.model_id, plan->id, {source, core::MilliCUPerDay{1}, 1}, core::Cents{0}, {});
        core::finish_playtest_evaluation(*sim.lab, *sim.capacity, sim.random, sim.date);
        core::deploy_model(*sim.deployments, *sim.lab, *sim.accounts, *sim.capacity, *sim.market, sim.date, release);
    }

    // 8. Reconcile exact requested company cash & optional starting company debt
    if (sim.accounts) {
        if (config.company_debt.value() > 0) {
            core::Debt debt;
            debt.id = "playtest.loan";
            debt.principal = config.company_debt;
            debt.monthly_interest = core::BasisPoints{60};
            debt.drawn_on = sim.date;
            debt.due = core::advance_days(sim.date, core::Days{365 * 3});
            core::draw_debt(*sim.accounts, sim.date, debt, "Playtest starting company loan");
        }
        const auto current_cash = core::accounts_report(*sim.accounts).cash;
        const auto diff = core::checked_unit_subtract(config.company_cash, current_cash);
        if (diff.value() > 0) {
            core::record_cash(*sim.accounts, sim.date, diff, core::Account::equity,
                              "Playtest starting capital contribution", "playtest.capital/setup");
        } else if (diff.value() < 0) {
            core::record_cash(*sim.accounts, sim.date, diff, core::Account::expenses,
                              "Playtest starting balance adjustment", "playtest.setup/cash");
        }
        sim.cash = core::accounts_report(*sim.accounts).cash;
    }

    if (sim.management && sim.management->people) {
        core::reconcile_workplace_research(sim);
    }
    sim.paused = true;
    validate_checkpoint(program, checkpoint);
    return checkpoint;
}

#endif
} // namespace pgp::runtime
