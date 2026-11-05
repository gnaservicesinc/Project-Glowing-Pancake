#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pgp::studio {
enum class Origin { reviewed, draft, engineering_fixture };
enum class Validation { validated, invalid, unsupported, blocked };
struct DefinitionEntry {
    std::string id;
    std::string kind;
    std::uint32_t revision;
    std::vector<std::string> tags;
    std::vector<std::string> uses;
    std::vector<std::string> used_by;
    std::string state{};
};
struct LibraryDocument {
    std::filesystem::path relative_path;
    Origin origin;
    Validation validation;
    std::string diagnostic;
    std::string content_version;
    std::string source_hash;
    std::string canonical_hash;
    std::vector<DefinitionEntry> definitions;
};
struct Library {
    std::filesystem::path checkout;
    std::uint32_t checkout_schema;
    std::vector<LibraryDocument> documents;
};

// Scans the selected development checkout only. Both unknown checkout
// schemas and failed documents are surfaced without interpreting their fields.
[[nodiscard]] Library inspect_checkout(const std::filesystem::path& selected);
} // namespace pgp::studio
