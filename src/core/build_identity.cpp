#include <pgp/core/build_identity.hpp>
#include <pgp/core/build_configuration.hpp>

#include "build_identity_data.hpp"

#include <ostream>

namespace pgp::core {

const BuildIdentity& build_identity() noexcept {
    return identity_data::identity;
}

void write_build_identity(std::ostream& output) {
    const auto& identity = build_identity();
    const auto configuration = build_configuration();
    output << "Project: " << identity.project_id << '\n'
           << "Phase: " << identity.phase << " (package qualification required separately)\n"
           << "Game: " << identity.game_version << '\n'
           << "Editor: " << identity.editor_version << '\n'
           << "Content: " << identity.content_version << '\n'
           << "Content schema: " << identity.content_schema << '\n'
           << "Simulation rules: " << identity.simulation_rules << '\n'
           << "Save format: " << identity.save_format << '\n'
           << "Configuration: " << configuration.configuration << '\n'
           << "Developer diagnostics: " << (configuration.developer_diagnostics ? "on" : "off") << '\n'
           << "Source commit (at configure): " << identity.source_commit << '\n'
           << "Source state (at configure): " << identity.source_state << '\n'
           << "Version manifest SHA-256: " << identity.manifest_sha256 << '\n'
           << "Runtime content hash: " << identity.runtime_content_hash << '\n'
           << "Target: " << identity.target_os << '/' << identity.target_architecture << '\n'
           << "Game artifact stem: " << identity.game_artifact_stem << '\n'
           << "Editor artifact stem: " << identity.editor_artifact_stem << '\n';
}

} // namespace pgp::core
