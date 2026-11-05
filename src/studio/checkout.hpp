#pragma once

#include <pgp/runtime/runtime.hpp>

#include <filesystem>
#include <string_view>

namespace pgp::studio::detail {
[[nodiscard]] std::filesystem::path checkout_root(const std::filesystem::path& selected);
[[nodiscard]] std::uint32_t checkout_schema(const std::filesystem::path& root);
[[nodiscard]] std::filesystem::file_status checked_path(const std::filesystem::path& root, const std::filesystem::path& relative);
[[nodiscard]] runtime::CompiledProgram compile_source(std::string_view bytes, const content::Registry& registry);
} // namespace pgp::studio::detail
