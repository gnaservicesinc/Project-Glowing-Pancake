#include "reservation_model.hpp"

#include <QThread>

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace pgp::bridge {
namespace {
QString decimal(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString cents(core::Cents value) { return QString::number(static_cast<qlonglong>(value.value())); }
QString name(const core::ReservationRow& row) { return QString::fromStdString(row.name); }
}

ReservationModel::ReservationModel(QObject* parent) : QAbstractListModel(parent) {}

int ReservationModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant ReservationModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.model() != this || index.column() != 0 ||
        index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case IdRole: return decimal(row.id);
    case NameRole: return name(row);
    case ReservedCentsRole: return cents(row.reserved);
    case ConsumedCentsRole: return cents(row.consumed);
    case StatusRole: return QString::fromUtf8(core::to_string(row.status).data(),
                                            static_cast<qsizetype>(core::to_string(row.status).size()));
    case ReversibleRole: return row.reversible;
    default: return {};
    }
}

QHash<int, QByteArray> ReservationModel::roleNames() const {
    return {{IdRole, "entityId"}, {NameRole, "name"}, {ReservedCentsRole, "reservedCents"},
            {ConsumedCentsRole, "consumedCents"}, {StatusRole, "status"},
            {ReversibleRole, "reversible"}};
}

QString ReservationModel::revision() const { return snapshot_ ? decimal(snapshot_->revision) : QString{}; }

void ReservationModel::requireOwningThread() const {
    if (QThread::currentThread() != thread())
        throw std::logic_error("Snapshot diffs must run on the model's owning GUI thread");
}

void ReservationModel::validate(const core::SimulationSnapshot& snapshot) {
    if (snapshot.reservations.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Too many presentation rows");
    std::set<std::uint64_t> ids;
    for (const auto& row : snapshot.reservations) {
        if (row.id == 0 || !ids.insert(row.id).second)
            throw std::invalid_argument("Snapshot rows require distinct nonzero entity IDs");
    }
}

void ReservationModel::sortRows(std::vector<core::ReservationRow>& rows) const {
    std::sort(rows.begin(), rows.end(), [this](const auto& left, const auto& right) {
        if (sort_by_name_ && left.name != right.name) return left.name < right.name;
        return left.id < right.id;
    });
}

void ReservationModel::reconcile(std::vector<core::ReservationRow> desired) {
    sortRows(desired);
    std::set<std::uint64_t> keep;
    for (const auto& row : desired) keep.insert(row.id);
    for (int index = rowCount() - 1; index >= 0; --index) {
        if (!keep.contains(rows_[static_cast<std::size_t>(index)].id)) {
            beginRemoveRows({}, index, index);
            rows_.erase(rows_.begin() + index);
            endRemoveRows();
        }
    }
    for (std::size_t i = 0; i < desired.size(); ++i) {
        const auto position = std::find_if(rows_.begin() + static_cast<std::ptrdiff_t>(i), rows_.end(),
            [&](const auto& row) { return row.id == desired[i].id; });
        const auto target = static_cast<int>(i);
        if (position == rows_.end()) {
            beginInsertRows({}, target, target);
            rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(i), desired[i]);
            endInsertRows();
        } else {
            const auto source = static_cast<int>(position - rows_.begin());
            if (source != target) {
                if (!beginMoveRows({}, source, source, {}, target))
                    throw std::logic_error("Invalid stable-row move");
                const auto moving = *position;
                rows_.erase(position);
                rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(i), moving);
                endMoveRows();
            }
            auto& current = rows_[i];
            QList<int> changed;
            if (current.name != desired[i].name) changed.push_back(NameRole);
            if (current.reserved != desired[i].reserved) changed.push_back(ReservedCentsRole);
            if (current.consumed != desired[i].consumed) changed.push_back(ConsumedCentsRole);
            if (current.status != desired[i].status) changed.push_back(StatusRole);
            if (current.reversible != desired[i].reversible) changed.push_back(ReversibleRole);
            if (!changed.empty()) {
                current = desired[i];
                emit dataChanged(index(target), index(target), changed);
            }
        }
    }
}

bool ReservationModel::applySnapshot(std::shared_ptr<const core::SimulationSnapshot> snapshot) {
    requireOwningThread();
    if (!snapshot) throw std::invalid_argument("A snapshot is required");
    if (snapshot_ && snapshot->revision <= snapshot_->revision) return false;
    validate(*snapshot);
    reconcile(snapshot->reservations);
    snapshot_ = std::move(snapshot);
    emit revisionChanged();
    return true;
}

void ReservationModel::beginCampaign(std::shared_ptr<const core::SimulationSnapshot> snapshot) {
    requireOwningThread();
    if (!snapshot) throw std::invalid_argument("A snapshot is required");
    validate(*snapshot);
    auto desired = snapshot->reservations;
    sortRows(desired);
    beginResetModel();
    rows_ = std::move(desired);
    snapshot_ = std::move(snapshot);
    endResetModel();
    emit revisionChanged();
}

void ReservationModel::setSortByName(bool enabled) {
    requireOwningThread();
    if (enabled == sort_by_name_) return;
    sort_by_name_ = enabled;
    reconcile(rows_);
    emit sortByNameChanged();
}
} // namespace pgp::bridge
