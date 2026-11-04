#include "simulation_worker.hpp"

#include <QThread>

#include <stdexcept>
#include <utility>

namespace pgp::bridge {
SimulationWorker::SimulationWorker(core::SimulationState initial, QObject* parent)
    : QObject(parent), simulation_(std::move(initial)), timer_(new QTimer(this)) {
    qRegisterMetaType<core::Command>();
    qRegisterMetaType<core::CommandResult>();
    qRegisterMetaType<std::shared_ptr<const core::SimulationSnapshot>>();
    timer_->setInterval(100); // Initial running snapshot cap: 10 Hz.
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &SimulationWorker::tick);
}
void SimulationWorker::requireOwningThread() const {
    if (QThread::currentThread() != thread())
        throw std::logic_error("Simulation worker commands require its owning thread");
}
void SimulationWorker::start() {
    requireOwningThread();
    timer_->start();
    emit snapshotReady(simulation_.snapshot());
}
void SimulationWorker::stop() { requireOwningThread(); timer_->stop(); }
void SimulationWorker::submitCommand(core::Command command) {
    requireOwningThread();
    emit commandCompleted(simulation_.submit(command));
    emit snapshotReady(simulation_.snapshot());
}
void SimulationWorker::tick() {
    const auto current = simulation_.snapshot();
    if (current->paused || current->phase != core::DayPhase::work_ready) return;
    const auto result = simulation_.submit({current->revision, core::AdvanceDay{}});
    if (!result.accepted()) {
        // Preserve an explicit failure receipt and pause at this safe boundary;
        // a full journal/content error must never produce an endless busy loop.
        (void)simulation_.submit({simulation_.snapshot()->revision, core::SetPaused{true}});
        timer_->stop();
        emit commandCompleted(result);
        emit executionHalted(result);
    }
    emit snapshotReady(simulation_.snapshot());
}
} // namespace pgp::bridge
