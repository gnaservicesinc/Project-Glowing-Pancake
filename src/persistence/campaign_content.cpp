#include <pgp/persistence/campaign_content.hpp>
#include <pgp/persistence/sha256.hpp>
#include <algorithm>
#include <chrono>

namespace pgp::persistence {
std::filesystem::path CampaignContent::packet(std::string_view hash) const {
    if(hash.size()!=64 || std::ranges::any_of(hash,[](char c){return !((c>='0' && c<='9') || (c>='a' && c<='f'));}))
        throw PersistenceError("content_hash","The saved content hash is invalid");
    return directory_/(std::string{hash}+".json");
}
void CampaignContent::write(const runtime::CompiledProgram& program,bool recovery) {
    const auto hash=sha256(program.canonical_content_bytes);
    if(!recovery && verified_.contains(hash)) return;
    (void)save_identity(program);
    const auto target=packet(hash);std::error_code error;
    const bool exists=std::filesystem::exists(std::filesystem::symlink_status(target,error));
    if(error && error!=std::errc::no_such_file_or_directory) throw PersistenceError("io","Cannot inspect the company's original content");
    if(exists) {
        bool valid=false;
        try {valid=read_bounded(target,content::maximum_document_bytes)==program.canonical_content_bytes;}
        catch(const PersistenceError&) {}
        if(valid) {verified_.insert(hash);return;}
        if(!recovery) throw PersistenceError("content_integrity","The company's cached rules are damaged. Original files were preserved; locate the original collection to restore them.");
    }
    if(recovery) {
        for(const auto* suffix:{"",".tmp",".bak.tmp",".bak"}) {
            auto prior=target;prior+=suffix;
            const bool present=std::filesystem::exists(std::filesystem::symlink_status(prior,error));
            if(error && error!=std::errc::no_such_file_or_directory) throw PersistenceError("io","Cannot inspect original content recovery files");
            if(!present) continue;
            auto preserved=prior;preserved+=".preserved-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            if(std::filesystem::exists(std::filesystem::symlink_status(preserved))) throw PersistenceError("recovery_conflict","The preserved content path already exists");
            std::filesystem::rename(prior,preserved);
        }
        if(std::filesystem::exists(directory_)) flush_local_directory(directory_);
    }
    try {
        atomic_write(target,program.canonical_content_bytes,[&](std::string_view prior) {
            if(prior!=program.canonical_content_bytes) throw PersistenceError("content_integrity","Original content changed while it was being preserved");
        },content::maximum_document_bytes,false);
    }catch(const PersistenceError& failure) {
        if(failure.code()=="pending_write") throw PersistenceError("content_integrity","An unfinished original rules write was preserved. Review the matching collection to restore it.");
        throw;
    }
    verified_.insert(hash);
}
void CampaignContent::preserve(const runtime::CompiledProgram& program) {write(program,false);}
void CampaignContent::restore(const runtime::CompiledProgram& program) {write(program,true);}
std::shared_ptr<const runtime::CompiledProgram> CampaignContent::for_save(
    std::shared_ptr<const runtime::CompiledProgram> installed,const std::filesystem::path& save) const {
    const auto bytes=read_bounded(save);const auto required=inspect_save_identity(bytes);const auto current=save_identity(*installed);
    if(required==current) return installed;
    if(required.save_format!=current.save_format || required.simulation_rules!=current.simulation_rules ||
       required.content_schema!=current.content_schema || required.game_version!=current.game_version) {
        (void)decode_save(*installed,bytes);
        throw PersistenceError("content_identity","The save requires an unsupported game build");
    }
    const auto original=packet(required.content_hash);
    try {
        const auto source=read_bounded(original,content::maximum_document_bytes);
        if(sha256(source)!=required.content_hash) throw PersistenceError("content_integrity","Cached content checksum differs");
        const auto registry=content::foundation_registry();
        auto program=runtime::compile_collection(content::load_collection(source,registry),registry);
        if(program.canonical_content_bytes!=source || program.collection.content_version!=required.content_version)
            throw PersistenceError("content_identity","Cached content is not the original canonical collection");
        return std::make_shared<const runtime::CompiledProgram>(std::move(program));
    } catch(const std::exception&) {
        throw PersistenceError("content_unavailable","This company uses content "+required.content_version+". Locate its original collection to resume under the same rules. Company and recovery files were preserved.");
    }
}
}
