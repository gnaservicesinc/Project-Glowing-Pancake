#include <pgp/core/build_configuration.hpp>

#include <iostream>

// Explicit checks remain active in Release; do not use assert() with NDEBUG.
int main() {
    const auto build = pgp::core::build_configuration();
    if (build.configuration != PGP_BUILD_CONFIGURATION) {
        std::cerr << "Core and executable configurations differ\n";
        return 1;
    }
    if (build.developer_diagnostics != (PGP_DEVELOPER_DIAGNOSTICS == 1)) {
        std::cerr << "Core and executable diagnostic settings differ\n";
        return 1;
    }
    if (build.developer_diagnostics != (build.configuration == "Debug")) {
        std::cerr << "Diagnostics must be enabled only for explicit Debug builds\n";
        return 1;
    }
    return 0;
}
