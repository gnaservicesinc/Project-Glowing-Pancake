#include <pgp/platform/audio.hpp>

#include "clip.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mutex>
#include <span>
#include <vector>

#if PGP_ENABLE_AUDIO
#include "miniaudio_config.hpp"
#include "miniaudio.h"
#endif

namespace pgp::audio {

struct Player::Impl {
    std::filesystem::path root;
    DeviceMode mode;
    mutable std::mutex mutex;
    detail::Clip clip;
    std::size_t cursor{};
    std::atomic<bool> playing{false};
    std::atomic<bool> muted{false};
    std::atomic<bool> lost{false};
    std::atomic<float> volume{1.0F};
    std::string backend{"silent"};
#if PGP_ENABLE_AUDIO
    ma_device device{};
    bool device_initialized{};
#endif

    explicit Impl(const std::filesystem::path& asset_root, DeviceMode device_mode) : mode(device_mode) {
        std::error_code error;
        root = std::filesystem::canonical(asset_root, error);
        if (error || !std::filesystem::is_directory(root, error)) root.clear();
    }

    void render(std::span<float> output) {
        std::lock_guard lock(mutex);
        if (!playing.load() || lost.load()) {
            std::fill(output.begin(), output.end(), 0.0F);
            return;
        }
        detail::mix(clip, cursor, muted.load() ? 0.0F : volume.load(), output);
        if (cursor == clip.samples.size()) playing.store(false);
    }

    void close_device() {
        playing.store(false);
#if PGP_ENABLE_AUDIO
        if (device_initialized) {
            ma_device_uninit(&device);
            device_initialized = false;
        }
#endif
    }
};

Player::Player(std::filesystem::path asset_root, DeviceMode mode)
    : impl_(std::make_unique<Impl>(asset_root, mode)) {}

Player::~Player() { impl_->close_device(); }

Result Player::load(const std::filesystem::path& path) {
    if (impl_->root.empty()) return {false, "Audio asset root does not exist."};
    const auto text = path.generic_string();
    if (text.starts_with("//") || text.starts_with("\\\\") || text.find("://") != std::string::npos) {
        return {false, "Audio requires a local path within the approved asset root."};
    }
    std::error_code error;
    const auto candidate = std::filesystem::canonical(
        path.is_absolute() ? path : impl_->root / path, error);
    if (error) return {false, "Audio file could not be resolved."};
    const auto mismatch = std::mismatch(impl_->root.begin(), impl_->root.end(), candidate.begin(), candidate.end());
    if (mismatch.first != impl_->root.end() || !std::filesystem::is_regular_file(candidate, error) || error) {
        return {false, "Audio file escapes the approved root or is not a regular file."};
    }
    const auto extension = candidate.extension().string();
    if (extension != ".wav" && extension != ".ogg") return {false, "Only local PCM16 WAV and Ogg Vorbis are accepted."};
    const auto size = std::filesystem::file_size(candidate, error);
    if (error || size == 0 || size > 4U * 1024U * 1024U) return {false, "Audio exceeds the 4 MiB encoded limit."};
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    std::ifstream input(candidate, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return {false, "Audio file could not be read completely."};
    }
    return load_bytes(bytes, extension == ".ogg");
}

Result Player::load_bundled_wave(std::span<const unsigned char> bytes) {
    return load_bytes(bytes, false);
}

Result Player::load_bytes(std::span<const unsigned char> bytes, bool vorbis) {
    if (bytes.empty() || bytes.size() > 4U * 1024U * 1024U) return {false, "Audio exceeds the 4 MiB encoded limit."};
    auto decoded = detail::decode(bytes, vorbis);
    if (!decoded.error.empty()) return {false, std::move(decoded.error)};
    // Decode first: failed loads preserve the previously validated clip.
    impl_->close_device();
    {
        std::lock_guard lock(impl_->mutex);
        impl_->clip = std::move(decoded.clip);
        impl_->cursor = 0;
    }
    return {true, "Local audio decoded; device remains closed until Play."};
}

Result Player::play() {
    impl_->close_device();
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->clip.samples.empty()) return {false, "Load a validated local audio clip first."};
        impl_->cursor = 0;
    }
    if (impl_->mode == DeviceMode::Silent) {
        impl_->backend = "silent";
        return {true, "Silent backend: audio is validated; no output device opened."};
    }
    if (impl_->lost.load()) {
        impl_->backend = "silent (device lost)";
        return {true, "Audio device was lost; silent fallback remains available."};
    }
#if PGP_ENABLE_AUDIO
    auto config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = impl_->clip.channels;
    config.sampleRate = impl_->clip.sample_rate;
    config.pUserData = impl_.get();
    config.dataCallback = [](ma_device* device, void* output, const void*, ma_uint32 frames) {
        auto* self = static_cast<Impl*>(device->pUserData);
        self->render(std::span<float>(static_cast<float*>(output),
                    static_cast<std::size_t>(frames) * device->playback.channels));
    };
    config.notificationCallback = [](const ma_device_notification* notification) {
        auto* self = static_cast<Impl*>(notification->pDevice->pUserData);
        if (notification->type == ma_device_notification_type_interruption_began ||
            (notification->type == ma_device_notification_type_stopped && self->playing.load())) {
            self->lost.store(true);
            self->playing.store(false);
        }
    };
    const auto initialized = ma_device_init(nullptr, &config, &impl_->device);
    if (initialized == MA_SUCCESS) {
        impl_->device_initialized = true;
        impl_->playing.store(true);
        const auto started = ma_device_start(&impl_->device);
        if (started == MA_SUCCESS) {
            impl_->backend = ma_get_backend_name(impl_->device.pContext->backend);
            return {true, "Playing local decoded PCM through " + impl_->backend + "."};
        }
        impl_->close_device();
    }
    impl_->backend = "silent (device unavailable)";
    return {true, "Native playback unavailable; continued with silent fallback."};
#else
    impl_->backend = "silent (native audio not compiled)";
    return {true, "Native playback is not compiled; continued with silent fallback."};
#endif
}

void Player::stop() { impl_->close_device(); }
void Player::set_volume(float value) { impl_->volume.store(std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F); }
void Player::set_muted(bool value) { impl_->muted.store(value); }

Status Player::status() const {
    std::lock_guard lock(impl_->mutex);
    return {!impl_->clip.samples.empty(), impl_->playing.load(), impl_->muted.load(),
            impl_->lost.load(), impl_->volume.load(), impl_->clip.sample_rate,
            impl_->clip.channels, impl_->clip.channels == 0 ? 0 : impl_->clip.samples.size() / impl_->clip.channels};
}

std::string Player::backend_name() const {
    return impl_->lost.load() ? "silent (device lost)" : impl_->backend;
}

void Player::notify_device_lost() {
    impl_->lost.store(true);
    impl_->playing.store(false);
}

} // namespace pgp::audio
