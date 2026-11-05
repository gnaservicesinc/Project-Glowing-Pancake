#include "campaign_model.hpp"

#include <QThread>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace pgp::bridge {
StableListModel::StableListModel(QObject* parent, bool required_first)
    : QAbstractListModel(parent), required_first_(required_first) {}
int StableListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}
QVariant StableListModel::data(const QModelIndex& item, int role) const {
    if (!item.isValid() || item.model() != this || item.column() != 0 || item.row() < 0 || item.row() >= rowCount()) return {};
    const auto& value = rows_[static_cast<std::size_t>(item.row())];
    switch (role) {
    case FieldsRole: return value.fields;
    case IdRole: return value.entity_id;
    case TitleRole: return value.title;
    case SubtitleRole: return value.subtitle;
    case DetailRole: return value.detail;
    case StatusRole: return value.status;
    case RequiredRole: case ActionableRole: return value.required;
    default: return {};
    }
}
QHash<int,QByteArray> StableListModel::roleNames() const {
    return {{IdRole,"entityId"},{TitleRole,"title"},{SubtitleRole,"subtitle"},
        {DetailRole,"detail"},{StatusRole,"status"},{RequiredRole,"required"},{ActionableRole,"actionable"},{FieldsRole,"fields"}};
}
int StableListModel::indexOf(const QString& id) const {
    const auto found = std::find_if(rows_.begin(),rows_.end(),[&](const auto& value) { return value.entity_id == id; });
    return found == rows_.end() ? -1 : static_cast<int>(found - rows_.begin());
}
QString StableListModel::idAt(int position) const {
    return position < 0 || position >= rowCount() ? QString{} : rows_[static_cast<std::size_t>(position)].entity_id;
}
QVariantMap StableListModel::row(const QString& id) const {
    const auto position = indexOf(id);
    if (position < 0) return {};
    const auto& value = rows_[static_cast<std::size_t>(position)];
    return {{QStringLiteral("entityId"),value.entity_id},{QStringLiteral("title"),value.title},
        {QStringLiteral("subtitle"),value.subtitle},{QStringLiteral("detail"),value.detail},
        {QStringLiteral("status"),value.status},{QStringLiteral("required"),value.required},
        {QStringLiteral("actionable"),value.required},{QStringLiteral("fields"),value.fields}};
}
void StableListModel::requireOwningThread() const {
    if (QThread::currentThread() != thread()) throw std::logic_error("Campaign models require their owning GUI thread");
}
void StableListModel::validateRows(const std::vector<CampaignRow>& rows) {
    if (rows.size() > 65536) throw std::invalid_argument("Campaign presentation collection exceeds its bound");
    std::set<QString> ids;
    for (const auto& value : rows) {
        if (value.entity_id.isEmpty() || value.entity_id.size() > 256 ||
            value.entity_id.contains(QChar{0}) || !ids.insert(value.entity_id).second ||
            value.title.isEmpty() || value.title.size() > 4096 || value.subtitle.size() > 4096 ||
            value.detail.size() > 16384 || value.status.size() > 256)
            throw std::invalid_argument("Campaign presentation rows require bounded text and distinct stable identities");
    }
}
std::vector<CampaignRow> StableListModel::visibleRows(const std::vector<CampaignRow>& rows) const {
    std::vector<CampaignRow> visible;
    for (const auto& value : rows) {
        if (required_only_ && !value.required) continue;
        if (!filter_text_.isEmpty() && !value.title.contains(filter_text_,Qt::CaseInsensitive) &&
            !value.subtitle.contains(filter_text_,Qt::CaseInsensitive) && !value.status.contains(filter_text_,Qt::CaseInsensitive)) continue;
        visible.push_back(value);
    }
    std::sort(visible.begin(),visible.end(),[&](const auto& left,const auto& right) {
        if (required_first_ && left.required != right.required) return left.required;
        if (sort_by_title_ && left.title != right.title) return left.title < right.title;
        return left.entity_id < right.entity_id;
    });
    return visible;
}
void StableListModel::repairContext(const QString& old_selected, const QVariantMap& old_row,
                                   const QString& old_anchor, qreal old_offset) {
    if (indexOf(selected_id_) < 0) selected_id_ = rows_.empty() ? QString{} : rows_.front().entity_id;
    if (indexOf(anchor_id_) < 0) {
        anchor_id_ = rows_.empty() ? QString{} : rows_.front().entity_id;
        anchor_offset_ = 0;
    }
    if (selected_id_ != old_selected) emit selectedIdChanged();
    if (selected_id_ != old_selected || selectedRow() != old_row) emit selectedRowChanged();
    if (anchor_id_ != old_anchor || anchor_offset_ != old_offset) emit anchorChanged();
}
void StableListModel::reconcile(std::vector<CampaignRow> desired) {
    if (desired == rows_) return;
    const auto old_selected = selected_id_;
    const auto old_row = selectedRow();
    const auto old_anchor = anchor_id_;
    const auto old_offset = anchor_offset_;
    const auto old_count = rowCount();
    emit aboutToReconcile();
    std::set<QString> keep;
    for (const auto& value : desired) keep.insert(value.entity_id);
    for (int position = rowCount() - 1; position >= 0; --position) {
        if (keep.contains(rows_[static_cast<std::size_t>(position)].entity_id)) continue;
        beginRemoveRows({},position,position);
        rows_.erase(rows_.begin() + position);
        endRemoveRows();
    }
    for (std::size_t i = 0; i < desired.size(); ++i) {
        const auto found = std::find_if(rows_.begin() + static_cast<std::ptrdiff_t>(i),rows_.end(),
            [&](const auto& value) { return value.entity_id == desired[i].entity_id; });
        const auto target = static_cast<int>(i);
        if (found == rows_.end()) {
            beginInsertRows({},target,target);
            rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(i),desired[i]);
            endInsertRows();
        } else {
            const auto from = static_cast<int>(found - rows_.begin());
            if (from != target) {
                if (!beginMoveRows({},from,from,{},target)) throw std::logic_error("Invalid campaign stable-row move");
                auto moving = std::move(*found);
                rows_.erase(found);
                rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(i),std::move(moving));
                endMoveRows();
            }
            auto& current = rows_[i];
            QList<int> changed;
            if (current.title != desired[i].title) changed.push_back(TitleRole);
            if (current.subtitle != desired[i].subtitle) changed.push_back(SubtitleRole);
            if (current.detail != desired[i].detail) changed.push_back(DetailRole);
            if (current.status != desired[i].status) changed.push_back(StatusRole);
            if (current.fields != desired[i].fields) changed.push_back(FieldsRole);
            if (current.required != desired[i].required) { changed.push_back(RequiredRole); changed.push_back(ActionableRole); }
            if (!changed.empty()) {
                current = std::move(desired[i]);
                emit dataChanged(index(target),index(target),changed);
            }
        }
    }
    repairContext(old_selected,old_row,old_anchor,old_offset);
    if (rowCount() != old_count) emit countChanged();
    emit reconciled();
}
void StableListModel::setRows(std::vector<CampaignRow> rows) {
    requireOwningThread();
    validateRows(rows);
    auto visible = visibleRows(rows);
    source_rows_ = std::move(rows);
    reconcile(std::move(visible));
}
void StableListModel::beginCampaign(std::vector<CampaignRow> rows) {
    requireOwningThread();
    validateRows(rows);
    auto visible = visibleRows(rows);
    const auto old_count = rowCount();
    const auto old_selected = selected_id_;
    const auto old_row = selectedRow();
    const auto old_anchor = anchor_id_;
    const auto old_offset = anchor_offset_;
    emit aboutToReconcile();
    beginResetModel();
    source_rows_ = std::move(rows); rows_ = std::move(visible);
    selected_id_ = rows_.empty() ? QString{} : rows_.front().entity_id;
    anchor_id_ = selected_id_; anchor_offset_ = 0;
    endResetModel();
    repairContext(old_selected,old_row,old_anchor,old_offset);
    if (rowCount() != old_count) emit countChanged();
    emit reconciled();
}
void StableListModel::setSortByTitle(bool value) {
    requireOwningThread();
    if (value == sort_by_title_) return;
    sort_by_title_ = value;
    reconcile(visibleRows(source_rows_));
    emit sortByTitleChanged();
}
void StableListModel::setFilterText(QString value) {
    requireOwningThread();
    value = value.left(4096);
    if (value == filter_text_) return;
    filter_text_ = std::move(value);
    reconcile(visibleRows(source_rows_));
    emit filterTextChanged();
}
void StableListModel::setRequiredOnly(bool value) {
    requireOwningThread();
    if (value == required_only_) return;
    required_only_ = value;
    reconcile(visibleRows(source_rows_));
    emit requiredOnlyChanged();
}
void StableListModel::setSelectedId(QString id) {
    requireOwningThread();
    if (id == selected_id_ || (!id.isEmpty() && indexOf(id) < 0)) return;
    selected_id_ = std::move(id);
    emit selectedIdChanged(); emit selectedRowChanged();
}
void StableListModel::setAnchorId(QString id) {
    requireOwningThread();
    if (id == anchor_id_ || (!id.isEmpty() && indexOf(id) < 0)) return;
    anchor_id_ = std::move(id);
    emit anchorChanged();
}
void StableListModel::setAnchorOffset(qreal offset) {
    requireOwningThread();
    if (!std::isfinite(offset) || std::abs(offset) > 1'000'000 || offset == anchor_offset_) return;
    anchor_offset_ = offset;
    emit anchorChanged();
}
}
