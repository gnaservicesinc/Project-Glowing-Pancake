#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <set>

namespace pgp::core {
namespace {
int prestige_points(std::size_t reputation, std::uint32_t maximum) {
    return static_cast<int>(std::min<std::size_t>(10, reputation) * maximum / 10);
}

int pay_points(Cents salary, Cents expected, std::uint32_t maximum) {
    if (expected.value() <= 0) return 0;
    const auto limit = static_cast<std::int64_t>(maximum);
    const auto difference = salary.value() - expected.value();
    return static_cast<int>(std::clamp(difference * 2 * limit / expected.value(), -limit, limit));
}
} // namespace

std::map<std::string, EmployeeMood> workplace_moods(const SimulationState& state) {
    std::map<std::string, EmployeeMood> results;
    if (!state.management || !state.management->people || !state.management->catalog.people->mood)
        return results;
    const auto& people = *state.management->people;
    const auto& rules = *state.management->catalog.people;
    const auto& mood = *rules.mood;

    struct Worker {
        const Employee* employee;
        std::vector<std::string> traits;
    };
    std::vector<Worker> workers;
    std::map<std::string, std::uint64_t, std::less<>> frequency;
    for (const auto& employee : people.employees) {
        if (employee.departed_on || employee.terms.profile.role == "Founder / researcher") continue;
        auto traits = distinct_person_traits(employee.terms.profile);
        for (const auto& trait : traits) ++frequency[trait];
        workers.push_back({&employee, std::move(traits)});
    }
    if (workers.empty()) return results;

    Cents inherited_salary{0};
    const auto inherited_staff = std::ranges::count_if(workers, [](const Worker& worker) { return worker.employee->opening; });
    if (inherited_staff && state.accounts) {
        const auto bill = std::ranges::find(state.accounts->recurring, std::string_view{"payroll_with_benefits"}, &RecurringObligation::id);
        if (bill != state.accounts->recurring.end())
            inherited_salary = Cents{bill->monthly_amount.value() / inherited_staff};
    }

    const auto* leader = state.succession && !state.succession->terms.empty() ? &state.succession->terms.back() : nullptr;
    std::size_t company_reputation = 0;
    std::size_t ceo_reputation = !leader || leader->person == "founder" ? people.founding_prestige : 0;
    std::set<std::string_view> researching;
    if (state.lab) {
        company_reputation = state.lab->publications.size();
        for (const auto& publication : state.lab->publications)
            if (!leader || publication.day >= leader->starts_on) ++ceo_reputation;
        std::set<std::string_view> active;
        if (state.capacity) for (const auto& project : state.capacity->projects)
            if (project.kind == ProjectKind::research && project.status == ProjectStatus::active)
                active.insert(project.id);
        for (const auto& run : state.lab->research)
            if (active.contains(run.project_id)) researching.insert(run.researchers.begin(), run.researchers.end());
    }

    const auto* manager = operations_manager_on(state, state.date);
    int amenity_points = 0;
    std::vector<std::string> amenity_names;
    for (const auto& [kind, name] : hq_active_amenities(state)) {
        const auto benefit = mood.amenities.find(kind);
        if (benefit == mood.amenities.end()) continue;
        amenity_points += static_cast<int>(benefit->second);
        amenity_names.push_back(name);
    }
    amenity_points = std::min(amenity_points, static_cast<int>(mood.amenity_cap));
    const auto gathering_points=personal_event_moods(state);

    for (const auto& worker : workers) {
        const auto& employee = *worker.employee;
        const auto& terms = employee.terms;
        const auto& person = terms.profile;
        EmployeeMood report;
        if(const auto found=gathering_points.find(person.id);found!=gathering_points.end())report.gatherings=found->second;
        report.pay = pay_points(employee.opening && !rules.dismissal ? inherited_salary : employee_monthly_salary(state,employee),
            terms.recruitment ? terms.recruitment->ask_salary : rules.monthly_salary, mood.pay);
        report.company_prestige = prestige_points(company_reputation, mood.company_prestige);
        report.ceo_prestige = prestige_points(ceo_reputation, mood.ceo_prestige);
        const auto role_weight = static_cast<int>(mood.role);
        report.role = person.preferred_mission == people.mission ? role_weight / 2 : -role_weight / 2;
        if (manager && manager->person == person.id) {
            report.role += person.role == "Software engineer" ? role_weight / 2 : -role_weight / 2;
        } else if (researching.contains(person.id)) {
            report.role += person.role == "Software engineer" ? -role_weight / 2 : role_weight / 2;
        }

        std::uint64_t shared = 0;
        for (const auto& trait : worker.traits) shared += frequency.at(trait) - 1;
        report.coworkers = static_cast<int>(shared_trait_compatibility(shared, workers.size() - 1) * mood.coworkers / 10000);
        for (const auto& trait : person.traits) {
            constexpr std::string_view prefix = "Temperament: ";
            if (!trait.starts_with(prefix)) continue;
            report.temperament = trait.substr(prefix.size());
            if (const auto effect = mood.temperaments.find(report.temperament); effect != mood.temperaments.end())
                report.temperament_effect = effect->second;
            break;
        }
        if (assigned_hq_desk(state, person.id)) {
            report.amenities = amenity_points;
            report.accessible_amenities = amenity_names;
        }
        report.score = std::clamp(50 + report.pay + report.company_prestige + report.ceo_prestige +
            report.role + report.coworkers + report.amenities + report.temperament_effect + report.gatherings, 0, 100);
        results.emplace(person.id, std::move(report));
    }
    return results;
}
} // namespace pgp::core
