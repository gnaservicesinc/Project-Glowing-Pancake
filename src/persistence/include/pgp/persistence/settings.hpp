#pragma once
#include <pgp/persistence/storage.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace pgp::persistence {
struct Settings {
    bool autosave_enabled = true;
    bool reduced_motion = false;
    bool auto_pause_alerts = true;
    bool auto_pause_projects = true;
    bool auto_pause_evaluations = true;
    bool compact_management = false;
    bool show_guide = true;
    std::uint32_t text_scale_percent = 100;
    std::uint32_t volume_percent = 100;
    bool music_enabled = false;
    auto operator<=>(const Settings&) const = default;
};
[[nodiscard]] std::string encode_settings(const Settings& settings);
[[nodiscard]] Settings decode_settings(std::string_view bytes);
void save_settings(const std::filesystem::path& path, const Settings& settings);
[[nodiscard]] Settings load_settings(const std::filesystem::path& path);
}
