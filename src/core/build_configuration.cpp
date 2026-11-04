#include <pgp/core/build_configuration.hpp>

#if !defined(PGP_DEVELOPER_DIAGNOSTICS) || !defined(PGP_BUILD_CONFIGURATION)
#error "Build configuration must be supplied by the project build target"
#endif

namespace pgp::core {

BuildConfiguration build_configuration() noexcept {
#if PGP_DEVELOPER_DIAGNOSTICS
    constexpr bool diagnostics = true;
#else
    constexpr bool diagnostics = false;
#endif
    return {.configuration = PGP_BUILD_CONFIGURATION, .developer_diagnostics = diagnostics};
}

} // namespace pgp::core
