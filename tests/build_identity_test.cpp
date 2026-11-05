#include <pgp/core/build_identity.hpp>

#include <iostream>
#include <sstream>

int main() {
    const auto& identity = pgp::core::build_identity();
    if (identity.game_version != PGP_EXPECTED_GAME_VERSION ||
        identity.editor_version != PGP_EXPECTED_EDITOR_VERSION ||
        identity.content_version != PGP_EXPECTED_CONTENT_VERSION ||
        identity.phase != PGP_EXPECTED_PHASE ||
        identity.manifest_sha256 != PGP_EXPECTED_MANIFEST_SHA256) {
        std::cerr << "Runtime identity differs from the validated manifest\n";
        return 1;
    }
    if (identity.runtime_content_hash != PGP_EXPECTED_RUNTIME_CONTENT_HASH ||
        identity.content_schema == 0 || identity.simulation_rules == 0 || identity.save_format == 0) {
        std::cerr << "Missing compatibility identity or fabricated content hash\n";
        return 1;
    }
    if (identity.target_os.empty() || identity.target_architecture.empty() ||
        identity.game_artifact_stem == identity.editor_artifact_stem ||
        identity.source_commit.empty() || identity.source_state.empty()) {
        std::cerr << "Incomplete or confused product/source identity\n";
        return 1;
    }
    std::ostringstream output;
    pgp::core::write_build_identity(output);
    if (output.str().find("Source state (at configure):") == std::string::npos ||
        output.str().find("package qualification required separately") == std::string::npos) {
        std::cerr << "Diagnostics must expose metadata scope and qualification limit\n";
        return 1;
    }
    return 0;
}
