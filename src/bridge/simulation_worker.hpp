#pragma once

#include <pgp/core/simulation.hpp>

#include <QObject>
#include <QTimer>

#include <memory>

Q_DECLARE_METATYPE(pgp::core::Command)
Q_DECLARE_METATYPE(pgp::core::CommandResult)
Q_DECLARE_METATYPE(std::shared_ptr<const pgp::core::SimulationSnapshot>)

namespace pgp::bridge {

// Move to one QThread before start(). Commands and timer steps are serialized
// there. Snapshot delivery is immutable; each command has its own result signal
// so a UI may coalesce obsolete snapshots without discarding command results.
class SimulationWorker final : public QObject {
    Q_OBJECT
public:
    explicit SimulationWorker(core::SimulationState initial, QObject* parent = nullptr);
public slots:
    void start();
    void stop();
    void submitCommand(pgp::core::Command command);
signals:
    void snapshotReady(std::shared_ptr<const pgp::core::SimulationSnapshot> snapshot);
    void commandCompleted(pgp::core::CommandResult result);
    // A recoverable execution error stops the driver even if state limits
    // prevent recording SetPaused. Consumers retain this required alert.
    void executionHalted(pgp::core::CommandResult result);
private:
    void requireOwningThread() const;
    void tick();
    core::Simulation simulation_;
    QTimer* timer_;
};
} // namespace pgp::bridge
