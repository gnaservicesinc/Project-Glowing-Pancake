#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

// TODO(v1.0): remove this implementation and its menu/API entry before shipping.
// Human testers currently use optimized Release builds, so it is included now.
namespace pgp::bridge {
void CampaignController::resetLocalData() {
    if(!beginOperation(true)) return;
    resetting_local_data_=true;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_]{worker->resetLocalData(token);},Qt::QueuedConnection);
}

void CampaignWorker::resetLocalData(std::uint64_t token) {
    requireOwningThread();
    pauseDriver();timer_->stop();
    try {
        if(!writer_lock_ || !writer_lock_->isLocked())
            throw std::runtime_error("Close other game windows before resetting local data.");
        namespace fs=std::filesystem;
        const auto root=paths_.saves_directory.parent_path();
        if(paths_.saves_directory.filename()!="saves" || root.empty() || root==root.root_path() ||
            fs::is_symlink(root) || fs::is_symlink(paths_.saves_directory))
            throw std::runtime_error("The game data directory cannot be reset through a redirected path.");
        const auto initial=runtime::begin_scenario(*installed_,"scenario.first_playable","US-CA");
        session::Campaign next{*installed_,initial,paths_.saves_directory/"autosaves"/persistence::save_identity(*installed_).content_hash,{}};
        // Keep the writer lock while clearing this app's save directory. A
        // symlink inside it is removed as a link, never followed.
        for(const auto& entry:fs::directory_iterator(paths_.saves_directory))
            if(entry.path().filename()!="current-campaign.lock") fs::remove_all(entry.path());
        fs::remove_all(root/"world-navigation");
        for(const auto& name:{"founding-draft.json","founding-draft.json.bak","founding-draft.json.tmp"}) fs::remove(root/name);
        for(const auto& suffix:{"",".bak",".tmp"}) fs::remove(fs::path{paths_.settings_file.string()+suffix});
        campaign_=std::move(next);current_=persistence::CurrentCampaign(paths_.saves_directory/"continuing");
        active_content_=installed_;active_=false;storage_blocked_=false;
        content_review_.reset();content_target_.clear();missing_content_hash_.clear();milestone_.reset();
        ++generation_;publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("Local playtest data reset. Ready for a new game."));
    } catch(const std::exception& error) {
        storage_blocked_=true;publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,QString::fromUtf8(error.what()));
    }
}
}
