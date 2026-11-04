#pragma once

#include <string_view>

namespace pgp::core {

// Optimization/diagnostics configuration; product identity is in build_identity.hpp.
struct BuildConfiguration {
    std::string_view configuration;
    bool developer_diagnostics;
};

[[nodiscard]] BuildConfiguration build_configuration() noexcept;

} // namespace pgp::core
