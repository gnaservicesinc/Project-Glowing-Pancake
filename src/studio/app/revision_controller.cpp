#include "revision_controller.hpp"

#include "probe_controller.hpp"
#include <pgp/core/integer_codec.hpp>
#include <pgp/persistence/storage.hpp>

#include <nlohmann/json.hpp>
#include <QVariantMap>
#include <QRegularExpression>
#include <algorithm>

namespace pgp::studio {
namespace {
using Json = nlohmann::json;
constexpr int page_size = 100;
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString value(const Json& item) { return item.is_string() ? text(item.get_ref<const std::string&>()) : text(item.dump()); }
QString label(const RevisionInfo& info) {
    return QStringLiteral("Generation %1 · %2").arg(number(info.generation), text(info.content_version));
}
QString description(const RevisionInfo& info) {
    return QStringLiteral("%1 · game %2\n%3").arg(label(info), text(info.game_version), text(info.summary));
}
QStringList scenarios(const std::optional<FinalizedRevision>& revision) {
    QStringList result;
    if (revision) for (const auto& [id, definition] : revision->program.collection.definitions)
        if (std::holds_alternative<content::ScenarioDefinition>(definition.data)) result.push_back(text(id));
    return result;
}
const content::ScenarioDefinition* scenario(const FinalizedRevision& revision, const QString& id) {
    const auto* definition = revision.program.collection.find(id.toStdString());
    return definition ? std::get_if<content::ScenarioDefinition>(&definition->data) : nullptr;
}
QVariantList choices(const runtime::Checkpoint& checkpoint, const FinalizedRevision& revision) {
    QVariantList result;
    for (const auto& card : checkpoint.runtime.presentations) {
        if (card.resolved) continue;
        for (const auto& choice : card.choices) {
            const auto* message = revision.program.collection.message(choice.label_key);
            result.push_back(QVariantMap{{"label", QStringLiteral("%1 / %2 · %3 (%4)")
                .arg(text(card.flow_id), text(card.node_id), text(message ? *message : choice.label_key), text(choice.id))},
                {"id", number(card.id)}, {"choice", text(choice.id)}});
        }
    }
    return result;
}
QString trace(const ProbeStep& step, const runtime::Checkpoint& checkpoint) {
    QStringList lines{QStringLiteral("%1 · %2").arg(text(runtime::to_string(step.trace.reason)), text(step.trace.explanation))};
    append_probe_step(lines, step);
    lines << QStringLiteral("\nCommitted effects in this step");
    append_probe_effects(lines, step.effects);
    append_probe_continuations(lines, checkpoint);
    return lines.join(u'\n');
}
QString migration_summary(const Json& report) {
    QStringList lines{value(report.at("summary")),
        QStringLiteral("Generation %1 → %2 · game %3 → %4")
            .arg(value(report.at("source").at("generation")), value(report.at("target").at("generation")),
                value(report.at("source").at("game_version")), value(report.at("target").at("game_version"))),
        QStringLiteral("%1 · %2").arg(value(report.at("date")), value(report.at("scenario"))),
        QStringLiteral("%1 protected graphs · %2 occurrence records · %3 effect receipts · %4 owned modifiers · %5 resolved random draws")
            .arg(value(report.at("protected_graphs")), value(report.at("occurrence_states")), value(report.at("receipts")),
                value(report.at("owned_modifiers")), value(report.at("resolved_random_draws"))),
        QStringLiteral("Original and exported copy verified.")};
    for (const auto& row : report.at("mappings"))
        lines << QStringLiteral("%1 %2 %3 → %4").arg(value(row.at("kind")), value(row.at("owner")),
            value(row.at("from")), value(row.at("to")));
    for (const auto& pending : report.at("pending")) {
        const auto& before = pending.at("before");
        const auto& after = pending.at("after");
        lines << QStringLiteral("\nActivation %1 · %2 · %3")
            .arg(value(pending.at("id")), value(after.at("status")), value(after.at("binding")));
        lines << QStringLiteral("%1 / %2 → %3 / %4 · due %5 · presentation %6")
            .arg(value(before.at("flow")), value(before.at("node")), value(after.at("flow")), value(after.at("node")),
                value(after.at("due")), value(after.at("presentation")));
        for (std::size_t index = 0; index < after.at("frames").size(); ++index) {
            const auto& old_frame = before.at("frames").at(index);
            const auto& frame = after.at("frames").at(index);
            lines << QStringLiteral("  %1 / %2 → %3 / %4 · child %5 · return %6 → %7")
                .arg(value(old_frame.at("flow")), value(old_frame.at("node")), value(frame.at("flow")), value(frame.at("node")),
                    value(frame.at("next_child")), value(old_frame.at("return")), value(frame.at("return")));
        }
    }
    return lines.join(u'\n');
}
} // namespace

template<class Action> bool RevisionController::perform(Action action) {
    message_.clear();
    try {
        action();
        emit changed();
        return true;
    } catch (const std::exception& error) {
        message_ = text(error.what());
        emit changed();
        return false;
    }
}

void RevisionController::reset_migration() {
    request_.reset();
    plan_id_.clear();
    migration_report_.clear();
    exported_path_.clear();
}

void RevisionController::reset_comparison() {
    before_.reset(); after_.reset(); fixture_.reset();
    comparison_.clear(); sections_.clear(); changes_.clear(); differences_.clear();
    before_trace_.clear(); after_trace_.clear(); before_choices_.clear(); after_choices_.clear();
    pages_ = 0;
    maps_.clear();
    reset_migration();
}

bool RevisionController::refresh(const QString& checkout) {
    return perform([&] {
        reset_comparison();
        archive_.reset();
        revisions_.clear();
        archive_.emplace(std::filesystem::path{checkout.toStdU16String()});
        for (const auto& info : archive_->revisions()) revisions_ << label(info);
    });
}

bool RevisionController::compare(int beforeIndex, int afterIndex) {
    return perform([&] {
        reset_comparison();
        if (!archive_ || beforeIndex < 0 || afterIndex <= beforeIndex || afterIndex >= revisions_.size())
            throw std::invalid_argument("Choose an earlier and a later finalized revision.");
        auto before = archive_->open(static_cast<std::uint64_t>(beforeIndex) + 1);
        auto after = archive_->open(static_cast<std::uint64_t>(afterIndex) + 1);
        changes_ = compare_finalized(before, after);
        for (const auto& change : changes_)
            if (sections_.isEmpty() || sections_.back() != text(change.section)) sections_ << text(change.section);
        comparison_ = description(before.info) + QStringLiteral("\n\n") + description(after.info);
        before_ = std::move(before);
        after_ = std::move(after);
        filter({}, 0);
    });
}

void RevisionController::filter(const QString& section, int page) {
    const auto selected = section.toStdString();
    differences_.clear();
    const auto first = static_cast<std::int64_t>(std::max(page, 0)) * page_size;
    static const QRegularExpression digest(QStringLiteral(R"(\b[a-fA-F0-9]{64}\b)"));
    const auto readable = [&](std::string_view value) {
        return text(value).replace(digest, QStringLiteral("Recorded fingerprint"));
    };
    int count = 0;
    for (const auto& change : changes_) {
        if (!selected.empty() && change.section != selected) continue;
        if (count >= first && count < first + page_size)
            differences_.push_back(QVariantMap{{"path", text(change.path)}, {"before", readable(change.before)}, {"after", readable(change.after)}});
        ++count;
    }
    pages_ = (count + page_size - 1) / page_size;
    emit changed();
}

QStringList RevisionController::beforeScenarios() const { return scenarios(before_); }
QStringList RevisionController::afterScenarios() const { return scenarios(after_); }

QStringList RevisionController::headquarters(const QString& before, const QString& after) const {
    if (!before_ || !after_) return {};
    const auto* left = scenario(*before_, before);
    const auto* right = scenario(*after_, after);
    QStringList result;
    if (left && right) for (const auto& id : left->headquarters_choices)
        if (std::find(right->headquarters_choices.begin(), right->headquarters_choices.end(), id) != right->headquarters_choices.end())
            result << text(id);
    return result;
}

bool RevisionController::startFixture(const QString& before, const QString& after, const QString& hq, const QString& seed) {
    return perform([&] {
        if (!before_ || !after_) throw std::invalid_argument("Compare two revisions before starting a fixture.");
        MatchedFixture next{*before_, *after_, before.toStdString(), after.toStdString(), hq.toStdString(),
            core::parse_json_u64(seed.trimmed().toStdString())};
        auto opening = next.prepare();
        fixture_ = std::move(next);
        show_steps(opening);
    });
}

void RevisionController::show_steps(const MatchedSteps& steps) {
    before_trace_ = trace(steps.before, fixture_->before());
    after_trace_ = trace(steps.after, fixture_->after());
    before_choices_ = choices(fixture_->before(), *before_);
    after_choices_ = choices(fixture_->after(), *after_);
}

bool RevisionController::advanceFixture() {
    return fixture_ && perform([&] { show_steps(fixture_->advance()); });
}

bool RevisionController::chooseFixture(int beforeChoice, int afterChoice) {
    return fixture_ && perform([&] {
        if (beforeChoice < 0 || beforeChoice >= before_choices_.size() || afterChoice < 0 || afterChoice >= after_choices_.size())
            throw std::invalid_argument("Select both fixture choices.");
        const auto selected = [](const QVariant& value) {
            const auto row = value.toMap();
            return FixtureChoice{core::parse_json_u64(row.value("id").toString().toStdString()), row.value("choice").toString().toStdString()};
        };
        show_steps(fixture_->choose(selected(before_choices_.at(beforeChoice)), selected(after_choices_.at(afterChoice))));
    });
}

bool RevisionController::canMigrate() const {
    return archive_ && before_ && after_ && after_->info.generation == archive_->revisions().back().generation;
}

QVariantList RevisionController::mappings() const {
    QVariantList rows;
    for (const auto& map : maps_) rows.push_back(QVariantMap{{"label", QStringLiteral("%1 %2 %3 → %4")
        .arg(text(migration_kind_name(map.kind)), text(map.owner), text(map.from), text(map.to))}});
    return rows;
}

QStringList RevisionController::mappingIds(int kind, const QString& owner, bool destination) const {
    if (!before_ || !after_ || kind < 0 || kind > static_cast<int>(MigrationKind::message)) return {};
    const auto& collection = (destination ? after_ : before_)->program.collection;
    const auto family = static_cast<MigrationKind>(kind);
    QStringList result;
    if (family == MigrationKind::node) {
        auto id = owner.toStdString();
        if (destination) for (const auto& row : maps_)
            if (row.kind == MigrationKind::flow && row.from == id) { id = row.to; break; }
        const auto* definition = collection.find(id);
        const auto* flow = definition ? std::get_if<content::FlowDefinition>(&definition->data) : nullptr;
        if (flow) for (const auto& item : flow->nodes) result << text(item.first);
    } else if (family == MigrationKind::variable) {
        for (const auto& variable : collection.variables) result << text(variable.id);
    } else if (family == MigrationKind::message) {
        for (const auto& item : collection.messages) result << text(item.first);
    } else {
        for (const auto& [id, definition] : collection.definitions)
            if (content::definition_kind(definition) == migration_kind_name(family)) result << text(id);
    }
    return result;
}

bool RevisionController::addMapping(int kind, const QString& owner, const QString& from, const QString& to) {
    return perform([&] {
        if (!canMigrate() || !mappingIds(kind, owner, false).contains(from) ||
            !mappingIds(kind, owner, true).contains(to) || from == to)
            throw std::invalid_argument("Choose distinct source and destination IDs from the selected type.");
        const auto family = static_cast<MigrationKind>(kind);
        const auto scope = family == MigrationKind::node ? owner.toStdString() : std::string{};
        const auto source = from.toStdString(), target = to.toStdString();
        if (std::any_of(maps_.begin(), maps_.end(), [&](const auto& row) {
            return row.kind == family && row.owner == scope && (row.from == source || row.to == target);
        })) throw std::invalid_argument("The selected source or destination already has a mapping.");
        maps_.push_back({family, scope, source, target});
        reset_migration();
    });
}

void RevisionController::removeMapping(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= maps_.size()) return;
    maps_.erase(maps_.begin() + index);
    clearMigration();
}

void RevisionController::clearMigration() {
    if (!request_ && exported_path_.isEmpty()) return;
    reset_migration();
    emit changed();
}

bool RevisionController::reviewMigration(const QString& source, const QString& summary) {
    return perform([&] {
        reset_migration();
        if (!canMigrate()) throw std::invalid_argument("Select the current finalized revision as destination.");
        const SaveMigrationRequest request{before_->info.generation, after_->info.generation,
            std::filesystem::path{source.toStdU16String()}, maps_, summary.trimmed().toStdString()};
        const auto review = review_save_migration(archive_->root(), request);
        const auto report = Json::parse(review.report_json);
        if (report.at("archive_sha256") != archive_->stamp())
            throw std::invalid_argument("Finalized checkout changed. Reopen the comparison before reviewing a save.");
        migration_report_ = migration_summary(report);
        plan_id_ = text(review.plan_id);
        request_ = request;
    });
}

bool RevisionController::exportCopy(const QString& plan, const QString& name) {
    return perform([&] {
        if (!request_ || plan.isEmpty() || plan != plan_id_)
            throw std::invalid_argument("Review this exact save and mapping before exporting.");
        const auto path = export_save_migration(archive_->root(), *request_, plan.toStdString(), name.toStdString());
        reset_migration();
        exported_path_ = QString::fromStdU16String(path.generic_u16string());
    });
}
} // namespace pgp::studio
