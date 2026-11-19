#pragma once

#include <pgp/runtime/runtime.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace pgp::runtime {

#if !PGP_DISTRIBUTION

struct PlaytestSetupConfig {
    std::string scenario_id = "scenario.founder_campaign";
    std::uint64_t seed = 20260402ULL;

    // Protagonist identity & biography
    std::string first_name = "Jordan";
    std::string last_name = "Mercer";
    std::string pronouns = "they/them";
    std::string gender = "nonbinary";
    std::string orientation = "questioning";
    core::PortraitRecipe appearance{};
    int birth_year = 1995;
    int launch_year = 2015;
    std::string family = "middle";
    std::string sociability = "outgoing";
    std::string activity = "sedentary";
    std::string disposition = "deliberate";
    std::string education = "Master's degree";
    std::vector<std::string> advantages{};
    std::vector<std::string> traits{};
    std::vector<std::string> skills{"ml_research", "programming"};
    std::uint32_t experience = 3;
    std::uint32_t research = 72;
    std::uint32_t engineering = 68;
    std::uint32_t evaluation = 64;
    std::uint32_t mission_fit = 85;
    std::uint32_t prestige = 20;
    std::uint32_t buzz = 10;
    core::Cents personal_cash{25000000}; // $250,000
    core::Cents personal_debt{0};

    // Company & governance
    std::string company_name = "Meridian AI Labs";
    std::string headquarters = "US-CA";
    std::string legal_form = "for_profit";
    std::string route = "self_funded";
    std::string mission = "research";
    std::string objective = "survival";
    std::string charter{};
    core::Cents company_cash{150000000}; // $1,500,000
    core::Cents company_debt{0};
    std::uint32_t founder_equity_bp = 8500;
    std::uint32_t founder_board_seats = 2;
    std::uint32_t outside_board_seats = 1;
    bool inherited_purpose = false;

    // Staff & relationships
    std::uint32_t extra_staff_count = 1;
    std::uint32_t staff_skill_level = 65;
    std::uint32_t best_friends_count = 1;

    // Premises, equipment & compute
    std::string office = "leased";
    std::uint32_t extra_laptops = 1;
    std::uint32_t extra_desktops = 1;
    core::MilliCUPerDay starting_compute{100000}; // 100 CU/day

    // Research, data, models & products
    std::vector<std::string> completed_research_ids{};
    std::vector<std::string> owned_dataset_ids{};
    bool include_starter_model = false;
    int industry_stage = 0;
    bool include_starter_product = false;

    // Optional God mode toggle
    bool playtest_unlimited = false;

    auto operator<=>(const PlaytestSetupConfig&) const = default;
};

[[nodiscard]] PlaytestSetupConfig default_playtest_setup(const CompiledProgram& program,
                                                         std::string_view scenario_id = "scenario.founder_campaign");
[[nodiscard]] std::vector<std::string> validate_playtest_setup(const CompiledProgram& program,
                                                               const PlaytestSetupConfig& config);
[[nodiscard]] Checkpoint begin_playtest_scenario(const CompiledProgram& program,
                                                 const PlaytestSetupConfig& config);

#endif

} // namespace pgp::runtime
