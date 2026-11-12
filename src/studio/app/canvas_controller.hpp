#pragma once
#include <pgp/studio/flow_graph.hpp>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace pgp::studio {
class CanvasController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY graphChanged)
    Q_PROPERTY(QVariantList edges READ edges NOTIFY graphChanged)
    Q_PROPERTY(QVariantMap nodeLookup READ nodeLookup NOTIFY graphChanged)
    Q_PROPERTY(QVariantList palette READ palette NOTIFY portChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY graphChanged)
    Q_PROPERTY(QString ticket READ ticket NOTIFY graphChanged)
    Q_PROPERTY(QString selectedKey READ selectedKey NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPort READ selectedPort NOTIFY portChanged)
    Q_PROPERTY(QString pendingOutput READ pendingOutput NOTIFY portChanged)
    Q_PROPERTY(QString portLabel READ portLabel NOTIFY portChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool layoutDirty READ layoutDirty NOTIFY layoutChanged)
    Q_PROPERTY(bool hasLayoutEdits READ hasLayoutEdits NOTIFY layoutChanged)
    Q_PROPERTY(bool canUndoLayout READ canUndoLayout NOTIFY layoutChanged)
    Q_PROPERTY(bool canRedoLayout READ canRedoLayout NOTIFY layoutChanged)
    Q_PROPERTY(QVariantMap view READ view NOTIFY viewChanged)
public:
    CanvasController();
    ~CanvasController() override;
    void set_document(DraftDocument* document, std::string flow, const std::filesystem::path& checkout);
    void set_selection(const QString& row, bool from_outline = false);
    [[nodiscard]] QString restored_row() const;
    void remember_selection();
    void discard_layouts();
    [[nodiscard]] const QVariantList& nodes() const { return nodes_; }
    [[nodiscard]] const QVariantList& edges() const { return edges_; }
    [[nodiscard]] const QVariantMap& nodeLookup() const { return lookup_; }
    [[nodiscard]] const QVariantList& palette() const { return palette_; }
    [[nodiscard]] const QStringList& warnings() const { return warnings_; }
    [[nodiscard]] QString ticket() const { return QString::number(static_cast<qulonglong>(generation_)); }
    [[nodiscard]] QString selectedKey() const { return selected_; }
    [[nodiscard]] QString selectedPort() const { return port_; }
    [[nodiscard]] QString pendingOutput() const { return pending_; }
    [[nodiscard]] QString portLabel() const { return port_label_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] bool layoutDirty() const { return layout_ && layout_->dirty(); }
    [[nodiscard]] bool hasLayoutEdits() const;
    [[nodiscard]] bool canUndoLayout() const { return layout_ && layout_->can_undo(); }
    [[nodiscard]] bool canRedoLayout() const { return layout_ && layout_->can_redo(); }
    [[nodiscard]] QVariantMap view() const;
    Q_INVOKABLE void selectNode(QString key);
    Q_INVOKABLE bool pickPort(QString ticket, QString port);
    Q_INVOKABLE void cancelConnection();
    Q_INVOKABLE bool prepareInsert();
    Q_INVOKABLE bool insertNode(QString ticket, QString type);
    Q_INVOKABLE bool moveNode(QString ticket, QString key, double x, double y);
    Q_INVOKABLE void autoLayout();
    Q_INVOKABLE void undoLayout();
    Q_INVOKABLE void redoLayout();
    Q_INVOKABLE void saveLayout();
    Q_INVOKABLE void reloadLayout();
    Q_INVOKABLE void rememberViewport(double x, double y, double zoom_permille);
    Q_INVOKABLE void previewViewport(double x, double y, double zoom_permille);
signals:
    void graphChanged();
    void selectionChanged();
    void portChanged();
    void messageChanged();
    void layoutChanged();
    void viewChanged();
    void documentEdited();
    void selectionRequested(QString row);
private:
    void rebuild();
    void refresh_palette();
    void flush_view();
    bool verify(const QString& ticket);
    void fail(const std::exception& error);
    DraftDocument* document_ = nullptr;
    std::string flow_;
    std::filesystem::path root_, source_;
    std::uint64_t revision_ = 0, generation_ = 0;
    std::optional<FlowGraph> graph_;
    std::map<std::string, std::unique_ptr<LayoutDocument>, std::less<>> layouts_;
    LayoutDocument* layout_ = nullptr;
    CanvasView view_;
    bool view_dirty_ = false;
    bool view_restored_ = false;
    QTimer view_timer_;
    QVariantList nodes_, edges_, palette_;
    QVariantMap lookup_;
    QStringList warnings_;
    QString selected_, port_, pending_, port_label_, message_;
};
} // namespace pgp::studio
