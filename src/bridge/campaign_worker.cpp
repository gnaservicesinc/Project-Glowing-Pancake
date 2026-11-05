#include "campaign_worker.hpp"

#include <QThread>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::bridge {
namespace {
bool pending_decision(const runtime::Checkpoint& checkpoint) {
    const auto& state = checkpoint.world.simulation;
    return state.phase == core::DayPhase::awaiting_choice ||
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
    qRegisterMetaType<CampaignSnapshotPtr>();
    timer_->setInterval(100);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_,&QTimer::timeout,this,&CampaignWorker::tick);
}
void CampaignWorker::requireOwningThread() const {
    if (QThread::currentThread() != thread()) throw std::logic_error("Campaign operation requires its worker thread");
}
void CampaignWorker::publish() {
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
    if (!result.command.accepted() || result.storage_notice) emit noticeRaised(message);
    publish();
    if (token != 0) emit operationCompleted(static_cast<qulonglong>(token),result.command.accepted(),
        result.command.accepted() && !result.storage_notice ? QString{} : message);
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
void CampaignWorker::stop() { requireOwningThread(); timer_->stop(); }
void CampaignWorker::submitCommand(std::uint64_t token, core::Command command) {
    requireOwningThread();
    try {
        const auto* pause = std::get_if<core::SetPaused>(&command.action);
        // Stop time even if a timer tick landed between the displayed revision
        // and the button press. Commands with business consequences stay stale-checked.
        if (pause && pause->paused) command.expected_revision = campaign_.checkpoint().world.simulation.revision;
        const auto result = campaign_.submit(command);
        if (pause && !pause->paused && result.command.accepted()) halted_ = false;
        complete(token,result);
    } catch (const std::exception& error) {
        pauseDriver(); emit noticeRaised(error_text(error)); publish();
        emit operationCompleted(static_cast<qulonglong>(token),false,error_text(error));
    }
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
    const auto checkpoint = campaign_.checkpoint();
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
        ++generation_;
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
