#include <pgp/platform/audio.hpp>
#include "clip.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<unsigned char> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(stream.good(), "Failed writing test fixture.");
}

void put32(std::vector<unsigned char>& bytes, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) bytes[at + i] = static_cast<unsigned char>(value >> (i * 8U));
}

struct Scratch {
    std::filesystem::path path = std::filesystem::path(PGP_AUDIO_TEST_SCRATCH_ROOT) /
        ("scratch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { std::filesystem::create_directories(path / "approved"); }
    ~Scratch() { std::error_code error; std::filesystem::remove_all(path, error); }
};

int contracts() {
    using namespace pgp::audio;
    const auto fixtures = std::filesystem::path(PGP_AUDIO_FIXTURE_ROOT);
    Scratch scratch;
    const auto approved = scratch.path / "approved";
    auto wav = read(fixtures / "tone.wav");
    const auto ogg = read(fixtures / "tone.ogg");
    write(approved / "tone.wav", wav);
    write(approved / "tone.ogg", ogg);
    write(scratch.path / "outside.wav", wav);
    Player player(approved);
    require(!player.play().ok, "Unloaded playback accepted.");
    require(player.load("tone.wav").ok, "Valid PCM16 WAV rejected.");
    const auto loaded = player.status();
    require(loaded.loaded && !loaded.playing && loaded.frames == 9600 &&
            loaded.channels == 2 && loaded.sample_rate == 48000, "WAV metadata mismatch/device opened on load.");
    require(player.load("tone.ogg").ok, "Valid Ogg Vorbis rejected.");
    const auto ogg_status = player.status();
    require(ogg_status.loaded && !ogg_status.playing && ogg_status.channels == 2 &&
            ogg_status.sample_rate == 48000 && ogg_status.frames > 0, "Ogg metadata mismatch.");
    require(!player.load("../outside.wav").ok, "Canonical parent escape accepted.");
    require(!player.load(scratch.path / "outside.wav").ok, "Absolute root escape accepted.");
    require(!player.load("https://example.invalid/a.wav").ok, "Remote URL accepted.");
    require(!player.load("//server/share/a.wav").ok, "Remote file host accepted.");
    std::error_code link_error;
    std::filesystem::create_symlink(scratch.path / "outside.wav", approved / "link.wav", link_error);
    if (!link_error) require(!player.load("link.wav").ok, "Symlink root escape accepted.");
    else std::cout << "Symlink test unavailable on this host: " << link_error.message() << '\n';
    write(approved / "tone.mp3", wav);
    write(approved / "fake.ogg", wav);
    require(!player.load("tone.mp3").ok && !player.load("fake.ogg").ok, "Unsupported/mislabelled codec accepted.");
    auto bad_format = wav;
    bad_format[20] = 3; // IEEE-float WAV, forbidden even if bytes remain PCM.
    write(approved / "float.wav", bad_format);
    require(!player.load("float.wav").ok, "Non-PCM WAV accepted.");
    bad_format = wav;
    bad_format[34] = 24;
    write(approved / "pcm24.wav", bad_format);
    require(!player.load("pcm24.wav").ok, "Non-16-bit WAV accepted.");
    bad_format.resize(20);
    write(approved / "truncated.wav", bad_format);
    require(!player.load("truncated.wav").ok, "Truncated WAV accepted.");
    auto excessive_duration = wav;
    excessive_duration.resize(44U + 31U * 8000U * 4U);
    put32(excessive_duration, 4, static_cast<std::uint32_t>(excessive_duration.size() - 8));
    put32(excessive_duration, 24, 8000);
    put32(excessive_duration, 28, 8000 * 4);
    put32(excessive_duration, 40, static_cast<std::uint32_t>(excessive_duration.size() - 44));
    write(approved / "too-long.wav", excessive_duration);
    require(!player.load("too-long.wav").ok, "WAV longer than the decoded duration limit accepted.");
    auto truncated_ogg = ogg;
    truncated_ogg.resize(ogg.size() / 2);
    write(approved / "truncated.ogg", truncated_ogg);
    require(!player.load("truncated.ogg").ok, "Truncated Ogg accepted.");
    write(approved / "large.wav", std::vector<unsigned char>(4U * 1024U * 1024U + 1U));
    require(!player.load("large.wav").ok, "Oversized encoded audio accepted.");
    require(player.status().frames == ogg_status.frames, "Failed load discarded previously validated audio.");

    player.set_volume(2.0F);
    require(player.status().volume == 1.0F, "Volume upper bound failed.");
    player.set_volume(-1.0F);
    require(player.status().volume == 0.0F, "Volume lower bound failed.");
    player.set_volume(std::numeric_limits<float>::quiet_NaN());
    require(player.status().volume == 0.0F, "Non-finite gain accepted.");
    player.set_volume(0.25F);
    player.set_muted(true);
    require(player.status().muted && player.status().volume == 0.25F, "Mute lost remembered volume.");
    require(player.play().ok && !player.status().playing && player.backend_name() == "silent", "Silent mode opened a device.");
    player.notify_device_lost();
    require(player.status().device_lost && !player.status().playing, "Device-loss transition failed.");
    Player fallback(approved, DeviceMode::NativeIfAvailable);
    require(fallback.load("tone.wav").ok, "Fallback fixture load failed.");
    fallback.notify_device_lost(); // deterministic unavailable/loss seam; no device touched
    require(fallback.play().ok && fallback.backend_name() == "silent (device lost)" &&
            !fallback.status().playing, "Unavailable-device silent fallback failed.");

    detail::Clip clip{{1.0F, -1.0F, 0.5F, -0.5F}, 48000, 2};
    std::size_t cursor = 0;
    std::array<float, 2> output{};
    detail::mix(clip, cursor, 0.25F, output);
    require(output[0] == 0.25F && output[1] == -0.25F && cursor == 2, "PCM volume mixing failed.");
    detail::mix(clip, cursor, 0.0F, output);
    require(output[0] == 0.0F && output[1] == 0.0F && cursor == 4, "Mute did not silence/advance PCM.");
    detail::mix(clip, cursor, 1.0F, output);
    require(output[0] == 0.0F && output[1] == 0.0F && cursor == 4, "Ended clip read beyond PCM.");
    std::cout << "Audio contract passed: WAV/Ogg, bounds, canonical paths, mute/gain, device-loss and silent fallback.\n";
    return 0;
}

int native_play() {
    using namespace pgp::audio;
    Player player(PGP_AUDIO_FIXTURE_ROOT, DeviceMode::NativeIfAvailable);
    require(player.load("tone.wav").ok, "Native fixture load failed.");
    const auto result = player.play();
    require(result.ok, "Native probe failed without recovery.");
    std::cout << result.message << "\nBackend: " << player.backend_name() << '\n';
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "After 300ms: playing=" << player.status().playing
              << " device_lost=" << player.status().device_lost << '\n';
    player.set_muted(true);
    player.set_volume(0.25F);
    require(player.play().ok, "Muted replay failed.");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    player.notify_device_lost();
    require(player.play().ok && player.backend_name() == "silent (device lost)", "Native device-loss seam failed.");
    player.stop();
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--native-play") return native_play();
        if (argc != 1) return 2;
        return contracts();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
