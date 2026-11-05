#include <pgp/session/campaign.hpp>

namespace pgp::session {
WeeklyNews group_weekly_news(const runtime::Checkpoint& checkpoint) {
    WeeklyNews weeks;
    const auto start = checkpoint.world.campaign_start;
    for (const auto& row : checkpoint.runtime.presentations) {
        if (row.kind != runtime::PresentationKind::news) continue;
        const auto elapsed = core::checked_subtract(row.day.value(), start.value());
        const auto first = core::advance_days(start, core::Days{elapsed / 7 * 7});
        weeks[first].push_back(&row);
    }
    return weeks;
}
Campaign::Campaign(runtime::CompiledProgram program, runtime::Checkpoint checkpoint,
                   std::filesystem::path autosave_directory, persistence::Settings settings)
    : engine_{std::move(program), std::move(checkpoint)}, autosaves_{std::move(autosave_directory)},
      settings_{settings} {
    (void)persistence::encode_settings(settings_);
}
void Campaign::set_settings(persistence::Settings settings) {
    (void)persistence::encode_settings(settings);
    settings_ = settings;
}
OperationResult Campaign::operate(
    const std::function<runtime::RuntimeResult(runtime::Runtime&)>& operation, bool before_commit) {
    const auto prior_phase = engine_.checkpoint().world.simulation.phase;
    const auto prior_day = engine_.checkpoint().world.simulation.date;
    const auto prior_revision = engine_.checkpoint().world.simulation.revision;
    runtime::RuntimeResult outcome;
    if (before_commit && settings_.autosave_enabled) {
        auto candidate = engine_;
        outcome = operation(candidate);
        if (!outcome.accepted()) {
            const bool halt = outcome.reason == runtime::RuntimeReason::invalid_state ||
                outcome.reason == runtime::RuntimeReason::budget_exceeded ||
                outcome.reason == runtime::RuntimeReason::arithmetic_overflow ||
                outcome.reason == runtime::RuntimeReason::missing_definition;
            return {std::move(outcome), {}, halt};
        }
        try {
            (void)autosaves_.write(engine_.program(), engine_.checkpoint(),
                persistence::AutosaveReason::before_irreversible_commitment);
        } catch (const persistence::PersistenceError& error) {
            runtime::RuntimeResult rejected;
            rejected.reason = runtime::RuntimeReason::effect_rejected;
            rejected.revision = prior_revision;
            rejected.explanation = "The action was not committed because its recovery autosave failed.";
            return {std::move(rejected), StorageNotice{error.code(), error.what()}, true};
        }
        engine_ = std::move(candidate);
    } else {
        outcome = operation(engine_);
        if (!outcome.accepted()) {
            const bool halt = outcome.reason == runtime::RuntimeReason::invalid_state ||
                outcome.reason == runtime::RuntimeReason::budget_exceeded ||
                outcome.reason == runtime::RuntimeReason::arithmetic_overflow ||
                outcome.reason == runtime::RuntimeReason::missing_definition;
            return {std::move(outcome), {}, halt};
        }
    }
    const auto& after = engine_.checkpoint();
    const bool ended = (after.world.simulation.phase == core::DayPhase::finished ||
                       after.world.simulation.phase == core::DayPhase::closed) &&
                      prior_phase != after.world.simulation.phase;
    if (settings_.autosave_enabled && (after.world.simulation.date != prior_day || ended)) {
        const auto prior_date = core::from_ordinal(prior_day);
        const auto current_date = core::from_ordinal(after.world.simulation.date);
        if (ended || prior_date.month != current_date.month || prior_date.year != current_date.year) {
            try {
                (void)autosaves_.write(engine_.program(), after, ended ?
                    persistence::AutosaveReason::campaign_end : persistence::AutosaveReason::monthly_boundary);
            } catch (const persistence::PersistenceError& error) {
                // The completed day remains current. An IO error must not erase
                // play or roll back already accepted daily consequences.
                return {std::move(outcome), StorageNotice{error.code(), error.what()}, true};
            }
        }
    }
    const bool liquidity_halt = after.world.simulation.liquidity &&
        (core::liquidity_report(*after.world.simulation.liquidity).pending_warning.has_value() ||
         after.world.simulation.liquidity->status == core::LiquidityStatus::closed);
    return {std::move(outcome), {}, liquidity_halt};
}
OperationResult Campaign::prepare_opening(std::uint64_t expected_revision) {
    auto result = operate([&](auto& candidate) { return candidate.prepare_opening(expected_revision); }, false);
    if (!result.command.accepted() && result.command.reason != runtime::RuntimeReason::stale_revision)
        result.halt_requested = true;
    return result;
}
OperationResult Campaign::submit(const core::Command& command) {
    const auto* reservation = std::get_if<core::ReserveFunds>(&command.action);
    const bool irreversible = (reservation && !reservation->reversible) ||
        std::holds_alternative<core::ConsumeReservation>(command.action) ||
        std::holds_alternative<core::ResolveOpeningChoice>(command.action) ||
        std::holds_alternative<core::AddRecurringObligation>(command.action) ||
        std::holds_alternative<core::IssueInvoice>(command.action) ||
        std::holds_alternative<core::SettleInvoice>(command.action) ||
        std::holds_alternative<core::DrawDebt>(command.action) ||
        std::holds_alternative<core::RepayDebt>(command.action) ||
        std::holds_alternative<core::InvestEquity>(command.action) ||
        std::holds_alternative<core::ReserveProject>(command.action) ||
        std::holds_alternative<core::CancelProject>(command.action) ||
        std::holds_alternative<core::AcceptContract>(command.action) ||
        std::holds_alternative<core::StartContract>(command.action) ||
        std::holds_alternative<core::CancelContract>(command.action) ||
        std::holds_alternative<core::BuyData>(command.action) ||
        std::holds_alternative<core::StartResearch>(command.action) ||
        std::holds_alternative<core::StartTraining>(command.action) ||
        std::holds_alternative<core::StartEvaluation>(command.action) ||
        std::holds_alternative<core::DeployModel>(command.action) ||
        std::holds_alternative<core::AcceptBusinessOffer>(command.action);
    auto result = operate([&](auto& candidate) { return candidate.submit(command); }, irreversible);
    if (std::holds_alternative<core::AdvanceDay>(command.action) && !result.command.accepted() &&
        result.command.reason != runtime::RuntimeReason::stale_revision)
        result.halt_requested = true;
    return result;
}
OperationResult Campaign::choose(std::uint64_t expected_revision, std::uint64_t presentation_id,
                                 std::string_view choice_id) {
    return operate([&](auto& candidate) { return candidate.choose(expected_revision, presentation_id, choice_id); }, true);
}
void Campaign::save(const std::filesystem::path& path) const {
    persistence::save_file(engine_.program(), engine_.checkpoint(), path);
}
LoadResult Campaign::load(const std::filesystem::path& path) {
    try {
        auto loaded = persistence::load_file(engine_.program(), path);
        runtime::Runtime candidate{engine_.program(), std::move(loaded.checkpoint)};
        engine_ = std::move(candidate);
        return {true, {}};
    } catch (const persistence::PersistenceError& error) {
        return {false, StorageNotice{error.code(), error.what()}};
    } catch (const std::exception& error) {
        return {false, StorageNotice{"invalid_state", error.what()}};
    }
}
} // namespace pgp::session
