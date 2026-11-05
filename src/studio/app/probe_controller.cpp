#include "probe_controller.hpp"

#include "flow_presentation.hpp"

#include <pgp/core/integer_codec.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::studio {
namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString truth(std::optional<bool> value) {
    return value ? (*value ? QStringLiteral("true") : QStringLiteral("false")) : QStringLiteral("unknown");
}
QString scalar(const content::Scalar& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return QString::number(static_cast<qlonglong>(*integer));
    if (const auto* boolean = std::get_if<bool>(&value)) return truth(*boolean);
    return text(std::get<std::string>(value));
}
QString scalar(const std::optional<content::Scalar>& value) {
    return value ? scalar(*value) : QStringLiteral("unavailable");
}
QString comparison(content::Comparison value) {
    switch (value) {
    case content::Comparison::equal: return QStringLiteral("=");
    case content::Comparison::not_equal: return QStringLiteral("≠");
    case content::Comparison::less: return QStringLiteral("<");
    case content::Comparison::less_equal: return QStringLiteral("≤");
    case content::Comparison::greater: return QStringLiteral(">");
    case content::Comparison::greater_equal: return QStringLiteral("≥");
    }
    throw std::invalid_argument("Unknown comparison.");
}
QString binding(const runtime::ScopeBinding& value) {
    return text(content::scope_name(value.scope)) + QStringLiteral(" · ") + text(value.id);
}
void condition_lines(QStringList& lines, const runtime::ConditionTrace& trace) {
    for (const auto& clause : trace.clauses) {
        lines << text(clause.path) << QStringLiteral("%1 · %2 · %3 %4 %5 · %6 · %7")
            .arg(binding(clause.binding), text(clause.fact), scalar(clause.actual), comparison(clause.comparison),
                 scalar(clause.expected), text(content::unit_name(clause.unit)), truth(clause.result));
    }
    for (const auto& duration : trace.durations) {
        lines << QStringLiteral("%1 · %2 · elapsed %3 / %4 days · %5")
            .arg(text(duration.path), binding(duration.binding),
                 duration.elapsed_days ? QString::number(static_cast<qlonglong>(*duration.elapsed_days)) : QStringLiteral("unsampled"),
                 QString::number(duration.required_days), truth(duration.result));
    }
    for (const auto& selection : trace.selections) {
        lines << QStringLiteral("%1 · %2 · %3 · %4 matched of %5 · %6")
            .arg(text(selection.path), binding(selection.owner), text(selection.relation),
                 selection.minimum_matches ? number(*selection.minimum_matches) : QStringLiteral("unknown"),
                 number(selection.members.size()), truth(selection.result));
        for (const auto& member : selection.members) lines << QStringLiteral("  %1 · %2").arg(binding(member.binding), truth(member.result));
    }
}
} // namespace

ProbeController::ProbeController() {
    timer_.setInterval(0);
    connect(&timer_, &QTimer::timeout, this, &ProbeController::advance_one);
}
void ProbeController::set_document(const runtime::CompiledProgram* program, std::string flow_id) {
    if (flow_ != flow_id) boundaries_.clear();
    flow_ = std::move(flow_id);
    if ((!program && !program_) ||
        (program && program_ && program->canonical_content_bytes == program_->canonical_content_bytes)) {
        refresh_report();
        emit changed();
        return;
    }
    timer_.stop();
    probe_.reset(); step_.reset(); boundaries_.clear();
    fixture_info_.clear(); status_.clear(); report_.clear();
    program_ = program ? std::optional{*program} : std::nullopt;
    emit changed();
}
QStringList ProbeController::scenarios() const {
    QStringList result;
    if (program_) for (const auto& [id, definition] : program_->collection.definitions)
        if (std::holds_alternative<content::ScenarioDefinition>(definition.data)) result.push_back(text(id));
    return result;
}
QStringList ProbeController::headquarters(const QString& scenario) const {
    QStringList result;
    const auto* definition = program_ ? program_->collection.find(scenario.toStdString()) : nullptr;
    const auto* data = definition ? std::get_if<content::ScenarioDefinition>(&definition->data) : nullptr;
    if (data) for (const auto& id : data->headquarters_choices) result.push_back(text(id));
    return result;
}
QString ProbeController::date() const { return probe_ ? text(core::iso_date(probe_->state().world.simulation.date)) : QString{}; }
QString ProbeController::nextDate() const {
    if (probe_ && probe_->state().world.simulation.date >= probe_->state().world.simulation.end_date) return date();
    return probe_ ? text(core::iso_date(core::advance_days(probe_->state().world.simulation.date, core::Days{1}))) : QString{};
}
template<class Action> bool ProbeController::perform(Action action) {
    try {
        action();
        refresh_report();
        emit changed();
        return true;
    } catch (const std::exception& error) {
        timer_.stop();
        status_ = text(error.what());
        emit changed();
        return false;
    }
}
bool ProbeController::start(const QString& scenario, const QString& headquarters, const QString& seed, bool isolate) {
    if (!program_ || busy()) return false;
    return perform([&] {
        ProbeFixture fixture{scenario.toStdString(), headquarters.toStdString(), isolate ? flow_ : std::string{}, {}};
        if (isolate && flow_.empty()) throw std::invalid_argument("Select the flow to isolate.");
        if (!seed.trimmed().isEmpty()) fixture.seed = core::parse_json_u64(seed.trimmed().toStdString());
        auto next = std::make_unique<FlowProbe>(*program_, fixture);
        auto result = next->prepare();
        fixture_info_ = QStringLiteral("%1 · %2 · seed %3\nRoots · %4\nIsolated test state and RNG")
            .arg(scenario, headquarters, number(next->state().world.root_seed),
                 isolate ? text(flow_) : QStringLiteral("authored scenario roots"));
        probe_ = std::move(next);
        show_step(result);
    });
}
void ProbeController::show_step(const ProbeStep& step, bool synthetic) {
    step_ = step; synthetic_ = synthetic; boundaries_.clear();
    status_ = QStringLiteral("%1 · %2").arg(text(runtime::to_string(step.trace.reason)), text(step.trace.explanation));
}
void ProbeController::advance(const QString& target) {
    if (!probe_ || busy()) return;
    perform([&] {
        until_ = core::parse_iso_date(target.toStdString());
        if (until_ <= probe_->state().world.simulation.date || until_ > probe_->state().world.simulation.end_date)
            throw std::invalid_argument("Choose a later date within this scenario.");
        timer_.start();
    });
}
void ProbeController::stop() { timer_.stop(); emit changed(); }
void ProbeController::advance_one() {
    if (!probe_) { stop(); return; }
    perform([&] {
        auto result = probe_->submit(core::AdvanceDay{});
        const bool reached = probe_->state().world.simulation.date >= until_;
        const bool waiting = probe_->state().world.simulation.phase != core::DayPhase::work_ready ||
            std::any_of(probe_->state().runtime.presentations.begin(), probe_->state().runtime.presentations.end(),
                [](const auto& row) { return row.kind == runtime::PresentationKind::dialogue && !row.resolved; });
        if (!result.trace.accepted() || reached || waiting) timer_.stop();
        show_step(result);
    });
}
bool ProbeController::choose(const QString& presentation, const QString& choice) {
    if (!probe_ || busy()) return false;
    return perform([&] { show_step(probe_->choose(core::parse_json_u64(presentation.toStdString()), choice.toStdString())); });
}
bool ProbeController::reload() {
    if (!probe_ || busy()) return false;
    return perform([&] {
        probe_->reload();
        status_ = QStringLiteral("The exact isolated checkpoint reloaded through the game save codec.");
    });
}
bool ProbeController::cleanup(const QString& activation) {
    if (!probe_ || busy()) return false;
    return perform([&] { show_step(probe_->synthetic_cleanup(core::parse_json_u64(activation.toStdString())), true); });
}
bool ProbeController::inspectBoundaries() {
    if (!probe_ || busy() || flow_.empty()) return false;
    return perform([&] {
        boundaries_ = probe_->boundaries(flow_);
        status_ = QStringLiteral("Generated %1 individual-clause boundary examples.").arg(number(boundaries_.size()));
    });
}
QStringList ProbeController::cleanupOwners() const {
    QStringList owners;
    if (probe_) for (const auto& activation : probe_->state().runtime.activations)
        if (activation.status == runtime::ActivationStatus::continuous_active) owners.push_back(number(activation.id));
    return owners;
}
QVariantList ProbeController::presentations() const {
    QVariantList rows;
    if (!probe_) return rows;
    for (const auto& row : probe_->state().runtime.presentations) {
        QVariantList choices;
        if (!row.resolved) for (const auto& choice : row.choices)
            choices.push_back(QVariantMap{{"choiceId", text(choice.id)}, {"label", bridge::choice_label(choice)}});
        rows.push_back(QVariantMap{{"id", number(row.id)}, {"title", bridge::presentation_title(row.kind)},
            {"detail", bridge::presentation_detail(probe_->state(), row)}, {"choices", choices},
            {"source", text(row.flow_id) + QStringLiteral(" / ") + text(row.node_id)}});
    }
    return rows;
}
void ProbeController::refresh_report() {
    if (!probe_ || !program_) { report_.clear(); return; }
    QStringList lines;
    lines << QStringLiteral("Current fixture · %1").arg(date());
    if (!flow_.empty()) {
        const auto inspect = [&](runtime::FlowPredicate predicate, const QString& label) {
            try {
                const auto inspected = probe_->inspect(flow_, predicate);
                lines << label + QStringLiteral(" · ") + binding(inspected.binding) + QStringLiteral(" · ") + truth(inspected.trace.result);
                condition_lines(lines, inspected.trace);
            } catch (const std::exception& error) {
                lines << label + QStringLiteral(" · inspection error · ") + text(error.what());
            }
        };
        inspect(runtime::FlowPredicate::trigger, QStringLiteral("Trigger-only inspection after the last operation"));
        const auto& flow = std::get<content::FlowDefinition>(program_->collection.definitions.at(flow_).data);
        if (flow.lifecycle.exit_when) inspect(runtime::FlowPredicate::exit_when, QStringLiteral("Current natural exit predicate"));
    }
    if (step_) {
        lines << (synthetic_ ? QStringLiteral("\nSynthetic cleanup probe on a separate copy") : QStringLiteral("\nLast real runtime operation"));
        lines << QStringLiteral("%1 → %2 · cash %3 → %4 constant 2014 cents")
            .arg(text(core::iso_date(step_->before_date)), text(core::iso_date(step_->after_date)),
                 QString::number(static_cast<qlonglong>(step_->before_cash.value())),
                 QString::number(static_cast<qlonglong>(step_->after_cash.value())));
        for (const auto& check : step_->trace.condition_checks)
            lines << QStringLiteral("%1 · %2 · %3").arg(text(check.path), binding(check.binding), truth(check.result));
        runtime::ConditionTrace observed;
        observed.clauses = step_->trace.clauses; observed.durations = step_->trace.durations; observed.selections = step_->trace.selections;
        condition_lines(lines, observed);
        for (const auto& path : step_->trace.paths)
            lines << QStringLiteral("%1 · %2 · %3 · %4").arg(text(path.path), binding(path.binding),
                path.committed ? QStringLiteral("committed") : QStringLiteral("candidate"), text(path.detail));
        lines << QStringLiteral("\nOperation ledger");
        for (const auto& posting : step_->postings)
            lines << QStringLiteral("#%1 · debit %2 · credit %3 · %4 constant 2014 cents · %5")
                .arg(number(posting.id), text(core::to_string(posting.debit)), text(core::to_string(posting.credit)),
                     QString::number(static_cast<qlonglong>(posting.amount.value())), text(posting.cause));
        lines << QStringLiteral("\nModifiers after this operation");
        for (const auto& modifier : step_->active_modifiers)
            lines << QStringLiteral("Owner %1 · %2 · %3 · %4 basis point delta")
                .arg(number(modifier.activation_id), binding(modifier.binding), text(modifier.field),
                     QString::number(static_cast<qlonglong>(modifier.add_basis_points)));
        if (synthetic_) for (const auto& receipt : step_->effects)
            lines << QStringLiteral("Separate-copy cleanup · owner %1 · %2 · %3 → %4")
                .arg(number(receipt.activation_id), binding(receipt.target), scalar(receipt.before), scalar(receipt.after));
    }
    lines << QStringLiteral("\nCommitted effects on the natural fixture timeline");
    for (const auto& receipt : probe_->state().runtime.receipts)
        lines << QStringLiteral("%1 · %2 / %3 · effect %4 · %5 · %6 · %7 → %8 · counterparty %9 cents")
            .arg(text(core::iso_date(receipt.day)), text(receipt.flow_id), text(receipt.node_id), number(receipt.effect_index + 1),
                 text(receipt.operation), binding(receipt.target), scalar(receipt.before), scalar(receipt.after),
                 QString::number(static_cast<qlonglong>(receipt.counterparty_delta.value())));
    lines << QStringLiteral("\nContinuation queue");
    for (const auto& activation : probe_->state().runtime.activations) {
        if (activation.status == runtime::ActivationStatus::completed) continue;
        lines << QStringLiteral("Owner %1 · %2 / %3 · %4 · due %5")
            .arg(number(activation.id), text(activation.current_flow), text(activation.current_node), binding(activation.binding),
                 activation.due ? text(core::iso_date(*activation.due)) : QStringLiteral("none"));
        for (const auto& frame : activation.frames)
            lines << QStringLiteral("  %1 / %2 · next child %3 · return %4")
                .arg(text(frame.flow_id), text(frame.sequence_node), number(frame.next_child), text(frame.return_node));
    }
    if (!boundaries_.empty()) {
        lines << QStringLiteral("\nTrigger-only clause boundary overrides · isolated facts · individual clause truth");
        for (const auto& sample : boundaries_)
            lines << QStringLiteral("%1 · %2 · %3 · %4 · %5 %6 · %7 · %8")
                .arg(text(sample.path), binding(sample.binding), text(sample.fact), text(sample.label),
                     sample.value ? QString::number(static_cast<qlonglong>(*sample.value)) : QStringLiteral("unavailable"),
                     text(content::unit_name(sample.unit)), truth(sample.result), text(sample.error));
    }
    report_ = lines.join(QLatin1Char('\n'));
}
} // namespace pgp::studio
