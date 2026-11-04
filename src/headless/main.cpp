#include <pgp/core/build_configuration.hpp>

#include <iostream>
#include <string_view>

namespace {
constexpr std::string_view usage =
    "Usage: pgp-headless [--help | --build-info]\n"
    "Development scaffold; scenario simulation is not implemented yet.\n";
}

int main(int argc, char* argv[]) {
    if (argc == 1 || (argc == 2 && std::string_view{argv[1]} == "--help")) {
        std::cout << usage;
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--build-info") {
        const auto build = pgp::core::build_configuration();
        std::cout << "Project Glowing Pancake development scaffold\n"
                  << "Configuration: " << build.configuration << '\n'
                  << "Developer diagnostics: " << (build.developer_diagnostics ? "on" : "off") << '\n'
                  << "Simulation: not implemented\n";
        return 0;
    }
    std::cerr << "Unknown or extra arguments.\n" << usage;
    return 2;
}
