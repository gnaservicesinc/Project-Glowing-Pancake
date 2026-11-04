#pragma once

#include <string_view>

namespace pgp::core {

// Development configuration only. Product identity is the separate M0-05 task.
struct BuildConfiguration {
    std::string_view configuration;
    bool developer_diagnostics;
};

[[nodiscard]] BuildConfiguration build_configuration() noexcept;

} // namespace pgp::core
