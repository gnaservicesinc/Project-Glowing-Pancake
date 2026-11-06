#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace pgp;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Function> void refuses(Function function) {
    try { function(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid campaign setup was accepted");
}
}
int main() {
    try {
        const auto registry = content::foundation_registry();
        const auto program = runtime::compile_collection(content::load_collection(
            persistence::read_bounded(PGP_SETUP_SOURCE,content::maximum_document_bytes),registry),registry);
        require(runtime::parse_world_seed("0") == 0, "Zero is a valid world seed");
        const auto maximum = std::numeric_limits<std::uint64_t>::max();
        require(runtime::parse_world_seed("18446744073709551615") == maximum, "Full 64-bit seeds must retain every digit");
        for (const auto* seed : {"", "-1", "1.5", "1e6", " 1", "18446744073709551616"})
            refuses([&] { (void)runtime::parse_world_seed(seed); });
        const auto generated = runtime::generate_campaign_identity(42);
        runtime::validate_campaign_identity(generated);
        require(generated == runtime::generate_campaign_identity(42), "Local names must be reproducible without a campaign RNG");
        refuses([] { runtime::validate_campaign_identity({"   ","Founder",""}); });
        refuses([] { runtime::validate_campaign_identity({"Company","Founder\n","they/them"}); });
        refuses([] { runtime::validate_campaign_identity({"Company",std::string(1,static_cast<char>(0xff)),""}); });

        const auto original = runtime::begin_scenario(program,"scenario.century_frontier","US-CA");
        const auto old_bytes = persistence::encode_save(program,original);
        const auto old_json = nlohmann::json::parse(old_bytes);
        require(!old_json.at("checkpoint").at("world").contains("identity") &&
            !old_json.at("checkpoint").at("world").at("simulation").at("succession").contains("founder_name"),
            "Default callers must retain the existing optional save shape");
        require(persistence::decode_save(program,old_bytes).checkpoint == original, "Legacy setup must reload exactly");

        runtime::ScenarioOptions options;
        options.seed = maximum;
        options.identity = runtime::CampaignIdentity{"Cedar & Co","Renée Chen","they/them"};
        auto custom = runtime::begin_scenario(program,"scenario.century_frontier","US-TX",{},options);
        require(custom == runtime::begin_scenario(program,"scenario.century_frontier","US-TX",{},options),
            "The same setup and seed must start the same world");
        require(custom.world.identity == options.identity && custom.world.root_seed == maximum &&
            custom.world.simulation.frontier->seed == maximum, "Custom identity and seed must reach the actual campaign");
        require(custom.world.simulation.succession->terms.front().name == "Renée Chen" &&
            custom.world.simulation.management->catalog.succession->founder_name != "Renée Chen",
            "The named founder must retain the authored leadership rules");
        require(custom.world.simulation.random != original.world.simulation.random,
            "A new world seed must change the authoritative random streams");
        auto forged = custom;
        forged.world.identity->founder_name = "Another founder";
        refuses([&] { runtime::validate_checkpoint(program,forged); });
        forged = custom;
        forged.world.root_seed ^= 1;
        refuses([&] { runtime::validate_checkpoint(program,forged); });

        custom.world.enabled_flows = std::vector<std::string>{};
        runtime::Runtime live{program,custom};
        require(live.prepare_opening(live.current_state().world.simulation.revision).accepted(), "The custom morning must prepare");
        require(live.submit({live.current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(),
            "The custom campaign must advance through ordinary commands");
        const auto bytes = persistence::encode_save(program,live.current_state());
        const auto loaded = persistence::decode_save(program,bytes);
        require(loaded.checkpoint == live.current_state(), "Custom setup must reload exactly with unchanged content identity");
        runtime::Runtime resumed{program,loaded.checkpoint};
        for (auto* game : {&live,&resumed})
            require(game->submit({game->current_state().world.simulation.revision,core::AdvanceDay{}}).accepted(), "The restored custom day must advance");
        require(live.current_state() == resumed.current_state(), "Custom-seed continuation must remain identical after reload");
        std::cout << "Campaign names, full-width seeds, legacy saves and next-day reload passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
