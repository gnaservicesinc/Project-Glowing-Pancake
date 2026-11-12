#include "snapshot_controller.hpp"
#include <pgp/studio/library.hpp>
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <QRegularExpression>
#include <set>

namespace pgp::studio {
namespace {
using Json = nlohmann::json;
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString value_text(const Json& value) {
    if (value.is_null()) return QStringLiteral("(absent)");
    if (!value.is_string()) return text(value.dump(2));
    const auto copy = text(value.get_ref<const std::string&>());
    static const QRegularExpression digest(QStringLiteral("^[a-fA-F0-9]{64}$"));
    return digest.match(copy).hasMatch() ? QStringLiteral("Recorded fingerprint") : copy;
}
// Exact integer strings, never QJson's floating-point conversion of game units.
void differences(const Json& before, const Json& after, const QString& path, QVariantList& rows) {
    if (before == after) return;
    if ((before.is_object() && after.is_object()) || (before.is_null() && after.is_object()) || (before.is_object() && after.is_null())) {
        std::set<std::string> keys;
        for (const auto& [key, value] : before.items()) { (void)value; keys.insert(key); }
        for (const auto& [key, value] : after.items()) { (void)value; keys.insert(key); }
        for (const auto& key : keys) differences(before.is_object() ? before.value(key, Json{}) : Json{}, after.is_object() ? after.value(key, Json{}) : Json{}, path.isEmpty() ? text(key) : path + QChar{'/'} + text(key), rows);
    } else if ((before.is_array() && after.is_array()) || (before.is_null() && after.is_array()) || (before.is_array() && after.is_null())) {
        const auto count = std::max(before.size(), after.size());
        for (std::size_t i = 0; i < count; ++i) differences(before.is_array() && i < before.size() ? before.at(i) : Json{}, after.is_array() && i < after.size() ? after.at(i) : Json{}, path + QStringLiteral("/%1").arg(i + 1), rows);
    } else rows.push_back(QVariantMap{{QStringLiteral("path"), path}, {QStringLiteral("before"), value_text(before)}, {QStringLiteral("after"), value_text(after)}});
}
QString suggest(const std::string& version) {
    const auto match = QRegularExpression(QStringLiteral("^([0-9]+)\\.([0-9]+)\\.([0-9]+)(?:-|$)")).match(text(version));
    if (!match.hasMatch() || match.captured(2).toULongLong() >= 2147483647U) return {};
    return QStringLiteral("%1.%2.0-dev.1").arg(match.captured(1)).arg(match.captured(2).toULongLong() + 1);
}
}
void SnapshotController::set_context(QString checkout, QString working_file, std::uint64_t revision, bool content_dirty, bool layout_dirty) {
    const bool root_changed = checkout_ != checkout;
    const bool document_changed = root_changed || working_ != working_file || revision_ != revision || content_dirty_ != content_dirty;
    const bool layout_changed = layout_dirty_ != layout_dirty;
    checkout_ = std::move(checkout); working_ = std::move(working_file); revision_ = revision;
    content_dirty_ = content_dirty; layout_dirty_ = layout_dirty;
    if (root_changed) { sources_.clear(); preferred_.clear(); suggested_.clear(); status_.clear(); pending_ = false; outcome_.clear(); }
    if (document_changed) { reset_review(); message_.clear(); }
    if (document_changed || layout_changed) emit changed();
}
QString SnapshotController::blocker() const {
    if (content_dirty_) return QStringLiteral("Save the working content as a draft before reviewing a batch.");
    if (layout_dirty_) return QStringLiteral("Layout moves are excluded from this batch. Save or undo them before a batch that writes files, then review again if needed.");
    return {};
}
void SnapshotController::reset_review() {
    plan_id_.clear(); versions_.clear(); validation_.clear(); release_.clear();
    changes_.clear(); texts_.clear(); collections_.clear(); writes_.clear(); notices_.clear(); runtime_changed_ = false;
}
void SnapshotController::clearReview() { reset_review(); message_.clear(); emit changed(); }
void SnapshotController::fail(const std::exception& error) {
    if (const auto* value = dynamic_cast<const persistence::PersistenceError*>(&error)) message_ = text(value->code()) + QStringLiteral(": ") + text(error.what());
    else message_ = text(error.what());
}
void SnapshotController::read_status() {
    pending_ = false;
    try {
        const auto result = check_snapshot(std::filesystem::path{checkout_.toStdU16String()});
        status_ = QStringLiteral("Current content %1 · generation %2\nRequired game %3 · %4.")
            .arg(text(result.content_version)).arg(result.generation).arg(text(result.required_game_version),
                result.game_version_requirement_resolved ? QStringLiteral("version requirement satisfied") : QStringLiteral("version requirement unresolved"));
        suggested_ = suggest(result.content_version);
    } catch (const persistence::PersistenceError& error) {
        pending_ = error.code() == "pending_snapshot";
        status_ = text(error.code()) + QStringLiteral(": ") + text(error.what());
    } catch (const std::exception& error) { status_ = text(error.what()); }
}
void SnapshotController::refresh() {
    reset_review(); message_.clear(); sources_.clear(); suggested_.clear();
    try {
        if (checkout_.isEmpty()) throw std::invalid_argument("Open a development checkout first.");
        const auto library = inspect_checkout(std::filesystem::path{checkout_.toStdU16String()});
        for (const auto& document : library.documents)
            if (document.validation == Validation::validated && document.origin != Origin::engineering_fixture)
                sources_.push_back(QString::fromStdU16String(document.relative_path.generic_u16string()));
        preferred_ = sources_.contains(working_) ? working_ : sources_.contains(QStringLiteral("data/runtime/first-playable.json")) ? QStringLiteral("data/runtime/first-playable.json") : sources_.value(0);
        read_status();
    } catch (const std::exception& error) { fail(error); }
    emit changed();
}
bool SnapshotController::review(const QString& source, const QString& version, const QString& summary, const QString& game_version) {
    reset_review(); message_.clear(); outcome_.clear();
    try {
        if (content_dirty_) throw std::invalid_argument(blocker().toStdString());
        if (!sources_.contains(source)) throw std::invalid_argument("Select a saved runtime or draft collection from this checkout.");
        request_ = {}; request_.source = std::filesystem::path{source.toStdU16String()};
        request_.content_version = version.trimmed().toStdString(); request_.summary = summary.trimmed().toStdString();
        if (!game_version.trimmed().isEmpty()) request_.game_version = game_version.trimmed().toStdString();
        const auto review = review_snapshot(std::filesystem::path{checkout_.toStdU16String()}, request_);
        const auto report = Json::parse(review.report_json);
        runtime_changed_ = review.runtime_changed;
        for (const auto& change : report.at("changes")) {
            QVariantList fields; differences(change.at("before"), change.at("after"), {}, fields);
            changes_.push_back(QVariantMap{{QStringLiteral("id"), text(change.at("id").get<std::string>())}, {QStringLiteral("kind"), text(change.at("kind").get<std::string>())},
                {QStringLiteral("change"), text(change.at("change").get<std::string>())}, {QStringLiteral("oldRevision"), value_text(change.at("old_revision"))},
                {QStringLiteral("revision"), value_text(change.at("revision"))}, {QStringLiteral("fields"), fields}});
        }
        for (const auto& change : report.at("text_changes")) texts_.push_back(QVariantMap{{QStringLiteral("path"), text(change.at("key").get<std::string>())},
            {QStringLiteral("before"), value_text(change.at("before"))}, {QStringLiteral("after"), value_text(change.at("after"))}});
        for (const auto& change : report.at("collection_changes")) differences(change.at("before"), change.at("after"), text(change.at("path").get<std::string>()), collections_);
        for (const auto& file : report.at("writes")) writes_.push_back(QVariantMap{{QStringLiteral("path"), text(file.at("path").get<std::string>())},
            {QStringLiteral("before"), file.at("before_hash").is_null() ? QStringLiteral("New file") : QStringLiteral("Existing file")},
            {QStringLiteral("after"), QStringLiteral("Update on finalize")}});
        const auto& before = report.at("versions_before"); const auto& after = report.at("versions_after");
        versions_ = QStringLiteral("Content %1 → %2\nGame %3 → %4\nEditor %5 → %6\n%7. %8 file(s) proposed.")
            .arg(value_text(before.at("content")), value_text(after.at("content")), value_text(before.at("game")), value_text(after.at("game")), value_text(before.at("editor")), value_text(after.at("editor")),
                runtime_changed_ ? QStringLiteral("Runtime changes; one revision increment per changed definition") : QStringLiteral("No runtime semantic change; no content/revision/history increment")).arg(writes_.size());
        const auto& validation = report.at("validation");
        validation_ = QStringLiteral("Shared game compiler and runtime · %1 scenario(s) · %2 prepared headquarters · %3 exact save roundtrips.")
            .arg(value_text(validation.at("scenarios")), value_text(validation.at("prepared_headquarters")), value_text(validation.at("exact_prepared_save_roundtrips")));
        for (const auto& lint : validation.at("lints")) notices_.push_back(text(lint.at("code").get<std::string>() + " · " + lint.at("path").get<std::string>() + " · " + lint.at("explanation").get<std::string>()));
        const auto& result = report.at("result");
        release_ = QStringLiteral("Required game version %1 · %2.\nOrdinary loads require exact content identity. Compare finalized revisions to validate and export an individual save copy.\nNative package and release qualification remain open.")
            .arg(value_text(result.at("required_game_version")), result.at("game_version_requirement_resolved").get<bool>() ? QStringLiteral("version requirement satisfied") : QStringLiteral("version requirement unresolved"));
        plan_id_ = text(review.plan_id);
        emit changed(); return true;
    } catch (const std::exception& error) { reset_review(); fail(error); read_status(); emit changed(); return false; }
}
bool SnapshotController::finalizeReview() {
    if (!canFinalize()) { message_ = blocker().isEmpty() ? QStringLiteral("Review this saved batch before finalizing.") : blocker(); emit changed(); return false; }
    const auto id = plan_id_.toStdString(); const bool writing = !writes_.isEmpty();
    reset_review(); message_.clear();
    try {
        const auto result = finalize_snapshot(std::filesystem::path{checkout_.toStdU16String()}, request_, id);
        outcome_ = QStringLiteral("%1 Content %2 · generation %3\nRequired game %4 · %5.")
            .arg(writing ? QStringLiteral("Finalized internal batch.") : QStringLiteral("No-op verified; files and versions are unchanged."), text(result.content_version))
            .arg(result.generation).arg(text(result.required_game_version), result.game_version_requirement_resolved ? QStringLiteral("version requirement satisfied") : QStringLiteral("version requirement unresolved"));
        read_status();
        if (writing) emit finalized(QString::fromStdU16String(request_.target.generic_u16string()));
        emit changed(); return true;
    } catch (const std::exception& error) { fail(error); read_status(); emit changed(); return false; }
}
bool SnapshotController::recover() {
    reset_review(); message_.clear(); outcome_.clear();
    try {
        outcome_ = text(recover_snapshot(std::filesystem::path{checkout_.toStdU16String()}));
        read_status(); emit recovered(); emit changed(); return true;
    } catch (const std::exception& error) { fail(error); read_status(); emit changed(); return false; }
}
}
