#pragma once

#include <pgp/studio/snapshot.hpp>
#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace pgp::studio {
// A graphical adapter only: the Qt-free writer owns validation, plans and writes.
class SnapshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources NOTIFY changed)
    Q_PROPERTY(QString preferredSource READ preferredSource NOTIFY changed)
    Q_PROPERTY(QString suggestedVersion READ suggestedVersion NOTIFY changed)
    Q_PROPERTY(QString currentStatus READ currentStatus NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString blocker READ blocker NOTIFY changed)
    Q_PROPERTY(QString planId READ planId NOTIFY changed)
    Q_PROPERTY(QString versionSummary READ versionSummary NOTIFY changed)
    Q_PROPERTY(QString validationSummary READ validationSummary NOTIFY changed)
    Q_PROPERTY(QString releaseSummary READ releaseSummary NOTIFY changed)
    Q_PROPERTY(QString outcome READ outcome NOTIFY changed)
    Q_PROPERTY(QVariantList changes READ changes NOTIFY changed)
    Q_PROPERTY(QVariantList textChanges READ textChanges NOTIFY changed)
    Q_PROPERTY(QVariantList collectionChanges READ collectionChanges NOTIFY changed)
    Q_PROPERTY(QVariantList writes READ writes NOTIFY changed)
    Q_PROPERTY(QStringList notices READ notices NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool canFinalize READ canFinalize NOTIFY changed)
    Q_PROPERTY(bool pendingRecovery READ pendingRecovery NOTIFY changed)
    Q_PROPERTY(bool runtimeChanged READ runtimeChanged NOTIFY changed)
public:
    void set_context(QString checkout, QString working_file, std::uint64_t revision, bool content_dirty, bool layout_dirty);
    [[nodiscard]] const QStringList& sources() const noexcept { return sources_; }
    [[nodiscard]] QString preferredSource() const { return preferred_; }
    [[nodiscard]] QString suggestedVersion() const { return suggested_; }
    [[nodiscard]] QString currentStatus() const { return status_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] QString blocker() const;
    [[nodiscard]] QString planId() const { return plan_id_; }
    [[nodiscard]] QString versionSummary() const { return versions_; }
    [[nodiscard]] QString validationSummary() const { return validation_; }
    [[nodiscard]] QString releaseSummary() const { return release_; }
    [[nodiscard]] QString outcome() const { return outcome_; }
    [[nodiscard]] const QVariantList& changes() const noexcept { return changes_; }
    [[nodiscard]] const QVariantList& textChanges() const noexcept { return texts_; }
    [[nodiscard]] const QVariantList& collectionChanges() const noexcept { return collections_; }
    [[nodiscard]] const QVariantList& writes() const noexcept { return writes_; }
    [[nodiscard]] const QStringList& notices() const noexcept { return notices_; }
    [[nodiscard]] bool ready() const noexcept { return !plan_id_.isEmpty(); }
    [[nodiscard]] bool canFinalize() const noexcept { return ready() && !content_dirty_ && (!layout_dirty_ || writes_.isEmpty()); }
    [[nodiscard]] bool pendingRecovery() const noexcept { return pending_; }
    [[nodiscard]] bool runtimeChanged() const noexcept { return runtime_changed_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearReview();
    Q_INVOKABLE bool review(const QString& source, const QString& version, const QString& summary, const QString& game_version);
    Q_INVOKABLE bool finalizeReview();
    Q_INVOKABLE bool recover();
signals:
    void changed();
    void finalized(const QString& target);
    void recovered();
private:
    void reset_review();
    void read_status();
    void fail(const std::exception& error);
    QString checkout_, working_, preferred_, suggested_, status_, message_, outcome_;
    QString plan_id_, versions_, validation_, release_;
    QStringList sources_, notices_;
    QVariantList changes_, texts_, collections_, writes_;
    SnapshotRequest request_;
    std::uint64_t revision_ = 0;
    bool content_dirty_ = false, layout_dirty_ = false, pending_ = false, runtime_changed_ = false;
};
}
