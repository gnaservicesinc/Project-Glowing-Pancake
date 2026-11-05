#include <pgp/studio/layout.hpp>
#include "checkout.hpp"
#include <pgp/persistence/sha256.hpp>
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

namespace pgp::studio {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::size_t bound = 1U * 1024U * 1024U;
[[noreturn]] void refuse(std::string code, std::string detail) { throw persistence::PersistenceError(std::move(code), std::move(detail)); }
Json parse(std::string_view bytes) {
    if (bytes.size() > bound) refuse("oversized", "Authoring layout/session data exceeds its byte bound.");
    std::size_t count = 0; std::vector<std::set<std::string>> keys;
    return Json::parse(bytes.begin(), bytes.end(), [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 12 || ++count > 100000) refuse("budget", "Authoring layout/session data exceeds its structural bound.");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) refuse("duplicate_key", "Duplicate authoring metadata key.");
        if (event == Json::parse_event_t::value && value.is_number_float()) refuse("integer_token", "Layout coordinates and local viewport records use integer tokens.");
        return true;
    });
}
void fields(const Json& value, std::initializer_list<std::string_view> allowed) {
    if (!value.is_object() || value.size() != allowed.size()) refuse("metadata", "Unsupported authoring layout/session fields; the file was preserved.");
    for (auto key : allowed) if (!value.contains(key)) refuse("metadata", "Missing authoring layout/session field.");
}
void key(std::string_view value) {
    if (value.empty() || value.size() > 1024 || std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20; }))
        refuse("metadata", "Invalid bounded authoring node/view key.");
}
std::optional<std::string> read(const fs::path& root, const fs::path& file) {
    const auto status = detail::checked_path(root, file);
    if (!fs::exists(status)) return {};
    if (!fs::is_regular_file(status)) refuse("path", "Layout/session data must be an ordinary local file.");
    return persistence::read_bounded(root / file, bound);
}
std::string marker(const fs::path& root) {
    if (fs::exists(detail::checked_path(root, ".pgp-editor/snapshot-transaction"))) refuse("pending_snapshot", "Recover the pending content snapshot before authoring writes.");
    if (detail::checkout_schema(root) != content::content_schema) refuse("unsupported_schema", "This checkout is read-only for the installed authoring schema.");
    const auto value = read(root, "project-versions.json");
    if (!value) refuse("conflict", "The checkout marker is missing.");
    return persistence::sha256(*value);
}
void validate_positions(const CanvasPositions& positions) {
    if (positions.size() > 4096) refuse("budget", "A flow layout supports at most 4096 boxes.");
    for (const auto& [id, point] : positions) {
        key(id);
        if (point.x < 0 || point.y < 0 || point.x > maximum_canvas_coordinate || point.y > maximum_canvas_coordinate ||
            point.x % canvas_grid != 0 || point.y % canvas_grid != 0) refuse("coordinate", "Use bounded integer canvas positions on the 16-point grid.");
    }
}
void validate_view(const CanvasView& view) {
    key(view.selection);
    if (view.pan_x < -maximum_canvas_pan || view.pan_x > maximum_canvas_pan ||
        view.pan_y < -maximum_canvas_pan || view.pan_y > maximum_canvas_pan ||
        view.zoom_permille < 100 || view.zoom_permille > 2000) refuse("viewport", "Local canvas pan/zoom exceeds its supported bounds.");
}
std::int32_t integer(const Json& value, std::int32_t low, std::int32_t high) {
    if (!value.is_number_integer() || value < low || value > high) refuse("integer_token", "Expected a bounded integer authoring coordinate.");
    return value.get<std::int32_t>();
}
void write(const fs::path& root, const fs::path& file, const std::string& bytes, const std::optional<std::string>& hash) {
    (void)detail::checked_path(root, file);
    const auto current = read(root, file);
    if ((current ? std::optional{persistence::sha256(*current)} : std::nullopt) != hash) refuse("conflict", "The authoring file changed externally. Reopen it before saving; no overwrite was made.");
    persistence::atomic_write(root / file, bytes, [&](std::string_view prior) {
        if (!hash || persistence::sha256(prior) != *hash) refuse("conflict", "A concurrent authoring edit was preserved.");
    }, bound, hash.has_value());
}
std::string view_key(const fs::path& root, const fs::path& source, std::string_view flow) {
    (void)detail::checked_path(root, source);
    if (!content::valid_id(flow)) refuse("path", "Choose a stable current flow ID.");
    auto result = source.generic_string() + "#" + std::string(flow); key(result); return result;
}
Json session(const std::optional<std::string>& bytes) {
    auto value = bytes ? parse(*bytes) : Json{{"format", 1}, {"views", Json::object()}};
    fields(value, {"format", "views"});
    if (!value["format"].is_number_integer() || value["format"] != 1 || !value["views"].is_object() || value["views"].size() > 64)
        refuse("metadata", "Unsupported local editor-session record; it was preserved.");
    for (const auto& [id, entry] : value["views"].items()) {
        key(id); fields(entry, {"pan_x", "pan_y", "zoom_permille", "selection"});
        CanvasView view{integer(entry["pan_x"], -maximum_canvas_pan, maximum_canvas_pan),
            integer(entry["pan_y"], -maximum_canvas_pan, maximum_canvas_pan), integer(entry["zoom_permille"], 100, 2000), entry["selection"].get<std::string>()};
        validate_view(view);
    }
    return value;
}
}

LayoutDocument::LayoutDocument(const fs::path& checkout, std::string flow)
    : root_{detail::checkout_root(checkout)}, flow_{std::move(flow)} {
    if (!content::valid_id(flow_)) refuse("path", "Choose a stable flow ID for its layout.");
    file_ = fs::path{"authoring/layouts"} / (flow_ + ".layout.json"); marker_hash_ = marker(root_);
    const auto bytes = read(root_, file_);
    if (!bytes) return;
    auto value = parse(*bytes); fields(value, {"format", "flow", "grid", "nodes"});
    if (!value["format"].is_number_integer() || value["format"] != 1 || value["flow"] != flow_ ||
        !value["grid"].is_number_integer() || value["grid"] != canvas_grid || !value["nodes"].is_object() || value["nodes"].size() > 4096)
        refuse("metadata", "Unsupported flow layout identity/grid; the original file was preserved.");
    for (const auto& [id, node] : value["nodes"].items()) {
        fields(node, {"x", "y"});
        positions_[id] = {integer(node["x"], 0, maximum_canvas_coordinate), integer(node["y"], 0, maximum_canvas_coordinate)};
    }
    validate_positions(positions_); saved_ = positions_; base_hash_ = persistence::sha256(*bytes);
}
bool LayoutDocument::replace(CanvasPositions positions) {
    validate_positions(positions);
    if (positions == positions_) return false;
    undo_.push_back(positions_); if (undo_.size() > 16) undo_.pop_front();
    redo_.clear(); positions_ = std::move(positions); return true;
}
bool LayoutDocument::move(std::string id, CanvasPoint point) {
    auto candidate = positions_; candidate[std::move(id)] = point; return replace(std::move(candidate));
}
bool LayoutDocument::restore(std::deque<CanvasPositions>& from, std::deque<CanvasPositions>& to) {
    if (from.empty()) return false;
    to.push_back(positions_); if (to.size() > 16) to.pop_front();
    positions_ = std::move(from.back()); from.pop_back(); return true;
}
bool LayoutDocument::undo() { return restore(undo_, redo_); }
bool LayoutDocument::redo() { return restore(redo_, undo_); }
void LayoutDocument::save() {
    if (!dirty()) return;
    if (marker(root_) != marker_hash_) refuse("conflict", "Project versions changed; reopen the layout before writing.");
    Json nodes = Json::object(); for (const auto& [id, point] : positions_) nodes[id] = {{"x", point.x}, {"y", point.y}};
    const auto bytes = Json{{"format", 1}, {"flow", flow_}, {"grid", canvas_grid}, {"nodes", std::move(nodes)}}.dump(2) + "\n";
    write(root_, file_, bytes, base_hash_); base_hash_ = persistence::sha256(bytes); saved_ = positions_;
}
CanvasView load_canvas_view(const fs::path& checkout, const fs::path& source, std::string_view flow, bool* restored) {
    if (restored) *restored = false;
    const auto root = detail::checkout_root(checkout); const auto id = view_key(root, source, flow);
    const auto record = session(read(root, ".work/editor-session.json"));
    if (!record["views"].contains(id)) return {};
    if (restored) *restored = true;
    const auto& v = record["views"].at(id);
    return {v["pan_x"].get<std::int32_t>(), v["pan_y"].get<std::int32_t>(), v["zoom_permille"].get<std::int32_t>(), v["selection"].get<std::string>()};
}
void save_canvas_view(const fs::path& checkout, const fs::path& source, std::string_view flow, const CanvasView& view) {
    const auto root = detail::checkout_root(checkout); (void)marker(root); validate_view(view);
    const auto id = view_key(root, source, flow); const fs::path file{".work/editor-session.json"};
    const auto old = read(root, file); auto record = session(old);
    auto& views = record["views"];
    if (!views.contains(id) && views.size() == 64) views.erase(views.begin());
    views[id] = {{"pan_x", view.pan_x}, {"pan_y", view.pan_y}, {"zoom_permille", view.zoom_permille}, {"selection", view.selection}};
    const auto bytes = record.dump(2) + "\n";
    if (old && bytes == *old) return;
    write(root, file, bytes, old ? std::optional{persistence::sha256(*old)} : std::nullopt);
}
} // namespace pgp::studio
