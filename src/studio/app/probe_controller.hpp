#pragma once

#include <pgp/studio/flow_probe.hpp>

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace pgp::studio {
void append_probe_step(QStringList& lines, const ProbeStep& step);
void append_probe_effects(QStringList& lines, const std::vector<runtime::EffectReceipt>& receipts);
void append_probe_continuations(QStringList& lines, const runtime::Checkpoint& checkpoint);

class ProbeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList scenarios READ scenarios NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString date READ date NOTIFY changed)
    Q_PROPERTY(QString nextDate READ nextDate NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString fixtureInfo READ fixtureInfo NOTIFY changed)
    Q_PROPERTY(QString report READ report NOTIFY changed)
    Q_PROPERTY(QVariantList presentations READ presentations NOTIFY changed)
    Q_PROPERTY(QVariantMap graphTrace READ graphTrace NOTIFY changed)
    Q_PROPERTY(QStringList cleanupOwners READ cleanupOwners NOTIFY changed)
public:
    ProbeController();
    void set_document(const runtime::CompiledProgram* program, std::string flow_id);
    [[nodiscard]] QStringList scenarios() const;
    [[nodiscard]] bool ready() const noexcept { return probe_ != nullptr; }
    [[nodiscard]] bool busy() const noexcept { return timer_.isActive(); }
    [[nodiscard]] QString date() const;
    [[nodiscard]] QString nextDate() const;
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QString fixtureInfo() const { return fixture_info_; }
    [[nodiscard]] QString report() const { return report_; }
    [[nodiscard]] QVariantList presentations() const;
    [[nodiscard]] QVariantMap graphTrace() const;
    [[nodiscard]] QStringList cleanupOwners() const;
    Q_INVOKABLE QStringList headquarters(const QString& scenario) const;
    Q_INVOKABLE bool start(const QString& scenario, const QString& headquarters, const QString& seed, bool isolate);
    Q_INVOKABLE void advance(const QString& date);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool choose(const QString& presentation, const QString& choice);
    Q_INVOKABLE bool reload();
    Q_INVOKABLE bool cleanup(const QString& activation);
    Q_INVOKABLE bool inspectBoundaries();
signals:
    void changed();
private:
    template<class Action> bool perform(Action action);
    void show_step(const ProbeStep& step, bool synthetic = false);
    void refresh_report();
    void advance_one();
    std::optional<runtime::CompiledProgram> program_;
    std::unique_ptr<FlowProbe> probe_;
    std::optional<ProbeStep> step_;
    std::string flow_;
    QString fixture_info_, status_, report_;
    QTimer timer_;
    core::OrdinalDay until_{0};
    bool synthetic_ = false;
    std::vector<BoundarySample> boundaries_;
    mutable bool graph_trace_dirty_ = true;
    mutable QVariantMap graph_trace_;
};
} // namespace pgp::studio
