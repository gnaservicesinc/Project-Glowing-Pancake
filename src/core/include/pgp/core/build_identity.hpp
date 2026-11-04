#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

namespace pgp::core {

// Immutable configure-time metadata. It is independent of simulation/save state.
struct BuildIdentity {
    std::string_view project_id;
    std::string_view phase;
    std::string_view game_version;
    std::string_view editor_version;
    std::string_view content_version;
    std::uint32_t content_schema;
    std::uint32_t simulation_rules;
    std::uint32_t save_format;
    std::string_view source_commit;
    std::string_view source_state;
    std::string_view manifest_sha256;
    std::string_view runtime_content_hash;
    std::string_view target_os;
    std::string_view target_architecture;
    std::string_view game_artifact_stem;
    std::string_view editor_artifact_stem;
};

[[nodiscard]] const BuildIdentity& build_identity() noexcept;
void write_build_identity(std::ostream& output);

} // namespace pgp::core
