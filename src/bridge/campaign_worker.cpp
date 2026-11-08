#include "campaign_worker.hpp"

#include <QThread>
#include <QDir>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/sha256.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::bridge {
namespace {
bool pending_decision(const runtime::Checkpoint& checkpoint) {
    if(runtime::observing_world(checkpoint.world)) return false;
    const auto& state = checkpoint.world.simulation;
    return runtime::required_objective_review(checkpoint.world) || core::required_management_notice(state) ||
        core::required_site_alert(state) || state.phase == core::DayPhase::awaiting_choice ||
        (state.liquidity && core::liquidity_report(*state.liquidity).pending_warning.has_value()) ||
        std::any_of(checkpoint.runtime.presentations.begin(),checkpoint.runtime.presentations.end(),[](const auto& row) {
            return row.kind == runtime::PresentationKind::dialogue && !row.resolved;
        });
}
bool clock_ready(const runtime::Checkpoint& checkpoint) {
    if(runtime::objective_time_stopped(checkpoint.world) || runtime::campaign_paused(checkpoint.world) || pending_decision(checkpoint)) return false;
    if(runtime::observing_world(checkpoint.world)) return runtime::campaign_day(checkpoint.world)<runtime::campaign_calendar_limit(checkpoint.world);
    return checkpoint.world.simulation.phase==core::DayPhase::work_ready;
}
std::filesystem::path autosave_path(const persistence::LocalPaths& paths,const runtime::CompiledProgram& program) {return paths.saves_directory/"autosaves"/persistence::sha256(program.canonical_content_bytes);}
QString error_text(const std::exception& error) { return QString::fromUtf8(error.what()); }
}
CampaignWorker::CampaignWorker(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
    persistence::LocalPaths paths, persistence::Settings settings, QObject* parent, bool continuous)
    : QObject(parent),installed_(std::make_shared<const runtime::CompiledProgram>(std::move(program))),active_content_(installed_),
      campaign_(*installed_,std::move(checkpoint),autosave_path(paths,*installed_),settings),
      paths_(std::move(paths)),current_(paths_.saves_directory / "continuing"),continuous_(continuous),timer_(new QTimer(this)) {
    last_publish_.start();
    qRegisterMetaType<CampaignSnapshotPtr>();
    timer_->setInterval(1000);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_,&QTimer::timeout,this,&CampaignWorker::tick);
}
void CampaignWorker::requireOwningThread() const {
    if (QThread::currentThread() != thread()) throw std::logic_error("Campaign operation requires its worker thread");
}
void CampaignWorker::publish() {
    last_publish_.restart();
    auto checkpoint = campaign_.checkpoint();
    const bool running = !halted_ && clock_ready(checkpoint);
    if (running && !timer_->isActive()) timer_->start();
    if (!running) timer_->stop();
    emit snapshotReady(std::make_shared<const CampaignSnapshot>(CampaignSnapshot{
        generation_,std::move(checkpoint),campaign_.settings(),halted_,active_,storage_blocked_,active_content_,missing_content_hash_}));
}
void CampaignWorker::pauseDriver() {
    halted_ = true;
}
bool CampaignWorker::persist(bool advanced_day) {
    if (!continuous_ || !active_) return true;
    try {
        if (!writer_lock_ || !writer_lock_->isLocked()) throw std::runtime_error("This campaign is open in another game window. Close that window and relaunch.");
        current_.commit(campaign_.program(),campaign_.checkpoint(),advanced_day);
        storage_blocked_=false;
        return true;
    } catch (const std::exception& error) {
        storage_blocked_=true;pauseDriver();
        emit noticeRaised(QStringLiteral("Progress could not be secured. Time is paused. Retry before exiting. ") + error_text(error));
        return false;
    }
}
void CampaignWorker::retryPersistence(std::uint64_t token) {
    requireOwningThread();
    if(!active_ && continuous_) start();
    const bool success=active_ && persist();
    publish();
    emit operationCompleted(static_cast<qulonglong>(token),success,success ? QStringLiteral("Progress secured locally.") : QStringLiteral("Recovery is still required. Existing files are preserved."));
}
void CampaignWorker::complete(std::uint64_t token, const session::OperationResult& result, bool advanced_day) {
    if (result.command.accepted()) (void)persist(advanced_day);
    if (result.halt_requested || result.storage_notice) pauseDriver();
    QString message = QString::fromStdString(result.command.explanation);
    if (result.storage_notice) message += QStringLiteral(" ") + QString::fromStdString(result.storage_notice->explanation);
    if (result.pause_notice) {message=QString::fromStdString(*result.pause_notice);emit noticeRaised(message);}
    if (milestone_ && session::milestone_reached(campaign_.checkpoint(),*milestone_)) {
        pauseDriver();milestone_.reset();emit noticeRaised(QStringLiteral("Reached the chosen milestone. Time is paused at the prepared morning."));
    }
    if (!result.command.accepted() || result.storage_notice) emit noticeRaised(message);
    const auto& current=campaign_.checkpoint();
    if (token!=0 || halted_ || !clock_ready(current) ||
        !result.command.accepted() || last_publish_.elapsed()>=100) publish();
    if (token != 0) emit operationCompleted(static_cast<qulonglong>(token),result.command.accepted(),
        result.command.accepted() && !result.storage_notice ? QString{} : message);
}
void CampaignWorker::newCampaign(std::uint64_t token,std::string headquarters,bool stories,std::string id,
    std::optional<core::ObjectiveSelection> objectives,runtime::ScenarioOptions options) {
    requireOwningThread();
    try {
        if (continuous_ && (storage_blocked_ || (active_ && !runtime::objective_time_stopped(campaign_.checkpoint().world) &&
            campaign_.checkpoint().world.simulation.phase!=core::DayPhase::closed && campaign_.checkpoint().world.simulation.phase!=core::DayPhase::finished)))
            throw std::runtime_error("Finish the current company through Leadership or its campaign ending before starting another.");
        if (generation_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Campaign generation is exhausted");
        if (id.empty()) for (const auto& [candidate,definition]:installed_->collection.definitions) {
            const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);
            if (scenario && scenario->service_catalog) { id=candidate;break; }
        }
        const auto* selected=installed_->collection.find(id);
        const auto* selected_scenario=selected?std::get_if<content::ScenarioDefinition>(&selected->data):nullptr;
        if (!selected_scenario || !selected_scenario->service_catalog) throw std::invalid_argument("Choose a playable scenario from this content collection");
        auto initial=runtime::begin_scenario(*installed_,id,headquarters,std::move(objectives),std::move(options));
        if (!stories && initial.world.enabled_flows) {
            auto& roots=*initial.world.enabled_flows;
            roots.erase(std::remove_if(roots.begin(),roots.end(),[&](const auto& flow_id) {
                const auto& flow=std::get<content::FlowDefinition>(installed_->collection.find(flow_id)->data);
                return std::find(flow.tags.begin(),flow.tags.end(),"story")!=flow.tags.end();
            }),roots.end());
        }
        session::Campaign next{*installed_,std::move(initial),autosave_path(paths_,*installed_),campaign_.settings()};
        const auto prepared=next.prepare_opening(next.checkpoint().world.simulation.revision);
        if (!prepared.command.accepted()) throw std::runtime_error(prepared.command.explanation);
        if (continuous_) {
            if (!writer_lock_ || !writer_lock_->isLocked()) throw std::runtime_error("Another game window owns this campaign.");
            current_.content().restore(next.program());
            if (active_) current_.archive(campaign_.program(),campaign_.checkpoint());
            current_.commit(next.program(),next.checkpoint());
            active_=true;
        }
        campaign_=std::move(next);active_content_=installed_;missing_content_hash_.clear();++generation_;halted_=true;milestone_.reset();publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("New campaign started. Time is paused."));
    } catch (const std::exception& error) {
        emit noticeRaised(error_text(error));emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::start() {
    requireOwningThread();
    try {
        if (continuous_) {
            const auto lock_path=paths_.saves_directory / "current-campaign.lock";
            if (!QDir{}.mkpath(QString::fromStdU16String(paths_.saves_directory.u16string())))
                throw std::runtime_error("Cannot create the local campaign folder.");
            if(!writer_lock_) {
                writer_lock_=std::make_unique<QLockFile>(QString::fromStdU16String(lock_path.u16string()));
                writer_lock_->setStaleLockTime(0);
                if (!writer_lock_->tryLock()) throw std::runtime_error("The continuing campaign is open in another game window. Close that window and relaunch.");
            } else if(!writer_lock_->isLocked() && !writer_lock_->tryLock()) throw std::runtime_error("Another game window owns the continuing company.");
            std::error_code error;auto selected=installed_;
            if(std::filesystem::exists(current_.path(),error)) selected=current_.content().for_save(installed_,current_.path());
            if (auto checkpoint=current_.resume(*selected)) {
                current_.content().preserve(*selected);
                campaign_=session::Campaign{*selected,std::move(*checkpoint),autosave_path(paths_,*selected),campaign_.settings()};
                active_content_=std::move(selected);
                active_=true;storage_blocked_=false;missing_content_hash_.clear();content_target_.clear();halted_=true;publish();return;
            }
        }
        const auto state = campaign_.checkpoint().world.simulation;
        if (!runtime::objective_time_stopped(campaign_.checkpoint().world) && !runtime::observing_world(campaign_.checkpoint().world) &&
            state.phase != core::DayPhase::finished && state.phase != core::DayPhase::closed) {
            const auto result = campaign_.prepare_opening(state.revision);
            if (!result.command.accepted() || result.storage_notice) {
                complete(0,result);
                return;
            }
            const auto prepared = campaign_.checkpoint().world.simulation;
            if (!prepared.paused) (void)campaign_.submit({prepared.revision,core::SetPaused{true}});
        }
        publish();
    } catch (const std::exception& error) {
        if(continuous_) storage_blocked_=true;
        if(const auto* storage=dynamic_cast<const persistence::PersistenceError*>(&error);storage && (storage->code()=="content_unavailable" || storage->code()=="content_integrity")) {
            content_target_=current_.path();content_target_current_=true;
            try {missing_content_hash_=persistence::inspect_save_identity(persistence::read_bounded(content_target_)).content_hash;}catch(const std::exception&) {}
        }
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
    }
}
void CampaignWorker::advanceTo(std::uint64_t token,std::uint64_t revision,std::string kind) {
    requireOwningThread();
    try {
        if (campaign_.checkpoint().world.simulation.revision!=revision) throw std::invalid_argument("State changed before the milestone was selected.");
        milestone_=session::plan_milestone(campaign_.checkpoint(),kind);
        const auto result=campaign_.submit({revision,core::SetPaused{false}});
        if (result.command.accepted()) halted_=false;else milestone_.reset();
        complete(token,result);
    } catch (const std::exception& error) {milestone_.reset();emit noticeRaised(error_text(error));emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));}
}
void CampaignWorker::setSpeed(int rate) { requireOwningThread();if (rate==1 || rate==10 || rate==30 || rate==60) timer_->setInterval(1000/rate); }
void CampaignWorker::stop() { requireOwningThread(); timer_->stop(); }
void CampaignWorker::submitCommand(std::uint64_t token, core::Command command) {
    requireOwningThread();
    try {
        const auto* pause = std::get_if<core::SetPaused>(&command.action);
        // Stop time even if a timer tick landed between the displayed revision
        // and the button press. Commands with business consequences stay stale-checked.
        if (pause && pause->paused) command.expected_revision = campaign_.checkpoint().world.simulation.revision;
        if (pause) milestone_.reset();
        const auto result = campaign_.submit(command);
        if (pause && !pause->paused && result.command.accepted()) halted_ = false;
        complete(token,result,std::holds_alternative<core::AdvanceDay>(command.action));
    } catch (const std::exception& error) {
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::respondCompetitiveOffer(std::uint64_t token,std::uint64_t revision,std::string id,bool accept) {
    requireOwningThread();
    try {complete(token,campaign_.respond_competitive_offer(revision,id,accept));}
    catch (const std::exception& error) {pauseDriver();emit noticeRaised(error_text(error));publish();emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));}
}
void CampaignWorker::proposeAcquisition(std::uint64_t token,std::uint64_t revision,std::string id,bool equity) {
    requireOwningThread();
    try {complete(token,campaign_.propose_acquisition(revision,id,equity));}
    catch (const std::exception& error) {pauseDriver();emit noticeRaised(error_text(error));publish();emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));}
}
void CampaignWorker::signWholesale(std::uint64_t token,std::uint64_t revision,std::string plan,std::string source,std::string buyer) {
    requireOwningThread();
    try {complete(token,campaign_.sign_wholesale(revision,plan,source,buyer));}
    catch (const std::exception& error) {pauseDriver();emit noticeRaised(error_text(error));publish();emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));}
}
void CampaignWorker::transferSubsidiaryFunds(std::uint64_t token,std::uint64_t revision,std::string id,core::Cents amount,bool funding) {
    requireOwningThread();
    try {complete(token,campaign_.transfer_subsidiary_funds(revision,id,amount,funding));}
    catch (const std::exception& error) {pauseDriver();emit noticeRaised(error_text(error));publish();emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));}
}
void CampaignWorker::choose(std::uint64_t token,std::uint64_t expected_revision,
                            std::uint64_t presentation_id,std::string choice_id) {
    requireOwningThread();
    try { complete(token,campaign_.choose(expected_revision,presentation_id,choice_id)); }
    catch (const std::exception& error) {
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::tick() {
    requireOwningThread();
    const auto& checkpoint = campaign_.checkpoint();
    const auto& state = checkpoint.world.simulation;
    if (halted_ || !clock_ready(checkpoint)) return;
    try { complete(0,campaign_.submit({state.revision,core::AdvanceDay{}}),true); }
    catch (const std::exception& error) { pauseDriver(); emit noticeRaised(error_text(error)); publish(); }
}
void CampaignWorker::saveTo(std::uint64_t token,std::filesystem::path path) {
    requireOwningThread();
    try {
        campaign_.save(path);
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("Campaign saved locally."));
    } catch (const std::exception& error) {
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::loadFrom(std::uint64_t token,std::filesystem::path path) {
    requireOwningThread();
    try {
        if (generation_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Campaign generation is exhausted");
        if (continuous_) {
            if (!writer_lock_ || !writer_lock_->isLocked()) throw std::runtime_error("Another game window owns this campaign.");
            if (active_ && !storage_blocked_) throw std::runtime_error("A company is already in progress. Its automatic continuation is active.");
            // Decode first; a bad import must not move or replace current files.
            auto selected=current_.content().for_save(installed_,path);
            auto recovered=persistence::load_file(*selected,path);
            session::Campaign next{*selected,std::move(recovered.checkpoint),autosave_path(paths_,*selected),campaign_.settings()};
            current_.content().preserve(*selected);
            current_.preserve_for_recovery();
            current_.commit(*selected,next.checkpoint());
            campaign_=std::move(next);active_content_=std::move(selected);
            active_=true;storage_blocked_=false;missing_content_hash_.clear();content_target_.clear();++generation_;milestone_.reset();halted_=true;publish();
            emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("Campaign recovered. Original files were preserved."));return;
        }
        const auto loaded = campaign_.load(path);
        if (!loaded.loaded) {
            const auto message = loaded.notice ? QString::fromStdString(loaded.notice->explanation) : QStringLiteral("The campaign could not be loaded.");
            emit noticeRaised(message);
            emit operationCompleted(static_cast<qulonglong>(token),false,message); return;
        }
        ++generation_;milestone_.reset();
        // Loading preserves every checkpoint field. Stop only the external clock.
        halted_ = true;
        publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("Campaign loaded and paused."));
    } catch (const std::exception& error) {
        if(const auto* storage=dynamic_cast<const persistence::PersistenceError*>(&error);continuous_ && storage && (storage->code()=="content_unavailable" || storage->code()=="content_integrity")) {
            content_target_=path;content_target_current_=false;storage_blocked_=true;
            try {missing_content_hash_=persistence::inspect_save_identity(persistence::read_bounded(path)).content_hash;}catch(const std::exception&) {}
        }
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::changeSettings(std::uint64_t token,persistence::Settings settings) {
    requireOwningThread();
    try {
        (void)persistence::encode_settings(settings);
        persistence::save_settings(paths_.settings_file,settings);
        campaign_.set_settings(settings); publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("Preferences saved locally."));
    } catch (const std::exception& error) {
        emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
}
