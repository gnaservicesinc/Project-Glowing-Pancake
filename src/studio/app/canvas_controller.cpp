#include "canvas_controller.hpp"
#include <algorithm>
#include <cmath>

namespace pgp::studio {
namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString type_name(const GraphPort& port) {
    const auto scope = text(content::scope_name(port.scope));
    if (port.type == content::PortType::execution) return QStringLiteral("Execution · ") + scope;
    if (port.type == content::PortType::condition) return QStringLiteral("True / false · ") + scope;
    if (port.type == content::PortType::scope) return QStringLiteral("Entity · ") + scope;
    QString unit;
    switch (port.unit) {
    case content::Unit::constant_2014_cents: unit = QStringLiteral("2014 cents"); break;
    case content::Unit::basis_points: unit = QStringLiteral("basis points"); break;
    case content::Unit::basis_point_delta: unit = QStringLiteral("basis point change"); break;
    case content::Unit::milli_CU: unit = QStringLiteral("milli-CU"); break;
    case content::Unit::milli_CU_per_day: unit = QStringLiteral("milli-CU/day"); break;
    default: unit = text(content::unit_name(port.unit)); break;
    }
    if (port.value_type == content::ValueType::text) unit = QStringLiteral("Text");
    if (port.value_type == content::ValueType::reference) unit = QStringLiteral("Reference · ") + text(port.reference_kind);
    return unit + QStringLiteral(" · ") + scope;
}
bool finite_integer(double value, double bound = maximum_canvas_coordinate) { return std::isfinite(value) && std::trunc(value) == value && value >= -bound && value <= bound; }
}
CanvasController::CanvasController() {
    view_timer_.setSingleShot(true); view_timer_.setInterval(250);
    connect(&view_timer_, &QTimer::timeout, this, &CanvasController::flush_view);
}
CanvasController::~CanvasController() { flush_view(); }
void CanvasController::fail(const std::exception& error) { message_ = text(error.what()); emit messageChanged(); }
bool CanvasController::verify(const QString& value) {
    if (!document_ || !graph_ || ticket() != value || document_->edit_revision() != revision_) {
        message_ = QStringLiteral("The canvas changed. Select its current ports again."); emit messageChanged(); return false;
    }
    return true;
}
bool CanvasController::hasLayoutEdits() const {
    return std::any_of(layouts_.begin(), layouts_.end(), [](const auto& pair) { return pair.second->dirty(); });
}
QVariantMap CanvasController::view() const {
    return {{QStringLiteral("x"), view_.pan_x}, {QStringLiteral("y"), view_.pan_y}, {QStringLiteral("zoom"), view_.zoom_permille}, {QStringLiteral("restored"), view_restored_}};
}
void CanvasController::set_document(DraftDocument* document, std::string flow, const std::filesystem::path& root) {
    if (!document || flow.empty()) {
        flush_view(); document_ = nullptr; graph_.reset(); layout_ = nullptr; flow_.clear();
        nodes_.clear(); edges_.clear(); lookup_.clear(); warnings_.clear(); selected_.clear(); cancelConnection(); ++generation_;
        emit graphChanged(); emit selectionChanged(); emit layoutChanged(); return;
    }
    const bool context = document_ != document || flow_ != flow || root_ != root;
    const bool source = context || source_ != document->relative_file();
    if (!source && revision_ == document->edit_revision()) return;
    if (source) flush_view();
    document_ = document; flow_ = std::move(flow); root_ = root; source_ = document->relative_file();
    if (context) { message_.clear(); emit messageChanged(); }
    const auto layout_key = root_.generic_string() + "#" + flow_;
    layout_ = nullptr;
    try {
        if (!layouts_.contains(layout_key)) {
            if (layouts_.size() >= 64) throw std::invalid_argument("Close saved layout work before opening more than 64 flow layouts.");
            layouts_.emplace(layout_key, std::make_unique<LayoutDocument>(root_, flow_));
        }
        layout_ = layouts_.at(layout_key).get();
    } catch (const std::exception& error) { fail(error); }
    if (context) {
        view_ = {}; view_restored_ = false; selected_.clear(); port_.clear(); pending_.clear();
        try { view_ = load_canvas_view(root_, source_, flow_, &view_restored_); } catch (const std::exception& error) { fail(error); }
    }
    rebuild();
    if (context) { selected_ = text(view_.selection); emit viewChanged(); }
}
void CanvasController::rebuild() {
    nodes_.clear(); edges_.clear(); lookup_.clear(); warnings_.clear(); graph_.reset(); ++generation_;
    if (document_) try {
        graph_ = FlowGraph::open(*document_, flow_, layout_ ? layout_->positions() : CanvasPositions{}); revision_ = document_->edit_revision();
        for (const auto& warning : document_->program().lints)
            if (warning.path.starts_with("/definitions/" + flow_ + "/")) warnings_.push_back(text(warning.code + " · " + warning.explanation));
        std::map<std::string, const GraphPort*, std::less<>> ports;
        for (const auto& port : graph_->ports()) ports[port.id] = &port;
        for (const auto& node : graph_->nodes()) {
            QVariantList entries;
            for (const auto& id : node.ports) {
                const auto& port = *ports.at(id);
                entries.push_back(QVariantMap{{QStringLiteral("id"), text(id)}, {QStringLiteral("label"), text(port.label)}, {QStringLiteral("row"), text(port.row)},
                    {QStringLiteral("type"), type_name(port)}, {QStringLiteral("kind"), static_cast<int>(port.type)}, {QStringLiteral("output"), port.output},
                    {QStringLiteral("connectable"), port.connectable}, {QStringLiteral("offset"), port.offset_y}, {QStringLiteral("help"), text(port.help)}});
            }
            QVariantMap value{{QStringLiteral("key"), text(node.key)}, {QStringLiteral("row"), text(node.row)}, {QStringLiteral("title"), text(node.title)},
                {QStringLiteral("ticket"), ticket()},
                {QStringLiteral("detail"), text(node.detail)}, {QStringLiteral("kind"), text(node.kind)}, {QStringLiteral("scope"), text(content::scope_name(node.scope))},
                {QStringLiteral("x"), node.position.x}, {QStringLiteral("y"), node.position.y}, {QStringLiteral("width"), node.width}, {QStringLiteral("height"), node.height},
                {QStringLiteral("ports"), entries}};
            lookup_.insert(text(node.key), value); nodes_.push_back(value);
        }
        for (const auto& edge : graph_->edges()) {
            const auto& from = *ports.at(edge.from); const auto& to = *ports.at(edge.to);
            edges_.push_back(QVariantMap{{QStringLiteral("from"), text(edge.from)}, {QStringLiteral("to"), text(edge.to)},
                {QStringLiteral("fromNode"), text(from.node)}, {QStringLiteral("toNode"), text(to.node)},
                {QStringLiteral("fromY"), from.offset_y}, {QStringLiteral("toY"), to.offset_y},
                {QStringLiteral("label"), text(edge.label)}, {QStringLiteral("kind"), text(edge.kind)}, {QStringLiteral("row"), text(edge.row)}});
        }
    } catch (const std::exception& error) { fail(error); }
    pending_.clear(); refresh_palette(); emit graphChanged(); emit layoutChanged();
}
QString CanvasController::restored_row() const {
    const auto found = lookup_.constFind(text(view_.selection));
    return found == lookup_.cend() ? text(view_.selection) : found->toMap().value(QStringLiteral("row")).toString();
}
void CanvasController::set_selection(const QString& row, bool from_outline) {
    const auto previous = lookup_.value(selected_).toMap();
    if (from_outline || previous.value(QStringLiteral("row")).toString() != row) {
        selected_.clear();
        for (const auto& entry : nodes_) {
            const auto node = entry.toMap();
            if (node.value(QStringLiteral("key")).toString() == row) { selected_ = row; break; }
            if (selected_.isEmpty() && node.value(QStringLiteral("row")).toString() == row) selected_ = node.value(QStringLiteral("key")).toString();
        }
        if (selected_.isEmpty()) for (auto it = lookup_.cbegin(); it != lookup_.cend(); ++it)
            if (row.startsWith(it.key() + QChar{'/'}) && it.key().size() > selected_.size()) selected_ = it.key();
    }
    view_.selection = (selected_.isEmpty() ? row : selected_).toStdString(); emit selectionChanged();
}
void CanvasController::remember_selection() { if (document_) { view_dirty_ = true; view_timer_.start(); } }
void CanvasController::selectNode(QString key) {
    const auto found = lookup_.constFind(key); if (found == lookup_.cend()) return;
    selected_ = std::move(key); emit selectionRequested(found->toMap().value(QStringLiteral("row")).toString());
    remember_selection(); emit selectionChanged();
}
void CanvasController::refresh_palette() {
    palette_.clear(); port_label_.clear();
    if (graph_ && !port_.isEmpty()) try {
        const auto found = std::find_if(graph_->ports().begin(), graph_->ports().end(), [&](const auto& value) { return text(value.id) == port_; });
        if (found == graph_->ports().end()) port_.clear();
        else {
            port_label_ = text(found->label) + QStringLiteral(" · ") + type_name(*found);
            for (const auto& option : graph_->palette(found->id)) palette_.push_back(QVariantMap{{QStringLiteral("value"), text(option.value)}, {QStringLiteral("label"), text(option.label)}});
        }
    } catch (const std::exception& error) { fail(error); }
    emit portChanged();
}
bool CanvasController::pickPort(QString value, QString id) {
    if (!verify(value)) return false;
    try {
        const auto found = std::find_if(graph_->ports().begin(), graph_->ports().end(), [&](const auto& entry) { return text(entry.id) == id; });
        if (found == graph_->ports().end()) throw std::invalid_argument("Select a current canvas port.");
        const auto port = *found; port_ = id; refresh_palette(); selected_ = text(port.node);
        emit selectionRequested(text(port.row)); remember_selection(); emit selectionChanged();
        if (!port.connectable) { message_ = text(port.help); emit messageChanged(); return true; }
        if (port.output) {
            pending_ = id; message_ = QStringLiteral("Choose a compatible input to connect, or a palette item to create. Predicate/value connections copy owned trees.");
        } else if (!pending_.isEmpty()) {
            const auto changed = graph_->connect(*document_, pending_.toStdString(), id.toStdString()); pending_.clear();
            message_ = changed ? QStringLiteral("Connected through the shared compiler. Undo restores the previous graph.") : QStringLiteral("Already connected; runtime content is unchanged.");
            if (changed) emit documentEdited();
        } else message_ = QStringLiteral("Choose an output first. This input's box uses the same settings as the outline.");
        emit portChanged(); emit messageChanged(); return true;
    } catch (const std::exception& error) { fail(error); return false; }
}
void CanvasController::cancelConnection() { pending_.clear(); port_.clear(); refresh_palette(); }
bool CanvasController::insertNode(QString value, QString type) {
    if (!verify(value) || port_.isEmpty()) return false;
    try {
        const auto before = lookup_; const auto changed = graph_->insert(*document_, port_.toStdString(), type.toStdString());
        if (changed) {
            emit documentEdited();
            QString created;
            for (const auto& entry : nodes_) {
                const auto node = entry.toMap(); const auto key = node.value(QStringLiteral("key")).toString();
                if (before.contains(key)) continue;
                if (created.isEmpty()) created = key;
                if (node.value(QStringLiteral("kind")).toString() == QStringLiteral("execution")) {
                    const auto& flow = std::get<content::FlowDefinition>(document_->collection().definitions.at(flow_).data);
                    if (flow.nodes.at(key.mid(5).toStdString()).kind != content::NodeKind::end) { created = key; break; }
                }
            }
            if (!created.isEmpty()) selectNode(created);
        }
        message_ = changed ? QStringLiteral("Created through the shared typed document.") : QStringLiteral("The selected type is already in place."); emit messageChanged(); return true;
    } catch (const std::exception& error) { fail(error); return false; }
}
bool CanvasController::moveNode(QString value, QString key, double x, double y) {
    if (!verify(value)) return false;
    try {
        if (!layout_) throw std::invalid_argument("The layout file is read-only. Its original data was preserved.");
        if (!lookup_.contains(key) || !finite_integer(x) || !finite_integer(y) || x < 0 || y < 0) throw std::invalid_argument("Choose a current box and bounded integer grid position.");
        const CanvasPoint position{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
        const auto old = lookup_.value(key).toMap();
        if (old.value(QStringLiteral("x")).toInt() == position.x && old.value(QStringLiteral("y")).toInt() == position.y) return true;
        if (layout_->move(key.toStdString(), position)) rebuild();
        return true;
    } catch (const std::exception& error) { fail(error); return false; }
}
void CanvasController::autoLayout() { if (graph_ && layout_) try { if (layout_->replace(graph_->automatic_layout())) rebuild(); } catch (const std::exception& error) { fail(error); } }
void CanvasController::undoLayout() { if (layout_ && layout_->undo()) rebuild(); }
void CanvasController::redoLayout() { if (layout_ && layout_->redo()) rebuild(); }
void CanvasController::saveLayout() {
    if (!layout_) return;
    try { layout_->save(); message_ = QStringLiteral("Layout saved. Runtime content, revisions and versions are unchanged."); emit messageChanged(); emit layoutChanged(); }
    catch (const std::exception& error) { fail(error); }
}
void CanvasController::reloadLayout() {
    if (!document_) return;
    try {
        const auto key = root_.generic_string() + "#" + flow_; auto next = std::make_unique<LayoutDocument>(root_, flow_);
        layout_ = next.get(); layouts_[key] = std::move(next); rebuild(); message_.clear(); emit messageChanged();
    } catch (const std::exception& error) { fail(error); }
}
void CanvasController::discard_layouts() { layouts_.clear(); layout_ = nullptr; }
void CanvasController::rememberViewport(double x, double y, double zoom) {
    if (!document_) return;
    if (!finite_integer(x, maximum_canvas_pan) || !finite_integer(y, maximum_canvas_pan) || !std::isfinite(zoom) || std::trunc(zoom) != zoom || zoom < 100 || zoom > 2000) return;
    view_.pan_x = static_cast<std::int32_t>(x); view_.pan_y = static_cast<std::int32_t>(y); view_.zoom_permille = static_cast<std::int32_t>(zoom); view_dirty_ = true; view_timer_.start();
}
void CanvasController::previewViewport(double x, double y, double zoom) {
    if (!document_ || !finite_integer(x, maximum_canvas_pan) || !finite_integer(y, maximum_canvas_pan) || !std::isfinite(zoom) || std::trunc(zoom) != zoom || zoom < 100 || zoom > 2000) return;
    view_.pan_x = static_cast<std::int32_t>(x); view_.pan_y = static_cast<std::int32_t>(y); view_.zoom_permille = static_cast<std::int32_t>(zoom);
}
void CanvasController::flush_view() {
    view_timer_.stop();
    if (!view_dirty_ || !document_) return;
    view_dirty_ = false;
    try { save_canvas_view(root_, source_, flow_, view_); } catch (const std::exception& error) { fail(error); }
}
} // namespace pgp::studio
