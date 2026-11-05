#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pgp::persistence {
inline constexpr std::size_t default_maximum_save_bytes = 25U * 1024U * 1024U;
class PersistenceError : public std::runtime_error {
public:
    PersistenceError(std::string code, std::string detail);
    [[nodiscard]] const std::string& code() const noexcept { return code_; }
private:
    std::string code_;
};
[[nodiscard]] std::string read_bounded(const std::filesystem::path& path,
                                      std::size_t maximum_bytes = default_maximum_save_bytes);
// Existing files must validate before replacement. Writes a flushed temporary
// sibling, preserves the prior valid bytes as .bak, then atomically replaces.
// An existing .tmp is preserved and causes a refusal, never blind cleanup.
// An explicit existence expectation can reject newly created or removed input.
void atomic_write(const std::filesystem::path& path, std::string_view bytes,
                  const std::function<void(std::string_view)>& validate_existing,
                  std::size_t maximum_bytes = default_maximum_save_bytes,
                  std::optional<bool> expected_exists = std::nullopt);

// No directory is created by this query. Portable callers may choose another
// local directory explicitly. No account name/device identity is recorded.
[[nodiscard]] std::filesystem::path standard_data_directory();
[[nodiscard]] std::filesystem::path standard_settings_directory();
struct LocalPaths {
    std::filesystem::path saves_directory;
    std::filesystem::path settings_file;
};
[[nodiscard]] LocalPaths local_paths(std::optional<std::filesystem::path> portable_root = std::nullopt);
}
