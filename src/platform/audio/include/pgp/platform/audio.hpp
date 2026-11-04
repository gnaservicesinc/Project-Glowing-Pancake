#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace pgp::audio {

enum class DeviceMode { Silent, NativeIfAvailable };

struct Result {
    bool ok{};
    std::string message;
};

struct Status {
    bool loaded{};
    bool playing{};
    bool muted{};
    bool device_lost{};
    float volume{1.0F};
    std::uint32_t sample_rate{};
    std::uint16_t channels{};
    std::size_t frames{};
};

// Local playback adapter. Construction/decoding never opens an audio device;
// only an explicit play() may do so. Paths must resolve within asset_root.
// Native backend failure is recoverable through the always-present silent mode.
// Controls/load/status belong to one owning thread. notify_device_lost() is
// callback-safe and only changes atomic state.
class Player final {
public:
    explicit Player(std::filesystem::path asset_root, DeviceMode mode = DeviceMode::Silent);
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    [[nodiscard]] Result load(const std::filesystem::path& path);
    [[nodiscard]] Result play();
    void stop();
    void set_volume(float value);
    void set_muted(bool value);
    [[nodiscard]] Status status() const;
    [[nodiscard]] std::string backend_name() const;

    // Also used by platform/device notifications. This only changes adapter
    // state; device teardown happens on the calling/UI thread, never in callback.
    void notify_device_lost();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace pgp::audio
