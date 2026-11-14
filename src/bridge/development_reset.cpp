#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

namespace pgp::bridge {
void CampaignController::resetLocalData() {
    if(!beginOperation(true)) return;
    resetting_local_data_=true;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_]{worker->resetLocalData(token,false);},Qt::QueuedConnection);
}

void CampaignController::deleteCurrentGame() {
    if(!beginOperation(true)) return;
    resetting_local_data_=true;
    QMetaObject::invokeMethod(worker_,[worker=worker_,token=token_]{worker->resetLocalData(token,true);},Qt::QueuedConnection);
}

void CampaignWorker::resetLocalData(std::uint64_t token, bool current_only) {
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
        auto initial=runtime::begin_scenario(*installed_,"scenario.first_playable","US-CA");
        initial.world.developer_playtest=campaign_.checkpoint().world.developer_playtest;
        session::Campaign next{*installed_,initial,paths_.saves_directory/"autosaves"/persistence::save_identity(*installed_).content_hash,
            current_only ? campaign_.settings() : persistence::Settings{}};
        if(current_only) {
            const auto continuing=paths_.saves_directory/"continuing";
            if(fs::is_symlink(continuing)) throw std::runtime_error("The continuing campaign folder is redirected.");
            for(const auto* name:{"current-days.json.tmp","current-days.json.bak","current-days.json",
                                  "current.pgpsave.tmp","current.pgpsave.bak","current.pgpsave"})
                fs::remove(continuing/name);
        } else {
            // Keep the writer lock while clearing this app's save directory.
            for(const auto& entry:fs::directory_iterator(paths_.saves_directory))
                if(entry.path().filename()!="current-campaign.lock") fs::remove_all(entry.path());
            for(const auto& suffix:{"",".bak",".tmp"}) fs::remove(fs::path{paths_.settings_file.string()+suffix});
            fs::remove(root/"founding-history.ini");
            fs::remove_all(root/"world-navigation");
        }
        for(const auto& name:{"founding-draft.json","founding-draft.json.bak","founding-draft.json.tmp"}) fs::remove(root/name);
        campaign_=std::move(next);current_=persistence::CurrentCampaign(paths_.saves_directory/"continuing");
        active_content_=installed_;active_=false;storage_blocked_=false;
        content_review_.reset();content_target_.clear();missing_content_hash_.clear();milestone_.reset();
        ++generation_;publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,current_only
            ? QStringLiteral("Current game deleted. Ready for a new game.")
            : QStringLiteral("Local playtest data reset. Ready for a new game."));
    } catch(const std::exception& error) {
        storage_blocked_=true;publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,QString::fromUtf8(error.what()));
    }
}
}
