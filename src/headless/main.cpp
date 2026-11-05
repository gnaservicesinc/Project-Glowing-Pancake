#include <pgp/core/build_identity.hpp>
#include "foundation_demo.hpp"
#include "content_validation.hpp"
#include "flow_demo.hpp"
#include "accounts_demo.hpp"
#include "market_demo.hpp"
#include "business_run.hpp"
#include "lab_demo.hpp"
#include "events_demo.hpp"

#include <iostream>
#include <string_view>

namespace {
constexpr std::string_view usage =
    "Usage: pgp-headless [--help | --build-info | --foundation-demo | --validate-content FILE | --accounts-demo FILE | --capacity-demo FILE | --market-demo FILE | --business-run FILE STRATEGY | --flow-demo FILE [--save FILE]]\n"
    "Also: --lab-demo FILE | --events-demo FILE\nBusiness strategies: expansion, baseline, rescue. Builds never launch the application.\n";
}

int main(int argc, char* argv[]) {
    if (argc == 1 || (argc == 2 && std::string_view{argv[1]} == "--help")) {
        std::cout << usage;
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--build-info") {
        std::cout << "Project Glowing Pancake development scaffold\n";
        pgp::core::write_build_identity(std::cout);
        std::cout << "Simulation: offline business accounts, resources, contracts and liquidity recovery\n";
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--foundation-demo") {
        try { run_foundation_demo(std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string_view{argv[1]} == "--validate-content")
        return validate_content_file(argv[2], std::cout, std::cerr);
    if (argc == 3 && std::string_view{argv[1]} == "--accounts-demo") {
        try { run_accounts_demo(argv[2], std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string_view{argv[1]} == "--lab-demo") {
        try { run_lab_demo(argv[2],std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string_view{argv[1]} == "--events-demo") {
        try { run_events_demo(argv[2],std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string_view{argv[1]} == "--capacity-demo") {
        try { run_capacity_demo(argv[2], std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 3 && std::string_view{argv[1]} == "--market-demo") {
        try { run_market_demo(argv[2], std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 4 && std::string_view{argv[1]} == "--business-run") {
        try { run_business_file(argv[2],argv[3],std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if ((argc == 3 || (argc == 5 && std::string_view{argv[3]} == "--save")) &&
        std::string_view{argv[1]} == "--flow-demo") {
        try { run_flow_demo(argv[2], argc == 5 ? argv[4] : nullptr, std::cout); return 0; }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    std::cerr << "Unknown or extra arguments.\n" << usage;
    return 2;
}
