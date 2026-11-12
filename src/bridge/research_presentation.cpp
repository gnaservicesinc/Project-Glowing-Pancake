#include "management_controller.hpp"
#include "lab_inputs.hpp"
#include <pgp/core/workplace_research.hpp>
#include <pgp/core/funding_support.hpp>
#include <QLocale>
#include <algorithm>
#include <functional>
#include <map>

namespace pgp::bridge {
namespace {
QString q(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString cu(std::int64_t value) {
    return QLocale::c()
        .toString(static_cast<double>(value) / 1000, 'f', 3)
        .remove(QRegularExpression{QStringLiteral("\\.?0+$")});
}
QString money(core::Cents value) {
    return QLocale{QLocale::English, QLocale::UnitedStates}.toCurrencyString(static_cast<double>(value.value()) / 100);
}
template <class Rows> const auto *find_id(const Rows &rows, std::string_view id) {
    const auto found = std::ranges::find_if(rows, [&](const auto &row) { return row.id == id; });
    return found == rows.end() ? nullptr : &*found;
}
const core::ResearchRun *latest_run(const core::LabState &lab, std::string_view id) {
    const auto found = std::find_if(lab.research.rbegin(), lab.research.rend(),
                                    [&](const auto &run) { return run.research_id == id; });
    return found == lab.research.rend() ? nullptr : &*found;
}
bool has_result(const core::LabState &lab, std::string_view id) {
    return std::ranges::any_of(lab.research, [&](const auto &row) { return row.research_id == id && row.completed; });
}
unsigned innovation_level(const core::LabCatalog& catalog,const core::LabResearch& topic) {
    unsigned level=1;
    for(auto* previous=&topic;previous && previous->innovation;
        previous=find_id(catalog.research,previous->innovation->previous)) ++level;
    return level;
}
std::uint32_t results(const core::LabState &lab, std::string_view id) {
    return static_cast<std::uint32_t>(
        std::ranges::count_if(lab.research, [&](const auto &row) { return row.research_id == id && row.completed; }));
}
core::LabAllocation allocation(QString source, QString rate, const QStringList &workers) {
    if (workers.isEmpty() || workers.size() > 32)
        throw std::invalid_argument("Choose one or more researchers.");
    auto plan = lab_inputs::allocation(source, rate, QString::number(workers.size()));
    for (const auto &worker : workers)
        plan.researchers.push_back(worker.toStdString());
    return plan;
}
std::int64_t free_compute(const core::CapacityState &capacity, const core::CapacitySource &source) {
    auto free = core::company_capacity(source).value();
    for (const auto &project : capacity.projects) {
        if (project.source_id != source.id || project.status != core::ProjectStatus::active)
            continue;
        const auto rate =
            project.kind == core::ProjectKind::serving || project.kind == core::ProjectKind::replication
                ? project.requested.value()
                : std::min(project.requested.value(), project.total_work.value() - project.completed_work.value());
        free = core::checked_subtract(free, rate);
    }
    return std::max<std::int64_t>(0, free);
}
QString source_name(const core::SimulationState &state, const core::CapacitySource &source) {
    constexpr std::string_view leased = "lease/";
    constexpr std::string_view equipment = "hq.equipment/";
    if (source.id.starts_with(leased) && state.business_offers) {
        if (const auto *offer =
                find_id(state.business_offers->catalog, std::string_view{source.id}.substr(leased.size())))
            return q(offer->name);
    }
    if (source.id.starts_with(equipment)) {
        const auto &items = state.infrastructure->workplace->headquarters->items;
        const auto id = std::string_view{source.id}.substr(equipment.size());
        const auto it = std::ranges::find_if(items, [&](const auto &item) { return std::to_string(item.id) == id; });
        if (it != items.end())
            if (const auto *offer =
                    find_id(state.infrastructure->catalog.workplace->headquarters->equipment, it->offer))
                return QObject::tr("%1 · HQ unit %2").arg(q(offer->name)).arg(it->id);
    }
    if (state.infrastructure) {
        const auto hardware = std::ranges::find_if(state.infrastructure->hardware,
                                                   [&](const auto &item) { return item.source_id == source.id; });
        if (hardware != state.infrastructure->hardware.end()) {
            const auto *location = find_id(state.infrastructure->sites, hardware->site_id);
            if (location)
                return QObject::tr("%1 · %2").arg(q(location->name), q(hardware->id));
        }
    }
    return q(source.id);
}
} // namespace

bool ManagementController::groundedResearch() const { return snapshot_ && core::workplace_research_enabled(state()); }
QVariantList ManagementController::researchWorkers() const {
    QVariantList rows;
    if (groundedResearch())
        for (const auto &person : core::research_workers(state()))
            rows.push_back(QVariantMap{{"id", q(person.id)},
                                       {"name", q(person.name)},
                                       {"skill", person.skill},
                                       {"blocker", q(person.unavailable)}});
    return rows;
}
QVariantList ManagementController::researchSources() const {
    QVariantList rows;
    if (!groundedResearch())
        return rows;
    for (const auto &source : state().capacity->sources) {
        const auto free = free_compute(*state().capacity, source);
        rows.push_back(QVariantMap{{"id", q(source.id)},
                                   {"name", tr("%1 · %2 CU/day free").arg(source_name(state(), source), cu(free))},
                                   {"free", cu(free)},
                                   {"canAllocate", free > 0}});
    }
    return rows;
}
QVariantList ManagementController::researchRentals() const {
    QVariantList rows;
    if (!groundedResearch() || !state().business_offers)
        return rows;
    const auto &s = state();
    const auto protected_cash = core::checked_unit_subtract(s.cash, core::discretionary_cash(s));
    for (const auto &offer : s.business_offers->catalog) {
        if (offer.kind != core::BusinessOfferKind::compute_lease || offer.available_from > s.date ||
            offer.available_before <= s.date)
            continue;
        const auto &accepted = s.business_offers->accepted;
        if (std::ranges::any_of(accepted, [&](const auto &row) { return row.offer_id == offer.id; }))
            continue;
        rows.push_back(QVariantMap{
            {"id", q(offer.id)},
            {"name", q(offer.name)},
            {"terms", tr("%1 CU/day · %2 setup · %3/month for %4 days. Electricity included.")
                          .arg(cu(offer.capacity.value()), money(offer.setup_cost), money(offer.monthly_amount))
                          .arg(offer.term.value())},
            {"eligible", core::business_offer_eligible(*s.business_offers, offer, *s.accounts, *s.capacity, s.market,
                                                       s.date, protected_cash)}});
    }
    return rows;
}

QVariantList ManagementController::researchGraph() const {
    QVariantList nodes;
    if (!groundedResearch())
        return nodes;
    const auto &s = state();
    const auto &lab = *s.lab;
    const auto title = [&](const std::string &id) {
        return research_->sourceRow(q(id)).value("title", q(id)).toString();
    };
    std::map<std::string, int> depths;
    std::map<int, int> next_slot;
    const std::function<int(const core::LabResearch &)> rank = [&](const auto &node) {
        if (const auto found = depths.find(node.id); found != depths.end())
            return found->second;
        int depth = 0;
        for (const auto *dependencies : {&node.all_of, &node.any_of})
            for (const auto &id : *dependencies)
                if (const auto *required = find_id(lab.catalog.research, id))
                    depth = std::max(depth, 1 + rank(*required));
        return depths.emplace(node.id, depth).first->second;
    };
    for (const auto &topic : lab.catalog.research) {
        const auto level = rank(topic);
        const auto slot = next_slot[level]++;
        const auto *run = latest_run(lab, topic.id);
        const auto *work = run ? find_id(s.capacity->projects, run->project_id) : nullptr;
        const auto pacing=core::research_pacing(lab,*s.capacity,topic.id,s.date);
        const bool missing=pacing.missing_groups!=0;
        const bool locked =
            pacing.threshold==UINT32_MAX ||
            !core::frontier_technology_block(s, core::FrontierTechnology::research, topic.id, s.date).empty();
        QString status = locked ? QStringLiteral("locked") : missing ? QStringLiteral("creative") : QStringLiteral("ready");
        if (results(lab, topic.id) >= topic.repeat_limit)
            status = "complete";
        if (work && work->status == core::ProjectStatus::active)
            status = "active";
        if (work && work->status == core::ProjectStatus::paused)
            status = "paused";
        QStringList all, any;
        for (const auto &id : topic.all_of)
            all.push_back(q(id));
        for (const auto &id : topic.any_of)
            any.push_back(q(id));
        nodes.push_back(QVariantMap{{"id", q(topic.id)},
                                    {"name", title(topic.id)},
                                    {"status", status},
                                    {"level", level},
                                    {"slot", slot},
                                    {"all", all},
                                    {"any", any}});
    }
    return nodes;
}

QVariantMap ManagementController::researchTopic(QString id) const {
    if (!groundedResearch())
        return {};
    const auto &s = state();
    const auto *topic = find_id(s.lab->catalog.research, id.toStdString());
    if (!topic)
        return {};
    const auto authored = research_->sourceRow(id);
    const auto required = core::research_equipment(s, topic->id);
    QStringList equipment;
    const auto &models = s.infrastructure->catalog.workplace->headquarters->equipment;
    for (const auto &name : required)
        if (const auto *offer = find_id(models, name))
            equipment.push_back(q(offer->name));
    const auto *run = latest_run(*s.lab, topic->id);
    const auto *work = run ? find_id(s.capacity->projects, run->project_id) : nullptr;
    const auto technology = core::frontier_technology_block(s, core::FrontierTechnology::research, topic->id, s.date);
    const bool active = work && work->status == core::ProjectStatus::active;
    const bool paused = work && work->status == core::ProjectStatus::paused;
    const bool completed = results(*s.lab, topic->id) >= topic->repeat_limit;
    QString why;
    if (!technology.empty())
        why = q(technology);
    else if (core::research_creativity_threshold(*s.lab,*s.capacity,topic->id,s.date)==UINT32_MAX) {
        QStringList missing;
        const auto title=[&](const auto& prerequisite){return research_->sourceRow(q(prerequisite)).value("title",q(prerequisite)).toString();};
        for(const auto& prerequisite:topic->all_of)
            if(!has_result(*s.lab,prerequisite)) missing.push_back(title(prerequisite));
        if(!topic->any_of.empty() && std::ranges::none_of(topic->any_of,[&](const auto& prerequisite){return has_result(*s.lab,prerequisite);})) {
            QStringList alternatives;
            for(const auto& prerequisite:topic->any_of) alternatives.push_back(title(prerequisite));
            missing.push_back(tr("one of %1").arg(alternatives.join(", ")));
        }
        why=tr("Requires %1.").arg(missing.join(" · "));
    }
    else if (!core::hq_has_lab_equipment(s, required))
        why = tr("Install the required equipment together in one room.");
    QString progress, hold;
    if (work) {
        progress = tr("%1 of %2 staff-days · %3 of %4 CU settled")
                       .arg(work->completed_staff_days)
                       .arg(work->required_staff_days)
                       .arg(cu(work->completed_work.value()), cu(work->total_work.value()));
        QStringList team;
        for (const auto &worker : run->researchers) {
            const auto &roster = s.management->people->employees;
            const auto person =
                std::ranges::find_if(roster, [&](const auto &employee) { return employee.terms.profile.id == worker; });
            if (person != roster.end())
                team.push_back(q(person->terms.profile.name));
        }
        if (const auto *source = find_id(s.capacity->sources, work->source_id))
            progress += tr("\n%1 · %2 CU/day reserved from %3")
                            .arg(team.join(", "), cu(work->requested.value()), source_name(s, *source));
        if (paused)
            hold = q(core::research_workplace_blocker(
                s, topic->id, {work->source_id, work->requested, work->staff, run->researchers}, work->id));
        if (run->failed && topic->setback)
            progress += "\n" + q(topic->setback->finding);
    }
    const auto next=std::ranges::find_if(s.lab->catalog.research,[&](const auto& row){return row.innovation && row.innovation->previous==topic->id;});
    QStringList effects;
    if(topic->innovation) for(const auto& [recipe,amount]:topic->innovation->inference_savings) {
        const auto* terms=find_id(s.lab->catalog.recipes,recipe);
        if(terms) effects.push_back(tr("%1 · %2% less base compute for future releases before rounding")
            .arg(q(terms->name)).arg(static_cast<double>(amount.value())/100.0));
    }
    const auto fee=core::research_cash_fee(s,topic->id);
    return {{"name", authored.value("title", id)},
            {"description", authored.value("detail")},
            {"published", authored.value("fields").toMap().value("published")},
            {"canAdopt", authored.value("fields").toMap().value("canAdopt")},
            {"knowledge", has_result(*s.lab, topic->id)},
            {"staffDays", topic->staff_days.value()},
            {"experiment", cu(topic->experiment_work.value())},
            {"innovation", topic->innovation.has_value()},
            {"innovationLevel", innovation_level(s.lab->catalog,*topic)},
            {"nextInnovation",next==s.lab->catalog.research.end()?QString{}:q(next->id)},
            {"effects",effects.join(u'\n')},
            {"fee",fee.value()>0?money(fee):QString{}},
            {"equipment", equipment},
            {"blocker", why},
            {"active", active},
            {"paused", paused},
            {"completed", completed},
            {"progress", progress},
            {"hold", hold},
            {"project", work ? q(work->id) : QString{}}};
}

QVariantMap ManagementController::researchTeam(QString id,QStringList workers) const {
    if(!groundedResearch()) return {};
    const auto& s=state();const auto* topic=find_id(s.lab->catalog.research,id.toStdString());
    if(!topic || !s.lab->catalog.creative_research.contains(topic->id)) return {};
    std::vector<std::string> ids;for(const auto& worker:workers) ids.push_back(worker.toStdString());
    try {
        const auto team=core::research_team(s,ids);
        const auto pacing=core::research_pacing(*s.lab,*s.capacity,topic->id,s.date);
        const auto& rule=s.lab->catalog.creative_research.at(topic->id);
        const auto staff_days=core::coordinated_staff_days(s.lab->catalog,*topic,team.compatibility_bp);
        QStringList traits;
        for(const auto& worker:s.management->people->employees)if(std::ranges::find(ids,worker.terms.profile.id)!=ids.end())
            for(const auto& trait:core::visible_traits(s,worker.terms.profile))if(!traits.contains(q(trait)))traits.push_back(q(trait));
        const auto calculation=pacing.missing_groups
            ? tr("Expected year %1 (year %2 of the campaign). Difficulty %3 · %4 of %5 prerequisite groups missing · ×%6 early start multiplier.")
                .arg(rule.earliest_year).arg(pacing.expected_offset).arg(rule.difficulty).arg(pacing.missing_groups).arg(pacing.prerequisite_groups).arg(pacing.early_multiplier)
            : tr("Expected year %1. Your team can follow the existing research path without a creativity threshold.").arg(rule.earliest_year);
        return {{"creativity",team.creativity},{"required",pacing.threshold},{"compatibility",static_cast<double>(team.compatibility_bp)/100},
            {"days",staff_days},
            {"explanation",tr("Creativity %1 / %2 needed. Each distinct trait counts once.\n%3\nShared-trait compatibility %4% · %5 staff-days after coordination.")
                .arg(team.creativity).arg(pacing.threshold).arg(calculation).arg(static_cast<double>(team.compatibility_bp)/100,0,'f',1).arg(staff_days)},
            {"traits",traits.join(" · ")}};
    } catch(const std::exception&) {return {};}
}

QString ManagementController::researchBlocker(QString id, QString source, QString rate, QStringList workers) const {
    const auto detail = researchTopic(id);
    if (detail.isEmpty())
        return tr("Choose a technology.");
    if (detail.value("active").toBool() || detail.value("paused").toBool() || detail.value("completed").toBool())
        return tr("This technology already has a result or unfinished work.");
    if (const auto why = detail.value("blocker").toString(); !why.isEmpty())
        return why;
    if (workers.isEmpty())
        return tr("Choose the researchers who will do this work.");
    if (source.isEmpty())
        return tr("Place and power a computer or rack, or sign a server rental to supply compute.");
    try {
        const auto plan = allocation(source, rate, workers);
        const auto &s = state();
        if(core::research_cash_fee(s,id.toStdString())>core::discretionary_cash(s))
            return tr("Available company cash does not cover the project fee.");
        const auto blocker = core::research_workplace_blocker(s, id.toStdString(), plan);
        if (!blocker.empty())
            return q(blocker);
        const auto *supply = find_id(s.capacity->sources, plan.source_id);
        if (!supply || free_compute(*s.capacity, *supply) < plan.daily_compute.value())
            return tr("The source has insufficient unreserved CU/day.");
        const auto capacity = core::capacity_report(*s.capacity);
        if (capacity.available_staff - capacity.assigned_staff < plan.staff)
            return tr("Other company work already reserves the available staff time.");
    } catch (const std::exception &error) {
        return QString::fromUtf8(error.what());
    }
    return {};
}

void ManagementController::beginGroundedResearch(QString id, QString source, QString rate, QStringList workers) {
    if (!editable())
        return;
    const auto blocker = researchBlocker(id, source, rate, workers);
    if (!blocker.isEmpty()) {
        emit noticeRaised(blocker);
        return;
    }
    emit actionRequested(core::StartResearch{id.toStdString(), allocation(source, rate, workers)});
}
} // namespace pgp::bridge
