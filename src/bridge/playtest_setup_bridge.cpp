#include "campaign_controller.hpp"
#include "campaign_worker.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <limits>
#include <QUuid>
#include <pgp/persistence/sha256.hpp>

namespace pgp::bridge {
#if !PGP_DISTRIBUTION
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }

QStringList toQStringList(const std::vector<std::string>& rows) {
    QStringList out;
    for (const auto& s : rows) out.push_back(q(s));
    return out;
}

std::vector<std::string> fromQStringList(const QVariant& value) {
    std::vector<std::string> out;
    for (const auto& s : value.toStringList()) {
        const auto trimmed = s.trimmed();
        if (!trimmed.isEmpty()) out.push_back(trimmed.toStdString());
    }
    return out;
}

QVariantMap configToVariant(const runtime::PlaytestSetupConfig& c) {
    QVariantList parts, style;
    for (auto v : c.appearance.parts) parts << static_cast<int>(v);
    for (auto v : c.appearance.style) style << static_cast<int>(v);
    return {
        {"scenarioId", q(c.scenario_id)},
        {"seed", QString::number(static_cast<qulonglong>(c.seed))},
        {"firstName", q(c.first_name)},
        {"lastName", q(c.last_name)},
        {"pronouns", q(c.pronouns)},
        {"gender", q(c.gender)},
        {"orientation", q(c.orientation)},
        {"appearance", parts},
        {"style", style},
        {"birthYear", c.birth_year},
        {"launchYear", c.launch_year},
        {"family", q(c.family)},
        {"sociability", q(c.sociability)},
        {"activity", q(c.activity)},
        {"disposition", q(c.disposition)},
        {"education", q(c.education)},
        {"advantages", toQStringList(c.advantages)},
        {"traits", toQStringList(c.traits)},
        {"skills", toQStringList(c.skills)},
        {"experience", static_cast<int>(c.experience)},
        {"research", static_cast<int>(c.research)},
        {"engineering", static_cast<int>(c.engineering)},
        {"evaluation", static_cast<int>(c.evaluation)},
        {"missionFit", static_cast<int>(c.mission_fit)},
        {"prestige", static_cast<int>(c.prestige)},
        {"buzz", static_cast<int>(c.buzz)},
        {"personalCashDollars", static_cast<qlonglong>(c.personal_cash.value() / 100)},
        {"personalDebtDollars", static_cast<qlonglong>(c.personal_debt.value() / 100)},
        {"companyName", q(c.company_name)},
        {"headquarters", q(c.headquarters)},
        {"legalForm", q(c.legal_form)},
        {"route", q(c.route)},
        {"mission", q(c.mission)},
        {"objective", q(c.objective)},
        {"charter", q(c.charter)},
        {"companyCashDollars", static_cast<qlonglong>(c.company_cash.value() / 100)},
        {"companyDebtDollars", static_cast<qlonglong>(c.company_debt.value() / 100)},
        {"founderEquityPct", static_cast<int>(c.founder_equity_bp / 100)},
        {"founderBoardSeats", static_cast<int>(c.founder_board_seats)},
        {"outsideBoardSeats", static_cast<int>(c.outside_board_seats)},
        {"inheritedPurpose", c.inherited_purpose},
        {"extraStaffCount", static_cast<int>(c.extra_staff_count)},
        {"staffSkillLevel", static_cast<int>(c.staff_skill_level)},
        {"bestFriendsCount", static_cast<int>(c.best_friends_count)},
        {"office", q(c.office)},
        {"extraLaptops", static_cast<int>(c.extra_laptops)},
        {"extraDesktops", static_cast<int>(c.extra_desktops)},
        {"startingComputeCu", static_cast<qlonglong>(c.starting_compute.value() / 1000)},
        {"completedResearchIds", toQStringList(c.completed_research_ids)},
        {"ownedDatasetIds", toQStringList(c.owned_dataset_ids)},
        {"includeStarterModel", c.include_starter_model},
        {"industryStage", c.industry_stage},
        {"includeStarterProduct", c.include_starter_product},
        {"playtestUnlimited", c.playtest_unlimited}
    };
}

runtime::PlaytestSetupConfig variantToConfig(const runtime::CompiledProgram& program, const QVariantMap& m) {
    auto c = runtime::default_playtest_setup(program);
    const auto str = [&](const char* k, std::string& dst) {
        if (m.contains(QLatin1String{k})) dst = m.value(QLatin1String{k}).toString().trimmed().toStdString();
    };
    const auto u32 = [&](const char* k, std::uint32_t& dst) {
        if (m.contains(QLatin1String{k})) dst = static_cast<std::uint32_t>(std::max(0, m.value(QLatin1String{k}).toInt()));
    };
    str("scenarioId", c.scenario_id);
    if (m.contains("seed")) {
        bool ok = false;
        const auto s = m.value("seed").toString().trimmed().toULongLong(&ok);
        if (ok) c.seed = s;
    }
    str("firstName", c.first_name);
    str("lastName", c.last_name);
    str("pronouns", c.pronouns);
    str("gender", c.gender);
    str("orientation", c.orientation);
    if (m.contains("birthYear")) c.birth_year = m.value("birthYear").toInt();
    if (m.contains("launchYear")) c.launch_year = m.value("launchYear").toInt();
    str("family", c.family);
    str("sociability", c.sociability);
    str("activity", c.activity);
    str("disposition", c.disposition);
    str("education", c.education);
    if (m.contains("advantages")) c.advantages = fromQStringList(m.value("advantages"));
    if (m.contains("traits")) c.traits = fromQStringList(m.value("traits"));
    if (m.contains("skills")) c.skills = fromQStringList(m.value("skills"));
    u32("experience", c.experience);
    u32("research", c.research);
    u32("engineering", c.engineering);
    u32("evaluation", c.evaluation);
    u32("missionFit", c.mission_fit);
    u32("prestige", c.prestige);
    u32("buzz", c.buzz);
    if (m.contains("personalCashDollars")) c.personal_cash = core::Cents{m.value("personalCashDollars").toLongLong() * 100};
    if (m.contains("personalDebtDollars")) c.personal_debt = core::Cents{m.value("personalDebtDollars").toLongLong() * 100};
    str("companyName", c.company_name);
    str("headquarters", c.headquarters);
    str("legalForm", c.legal_form);
    str("route", c.route);
    str("mission", c.mission);
    str("objective", c.objective);
    str("charter", c.charter);
    if (m.contains("companyCashDollars")) c.company_cash = core::Cents{m.value("companyCashDollars").toLongLong() * 100};
    if (m.contains("companyDebtDollars")) c.company_debt = core::Cents{m.value("companyDebtDollars").toLongLong() * 100};
    if (m.contains("founderEquityPct")) c.founder_equity_bp = static_cast<std::uint32_t>(std::max(0, m.value("founderEquityPct").toInt())) * 100;
    u32("founderBoardSeats", c.founder_board_seats);
    u32("outsideBoardSeats", c.outside_board_seats);
    if (m.contains("inheritedPurpose")) c.inherited_purpose = m.value("inheritedPurpose").toBool();
    u32("extraStaffCount", c.extra_staff_count);
    u32("staffSkillLevel", c.staff_skill_level);
    u32("bestFriendsCount", c.best_friends_count);
    str("office", c.office);
    u32("extraLaptops", c.extra_laptops);
    u32("extraDesktops", c.extra_desktops);
    if (m.contains("startingComputeCu")) c.starting_compute = core::MilliCUPerDay{std::max<qlonglong>(0, m.value("startingComputeCu").toLongLong()) * 1000};
    if (m.contains("completedResearchIds")) c.completed_research_ids = fromQStringList(m.value("completedResearchIds"));
    if (m.contains("ownedDatasetIds")) c.owned_dataset_ids = fromQStringList(m.value("ownedDatasetIds"));
    if (m.contains("includeStarterModel")) c.include_starter_model = m.value("includeStarterModel").toBool();
    if (m.contains("industryStage")) c.industry_stage = m.value("industryStage").toInt();
    if (m.contains("includeStarterProduct")) c.include_starter_product = m.value("includeStarterProduct").toBool();
    if (m.contains("playtestUnlimited")) c.playtest_unlimited = m.value("playtestUnlimited").toBool();
    if (m.contains("appearance")) {
        const auto parts = m.value("appearance").toList();
        if (parts.size() == static_cast<qsizetype>(c.appearance.parts.size())) {
            for (std::size_t i = 0; i < c.appearance.parts.size(); ++i)
                c.appearance.parts[i] = static_cast<std::uint8_t>(std::clamp(parts[static_cast<qsizetype>(i)].toInt(), 0, static_cast<int>(core::portrait_part_counts[i]) - 1));
        }
    }
    if (m.contains("style")) {
        const auto style = m.value("style").toList();
        if (style.size() == static_cast<qsizetype>(c.appearance.style.size())) {
            for (std::size_t i = 0; i < c.appearance.style.size(); ++i)
                c.appearance.style[i] = static_cast<std::uint8_t>(std::clamp(style[static_cast<qsizetype>(i)].toInt(), 0, static_cast<int>(core::portrait_style_counts[i]) - 1));
        }
    }
    return c;
}

QStringList setupErrors(const runtime::CompiledProgram& program, const QVariantMap& draft) {
    QStringList errors;
    const auto defaults = configToVariant(runtime::default_playtest_setup(program));
    for (auto it = defaults.begin(); it != defaults.end(); ++it) {
        const auto type = it.value().typeId();
        if (!draft.contains(it.key()) || (type != QMetaType::Int && type != QMetaType::LongLong && it.key() != "seed")) continue;
        bool ok = false;
        const auto value = draft.value(it.key()).toString().trimmed().toULongLong(&ok);
        if (!ok || draft.value(it.key()).toString().trimmed().startsWith('-') ||
            (type == QMetaType::Int && value > static_cast<qulonglong>(std::numeric_limits<int>::max())) ||
            (type == QMetaType::LongLong && value > 10000000000ULL))
            errors.push_back(QObject::tr("%1 requires a nonnegative whole number within range.").arg(it.key()));
    }
    if (!errors.isEmpty()) return errors;
    const auto config = variantToConfig(program, draft);
    for (const auto& error : runtime::validate_playtest_setup(program, config)) errors.push_back(q(error));
    return errors;
}

std::filesystem::path draftFilePath(const QUrl& saveFolder) {
    return std::filesystem::path{saveFolder.toLocalFile().toStdString()} / "playtest-setup-draft.json";
}

std::filesystem::path playtestAutosavePath(const persistence::LocalPaths& paths, const runtime::CompiledProgram& program) {
    return paths.saves_directory / "autosaves" / persistence::sha256(program.canonical_content_bytes);
}
} // namespace

QVariantMap CampaignController::playtestSetupCatalog() const {
    QVariantMap out;
    const auto* def = program_.collection.find("scenario.founder_campaign");
    const auto* scenario = def ? std::get_if<content::ScenarioDefinition>(&def->data) : nullptr;
    if (!scenario) return out;

    QVariantList hqs, charters, skills, research, datasets, traits;
    for (const auto& h : scenario->headquarters_choices) {
        QString label = q(h);
        if (h == "US-CA") label = tr("California (US-CA)");
        else if (h == "US-TX") label = tr("Texas (US-TX)");
        else if (h == "US-WA") label = tr("Washington (US-WA)");
        hqs.push_back(QVariantMap{{"id", q(h)}, {"name", label}});
    }
    charters.push_back(QVariantMap{{"id", ""}, {"name", tr("No optional charter")}});
    if (scenario->management && scenario->management->governance) {
        for (const auto& ch : scenario->management->governance->charters)
            charters.push_back(QVariantMap{{"id", q(ch.id)}, {"name", q(ch.name)}});
    }
    if (scenario->management && scenario->management->people) {
        if (scenario->management->people->talent) {
            for (const auto& sk : scenario->management->people->talent->skills)
                skills.push_back(QVariantMap{{"id", q(sk.id)}, {"name", q(sk.name)}});
        }
        for (const auto& grp : scenario->management->people->trait_groups) {
            for (const auto& tr_name : grp) traits.push_back(q(tr_name));
        }
    }
    if (scenario->lab) {
        const auto lab = content::scenario_lab_catalog(program_.collection, *scenario);
        for (const auto& r : lab.research) {
            QStringList prereqs;
            for (const auto& p : r.all_of) prereqs.push_back(q(p));
            const auto* title_msg = program_.collection.message(r.id + ".title");
            research.push_back(QVariantMap{{"id", q(r.id)}, {"name", title_msg ? q(*title_msg) : q(r.id)}, {"prerequisites", prereqs}});
        }
        for (const auto& d : lab.data) {
            datasets.push_back(QVariantMap{{"id", q(d.id)}, {"name", q(d.name)}});
        }
    }
    QVariantList families;
    if (scenario->founding) for (const auto& family : scenario->founding->families)
        families.push_back(QVariantMap{{"id", q(family.id)}, {"name", q(family.name)}});
    out["families"] = families;
    QStringList orientations;
    for (const auto value : content::founder_orientations) orientations.push_back(QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
    out["orientations"] = orientations;
    out["headquarters"] = hqs;
    out["charters"] = charters;
    out["skills"] = skills;
    out["research"] = research;
    out["datasets"] = datasets;
    out["traits"] = traits;
    out["minYear"] = core::from_ordinal(scenario->starts_on).year;
    out["maxYear"] = std::max(core::from_ordinal(scenario->starts_on).year, core::from_ordinal(scenario->ends_before).year - 2);
    return out;
}

QVariantMap CampaignController::playtestSetupDraft() const {
    auto draft = configToVariant(runtime::default_playtest_setup(program_));
    const auto path = draftFilePath(save_folder_);
    QFile file{QString::fromStdU16String(path.u16string())};
    if (file.open(QIODevice::ReadOnly) && file.size() <= 65536) {
        const auto doc = QJsonDocument::fromJson(file.readAll()).object();
        const auto saved = doc.value("draft").toObject().toVariantMap();
        for (auto it = saved.begin(); it != saved.end(); ++it) {
            if (draft.contains(it.key())) draft[it.key()] = it.value();
        }
    }
    auto errorList = setupErrors(program_, draft);
    return {{"draft", draft}, {"valid", errorList.empty()}, {"errors", errorList}};
}

QVariantMap CampaignController::savePlaytestSetupDraft(QVariantMap draft) const {
    auto errorList = setupErrors(program_, draft);
    try {
        const auto path = draftFilePath(save_folder_);
        std::filesystem::create_directories(path.parent_path());
        QSaveFile file{QString::fromStdU16String(path.u16string())};
        const auto bytes = QJsonDocument{QJsonObject::fromVariantMap({{"format", "playtest-setup-1"}, {"draft", draft}})}.toJson(QJsonDocument::Compact);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) errorList.push_back(tr("Could not preserve the setup draft in local storage."));
    } catch (const std::exception& error) {
        errorList.push_back(QString::fromUtf8(error.what()));
    }
    return {{"draft", draft}, {"valid", errorList.empty()}, {"errors", errorList}};
}

QVariantMap CampaignController::resetPlaytestSetupDraft() const {
    const auto draft = configToVariant(runtime::default_playtest_setup(program_));
    return savePlaytestSetupDraft(draft);
}

void CampaignController::startPlaytestCustom(QVariantMap draft) {
    if (!snapshot_ || !snapshot_->checkpoint.world.developer_playtest) {
        emit commandFinished(false, tr("Open custom setup in the isolated playtest environment."));
        return;
    }
    const auto saved = savePlaytestSetupDraft(draft);
    if (!saved.value("valid").toBool()) {
        const auto reason = saved.value("errors").toStringList().value(0);
        setNotice(reason);
        emit commandFinished(false, reason);
        return;
    }
    const auto cfg = variantToConfig(program_, draft);
    if (!beginOperation()) return;
    starting_founder_ = false;
    QMetaObject::invokeMethod(worker_, [worker = worker_, token = token_, cfg]() mutable {
        worker->startPlaytestCustom(token, std::move(cfg));
    }, Qt::QueuedConnection);
}

void CampaignWorker::startPlaytestCustom(std::uint64_t token, runtime::PlaytestSetupConfig config) {
    requireOwningThread();
    try {
        if (!campaign_.checkpoint().world.developer_playtest)
            throw std::runtime_error("Custom setup requires isolated playtest storage.");
        if (generation_ == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Campaign generation is exhausted");
        auto initial = runtime::begin_playtest_scenario(*installed_, config);
        if (initial.world.identity && initial.world.identity->instance_id.empty()) {
            initial.world.identity->instance_id = QUuid::createUuid().toString(QUuid::Id128).toStdString();
        }
        session::Campaign next{*installed_, std::move(initial), playtestAutosavePath(paths_, *installed_), campaign_.settings()};
        const auto prepared = next.prepare_opening(next.checkpoint().world.simulation.revision);
        if (!prepared.command.accepted()) throw std::runtime_error(prepared.command.explanation);
        const auto paused_state = next.checkpoint().world.simulation;
        if (!paused_state.paused) (void)next.submit({paused_state.revision, core::SetPaused{true}});
        if (continuous_) {
            if (!writer_lock_ || !writer_lock_->isLocked())
                throw std::runtime_error("Another game window owns this campaign.");
            current_.content().restore(next.program());
            if (active_) current_.archive(campaign_.program(), campaign_.checkpoint());
            current_.commit(next.program(), next.checkpoint());
            active_ = true;
        }
        campaign_ = std::move(next);
        active_content_ = installed_;
        missing_content_hash_.clear();
        ++generation_;
        halted_ = true;
        milestone_.reset();
        publish();
        emit operationCompleted(static_cast<qulonglong>(token), true, QString{});
    } catch (const std::exception& error) {
        emit noticeRaised(QString::fromUtf8(error.what()));
        emit operationCompleted(static_cast<qulonglong>(token), false, QString::fromUtf8(error.what()));
    }
}
#endif
} // namespace pgp::bridge
