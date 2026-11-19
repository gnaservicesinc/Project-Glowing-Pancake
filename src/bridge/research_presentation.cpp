#include "management_controller.hpp"
#include <pgp/core/research_data.hpp>
#include <ranges>
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
bool has_access(const core::LabState &lab, const core::CapacityState &capacity, std::string_view id, core::OrdinalDay today) {
    return core::has_research_access(lab, capacity, id, today);
}
QVariantList eligible_research_datasets(const core::SimulationState &s, const core::ResearchDataRequirement &req) {
    QVariantList result;
    if (!s.lab) return result;
    const auto report = core::lab_report(*s.lab, *s.capacity);
    for (const auto &offer : report.data_offers) {
        const auto available = core::data_available_on(*s.lab, *s.capacity, offer.id);
        if (!available || *available > s.date || !core::research_data_fits(req, offer)) continue;
        result.push_back(QVariantMap{
            {"id", q(offer.id)},
            {"name", q(offer.name)},
            {"uniqueUnits", static_cast<qulonglong>(offer.unique_units)},
            {"qualityPct", static_cast<double>(offer.quality.value()) / 100.0}
        });
    }
    return result;
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
QString topic_domain_id(std::string_view id) {
    if (id.find(".data.") != std::string_view::npos || id.find("curation") != std::string_view::npos || id.find("dataset") != std::string_view::npos || id.find("synthetic") != std::string_view::npos || id.find("provenance") != std::string_view::npos || id.find("annotation") != std::string_view::npos || id.find("dedup") != std::string_view::npos)
        return QStringLiteral("data");
    if (id.find(".math.") != std::string_view::npos || id.find("efficiency") != std::string_view::npos || id.find("sample") != std::string_view::npos || id.find("distill") != std::string_view::npos || id.find("quantiz") != std::string_view::npos || id.find("sparse") != std::string_view::npos || id.find("prun") != std::string_view::npos || id.find("compact") != std::string_view::npos || id.find("small_model") != std::string_view::npos)
        return QStringLiteral("math");
    if (id.find(".compute.") != std::string_view::npos || id.find(".hardware.") != std::string_view::npos || id.find("serving") != std::string_view::npos || id.find("kernel") != std::string_view::npos || id.find("cluster") != std::string_view::npos || id.find("compiler") != std::string_view::npos || id.find("memory") != std::string_view::npos || id.find("cool") != std::string_view::npos || id.find("silicon") != std::string_view::npos)
        return QStringLiteral("compute");
    if (id.find(".robotics.") != std::string_view::npos || id.find("robot") != std::string_view::npos || id.find("embod") != std::string_view::npos || id.find("actuator") != std::string_view::npos || id.find("control_loop") != std::string_view::npos || id.find("manipulation") != std::string_view::npos)
        return QStringLiteral("robotics");
    if (id.find(".bio.") != std::string_view::npos || id.find(".science.") != std::string_view::npos || id.find("protein") != std::string_view::npos || id.find("molecular") != std::string_view::npos || id.find("materials") != std::string_view::npos || id.find("climate") != std::string_view::npos || id.find("genom") != std::string_view::npos)
        return QStringLiteral("science");
    if (id.find(".safety.") != std::string_view::npos || id.find("eval") != std::string_view::npos || id.find("align") != std::string_view::npos || id.find("interpret") != std::string_view::npos || id.find("audit") != std::string_view::npos || id.find("red_team") != std::string_view::npos || id.find("oversight") != std::string_view::npos || id.find("guardrail") != std::string_view::npos || id.find("verify") != std::string_view::npos || id.find("robust") != std::string_view::npos || id.find("reproducible") != std::string_view::npos)
        return QStringLiteral("safety");
    if (id.find(".product.") != std::string_view::npos || id.find(".business.") != std::string_view::npos || id.find("workflow") != std::string_view::npos || id.find("enterprise") != std::string_view::npos || id.find("edge") != std::string_view::npos || id.find("agent_ux") != std::string_view::npos || id.find("privacy") != std::string_view::npos || id.find("compliance") != std::string_view::npos)
        return QStringLiteral("product");
    return QStringLiteral("ml");
}
QString topic_domain_label(const QString& domain) {
    if (domain == "data") return QObject::tr("Data & Curation");
    if (domain == "math") return QObject::tr("Math & Efficiency");
    if (domain == "compute") return QObject::tr("Compute & Systems");
    if (domain == "robotics") return QObject::tr("Robotics & Control");
    if (domain == "science") return QObject::tr("Biology & Science");
    if (domain == "safety") return QObject::tr("Safety & Verification");
    if (domain == "product") return QObject::tr("Product & Operations");
    return QObject::tr("ML & Architectures");
}
} // namespace

bool ManagementController::groundedResearch() const { return snapshot_ && core::workplace_research_enabled(state()); }
QVariantList ManagementController::researchWorkers() const {
    QVariantList rows;
    if (groundedResearch()) {
        const auto &s = state();
        for (const auto &person : core::research_workers(s)) {
            QString role = tr("Researcher");
            if (s.management && s.management->people) {
                const auto it = std::ranges::find_if(s.management->people->employees,
                    [&](const auto &e) { return e.terms.profile.id == person.id; });
                if (it != s.management->people->employees.end()) role = q(it->terms.profile.role);
            }
            const bool needs_ws = person.unavailable.find("computer") != std::string::npos ||
                                  person.unavailable.find("desk") != std::string::npos ||
                                  person.unavailable.find("laptop") != std::string::npos;
            rows.push_back(QVariantMap{{"id", q(person.id)},
                                       {"name", q(person.name)},
                                       {"role", role},
                                       {"skill", person.skill},
                                       {"available", person.unavailable.empty()},
                                       {"needsWorkstation", needs_ws},
                                       {"blocker", q(person.unavailable)}});
        }
    }
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
        for (const auto &group : node.any_of_groups)
            for (const auto &id : group)
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
        const bool licensed_active = has_access(lab, *s.capacity, topic.id, s.date) && results(lab, topic.id) == 0;
        if (results(lab, topic.id) >= topic.repeat_limit || licensed_active)
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
        QVariantList any_groups;
        if (!any.isEmpty()) any_groups.push_back(any);
        for (const auto &group : topic.any_of_groups) {
            QStringList g;
            for (const auto &id : group) g.push_back(q(id));
            if (!g.isEmpty()) any_groups.push_back(g);
        }
        const auto domain = topic_domain_id(topic.id);
        const auto detail = research_->sourceRow(q(topic.id)).value("detail").toString();
        const auto first_sentence = detail.section(u'.', 0, 0).trimmed();
        QString active_progress;
        if (work && (work->status == core::ProjectStatus::active || work->status == core::ProjectStatus::paused)) {
            active_progress = tr("%1/%2 staff-days · %3/%4 CU")
                .arg(work->completed_staff_days).arg(work->required_staff_days)
                .arg(cu(work->completed_work.value()), cu(work->total_work.value()));
        }
        nodes.push_back(QVariantMap{{"id", q(topic.id)},
                                    {"name", title(topic.id)},
                                    {"domain", domain},
                                    {"domainLabel", topic_domain_label(domain)},
                                    {"summary", first_sentence.isEmpty() ? detail : (first_sentence + u'.')},
                                    {"status", status},
                                    {"licensed", licensed_active},
                                    {"producesPoc", topic.poc_spec.produces_poc},
                                    {"requiresData", topic.data_requirement.required},
                                    {"level", level},
                                    {"slot", slot},
                                    {"staffDays", topic.staff_days.value()},
                                    {"experiment", cu(topic.experiment_work.value())},
                                    {"activeProgress", active_progress},
                                    {"all", all},
                                    {"any", any},
                                    {"anyGroups", any_groups}});
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
    const bool licensed_active = has_access(*s.lab, *s.capacity, topic->id, s.date) && !completed;
    QString why;
    if (!technology.empty())
        why = q(technology);
    else if (core::research_creativity_threshold(*s.lab,*s.capacity,topic->id,s.date)==UINT32_MAX) {
        QStringList missing;
        const auto title=[&](const auto& prerequisite){return research_->sourceRow(q(prerequisite)).value("title",q(prerequisite)).toString();};
        for(const auto& prerequisite:topic->all_of)
            if(!has_access(*s.lab,*s.capacity,prerequisite,s.date)) missing.push_back(title(prerequisite));
        if(!topic->any_of.empty() && std::ranges::none_of(topic->any_of,[&](const auto& prerequisite){return has_access(*s.lab,*s.capacity,prerequisite,s.date);})) {
            QStringList alternatives;
            for(const auto& prerequisite:topic->any_of) alternatives.push_back(title(prerequisite));
            missing.push_back(tr("one of (%1)").arg(alternatives.join(" OR ")));
        }
        for(const auto& group:topic->any_of_groups) {
            if(!group.empty() && std::ranges::none_of(group,[&](const auto& prerequisite){return has_access(*s.lab,*s.capacity,prerequisite,s.date);})) {
                QStringList alternatives;
                for(const auto& prerequisite:group) alternatives.push_back(title(prerequisite));
                missing.push_back(tr("one of (%1)").arg(alternatives.join(" OR ")));
            }
        }
        why=tr("Requires %1.").arg(missing.join(" AND "));
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
                s, topic->id, {work->source_id, work->requested, work->staff, run->researchers, run->data_ids}, work->id));
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
    const auto &te = topic->technical_effect;
    if (te.data_efficiency_bp > 0)
        effects.push_back(tr("Data efficiency · +%1% sample effectiveness during model training").arg(static_cast<double>(te.data_efficiency_bp)/100.0, 0, 'f', 1));
    if (te.compute_efficiency_bp > 0)
        effects.push_back(tr("Compute efficiency · +%1% compute scaling effectiveness during training").arg(static_cast<double>(te.compute_efficiency_bp)/100.0, 0, 'f', 1));
    if (te.latency_bonus_bp > 0)
        effects.push_back(tr("Serving speed & responsiveness · +%1% product latency rating").arg(static_cast<double>(te.latency_bonus_bp)/100.0, 0, 'f', 1));
    if (te.robustness_bonus_bp > 0)
        effects.push_back(tr("Operational reliability · +%1% product robustness & verification fit").arg(static_cast<double>(te.robustness_bonus_bp)/100.0, 0, 'f', 1));
    if (!te.modality_unlock.empty())
        effects.push_back(tr("Unlocks technical modality · %1").arg(q(te.modality_unlock)));
    if (!te.deployment_setting_unlock.empty())
        effects.push_back(tr("Unlocks deployment setting · %1").arg(q(te.deployment_setting_unlock)));
    if (topic->poc_spec.produces_poc)
        effects.push_back(tr("Proof-of-concept prototype · produces %1 (%2) upon completion").arg(q(topic->poc_spec.method_label), q(topic->poc_spec.task_scope)));

    QStringList opportunities;
    QVariantList examples;
    const auto dependent=[&](const auto& rows,const QString& action,const QString& kind,const QString& app,const QString& sec) {
        for(const auto& row:rows) {
            if(std::ranges::find(row.requires_research,topic->id)==row.requires_research.end()) continue;
            QStringList remaining;
            for(const auto& other:row.requires_research)
                if(other!=topic->id && !has_access(*s.lab,*s.capacity,other,s.date))
                    remaining.push_back(research_->sourceRow(q(other)).value("title",q(other)).toString());
            auto line=action.arg(q(row.name));
            if(!remaining.empty()) line+=tr(" · also requires %1").arg(remaining.join(", "));
            opportunities.push_back(line);
            examples.push_back(QVariantMap{
                {"kind", kind},
                {"label", line},
                {"targetId", q(row.id)},
                {"actionApp", app},
                {"actionSection", sec},
                {"extraRequirement", remaining.join(", ")}
            });
        }
    };
    dependent(s.lab->catalog.recipes,tr("Train · %1"),QStringLiteral("model"),QStringLiteral("lab"),QStringLiteral("train"));
    dependent(s.lab->catalog.evaluations,tr("Evaluate · %1"),QStringLiteral("evaluation"),QStringLiteral("lab"),QStringLiteral("evaluate"));
    dependent(s.lab->catalog.synthesis,tr("Generate data · %1"),QStringLiteral("dataset"),QStringLiteral("lab"),QStringLiteral("data"));
    for(const auto& follow:s.lab->catalog.research) {
        bool is_req=std::ranges::find(follow.all_of,topic->id)!=follow.all_of.end() ||
                    std::ranges::find(follow.any_of,topic->id)!=follow.any_of.end();
        for (const auto& group : follow.any_of_groups)
            if (std::ranges::find(group, topic->id) != group.end()) is_req = true;
        if(!is_req) continue;
        const auto fname=research_->sourceRow(q(follow.id)).value("title",q(follow.id)).toString();
        examples.push_back(QVariantMap{
            {"kind", QStringLiteral("research")},
            {"label", tr("Unlocks research · %1").arg(fname)},
            {"targetId", q(follow.id)},
            {"actionApp", QStringLiteral("research")},
            {"actionSection", QString{}},
            {"extraRequirement", QString{}}
        });
    }
    if (topic->poc_spec.produces_poc) {
        examples.push_back(QVariantMap{
            {"kind", QStringLiteral("poc")},
            {"label", tr("Creates Proof-of-Concept model · %1 (%2)").arg(q(topic->poc_spec.method_label), q(topic->poc_spec.task_scope))},
            {"targetId", q("poc." + topic->id)},
            {"actionApp", QStringLiteral("models")},
            {"actionSection", QString{}},
            {"extraRequirement", tr("Continue into production training before customer release")}
        });
    }
    QVariantList checklist;
    const auto req_title=[&](const std::string& pid){return research_->sourceRow(q(pid)).value("title",q(pid)).toString();};
    for(const auto& pid:topic->all_of) {
        const bool met=has_access(*s.lab,*s.capacity,pid,s.date);
        checklist.push_back(QVariantMap{{"label",tr("Required research: %1").arg(req_title(pid))},{"met",met},{"kind",QStringLiteral("research")},{"targetId",q(pid)}});
    }
    if(!topic->any_of.empty()) {
        const bool met=std::ranges::any_of(topic->any_of,[&](const auto& pid){return has_access(*s.lab,*s.capacity,pid,s.date);});
        QStringList names;
        for(const auto& pid:topic->any_of) names.push_back(req_title(pid));
        checklist.push_back(QVariantMap{{"label",tr("Any one of: %1").arg(names.join(" OR "))},{"met",met},{"kind",QStringLiteral("research")},{"targetId",q(topic->any_of.front())}});
    }
    for(const auto& group:topic->any_of_groups) {
        if (group.empty()) continue;
        const bool met=std::ranges::any_of(group,[&](const auto& pid){return has_access(*s.lab,*s.capacity,pid,s.date);});
        QStringList names;
        for(const auto& pid:group) names.push_back(req_title(pid));
        checklist.push_back(QVariantMap{{"label",tr("Any one of: %1").arg(names.join(" OR "))},{"met",met},{"kind",QStringLiteral("research")},{"targetId",q(group.front())}});
    }
    const auto eligible_data = topic->data_requirement.required ? eligible_research_datasets(s, topic->data_requirement) : QVariantList{};
    if (topic->data_requirement.required) {
        checklist.push_back(QVariantMap{
            {"label", tr("Qualifying dataset (≥ %1 unique records, ≥ %2% quality)")
                          .arg(topic->data_requirement.minimum_unique_units)
                          .arg(static_cast<double>(topic->data_requirement.minimum_quality_bp) / 100.0, 0, 'f', 0)},
            {"met", !eligible_data.isEmpty()},
            {"kind", QStringLiteral("data")},
            {"targetId", QString{}}
        });
    }
    if(!equipment.isEmpty()) {
        checklist.push_back(QVariantMap{{"label",tr("Equipped HQ room with: %1").arg(equipment.join(", "))},{"met",core::hq_has_lab_equipment(s,required)},{"kind",QStringLiteral("equipment")},{"targetId",QString{}}});
    }
    const bool has_free_compute=std::ranges::any_of(s.capacity->sources,[&](const auto& src){return free_compute(*s.capacity,src)>0;});
    checklist.push_back(QVariantMap{{"label",tr("Available compute source (%1 CU needed)").arg(cu(topic->experiment_work.value()))},{"met",has_free_compute},{"kind",QStringLiteral("compute")},{"targetId",QString{}}});
    const auto fee=core::research_cash_fee(s,topic->id);
    if(fee.value()>0) {
        checklist.push_back(QVariantMap{{"label",tr("Project fee %1").arg(money(fee))},{"met",fee<=core::discretionary_cash(s)},{"kind",QStringLiteral("cash")},{"targetId",QString{}}});
    }
    QVariantMap data_req_map{
        {"required", topic->data_requirement.required},
        {"minimumUniqueUnits", static_cast<qulonglong>(topic->data_requirement.minimum_unique_units)},
        {"minimumQualityPct", static_cast<double>(topic->data_requirement.minimum_quality_bp) / 100.0},
        {"domainHint", q(topic->data_requirement.domain_hint)},
        {"eligibleDatasets", eligible_data}
    };
    QVariantMap poc_map{
        {"producesPoc", topic->poc_spec.produces_poc},
        {"recipeId", q(topic->poc_spec.recipe_id)},
        {"taskScope", q(topic->poc_spec.task_scope)},
        {"methodLabel", q(topic->poc_spec.method_label)}
    };
    QVariantMap tech_effect_map{
        {"dataEfficiencyBp", topic->technical_effect.data_efficiency_bp},
        {"computeEfficiencyBp", topic->technical_effect.compute_efficiency_bp},
        {"latencyBonusBp", topic->technical_effect.latency_bonus_bp},
        {"robustnessBonusBp", topic->technical_effect.robustness_bonus_bp},
        {"modalityUnlock", q(topic->technical_effect.modality_unlock)},
        {"deploymentSettingUnlock", q(topic->technical_effect.deployment_setting_unlock)},
        {"refinementFamily", q(topic->technical_effect.refinement_family)},
        {"diminishingTier", topic->technical_effect.diminishing_tier}
    };
    QVariantMap license_map{{"available", false}};
    for (const auto &offer : s.lab->catalog.tech_licenses) {
        if (offer.research_id != topic->id) continue;
        const auto active_it = std::ranges::find_if(s.lab->tech_licenses | std::views::reverse, [&](const auto &row) { return row.offer_id == offer.id; });
        const auto *active_lic = active_it == s.lab->tech_licenses.rend() ? nullptr : &*active_it;
        const bool is_active = active_lic && active_lic->active_on <= s.date && s.date < active_lic->expires_on;
        const bool is_integrating = active_lic && s.date < active_lic->active_on;
        const bool is_expired = active_lic && s.date >= active_lic->expires_on;
        license_map = QVariantMap{
            {"available", true},
            {"offerId", q(offer.id)},
            {"licensor", q(offer.licensor)},
            {"upfrontFee", money(offer.upfront_fee)},
            {"monthlyRoyalty", money(offer.monthly_royalty)},
            {"termDays", offer.term_days.value()},
            {"integrationDays", offer.integration_days.value()},
            {"scopeLabel", q(offer.scope_label)},
            {"active", is_active},
            {"integrating", is_integrating},
            {"expired", is_expired},
            {"readyOn", active_lic ? q(core::iso_date(active_lic->active_on)) : QString{}},
            {"expiresBefore", active_lic ? q(core::iso_date(active_lic->expires_on)) : QString{}},
            {"canLicense", (!active_lic || is_expired) && !completed && offer.upfront_fee <= core::discretionary_cash(s)},
            {"canRenew", is_active && offer.renewable && core::Cents{offer.upfront_fee.value()/2} <= core::discretionary_cash(s)}
        };
        break;
    }
    const auto domain=topic_domain_id(topic->id);
    const auto* description = program_->collection.message(topic->id + ".detail");
    return {{"id", q(topic->id)},
            {"name", authored.value("title", id)},
            {"domain", domain},
            {"domainLabel", topic_domain_label(domain)},
            {"description", description ? q(*description) : authored.value("detail")},
            {"published", authored.value("fields").toMap().value("published")},
            {"canAdopt", authored.value("fields").toMap().value("canAdopt")},
            {"knowledge", has_result(*s.lab, topic->id)},
            {"licensed", licensed_active},
            {"staffDays", topic->staff_days.value()},
            {"experiment", cu(topic->experiment_work.value())},
            {"innovation", topic->innovation.has_value()},
            {"innovationLevel", innovation_level(s.lab->catalog,*topic)},
            {"nextInnovation",next==s.lab->catalog.research.end()?QString{}:q(next->id)},
            {"effects",effects.join(u'\n')},
            {"opportunities",opportunities},
            {"examples",examples},
            {"missingChecklist",checklist},
            {"dataRequirement", data_req_map},
            {"pocSpec", poc_map},
            {"technicalEffect", tech_effect_map},
            {"licenseOffer", license_map},
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
        const auto creativity=pacing.threshold==UINT32_MAX
            ? tr("This topic requires its prerequisite discoveries.")
            : pacing.threshold ? tr("Team creativity %1 of %2 needed. Each distinct trait counts once.").arg(team.creativity).arg(pacing.threshold)
            : tr("Team creativity %1.").arg(team.creativity);
        const auto calculation=pacing.missing_groups
            ? tr("Expected year %1 (year %2 of the campaign). Difficulty %3 · %4 of %5 prerequisite groups missing · ×%6 early start multiplier.")
                .arg(rule.earliest_year).arg(pacing.expected_offset).arg(rule.difficulty).arg(pacing.missing_groups).arg(pacing.prerequisite_groups).arg(pacing.early_multiplier)
            : tr("Expected year %1. Your team can follow the existing research path without a creativity threshold.").arg(rule.earliest_year);
        return {{"creativity",team.creativity},{"required",pacing.threshold},{"compatibility",static_cast<double>(team.compatibility_bp)/100},
            {"days",staff_days},
            {"explanation",tr("%1\n%2\nShared-trait compatibility %3% · %4 staff-days after coordination.")
                .arg(creativity,calculation).arg(static_cast<double>(team.compatibility_bp)/100,0,'f',1).arg(staff_days)},
            {"traits",traits.join(" · ")}};
    } catch(const std::exception&) {return {};}
}

QString ManagementController::researchBlocker(QString id, QString source, QString rate, QStringList workers, QStringList dataIds) const {
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
        auto plan = allocation(source, rate, workers);
        const auto &s = state();
        const auto *topic = find_id(s.lab->catalog.research, id.toStdString());
        if (topic && topic->data_requirement.required) {
            for (const auto& id : dataIds) plan.data_ids.push_back(id.toStdString());
            if (plan.data_ids.empty())
                return tr("This empirical research topic requires an owned dataset with at least %1 unique records and %2% quality.")
                    .arg(topic->data_requirement.minimum_unique_units)
                    .arg(static_cast<double>(topic->data_requirement.minimum_quality_bp) / 100.0, 0, 'f', 0);
        }
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

void ManagementController::beginGroundedResearch(QString id, QString source, QString rate, QStringList workers, QStringList dataIds) {
    if (!editable())
        return;
    const auto blocker = researchBlocker(id, source, rate, workers, dataIds);
    if (!blocker.isEmpty()) {
        emit noticeRaised(blocker);
        return;
    }
    auto plan = allocation(source, rate, workers);
    const auto &s = state();
    const auto *topic = find_id(s.lab->catalog.research, id.toStdString());
    if (topic && topic->data_requirement.required) {
        for (const auto& id : dataIds) plan.data_ids.push_back(id.toStdString());
    }
    emit actionRequested(core::StartResearch{id.toStdString(), std::move(plan)});
}

QVariantList ManagementController::processedDatasets() const {
    QVariantList result;
    if (!available()) return result;
    const auto &s = state();
    for (const auto &job : s.lab->processed) {
        const auto *work = find_id(s.capacity->projects, job.project_id);
        if (!work) continue;
        const bool active = work->status == core::ProjectStatus::active;
        const bool paused = work->status == core::ProjectStatus::paused;
        const QString status = job.output ? tr("Processed & available") : paused ? tr("Paused") : active ? tr("Processing") : tr("Cancelled");
        result.push_back(QVariantMap{
            {"id", q(job.id)},
            {"sourceDatasetId", q(job.source_dataset_id)},
            {"operation", q(job.operation)},
            {"status", status},
            {"projectId", q(job.project_id)},
            {"active", active},
            {"paused", paused},
            {"completed", job.output.has_value()},
            {"uniqueUnits", job.output ? static_cast<qulonglong>(job.output->unique_units) : 0ULL},
            {"qualityPct", job.output ? static_cast<double>(job.output->quality.value()) / 100.0 : 0.0},
            {"detail", tr("Source %1 · %2 · %3 of %4 CU completed")
                           .arg(q(job.source_dataset_id), q(job.operation), cu(work->completed_work.value()), cu(work->total_work.value()))}
        });
    }
    return result;
}

QVariantList ManagementController::creativeJobs() const {
    QVariantList result;
    if (!available()) return result;
    const auto &s = state();
    for (const auto &job : s.lab->creative_jobs) {
        const auto *work = find_id(s.capacity->projects, job.project_id);
        if (!work) continue;
        const bool active = work->status == core::ProjectStatus::active;
        const bool paused = work->status == core::ProjectStatus::paused;
        const QString status = job.output ? tr("Delivered & cleared") : paused ? tr("Paused") : active ? tr("Authoring") : tr("Cancelled");
        result.push_back(QVariantMap{
            {"id", q(job.id)},
            {"domain", q(job.domain)},
            {"targetUnits", static_cast<qulonglong>(job.target_units)},
            {"status", status},
            {"projectId", q(job.project_id)},
            {"active", active},
            {"paused", paused},
            {"completed", job.output.has_value()},
            {"qualityPct", job.output ? static_cast<double>(job.output->quality.value()) / 100.0 : 0.0},
            {"detail", tr("Domain %1 · %2 records · %3/%4 staff-days · %5/%6 CU")
                           .arg(q(job.domain))
                           .arg(job.target_units)
                           .arg(work->completed_staff_days)
                           .arg(work->required_staff_days)
                           .arg(cu(work->completed_work.value()), cu(work->total_work.value()))}
        });
    }
    return result;
}

QVariantList ManagementController::creativeWorkers() const {
    QVariantList rows;
    if (!snapshot_) return rows;
    const auto &s = state();
    if (!s.management || !s.management->people) return rows;
    for (const auto &emp : s.management->people->employees) {
        if (emp.departed_on && *emp.departed_on <= s.date) continue;
        const bool ready = emp.ready_on <= s.date;
        const bool is_data_creative = emp.terms.profile.role == "Data Creative";
        rows.push_back(QVariantMap{
            {"id", q(emp.terms.profile.id)},
            {"name", q(emp.terms.profile.name)},
            {"role", q(emp.terms.profile.role)},
            {"creativeSkill", emp.terms.profile.creative},
            {"researchSkill", emp.terms.profile.research},
            {"isDataCreative", is_data_creative},
            {"available", ready},
            {"blocker", ready ? QString{} : tr("Onboarding until %1").arg(q(core::iso_date(emp.ready_on)))}
        });
    }
    return rows;
}

QVariantMap ManagementController::dataProcessingQuote(QString sourceDatasetId, QString operation, QString source, QString rate, QString staff) const {
    if (!available() || sourceDatasetId.isEmpty() || operation.isEmpty()) return {};
    try {
        const auto &s = state();
        const auto plan = lab_inputs::allocation(source, rate, staff);
        const core::ProcessDataset req{
            sourceDatasetId.toStdString(),
            operation.toStdString(),
            nextId(QStringLiteral("processed.")).toStdString(),
            plan
        };
        const auto quote = core::preview_data_processing(*s.lab, *s.capacity, req);
        const auto &src_offer = core::lab_data(*s.lab, req.source_dataset_id);
        const auto *supply = find_id(s.capacity->sources, plan.source_id);
        const bool has_compute = supply && free_compute(*s.capacity, *supply) >= plan.daily_compute.value();
        const auto cap = core::capacity_report(*s.capacity);
        const bool has_staff = cap.available_staff - cap.assigned_staff >= plan.staff;
        const bool funded = core::discretionary_cash(s) >= quote.fee;
        QString detail = tr("Operation: %1 on %2\n"
                            "Fee %3 · %4 CU · %5 staff · earliest %6 days\n"
                            "Retained records: %7 → %8\n"
                            "Quality: %9% → %10% · Duplication: %11% → %12% · Contamination: %13% → %14%\n"
                            "%15")
                             .arg(operation, sourceDatasetId, money(quote.fee), cu(quote.work.value()), staff)
                             .arg(quote.minimum_days.value())
                             .arg(src_offer.unique_units)
                             .arg(quote.estimated_unique_units)
                             .arg(static_cast<double>(src_offer.quality.value()) / 100.0, 0, 'f', 1)
                             .arg(static_cast<double>(quote.estimated_quality.value()) / 100.0, 0, 'f', 1)
                             .arg(static_cast<double>(src_offer.duplication_bp.value()) / 100.0, 0, 'f', 1)
                             .arg(static_cast<double>(quote.estimated_duplication.value()) / 100.0, 0, 'f', 1)
                             .arg(static_cast<double>(src_offer.contamination.value()) / 100.0, 0, 'f', 1)
                             .arg(static_cast<double>(quote.estimated_contamination.value()) / 100.0, 0, 'f', 1)
                             .arg(q(quote.explanation));
        if (!has_compute || !has_staff) detail += tr("\nInsufficient unreserved compute or staff.");
        if (!funded) detail += tr("\nFee exceeds discretionary cash.");
        return {
            {"ready", has_compute && has_staff && funded},
            {"detail", detail},
            {"fee", money(quote.fee)},
            {"workCu", cu(quote.work.value())},
            {"minimumDays", quote.minimum_days.value()},
            {"retainedUnits", static_cast<qulonglong>(quote.estimated_unique_units)},
            {"qualityBeforePct", static_cast<double>(src_offer.quality.value()) / 100.0},
            {"qualityAfterPct", static_cast<double>(quote.estimated_quality.value()) / 100.0},
            {"duplicationBeforePct", static_cast<double>(src_offer.duplication_bp.value()) / 100.0},
            {"duplicationAfterPct", static_cast<double>(quote.estimated_duplication.value()) / 100.0},
            {"contaminationBeforePct", static_cast<double>(src_offer.contamination.value()) / 100.0},
            {"contaminationAfterPct", static_cast<double>(quote.estimated_contamination.value()) / 100.0},
            {"rightsWarning", q(quote.explanation)}
        };
    } catch (const std::exception &error) {
        return {{"ready", false}, {"detail", QString::fromUtf8(error.what())}};
    }
}

void ManagementController::processDataset(QString sourceDatasetId, QString operation, QString source, QString rate, QString staff) {
    if (!editable()) return;
    try {
        const auto plan = lab_inputs::allocation(source, rate, staff);
        emit actionRequested(core::ProcessDataset{
            sourceDatasetId.toStdString(),
            operation.toStdString(),
            nextId(QStringLiteral("processed.")).toStdString(),
            plan
        });
    } catch (const std::exception &error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
    }
}

QVariantMap ManagementController::creativeProductionQuote(QString domain, QString units, QString source, QString rate, QStringList workers) const {
    if (!available() || domain.isEmpty()) return {};
    try {
        const auto &s = state();
        auto plan = allocation(source, rate, workers);
        const auto target_units = static_cast<std::uint64_t>(std::max<std::int64_t>(1000, units.toLongLong()));
        const core::StartCreativeProduction req{
            nextId(QStringLiteral("data.creative_")).toStdString(),
            domain.toStdString(),
            target_units,
            plan
        };
        const auto quote = core::preview_creative_production(s, req);
        const auto *supply = find_id(s.capacity->sources, plan.source_id);
        const bool has_compute = supply && free_compute(*s.capacity, *supply) >= plan.daily_compute.value();
        const auto cap = core::capacity_report(*s.capacity);
        const bool has_staff = cap.available_staff - cap.assigned_staff >= plan.staff;
        const bool funded = core::discretionary_cash(s) >= quote.fee;
        QString detail = tr("In-house work-for-hire creative dataset (%1)\n"
                            "%2 clean records · Fee %3 · %4 CU · earliest %5 days\n"
                            "Estimated quality: %6% (scaled by team creative skill; never automatic 100%)\n"
                            "Provenance: 100% verified work-for-hire · Zero third-party controversy · Commercial & open-weight cleared.")
                             .arg(domain)
                             .arg(target_units)
                             .arg(money(quote.fee), cu(quote.work.value()))
                             .arg(quote.minimum_days.value())
                             .arg(static_cast<double>(quote.estimated_quality.value()) / 100.0, 0, 'f', 1);
        if (!has_compute || !has_staff) detail += tr("\nInsufficient unreserved compute or staff.");
        if (!funded) detail += tr("\nFee exceeds discretionary cash.");
        return {
            {"ready", has_compute && has_staff && funded},
            {"detail", detail},
            {"fee", money(quote.fee)},
            {"workCu", cu(quote.work.value())},
            {"minimumDays", quote.minimum_days.value()},
            {"estimatedQualityPct", static_cast<double>(quote.estimated_quality.value()) / 100.0}
        };
    } catch (const std::exception &error) {
        return {{"ready", false}, {"detail", QString::fromUtf8(error.what())}};
    }
}

void ManagementController::startCreativeProduction(QString domain, QString units, QString source, QString rate, QStringList workers) {
    if (!editable()) return;
    try {
        auto plan = allocation(source, rate, workers);
        const auto target_units = static_cast<std::uint64_t>(std::max<std::int64_t>(1000, units.toLongLong()));
        emit actionRequested(core::StartCreativeProduction{
            nextId(QStringLiteral("data.creative_")).toStdString(),
            domain.toStdString(),
            target_units,
            std::move(plan)
        });
    } catch (const std::exception &error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
    }
}

QVariantList ManagementController::techLicenses() const {
    QVariantList rows;
    if (!available()) return rows;
    const auto &s = state();
    for (const auto &offer : s.lab->catalog.tech_licenses) {
        const auto active_it = std::ranges::find_if(s.lab->tech_licenses | std::views::reverse, [&](const auto &row) { return row.offer_id == offer.id; });
        const auto *active_lic = active_it == s.lab->tech_licenses.rend() ? nullptr : &*active_it;
        const bool completed = has_result(*s.lab, offer.research_id);
        const bool is_active = active_lic && active_lic->active_on <= s.date && s.date < active_lic->expires_on;
        const bool is_integrating = active_lic && s.date < active_lic->active_on;
        const bool is_expired = active_lic && s.date >= active_lic->expires_on;
        const auto topic_title = research_->sourceRow(q(offer.research_id)).value("title", q(offer.research_id)).toString();
        rows.push_back(QVariantMap{
            {"offerId", q(offer.id)},
            {"researchId", q(offer.research_id)},
            {"researchTitle", topic_title},
            {"licensor", q(offer.licensor)},
            {"upfrontFee", money(offer.upfront_fee)},
            {"monthlyRoyalty", money(offer.monthly_royalty)},
            {"termDays", offer.term_days.value()},
            {"integrationDays", offer.integration_days.value()},
            {"scopeLabel", q(offer.scope_label)},
            {"active", is_active},
            {"integrating", is_integrating},
            {"expired", is_expired},
            {"internallyDiscovered", completed},
            {"readyOn", active_lic ? q(core::iso_date(active_lic->active_on)) : QString{}},
            {"expiresBefore", active_lic ? q(core::iso_date(active_lic->expires_on)) : QString{}},
            {"canLicense", (!active_lic || is_expired) && !completed && offer.upfront_fee <= core::discretionary_cash(s)},
            {"canRenew", is_active && offer.renewable && core::Cents{offer.upfront_fee.value()/2} <= core::discretionary_cash(s)}
        });
    }
    return rows;
}

void ManagementController::licenseTechnology(QString offerId) {
    if (!editable()) return;
    try {
        emit actionRequested(core::LicenseTechnology{offerId.toStdString()});
    } catch (const std::exception &error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
    }
}

void ManagementController::renewTechLicense(QString offerId) {
    if (!editable()) return;
    try {
        emit actionRequested(core::RenewTechLicense{offerId.toStdString()});
    } catch (const std::exception &error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
    }
}

QVariantMap ManagementController::modelTechnicalProfile(QString modelId) const {
    if (!available() || modelId.isEmpty()) return {};
    try {
        const auto &s = state();
        const auto profile = core::product_technical_profile(s, modelId.toStdString());
        QStringList modalities, settings;
        for (const auto &m : profile.modalities) modalities.push_back(q(m));
        for (const auto &st : profile.deployment_settings) settings.push_back(q(st));
        return {
            {"modelId", modelId},
            {"latencyBonusPct", static_cast<double>(profile.latency_bonus_bp) / 100.0},
            {"robustnessBonusPct", static_cast<double>(profile.robustness_bonus_bp) / 100.0},
            {"dataEfficiencyPct", static_cast<double>(profile.data_efficiency_bp) / 100.0},
            {"computeEfficiencyPct", static_cast<double>(profile.compute_efficiency_bp) / 100.0},
            {"cleanProvenance", profile.clean_provenance},
            {"modalities", modalities},
            {"deploymentSettings", settings}
        };
    } catch (const std::exception &) {
        return {};
    }
}

QVariantMap ManagementController::industryStage() const {
    if (!snapshot_) return {};
    const auto &world = snapshot_->checkpoint.world;
    const auto industry = runtime::industry_activity(world);
    const auto level = industry.stage;
    QString title = tr("Stage 0 · Early Frontier");
    QString summary = tr("Skeptical investors, scarce specialized talent, small early-adopter contracts, and low regulatory pressure.");
    if (level == 3) {
        title = tr("Stage 3 · Maturing Market");
        summary = tr("Consolidated heavyweight competition, high regulatory & provenance scrutiny, and demanding enterprise SLAs.");
    } else if (level == 2) {
        title = tr("Stage 2 · Industry Boom");
        summary = tr("Broad enterprise adoption, aggressive talent poaching, GPU supply pressure, and active press/regulatory attention.");
    } else if (level == 1) {
        title = tr("Stage 1 · Rising Interest");
        summary = tr("First public breakthroughs and commercial services attract angel/seed investors, university talent, and trade press.");
    }
    return {
        {"level", level},
        {"title", title},
        {"summary", summary},
        {"publicBreakthroughs", static_cast<qlonglong>(industry.publications)},
        {"deployedServices", static_cast<qlonglong>(industry.deployments)}
    };
}

namespace {
QString classify_expense_posting(const core::Posting &p) {
    if (p.key.starts_with("payroll/") || p.key.starts_with("specialist/") || p.key.starts_with("people/") ||
        p.cause.find("payroll") != std::string::npos || p.cause.find("salary") != std::string::npos || p.cause.find("signing") != std::string::npos)
        return QStringLiteral("payroll");
    if (p.key.starts_with("lease/") || p.key.starts_with("offer.supplier") || p.cause.find("lease") != std::string::npos || p.cause.find("compute") != std::string::npos)
        return QStringLiteral("compute");
    if (p.key.starts_with("lab/data/") || p.key.starts_with("lab/acquire/") || p.key.starts_with("lab/process/") ||
        p.key.starts_with("lab/creative/") || p.key.starts_with("lab/synthesize/") || p.key.starts_with("lab/tech_license/"))
        return QStringLiteral("data_licenses");
    if (p.key.starts_with("hq.") || p.key.starts_with("infrastructure/") || p.key.starts_with("site/") || p.cause.find("rent") != std::string::npos || p.cause.find("equipment") != std::string::npos)
        return QStringLiteral("facilities");
    if (p.key.starts_with("lab/research/") || p.key.starts_with("lab/evaluate/"))
        return QStringLiteral("research_eval");
    if (p.key.starts_with("tax/") || p.key.starts_with("debt/") || p.key.starts_with("repayment") || p.cause.find("tax") != std::string::npos || p.cause.find("interest") != std::string::npos)
        return QStringLiteral("taxes_debt");
    return QStringLiteral("other");
}
} // namespace

QVariantList ManagementController::expenseBreakdown() const {
    QVariantList rows;
    if (!snapshot_ || !state().accounts) return rows;
    const auto &postings = state().accounts->postings;
    struct Bucket {
        QString id;
        QString label;
        QString color;
        std::int64_t cents = 0;
        int count = 0;
    };
    std::vector<Bucket> buckets{
        {QStringLiteral("payroll"), tr("Payroll & Hiring"), QStringLiteral("#4a90d9"), 0, 0},
        {QStringLiteral("compute"), tr("Compute Leases"), QStringLiteral("#d98c3a"), 0, 0},
        {QStringLiteral("data_licenses"), tr("Data & Tech Licenses"), QStringLiteral("#56b881"), 0, 0},
        {QStringLiteral("facilities"), tr("Facilities & HQ"), QStringLiteral("#9b6bd9"), 0, 0},
        {QStringLiteral("research_eval"), tr("Research & Evaluation"), QStringLiteral("#3ab5c9"), 0, 0},
        {QStringLiteral("taxes_debt"), tr("Taxes & Debt Service"), QStringLiteral("#d95b5b"), 0, 0},
        {QStringLiteral("other"), tr("Operations & Other"), QStringLiteral("#7b8898"), 0, 0},
    };
    std::int64_t total_cents = 0;
    for (const auto &p : postings) {
        if (p.credit != core::Account::cash || p.amount.value() <= 0) continue;
        const auto cat = classify_expense_posting(p);
        for (auto &b : buckets) {
            if (b.id == cat) {
                b.cents = core::checked_add(b.cents, p.amount.value());
                ++b.count;
                total_cents = core::checked_add(total_cents, p.amount.value());
                break;
            }
        }
    }
    for (const auto &b : buckets) {
        const double share = total_cents > 0 ? static_cast<double>(b.cents) / static_cast<double>(total_cents) : 0.0;
        rows.push_back(QVariantMap{
            {"id", b.id},
            {"label", b.label},
            {"color", b.color},
            {"cents", static_cast<qlonglong>(b.cents)},
            {"amount", money(core::Cents{b.cents})},
            {"count", b.count},
            {"share", share},
            {"percentText", QStringLiteral("%1%").arg(share * 100.0, 0, 'f', 1)}
        });
    }
    return rows;
}

QVariantList ManagementController::ledgerPostingsByCategory(QString category) const {
    QVariantList entries;
    if (!snapshot_ || !state().accounts) return entries;
    const auto &postings = state().accounts->postings;
    for (auto it = postings.rbegin(); it != postings.rend(); ++it) {
        const auto &p = *it;
        const bool received = p.debit == core::Account::cash;
        const bool paid = p.credit == core::Account::cash;
        if (!received && !paid) continue;
        const QString cat = paid ? classify_expense_posting(p) : QStringLiteral("revenue");
        if (!category.isEmpty() && category != QStringLiteral("all") && cat != category) continue;
        entries.push_back(QVariantMap{
            {"date", q(core::iso_date(p.day))},
            {"cause", q(p.cause)},
            {"reference", q(p.key)},
            {"category", cat},
            {"amount", (received ? QStringLiteral("+") : QStringLiteral("−")) + money(p.amount)},
            {"incoming", received}
        });
        if (entries.size() >= 40) break;
    }
    return entries;
}
} // namespace pgp::bridge
