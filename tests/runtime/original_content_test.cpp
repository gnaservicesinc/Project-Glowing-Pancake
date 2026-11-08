#include "integration_fixture.hpp"
#include <pgp/persistence/campaign_content.hpp>
#include <pgp/persistence/current_campaign.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>

using namespace pgp;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void refuses(F action,const char* message) {try {action();}catch(const persistence::PersistenceError&){return;}throw std::runtime_error(message);}
void write(const std::filesystem::path& path,std::string_view bytes) {std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<bytes;if(!out) throw std::runtime_error("Fixture write failed");}
}
int main() {
    const auto root=std::filesystem::path{PGP_INTEGRATION_SCRATCH_PATH}/("content-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto document=nlohmann::json::parse(persistence::read_bounded(PGP_RUNTIME_FIXTURE_PATH));
        document["variables"]=nlohmann::json::array({{{"id","story.original"},{"scope","company"},{"type","integer"},{"unit","count"},{"minimum",0},{"maximum",10},{"default",2},{"help","Original rule."}}});
        const auto compile=[&] {const auto registry=content::foundation_registry();return runtime::compile_collection(content::load_collection(document.dump(),registry),registry);};
        const auto original=std::make_shared<const runtime::CompiledProgram>(compile());
        document["content_version"]="0.0.2-dev";document["variables"][0]["maximum"]=1;document["variables"][0]["default"]=0;
        const auto installed=std::make_shared<const runtime::CompiledProgram>(compile());
        auto checkpoint=runtime::begin_scenario(*original,"scenario.foundation","US-CA");checkpoint.world.enabled_flows=std::vector<std::string>{};
        runtime::Runtime engine{*original,checkpoint};check(engine.prepare_opening(checkpoint.world.simulation.revision).accepted(),"Opening");
        persistence::CurrentCampaign current{root/"continuing"};
        current.commit(*original,engine.checkpoint());
        const auto packet=current.content().packet(persistence::sha256(original->canonical_content_bytes));
        const auto modified=std::filesystem::last_write_time(packet);
        for(int day=0;day<3;++day) {check(engine.submit({engine.checkpoint().world.simulation.revision,core::AdvanceDay{}}).accepted(),"Day");current.commit(*original,engine.checkpoint(),true);}
        check(modified==std::filesystem::last_write_time(packet),"Daily journal must not rewrite original content");
        const auto save=persistence::read_bounded(current.path());const auto journal=persistence::read_bounded(root/"continuing/current-days.json");
        const auto selected=current.content().for_save(installed,current.path());
        check(selected->canonical_content_bytes==original->canonical_content_bytes,"Select original collection");
        persistence::CurrentCampaign reopen{root/"continuing"};check(reopen.resume(*selected)==engine.checkpoint(),"Journal resumes exactly with original rules");
        check(std::get<std::int64_t>(runtime::read_fact(engine.checkpoint().world,*selected->registry.fact("story.original"),{content::Scope::company,engine.checkpoint().world.company_id}))==2,"Original variable defaults use original registry");
        check(installed->collection.content_version=="0.0.2-dev","Installed new-company rules remain independent");
        refuses([&]{(void)persistence::decode_save(*installed,save);},"Changed rules cannot decode original save");
        std::filesystem::remove(packet);
        refuses([&]{(void)current.content().for_save(installed,current.path());},"Missing source must refuse");
        write(packet,"damaged");
        refuses([&]{(void)current.content().for_save(installed,current.path());},"Corrupt source must refuse");
        check(save==persistence::read_bounded(current.path()) && journal==persistence::read_bounded(root/"continuing/current-days.json"),"Failures preserve company and journal bytes");
        persistence::CampaignContent restore{root/"campaign-content"};restore.restore(*original);
        bool preserved=false;for(const auto& entry:std::filesystem::directory_iterator(packet.parent_path())) if(entry.path().filename().string().find(".preserved-")!=std::string::npos) preserved=persistence::read_bounded(entry.path())=="damaged";
        check(preserved && persistence::read_bounded(packet)==original->canonical_content_bytes,"Explicit source restoration preserves damaged original");
        std::filesystem::remove(packet);write(packet.string()+".tmp","unfinished");
        persistence::CampaignContent interrupted{root/"campaign-content"};
        refuses([&]{interrupted.preserve(*original);},"Unfinished content write must be preserved");
        interrupted.restore(*original);
        check(persistence::read_bounded(packet)==original->canonical_content_bytes && !std::filesystem::exists(packet.string()+".tmp"),"Explicit restoration retains pending data and installs the verified collection");
        bool pending_preserved=false;for(const auto& entry:std::filesystem::directory_iterator(packet.parent_path())) if(entry.path().filename().string().find(".tmp.preserved-")!=std::string::npos) pending_preserved=persistence::read_bounded(entry.path())=="unfinished";
        check(pending_preserved,"Pending original content bytes are retained");
        refuses([&]{(void)restore.packet("../../company");},"Content hash must be a bounded lowercase digest");
        auto damaged=save;damaged.back()='!';write(root/"damaged.pgpsave",damaged);
        refuses([&]{(void)restore.for_save(installed,root/"damaged.pgpsave");},"Invalid envelope cannot choose content");
        auto unsupported=nlohmann::json::parse(save);unsupported["identity"]["simulation_rules"]=999;unsupported.erase("sha256");unsupported["sha256"]=persistence::sha256(unsupported.dump());write(root/"future.pgpsave",unsupported.dump());
        refuses([&]{(void)restore.for_save(installed,root/"future.pgpsave");},"Unsupported simulation rules cannot use content fallback");
        std::filesystem::remove_all(root);std::cout<<"Original content, exact journal replay, registry isolation and recovery preservation passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
