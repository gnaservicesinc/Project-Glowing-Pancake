#pragma once

#include <pgp/core/simulation.hpp>

#include <QAbstractListModel>

#include <memory>
#include <vector>

namespace pgp::bridge {

// One persistent model per campaign. Authoritative integers cross QML as
// decimal strings; presentation must never round a 64-bit ID or cent amount.
class ReservationModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool sortByName READ sortByName WRITE setSortByName NOTIFY sortByNameChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ReservedCentsRole,
        ConsumedCentsRole,
        StatusRole,
        ReversibleRole
    };
    Q_ENUM(Role)

    explicit ReservationModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString revision() const;
    [[nodiscard]] bool sortByName() const noexcept { return sort_by_name_; }
    void setSortByName(bool enabled);

    // Apply only newer snapshots on the owning GUI thread. Coalescing row
    // snapshots is safe; command receipts/required choices use separate queues.
    // Returns false for an obsolete or already applied revision.
    bool applySnapshot(std::shared_ptr<const core::SimulationSnapshot> snapshot);
    // The sole reset path: the owner explicitly starts a different campaign.
    void beginCampaign(std::shared_ptr<const core::SimulationSnapshot> snapshot);

signals:
    void revisionChanged();
    void sortByNameChanged();

private:
    void requireOwningThread() const;
    void reconcile(std::vector<core::ReservationRow> desired);
    void sortRows(std::vector<core::ReservationRow>& rows) const;
    static void validate(const core::SimulationSnapshot& snapshot);

    std::shared_ptr<const core::SimulationSnapshot> snapshot_;
    std::vector<core::ReservationRow> rows_;
    bool sort_by_name_ = false;
};

} // namespace pgp::bridge
