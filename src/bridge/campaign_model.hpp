#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

#include <vector>

namespace pgp::bridge {
struct CampaignRow {
    QString entity_id;
    QString title;
    QString subtitle;
    QString detail;
    QString status;
    bool required = false;
    bool operator==(const CampaignRow&) const = default;
};

// Project and inbox models share the same small presentation row. Authoritative
// numeric values have already been formatted losslessly by the adapter.
class StableListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool sortByTitle READ sortByTitle WRITE setSortByTitle NOTIFY sortByTitleChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool requiredOnly READ requiredOnly WRITE setRequiredOnly NOTIFY requiredOnlyChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(QVariantMap selectedRow READ selectedRow NOTIFY selectedRowChanged)
    Q_PROPERTY(QString anchorId READ anchorId WRITE setAnchorId NOTIFY anchorChanged)
    Q_PROPERTY(qreal anchorOffset READ anchorOffset WRITE setAnchorOffset NOTIFY anchorChanged)
public:
    enum Role {
        IdRole = Qt::UserRole + 1, TitleRole, SubtitleRole, DetailRole,
        StatusRole, RequiredRole, ActionableRole
    };
    Q_ENUM(Role)
    explicit StableListModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int,QByteArray> roleNames() const override;
    [[nodiscard]] bool sortByTitle() const noexcept { return sort_by_title_; }
    [[nodiscard]] QString filterText() const { return filter_text_; }
    [[nodiscard]] bool requiredOnly() const noexcept { return required_only_; }
    [[nodiscard]] QString selectedId() const { return selected_id_; }
    [[nodiscard]] QVariantMap selectedRow() const { return row(selected_id_); }
    [[nodiscard]] QString anchorId() const { return anchor_id_; }
    [[nodiscard]] qreal anchorOffset() const noexcept { return anchor_offset_; }
    void setSortByTitle(bool value);
    void setFilterText(QString value);
    void setRequiredOnly(bool value);
    void setSelectedId(QString id);
    void setAnchorId(QString id);
    void setAnchorOffset(qreal offset);
    void setRows(std::vector<CampaignRow> rows);
    void beginCampaign(std::vector<CampaignRow> rows);
    Q_INVOKABLE [[nodiscard]] int indexOf(const QString& id) const;
    Q_INVOKABLE [[nodiscard]] QVariantMap row(const QString& id) const;
    Q_INVOKABLE [[nodiscard]] QString idAt(int index) const;
signals:
    void countChanged();
    void sortByTitleChanged();
    void filterTextChanged();
    void requiredOnlyChanged();
    void selectedIdChanged();
    void selectedRowChanged();
    void anchorChanged();
    // Consumers may capture/restore the viewport with a stable row ID and pixel
    // offset; never retain a numeric row index across inserts or moves.
    void aboutToReconcile();
    void reconciled();
private:
    void requireOwningThread() const;
    static void validateRows(const std::vector<CampaignRow>& rows);
    [[nodiscard]] std::vector<CampaignRow> visibleRows(const std::vector<CampaignRow>& rows) const;
    void reconcile(std::vector<CampaignRow> desired);
    void repairContext(const QString& old_selected, const QVariantMap& old_row,
                       const QString& old_anchor, qreal old_offset);
    std::vector<CampaignRow> source_rows_;
    std::vector<CampaignRow> rows_;
    bool sort_by_title_ = false;
    QString filter_text_;
    bool required_only_ = false;
    QString selected_id_;
    QString anchor_id_;
    qreal anchor_offset_ = 0;
};
}
