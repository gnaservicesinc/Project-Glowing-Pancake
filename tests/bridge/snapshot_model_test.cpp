#include "reservation_model.hpp"
#include "simulation_worker.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QThread>

#include <atomic>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace pgp;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::shared_ptr<const core::SimulationSnapshot> snapshot(std::uint64_t revision,
                                                        std::vector<core::ReservationRow> rows) {
    return std::make_shared<const core::SimulationSnapshot>(core::SimulationSnapshot{
        revision, core::OrdinalDay{0}, core::DayPhase::work_ready, true,
        core::Cents{1000}, core::Cents{500}, {}, std::move(rows)});
}
core::ReservationRow row(std::uint64_t id, std::string name, std::int64_t reserved = 100) {
    return {id, std::move(name), core::Cents{reserved}, core::Cents{0}, true,
            core::ReservationStatus::active};
}
void persistent_rows() {
    bridge::ReservationModel model;
    int resets = 0;
    int moves = 0;
    int changes = 0;
    QList<int> changed_roles;
    QObject::connect(&model, &QAbstractItemModel::modelReset, [&] { ++resets; });
    QObject::connect(&model, &QAbstractItemModel::rowsMoved, [&] { ++moves; });
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
        [&](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
            ++changes; changed_roles = roles;
        });
    require(model.applySnapshot(snapshot(1, {row(8, "Beta"), row(3, "Zulu")})), "Initial apply");
    QItemSelectionModel selection(&model);
    QPersistentModelIndex anchor(model.index(0)); // Stable ID 3; an editor's focus/scroll anchor.
    selection.setCurrentIndex(anchor, QItemSelectionModel::ClearAndSelect);
    model.setSortByName(true);
    require(anchor.row() == 1 && anchor.data(bridge::ReservationModel::IdRole) == "3",
            "Sort must move the index with its entity");
    require(selection.currentIndex() == anchor && selection.isSelected(anchor), "Selection follows entity");
    require(model.applySnapshot(snapshot(2, {row(9, "Alpha"), row(3, "Aardvark", 70)})), "Insert/remove/update");
    require(anchor.isValid() && anchor.row() == 0, "Name changes move stable anchor");
    require(selection.currentIndex() == anchor && selection.isSelected(anchor), "Selection survives diff");
    require(changes == 1 && changed_roles == QList<int>{bridge::ReservationModel::NameRole,
                                                      bridge::ReservationModel::ReservedCentsRole},
            "Only changed roles are notified");
    const auto immutable = snapshot(3, {row(3, "Aardvark", 70), row(9, "Alpha")});
    require(model.applySnapshot(immutable), "New revision applies");
    require(!model.applySnapshot(immutable) && !model.applySnapshot(snapshot(1, {})), "Stale snapshots ignored");
    require(changes == 1, "Unchanged data emits no role changes");

    // Frequent snapshots, input-order changes and name sorting retain identity.
    for (std::uint64_t revision = 4; revision != 204; ++revision) {
        auto rows = std::vector{row(3, revision % 2 ? "Zulu" : "Aardvark", 70), row(9, "Alpha")};
        if (revision % 3 == 0) rows.push_back(row(100, "Middle"));
        require(model.applySnapshot(snapshot(revision, std::move(rows))), "Running snapshot");
        require(anchor.isValid() && anchor.data(bridge::ReservationModel::IdRole) == "3" &&
                    selection.currentIndex() == anchor, "Running snapshots retain selection/anchor");
    }
    require(resets == 0 && moves > 0, "Routine snapshots must never reset models");
    try { model.applySnapshot(snapshot(205, {row(3, "X"), row(3, "Y")}));
        throw std::runtime_error("Duplicate rows accepted");
    } catch (const std::invalid_argument&) {}
    require(model.revision() == "203" && anchor.isValid(), "Invalid snapshot leaves current collection intact");

    std::atomic<bool> denied{false};
    std::thread other([&] {
        try { model.applySnapshot(snapshot(205, {})); }
        catch (const std::logic_error&) { denied = true; }
    });
    other.join();
    require(denied, "Worker cannot mutate GUI model directly");

    model.beginCampaign(snapshot(0, {row(std::numeric_limits<std::uint64_t>::max(), "Exact",
                                      std::numeric_limits<std::int64_t>::max())}));
    require(resets == 1 && !anchor.isValid(), "Only campaign replacement resets indexes");
    require(model.index(0).data(bridge::ReservationModel::IdRole) == "18446744073709551615" &&
                model.index(0).data(bridge::ReservationModel::ReservedCentsRole) == "9223372036854775807",
            "Presentation preserves full integer precision");
}

template <class Predicate> void process_until(Predicate ready) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready() && elapsed.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    require(ready(), "Queued worker result timed out");
}
void worker_boundary(QCoreApplication& app) {
    QThread thread;
    auto* worker = new bridge::SimulationWorker(core::SimulationState{.cash = core::Cents{1000}});
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    std::shared_ptr<const core::SimulationSnapshot> current;
    std::vector<core::CommandResult> results;
    bool callbacks_on_gui = true;
    QObject::connect(worker, &bridge::SimulationWorker::snapshotReady, &app,
        [&](const auto& value) { current = value; callbacks_on_gui &= QThread::currentThread() == app.thread(); });
    QObject::connect(worker, &bridge::SimulationWorker::commandCompleted, &app,
        [&](const auto& value) { results.push_back(value); callbacks_on_gui &= QThread::currentThread() == app.thread(); });
    thread.start();
    try {
        QMetaObject::invokeMethod(worker, &bridge::SimulationWorker::start, Qt::QueuedConnection);
        process_until([&] { return static_cast<bool>(current); });
        const auto initial = current;
        const auto revision = current->revision;
        QMetaObject::invokeMethod(worker, [worker, revision] {
            worker->submitCommand({revision, core::ReserveFunds{"First", core::Cents{700}}});
            worker->submitCommand({revision, core::ReserveFunds{"Stale", core::Cents{700}}});
        }, Qt::QueuedConnection);
        process_until([&] { return results.size() == 2 && current->revision > revision; });
        require(results[0].accepted() && results[1].reason == core::CommandReason::stale_revision,
                "Worker serializes commands and preserves rejection receipts");
        require(current->reservations.size() == 1 && current->available == core::Cents{300}, "No stale overspend");
        require(initial->reservations.empty() && initial->available == core::Cents{1000}, "Old snapshot is immutable");
        const auto running_revision = current->revision;
        QMetaObject::invokeMethod(worker, [worker, running_revision] {
            worker->submitCommand({running_revision, core::SetPaused{false}});
        }, Qt::QueuedConnection);
        process_until([&] { return current->date.value() >= 2; });
        const auto pause_revision = current->revision;
        QMetaObject::invokeMethod(worker, [worker, pause_revision] {
            worker->submitCommand({pause_revision, core::SetPaused{true}});
        }, Qt::QueuedConnection);
        process_until([&] { return current->paused; });
        require(callbacks_on_gui && results.size() == 4, "Queued GUI delivery loses no command results");
        QMetaObject::invokeMethod(worker, &bridge::SimulationWorker::stop, Qt::BlockingQueuedConnection);
    } catch (...) { thread.quit(); thread.wait(); throw; }
    thread.quit();
    thread.wait();
}
void worker_required_choice_and_fault(QCoreApplication& app, bool exhaust_revision) {
    QThread thread;
    core::SimulationState initial;
    initial.cash = core::Cents{1000};
    initial.paused = false;
    if (exhaust_revision) {
        initial.revision = std::numeric_limits<std::uint64_t>::max();
    } else {
        initial.next_reservation_id = 2;
        initial.reservations = {row(1, "Due commitment", 300)};
        initial.opening_actions = {{1, core::OrdinalDay{1}, 1, core::Cents{300}, true, "Due decision"}};
    }
    auto* worker = new bridge::SimulationWorker(initial);
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    std::shared_ptr<const core::SimulationSnapshot> current;
    int halted = 0;
    bool halt_reason_valid = true;
    QObject::connect(worker, &bridge::SimulationWorker::snapshotReady, &app,
        [&](const auto& value) { current = value; });
    QObject::connect(worker, &bridge::SimulationWorker::executionHalted, &app,
        [&](const auto& value) {
            ++halted;
            halt_reason_valid &= value.reason == core::CommandReason::arithmetic_overflow;
        });
    thread.start();
    try {
        QMetaObject::invokeMethod(worker, &bridge::SimulationWorker::start, Qt::QueuedConnection);
        process_until([&] {
            return current && (exhaust_revision ? halted == 1 : current->pending_choice.has_value());
        });
        if (exhaust_revision) {
            require(halt_reason_valid, "Explicit halted error reason");
            require(current->revision == initial.revision && current->date == initial.date,
                    "Driver fault preserves authoritative state and delivers required error");
        } else {
            require(current->paused && current->date == core::OrdinalDay{1} &&
                        current->phase == core::DayPhase::awaiting_choice && current->cash == initial.cash,
                    "Running time stops before a required opening choice");
        }
        const auto stopped_revision = current->revision;
        QElapsedTimer observation;
        observation.start();
        while (observation.elapsed() < 220) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        require(current->revision == stopped_revision && halted == (exhaust_revision ? 1 : 0),
                "Waiting choice or fault cannot advance or repeat errors");
        QMetaObject::invokeMethod(worker, &bridge::SimulationWorker::stop, Qt::BlockingQueuedConnection);
    } catch (...) { thread.quit(); thread.wait(); throw; }
    thread.quit();
    thread.wait();
}
} // namespace
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        persistent_rows(); worker_boundary(app);
        worker_required_choice_and_fault(app, false);
        worker_required_choice_and_fault(app, true);
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Persistent snapshot rows and queued worker boundary pass\n";
}
