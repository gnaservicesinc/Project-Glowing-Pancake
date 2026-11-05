#include "campaign_worker.hpp"

#include <QThread>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::bridge {
namespace {
bool pending_decision(const runtime::Checkpoint& checkpoint) {
    const auto& state = checkpoint.world.simulation;
    return core::required_management_notice(state) || state.phase == core::DayPhase::awaiting_choice ||
        (state.liquidity && core::liquidity_report(*state.liquidity).pending_warning.has_value()) ||
        std::any_of(checkpoint.runtime.presentations.begin(),checkpoint.runtime.presentations.end(),[](const auto& row) {
            return row.kind == runtime::PresentationKind::dialogue && !row.resolved;
        });
}
QString error_text(const std::exception& error) { return QString::fromUtf8(error.what()); }
}
CampaignWorker::CampaignWorker(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
    persistence::LocalPaths paths, persistence::Settings settings, QObject* parent)
    : QObject(parent),campaign_(std::move(program),std::move(checkpoint),paths.saves_directory / "autosaves",settings),
      paths_(std::move(paths)),timer_(new QTimer(this)) {
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
    const auto& state = checkpoint.world.simulation;
    const bool running = !halted_ && !state.paused && state.phase == core::DayPhase::work_ready &&
        !pending_decision(checkpoint);
    if (running && !timer_->isActive()) timer_->start();
    if (!running) timer_->stop();
    emit snapshotReady(std::make_shared<const CampaignSnapshot>(CampaignSnapshot{
        generation_,std::move(checkpoint),campaign_.settings(),halted_}));
}
void CampaignWorker::pauseDriver() {
    halted_ = true;
}
void CampaignWorker::complete(std::uint64_t token, const session::OperationResult& result) {
    if (result.halt_requested || result.storage_notice) pauseDriver();
    QString message = QString::fromStdString(result.command.explanation);
    if (result.storage_notice) message += QStringLiteral(" ") + QString::fromStdString(result.storage_notice->explanation);
    if (result.pause_notice) {message=QString::fromStdString(*result.pause_notice);emit noticeRaised(message);}
    if (milestone_ && session::milestone_reached(campaign_.checkpoint(),*milestone_)) {
        pauseDriver();milestone_.reset();emit noticeRaised(QStringLiteral("Reached the chosen milestone. Time is paused at the prepared morning."));
    }
    if (!result.command.accepted() || result.storage_notice) emit noticeRaised(message);
    const auto& current=campaign_.checkpoint();
    if (token!=0 || halted_ || current.world.simulation.paused || pending_decision(current) ||
        current.world.simulation.phase==core::DayPhase::finished || current.world.simulation.phase==core::DayPhase::closed ||
        !result.command.accepted() || last_publish_.elapsed()>=100) publish();
    if (token != 0) emit operationCompleted(static_cast<qulonglong>(token),result.command.accepted(),
        result.command.accepted() && !result.storage_notice ? QString{} : message);
}
void CampaignWorker::newCampaign(std::uint64_t token,std::string headquarters,bool stories,std::string id) {
    requireOwningThread();
    try {
        if (generation_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Campaign generation is exhausted");
        if (id.empty()) for (const auto& [candidate,definition]:campaign_.program().collection.definitions) {
            const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);
            if (scenario && scenario->service_catalog) { id=candidate;break; }
        }
        const auto* selected=campaign_.program().collection.find(id);
        const auto* selected_scenario=selected?std::get_if<content::ScenarioDefinition>(&selected->data):nullptr;
        if (!selected_scenario || !selected_scenario->service_catalog) throw std::invalid_argument("Choose a playable scenario from this content collection");
        auto initial=runtime::begin_scenario(campaign_.program(),id,headquarters);
        if (!stories && initial.world.enabled_flows) {
            auto& roots=*initial.world.enabled_flows;
            roots.erase(std::remove_if(roots.begin(),roots.end(),[&](const auto& flow_id) {
                const auto& flow=std::get<content::FlowDefinition>(campaign_.program().collection.find(flow_id)->data);
                return std::find(flow.tags.begin(),flow.tags.end(),"story")!=flow.tags.end();
            }),roots.end());
        }
        session::Campaign next{campaign_.program(),std::move(initial),paths_.saves_directory/"autosaves",campaign_.settings()};
        const auto prepared=next.prepare_opening(next.checkpoint().world.simulation.revision);
        if (!prepared.command.accepted()) throw std::runtime_error(prepared.command.explanation);
        campaign_=std::move(next);++generation_;halted_=true;milestone_.reset();publish();
        emit operationCompleted(static_cast<qulonglong>(token),true,QStringLiteral("New campaign started. Time is paused."));
    } catch (const std::exception& error) {
        emit noticeRaised(error_text(error));emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
}
void CampaignWorker::start() {
    requireOwningThread();
    try {
        const auto state = campaign_.checkpoint().world.simulation;
        if (state.phase != core::DayPhase::finished && state.phase != core::DayPhase::closed) {
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
        complete(token,result);
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
    if (halted_ || state.paused || state.phase != core::DayPhase::work_ready || pending_decision(checkpoint)) return;
    try { complete(0,campaign_.submit({state.revision,core::AdvanceDay{}})); }
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
