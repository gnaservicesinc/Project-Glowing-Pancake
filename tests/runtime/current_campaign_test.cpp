#include "integration_fixture.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace pgp;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
}
int main() {
    const auto root=std::filesystem::path{PGP_INTEGRATION_SCRATCH_PATH}/("continuing-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto program=integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH,false);
        auto initial=runtime::begin_scenario(program,"scenario.foundation","US-CA");
        initial.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime engine{program,initial};
        require(engine.prepare_opening(initial.world.simulation.revision).accepted(),"Prepare opening");
        persistence::CurrentCampaign current{root};
        require(!current.resume(program),"Missing campaign is empty without writes");
        require(!std::filesystem::exists(root),"Read-only construction and missing resume");
        current.commit(program,engine.checkpoint());
        for(int day=0;day<34;++day) {
            require(engine.submit({engine.checkpoint().world.simulation.revision,core::AdvanceDay{}}).accepted(),"Advance day");
            current.commit(program,engine.checkpoint(),true);
            persistence::CurrentCampaign reopen{root};
            require(reopen.resume(program)==engine.checkpoint(),"Exact daily resume including compaction");
        }
        require(engine.submit({engine.checkpoint().world.simulation.revision,core::ReserveFunds{"Budget",core::Cents{10},true}}).accepted(),"Non-day command");
        current.commit(program,engine.checkpoint());
        persistence::CurrentCampaign reopen{root};
        require(reopen.resume(program)==engine.checkpoint(),"Command checkpoint supersedes old journal without replaying twice");
        {std::ofstream pending(root/"current-days.json.tmp");pending<<"uncommitted";}
        bool failed=false;
        try {current.commit(program,engine.checkpoint(),true);} catch(const persistence::PersistenceError&) {failed=true;}
        require(failed,"Pending file refuses day journal write");
        persistence::CurrentCampaign after_failure{root};
        require(after_failure.resume(program)==engine.checkpoint(),"Failed write retains last committed state");
        current.archive(program,engine.checkpoint());
        {std::ofstream corrupt(current.path(),std::ios::trunc);corrupt<<"damaged";}
        failed=false;
        try {(void)after_failure.resume(program);} catch(const persistence::PersistenceError&) {failed=true;}
        require(failed,"Corrupt current fails closed");
        require(std::filesystem::exists(root/"current.pgpsave.bak"),"Recovery copy survives");
        // This entire isolated scratch tree belongs to the test.
        std::filesystem::remove_all(root);
        std::cout<<"Continuing campaign exact replay, compaction and failure preservation passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<"\n";
        std::filesystem::remove_all(root);
        return 1;
    }
}
