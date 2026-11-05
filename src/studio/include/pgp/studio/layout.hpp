#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace pgp::studio {
inline constexpr std::int32_t canvas_grid = 16;
inline constexpr std::int32_t maximum_canvas_coordinate = 1'048'576;
inline constexpr std::int32_t maximum_canvas_pan = 4 * maximum_canvas_coordinate;
struct CanvasPoint {
    std::int32_t x = 0, y = 0;
    auto operator<=>(const CanvasPoint&) const = default;
};
using CanvasPositions = std::map<std::string, CanvasPoint, std::less<>>;

// Non-semantic, single-file authoring data. Opening never writes. Every move
// is grid-aligned and saved only on explicit Save layout with hash conflicts.
class LayoutDocument {
public:
    LayoutDocument(const std::filesystem::path& checkout, std::string flow);
    [[nodiscard]] const CanvasPositions& positions() const noexcept { return positions_; }
    [[nodiscard]] bool dirty() const noexcept { return positions_ != saved_; }
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    bool replace(CanvasPositions positions);
    bool move(std::string key, CanvasPoint point);
    bool undo();
    bool redo();
    void save();
private:
    bool restore(std::deque<CanvasPositions>& from, std::deque<CanvasPositions>& to);
    std::filesystem::path root_, file_;
    std::string flow_, marker_hash_;
    std::optional<std::string> base_hash_;
    CanvasPositions positions_, saved_;
    std::deque<CanvasPositions> undo_, redo_;
};

struct CanvasView {
    std::int32_t pan_x = 0, pan_y = 0, zoom_permille = 800;
    std::string selection = "flow";
    auto operator<=>(const CanvasView&) const = default;
};
// Ignored local session state, keyed by collection file + flow ID. It has no
// effect on runtime/layout identity and is saved only after explicit gestures.
[[nodiscard]] CanvasView load_canvas_view(const std::filesystem::path& checkout,
    const std::filesystem::path& source, std::string_view flow, bool* restored = nullptr);
void save_canvas_view(const std::filesystem::path& checkout, const std::filesystem::path& source,
    std::string_view flow, const CanvasView& view);
} // namespace pgp::studio
