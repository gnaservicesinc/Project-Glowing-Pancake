#include <pgp/persistence/save.hpp>
#include <pgp/persistence/settings.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace pgp;
using namespace pgp::persistence;
using Json=nlohmann::json;
std::size_t checks=0;
void check(bool condition,const char* message) {
    ++checks;if (!condition) throw std::runtime_error(message);
}
template <class Operation> void rejects(Operation operation,std::string_view reason={}) {
    ++checks;
    try {operation();}
    catch (const PersistenceError& error) {
        if (!reason.empty()&&error.code()!=reason)
            throw std::runtime_error("Unexpected persistence rejection: "+error.code()+" expected "+std::string(reason));
        return;
    }
    throw std::runtime_error("Expected persistence refusal");
}
void raw_write(const std::filesystem::path& path,std::string_view bytes) {
    std::ofstream file(path,std::ios::binary);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("Cannot write test file");
}
struct OwnedDirectory {
    std::filesystem::path path;
    OwnedDirectory() {
        const auto base=std::filesystem::path{PGP_PERSISTENCE_TEST_DIRECTORY};
        std::filesystem::create_directories(base);
        for (unsigned index=0;index<100;++index) {
            auto candidate=base/("case-"+std::to_string(index));
            if (std::filesystem::create_directory(candidate)) {path=std::move(candidate);return;}
        }
        throw std::runtime_error("No isolated persistence-test directory available");
    }
    ~OwnedDirectory() {std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
runtime::CompiledProgram program() {
    return runtime::compile_collection(content::load_collection(read_bounded(PGP_CONTENT_FIXTURE_PATH),content::foundation_registry()),
                                       content::foundation_registry());
}
runtime::Checkpoint fixture(const runtime::CompiledProgram& compiled) {
    auto world=runtime::foundation_world(core::Cents{100'000});
    world.enabled_flows=std::vector<std::string>{};
    world.root_seed=std::numeric_limits<std::uint64_t>::max();
    world.simulation.random=core::RandomStreams{world.root_seed}.snapshot();
    world.simulation.random[0]=std::numeric_limits<std::uint64_t>::max();
    world.simulation.random[1]=0;
    world.counterparty_cash=core::Cents{1000};
    core::Simulation commands{world.simulation};
    const auto reservation=commands.submit({commands.snapshot()->revision,core::ReserveFunds{"Delivery",core::Cents{5000},false}});
    check(reservation.accepted(),"Fixture reservation");
    world.simulation=commands.checkpoint();
    world.simulation.opening_actions.push_back({1,core::OrdinalDay{0},*reservation.reservation_id,core::Cents{1000},true,"Due fixture choice"});
    world.simulation.phase=core::DayPhase::opening;
    world.simulation=core::Simulation{world.simulation}.checkpoint();
    runtime::Checkpoint result{world,{}};
    runtime::validate_checkpoint(compiled,result);
    return result;
}
std::string rehash(Json body) {
    body.erase("sha256");body["sha256"]=sha256(body.dump());return body.dump();
}
void hashes() {
    check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","Empty SHA256 vector");
    check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc vector");
    check(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA256 two-block padding vector");
    constexpr std::array<std::pair<std::size_t,std::string_view>,5> binary_vectors{{
        {55,"463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
        {56,"da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
        {63,"29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
        {64,"fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
        {65,"4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"}}};
    for (const auto& [size,expected]:binary_vectors) {
        std::string bytes;for (std::size_t index=0;index<size;++index) bytes.push_back(static_cast<char>(index));
        check(sha256(bytes)==expected,"Independent binary SHA256 block/padding boundary vector");
    }
    check(sha256(std::string(1'000'000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA256 million-a vector");
}
void codec(const runtime::CompiledProgram& compiled,const runtime::Checkpoint& checkpoint) {
    const auto bytes=encode_save(compiled,checkpoint);
    const auto loaded=decode_save(compiled,bytes);
    check(loaded.checkpoint==checkpoint,"Whole core/runtime checkpoint round-trips exactly");
    check(loaded.identity.content_hash==sha256(compiled.canonical_content_bytes),"Identity hashes canonical definitions");
    check(loaded.checkpoint.world.root_seed==std::numeric_limits<std::uint64_t>::max() &&
          loaded.checkpoint.world.simulation.random[0]==std::numeric_limits<std::uint64_t>::max(),"Full unsigned seed/RNG preserved");
    check(loaded.checkpoint.world.simulation.phase==core::DayPhase::awaiting_choice,"Suspended opening phase/cursor preserved");
    check(encode_save(compiled,loaded.checkpoint)==bytes,"Save serialization deterministic");
    check(encode_save(compiled,checkpoint,99)!=bytes && decode_save(compiled,encode_save(compiled,checkpoint,99)).autosave_sequence==99,
          "Autosave generation is explicit envelope metadata");
    check(!bytes.empty()&&bytes.size()<25U*1024U*1024U,"Engineering save below configured size cap");
    auto value=Json::parse(bytes);
    auto corrupt=value;corrupt["checkpoint"]["world"]["simulation"]["cash_cents"]=200000;
    rejects([&]{(void)decode_save(compiled,corrupt.dump());},"integrity");
    auto future=value;future["identity"]["save_format"]=2;
    rejects([&]{(void)decode_save(compiled,future.dump());},"future_save_format");
    auto old=value;old["identity"]["save_format"]=0;
    rejects([&]{(void)decode_save(compiled,old.dump());},"save_format");
    auto rules=value;rules["identity"]["simulation_rules"]=2;
    rejects([&]{(void)decode_save(compiled,rules.dump());},"simulation_rules");
    auto content=value;content["identity"]["content_hash"]=std::string(64,'0');
    rejects([&]{(void)decode_save(compiled,content.dump());},"content_identity");
    auto scalar=value;scalar["checkpoint"]["world"]["simulation"]["paused"]=1;
    rejects([&]{(void)decode_save(compiled,rehash(scalar));},"invalid_state");
    auto floating=value;floating["checkpoint"]["world"]["root_seed"]=0.5;
    rejects([&]{(void)decode_save(compiled,floating.dump());},"integer_token");
    auto out_of_range=value;out_of_range["checkpoint"]["world"]["simulation"]["cash_cents"]=std::numeric_limits<std::uint64_t>::max();
    rejects([&]{(void)decode_save(compiled,rehash(out_of_range));},"invalid_state");
    auto missing_stream=value;missing_stream["checkpoint"]["world"]["simulation"]["random_streams"].erase("narrative");
    rejects([&]{(void)decode_save(compiled,rehash(missing_stream));},"invalid_state");
    auto cursor=value;cursor["checkpoint"]["world"]["simulation"]["opening_cursor"]=5;
    rejects([&]{(void)decode_save(compiled,rehash(cursor));},"invalid_state");
    auto reference=value;reference["checkpoint"]["world"]["simulation"]["opening_actions"][0]["reservation_id"]=999;
    rejects([&]{(void)decode_save(compiled,rehash(reference));},"invalid_state");
    auto unknown=value;unknown["checkpoint"]["world"]["upload_url"]="https://invalid.example";
    rejects([&]{(void)decode_save(compiled,rehash(unknown));},"invalid_state");
    auto duplicate=bytes;duplicate.insert(1,"\"format\":\"duplicate\",");
    rejects([&]{(void)decode_save(compiled,duplicate);},"duplicate_key");
    rejects([&]{(void)decode_save(compiled,"{\"format\":\"\xff\"}");},"json");
    rejects([&]{(void)decode_save(compiled,bytes,SaveLimits{bytes.size()-1});},"oversized");
    rejects([&]{(void)encode_save(compiled,checkpoint,0,SaveLimits{bytes.size()-1});},"oversized");
    rejects([&]{(void)encode_save(compiled,checkpoint,0,SaveLimits{bytes.size()+1,10});},"budget");
    rejects([&]{(void)decode_save(compiled,bytes,SaveLimits{bytes.size()+1,10});},"budget");
    rejects([&]{(void)decode_save(compiled,bytes,SaveLimits{0});},"limit");
    std::string deep="0";for (int depth=0;depth<80;++depth) deep="{\"x\":"+deep+"}";
    rejects([&]{(void)decode_save(compiled,deep);},"budget");
    check(checkpoint==loaded.checkpoint,"All decode failures leave live input checkpoint unchanged");
    auto stale_program=compiled;stale_program.canonical_content_bytes+=" ";
    rejects([&]{(void)decode_save(stale_program,bytes);},"program_identity");
    rejects([&]{validate_save_roundtrips(stale_program,std::span{&checkpoint,std::size_t{1}});},"program_identity");
}
void runtime_codec(const runtime::CompiledProgram& compiled) {
    auto world=runtime::foundation_world(core::Cents{100000});
    world.enabled_flows=std::vector<std::string>{"flow.hidden.sequence_resume","flow.hidden.sustained_workload",
        "flow.news.monthly_operations","flow.supplier.exclusivity_offer"};
    world.facts["company.monthly_compute_bill_cents"]=std::int64_t{2000000};
    world.facts["company.workload_stress_basis_points"]=std::int64_t{8000};
    runtime::Runtime uninterrupted{compiled,{world,{}}};
    check(uninterrupted.prepare_opening(world.simulation.revision).accepted(),"Runtime fixture prepares real flows");
    const auto waiting=uninterrupted.checkpoint();
    check(waiting.runtime.modifiers.size()==1,"Continuous owned modifier exists before save");
    const auto delayed=std::find_if(waiting.runtime.activations.begin(),waiting.runtime.activations.end(),[](const auto& activation){
        return activation.definition_id=="flow.hidden.sequence_resume";
    });
    check(delayed!=waiting.runtime.activations.end()&&delayed->frames.size()==2&&delayed->wait_started.has_value(),
          "Nested sequence delay has explicit continuation frames and wait date");
    const auto pending=std::find_if(waiting.runtime.presentations.begin(),waiting.runtime.presentations.end(),[](const auto& presentation){
        return presentation.kind==runtime::PresentationKind::dialogue&&!presentation.resolved;
    });
    check(pending!=waiting.runtime.presentations.end(),"Runtime fixture has actual pending dialogue");
    const auto roundtrip=decode_save(compiled,encode_save(compiled,waiting));
    check(roundtrip.checkpoint==waiting,"Nested frames, interval anchors, owned modifiers and dialogue round-trip exactly");
    runtime::Runtime resumed{compiled,roundtrip.checkpoint};
    check(resumed.prepare_opening(waiting.world.simulation.revision).accepted()&&resumed.checkpoint()==waiting,
          "Loading prepared/suspended flow cannot repeat its effects or dialogue");
    check(uninterrupted.choose(waiting.world.simulation.revision,pending->id,"review").accepted()&&
          resumed.choose(waiting.world.simulation.revision,pending->id,"review").accepted(),"Saved choice commits on both paths");
    check(uninterrupted.checkpoint()==resumed.checkpoint(),"Choice resumes identical whole state");
    for (int day=0;day<15;++day) {
        const auto revision=uninterrupted.checkpoint().world.simulation.revision;
        check(uninterrupted.submit({revision,core::AdvanceDay{}}).accepted()&&resumed.submit({revision,core::AdvanceDay{}}).accepted(),
              "Matched saved/uninterrupted daily continuation");
        check(uninterrupted.checkpoint()==resumed.checkpoint(),"Daily whole checkpoint parity after saved nested delay");
    }
    const auto completed=resumed.checkpoint();
    check(std::count(completed.world.decisions.begin(),completed.world.decisions.end(),"inner_done")==1&&
          std::count(completed.world.decisions.begin(),completed.world.decisions.end(),"outer_done")==1,
          "Saved nested delay commits later siblings exactly once");
    check(decode_save(compiled,encode_save(compiled,completed)).checkpoint==completed,
          "Resolved choice, effect receipts and completed continuations round-trip");
    auto tampered=Json::parse(encode_save(compiled,waiting));
    tampered["checkpoint"]["runtime"]["activations"][0]["frames"][0]["return_node"]="missing_node";
    rejects([&]{(void)decode_save(compiled,rehash(tampered));},"invalid_state");
    auto changed_day=Json::parse(encode_save(compiled,waiting));
    changed_day["checkpoint"]["runtime"]["activations"][0]["due"]=999;
    rejects([&]{(void)decode_save(compiled,rehash(changed_day));},"invalid_state");
}
void storage(const runtime::CompiledProgram& compiled,const runtime::Checkpoint& checkpoint,const std::filesystem::path& directory) {
    const auto path=directory/"session.pgpsave";
    save_file(compiled,checkpoint,path);
    const auto first=read_bounded(path);
    check(load_file(compiled,path).checkpoint==checkpoint,"Initial local save loads");
    auto advanced=checkpoint;advanced.world.simulation.random[2]=987654321;
    save_file(compiled,advanced,path);
    const auto second=read_bounded(path);
    check(first!=second && load_file(compiled,path).checkpoint==advanced,"Atomic replacement commits new complete save");
    auto backup=path;backup+=".bak";
    check(read_bounded(backup)==first && load_file(compiled,backup).checkpoint==checkpoint,"Prior valid save retained byte-for-byte");
    check(!std::filesystem::exists(std::filesystem::path{path.string()+".tmp"}),"Successful commit leaves no temporary sibling");
    const auto exported=directory/"export.pgpsave";save_file(compiled,checkpoint,exported);
    check(read_bounded(exported)==first && read_bounded(path)==second,"Explicit export leaves original save intact");
    const auto unicode_path=directory/std::filesystem::path{u8"save-\u6d4b\u8bd5-\u00e9.pgpsave"};
    save_file(compiled,checkpoint,unicode_path);save_file(compiled,advanced,unicode_path);
    auto unicode_backup=unicode_path;unicode_backup+=".bak";
    check(load_file(compiled,unicode_path).checkpoint==advanced&&load_file(compiled,unicode_backup).checkpoint==checkpoint,
          "Unicode save filename supports native reads, atomic replacement and prior backup");
    auto pending=path;pending+=".tmp";raw_write(pending,"foreign pending write");
    rejects([&]{save_file(compiled,checkpoint,path);},"pending_write");
    check(read_bounded(path)==second && read_bounded(backup)==first && read_bounded(pending)=="foreign pending write",
          "Pending sibling refusal preserves current/backup/foreign temporary data");
    std::filesystem::remove(pending);
    raw_write(path,"corrupt original");
    rejects([&]{(void)load_file(compiled,path);});
    rejects([&]{save_file(compiled,checkpoint,path);});
    check(read_bounded(path)=="corrupt original"&&read_bounded(backup)==first,"Corrupt original never overwritten or deleted");
    check(load_file(compiled,backup).checkpoint==checkpoint,"Recovery copy remains usable");
    rejects([&]{save_file(compiled,checkpoint,directory/"asset.json");},"extension");
    check(!std::filesystem::exists(directory/"asset.json"),"Save extension refusal creates no foreign file");
    const auto over_budget=directory/"over-budget.pgpsave";
    rejects([&]{save_file(compiled,checkpoint,over_budget,0,SaveLimits{default_maximum_save_bytes,10});},"budget");
    check(!std::filesystem::exists(over_budget),"Encoder budget refusal cannot write an unloadable save");
    rejects([&]{(void)read_bounded(directory/"missing.pgpsave");},"missing_file");
    rejects([&]{(void)read_bounded(directory);},"file_type");
    const auto conflict=directory/"conflict.pgpsave";raw_write(conflict,"before");
    rejects([&]{atomic_write(conflict,"new",[&](auto){raw_write(conflict,"external edit");});},"conflict");
    check(read_bounded(conflict)=="external edit","External edit during save preparation is preserved");
    std::error_code error;const auto link=directory/"linked.pgpsave";
    std::filesystem::create_symlink(exported,link,error);
    if (!error) {
        rejects([&]{(void)load_file(compiled,link);},"file_type");
        rejects([&]{save_file(compiled,checkpoint,link);},"file_type");
        check(read_bounded(exported)==first,"Symlink refusal preserves referenced original");
    }
}
void settings_and_ring(const runtime::CompiledProgram& compiled,const runtime::Checkpoint& checkpoint,const std::filesystem::path& directory) {
    const auto preferences=directory/"config"/"settings.json";
    check(load_settings(preferences)==Settings{},"Missing settings use defaults without writing");
    check(!std::filesystem::exists(preferences),"Reading defaults creates no file");
    auto settings=Settings{};settings.text_scale_percent=200;settings.autosave_enabled=false;
    settings.reduced_motion=true;settings.volume_percent=0;settings.compact_management=true;
    save_settings(preferences,settings);check(load_settings(preferences)==settings,"Local settings round-trip");
    const auto current=read_bounded(preferences);settings.volume_percent=20;save_settings(preferences,settings);
    auto backup=preferences;backup+=".bak";check(read_bounded(backup)==current,"Settings keep prior valid recovery copy");
    auto future=Json::parse(current);future["settings_format"]=2;
    rejects([&]{(void)decode_settings(future.dump());},"future_settings");
    auto personal=Json::parse(current);personal["device_id"]="forbidden";
    rejects([&]{(void)decode_settings(personal.dump());},"invalid_state");
    auto floating=Json::parse(current);floating["volume_percent"]=50.0;
    rejects([&]{(void)decode_settings(floating.dump());},"integer_token");
    auto bad_scale=settings;bad_scale.text_scale_percent=49;
    rejects([&]{(void)encode_settings(bad_scale);},"settings_range");
    const auto portable=local_paths(directory/"portable");
    check(portable.saves_directory==directory/"portable"/"saves" && portable.settings_file==directory/"portable"/"settings.json",
          "Explicit portable root owns both save/settings locations");
    check(!std::filesystem::exists(directory/"portable"),"Path discovery creates no directories");
    const auto standard=local_paths();check(standard.saves_directory.is_absolute()&&standard.settings_file.is_absolute(),"Platform standard paths are absolute");
    const auto disabled=directory/"disabled";AutosaveRing disabled_ring{disabled};
    check(!disabled_ring.write(compiled,checkpoint,AutosaveReason::before_irreversible_commitment,false) && !std::filesystem::exists(disabled),
          "Disabled autosave performs no file writes");
    const auto ring_directory=directory/"autosaves";AutosaveRing ring{ring_directory};
    for (std::uint64_t index=1;index<=5;++index) {
        auto changed=checkpoint;changed.world.simulation.random[3]=index;
        const auto target=ring.write(compiled,changed,AutosaveReason::before_irreversible_commitment);
        check(target.has_value()&&load_file(compiled,*target).autosave_sequence==index,"Three-slot autosave advances explicit generation");
    }
    std::size_t slots=0;for (const auto& entry:std::filesystem::directory_iterator(ring_directory))
        if (entry.path().extension()==".pgpsave") ++slots;
    check(slots==3 && ring.latest(compiled).autosave_sequence==5,"Autosave ring rotates exactly three primary slots");
    const auto backup_only_directory=directory/"backup-only";AutosaveRing backup_only{backup_only_directory};
    for (std::uint64_t index=1;index<=6;++index)
        (void)backup_only.write(compiled,checkpoint,AutosaveReason::before_irreversible_commitment);
    for (unsigned index=0;index<3;++index)
        std::filesystem::remove(backup_only_directory/("autosave-"+std::to_string(index)+".pgpsave"));
    check(backup_only.latest(compiled).autosave_sequence==3,"Backup-only ring retains prior generation history");
    const auto recovered=backup_only.write(compiled,checkpoint,AutosaveReason::before_irreversible_commitment);
    check(recovered&&load_file(compiled,*recovered).autosave_sequence==4&&backup_only.latest(compiled).autosave_sequence==4,
          "Backup-only ring never resets generation below retained recovery copies");
    raw_write(ring_directory/"autosave-1.pgpsave","damaged newest");
    check(ring.latest(compiled).autosave_sequence==4,"Autosave recovery chooses newest remaining valid save");
    rejects([&]{(void)ring.write(compiled,checkpoint,AutosaveReason::before_irreversible_commitment);});
    check(read_bounded(ring_directory/"autosave-1.pgpsave")=="damaged newest","Corrupt autosave preserved on failed replacement");
    auto prepared=checkpoint;prepared.world.simulation=core::Simulation{core::Cents{100000}}.checkpoint();
    AutosaveRing monthly{directory/"monthly"};
    check(monthly.write(compiled,prepared,AutosaveReason::monthly_boundary).has_value(),"Prepared first morning supports monthly autosave");
    auto final_state=prepared;
    final_state.world.simulation.date=core::OrdinalDay{36524};
    final_state.world.simulation.phase=core::DayPhase::finished;
    check(monthly.write(compiled,final_state,AutosaveReason::monthly_boundary).has_value(),
          "Exclusive campaign endpoint preserves the completed final month autosave");
    auto midmonth=prepared;
    midmonth.world.simulation.date=core::to_ordinal({2014,2,15});
    midmonth.world.simulation.end_date=midmonth.world.simulation.date;
    midmonth.world.simulation.phase=core::DayPhase::finished;
    check(monthly.write(compiled,midmonth,AutosaveReason::campaign_end).has_value(),
          "Campaign-end autosave accepts a valid scenario ending midway through a month");
    rejects([&]{(void)monthly.write(compiled,prepared,AutosaveReason::campaign_end);},"autosave_boundary");
    core::SimulationState january_state;
    january_state.date=core::to_ordinal({2014,1,31});january_state.cash=core::Cents{100000};
    core::Simulation january{january_state};
    const auto due_reservation=january.submit({january.snapshot()->revision,core::ReserveFunds{"First-morning delivery",core::Cents{1000},false}});
    check(due_reservation.accepted(),"Monthly-choice fixture reserves delivery funds");
    auto queued=january.checkpoint();
    queued.opening_actions.push_back({1,core::to_ordinal({2014,2,1}),*due_reservation.reservation_id,core::Cents{100},true,"Opening delivery choice"});
    january=core::Simulation{queued};
    check(january.submit({january.snapshot()->revision,core::AdvanceDay{}}).accepted(),"January settlement reaches suspended February opening");
    auto suspended_month=prepared;suspended_month.world.simulation=january.checkpoint();
    check(suspended_month.world.simulation.phase==core::DayPhase::awaiting_choice&&
          monthly.write(compiled,suspended_month,AutosaveReason::monthly_boundary).has_value(),
          "Completed prior month autosaves before a blocking first-morning core choice");
    prepared.world.simulation.date=core::OrdinalDay{1};
    rejects([&]{(void)monthly.write(compiled,prepared,AutosaveReason::monthly_boundary);},"autosave_boundary");
}
}
int main() {
    try {
        hashes();const auto compiled=program();const auto checkpoint=fixture(compiled);OwnedDirectory directory;
        codec(compiled,checkpoint);runtime_codec(compiled);storage(compiled,checkpoint,directory.path);settings_and_ring(compiled,checkpoint,directory.path);
        std::cout<<checks<<" local persistence, recovery, autosave and settings checks passed\n";
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
