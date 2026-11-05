#include <pgp/session/campaign.hpp>
#include <algorithm>

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
    const auto prior_notices=engine_.checkpoint().world.simulation.management?engine_.checkpoint().world.simulation.management->notices.size():0;
    const auto prior_rivals=engine_.checkpoint().world.competition?engine_.checkpoint().world.competition->notices.size():0;
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
    std::optional<std::string> pause_notice;
    if (after.world.simulation.date!=prior_day && after.world.simulation.capacity) {
        for (const auto& work:after.world.simulation.capacity->projects) {
            if (work.status!=core::ProjectStatus::completed || work.stopped_on!=prior_day) continue;
            if ((work.kind==core::ProjectKind::evaluation && settings_.auto_pause_evaluations) ||
                (work.kind!=core::ProjectKind::evaluation && work.kind!=core::ProjectKind::serving && settings_.auto_pause_projects)) {
                pause_notice="Auto-paused: "+work.cause+" completed. Its actual work and costs are retained.";break;
            }
        }
    }
    if (settings_.auto_pause_alerts) {
        if (after.world.simulation.management && after.world.simulation.management->notices.size()>prior_notices)
            pause_notice="Auto-paused for a new company alert. Review its cause and any real deadline in Office → Policies and alerts.";
        if (after.world.competition && after.world.competition->notices.size()>prior_rivals && std::any_of(after.world.competition->notices.begin()+static_cast<std::ptrdiff_t>(prior_rivals),after.world.competition->notices.end(),[](const auto& row){return row.kind=="warning";}))
            pause_notice="Auto-paused: a heavyweight published a dated mobilization warning. Review public rival notices.";
    }
    // Optional notification preferences never suppress a real deadline stop.
    // This external-driver halt happens on the prepared morning before any
    // due-day work; the player can still finance, pay or cancel explicitly.
    if (after.world.simulation.date!=prior_day) {
        const auto& state=after.world.simulation;
        if (state.management && state.management->governance) for (const auto& motion:state.management->governance->motions)
            if (motion.status==core::BoardMotionStatus::expired && motion.resolved_on==state.date)
                pause_notice="The board response deadline passed without a vote. The proposal expired without spending or dilution.";
        if (state.accounts) for (const auto& debt:state.accounts->debts)
            if (debt.principal.value()>0 && debt.due==state.date)
                pause_notice="Deadline today: debt principal is due. Review Accounts and the actual payment before continuing time.";
        if (state.business_offers) for (const auto& acceptance:state.business_offers->accepted) {
            const auto offer=std::find_if(state.business_offers->catalog.begin(),state.business_offers->catalog.end(),[&](const auto& o){return o.id==acceptance.offer_id;});
            if (offer!=state.business_offers->catalog.end() && offer->kind==core::BusinessOfferKind::compute_lease && core::lease_expiry(*state.business_offers,offer->id)==state.date)
                pause_notice="Deadline today: a compute lease expires. Its capacity is unavailable until an authorized renewal succeeds.";
        }
        if (state.market) for (const auto& contract:state.market->contracts)
            if (contract.status==core::ContractStatus::pending && contract.start_by==state.date)
                pause_notice="Deadline today: a customer commitment must start. Inspect its terms before continuing.";
        if (state.liquidity && core::liquidity_report(*state.liquidity).deadline==state.date)
            pause_notice="Final response day: unpaid obligations must be resolved before more work. The workout deadline has not changed.";
        if (after.world.competition) for (const auto& offer:after.world.competition->offers)
            if (offer.status==runtime::CompetitiveOfferStatus::expired && offer.resolved_on==state.date)
                pause_notice="A funded agreement reached its deadline. Review the recorded outcome in Office → Rivals.";
    }
    if (core::required_governance_notice(after.world.simulation)) pause_notice="A corporate concern or outcome needs review in Office → Board and people. Its response deadline is saved.";
    return {std::move(outcome), {}, liquidity_halt || core::required_management_notice(after.world.simulation) || core::required_governance_notice(after.world.simulation) || pause_notice.has_value(),std::move(pause_notice)};
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
        std::holds_alternative<core::CancelProject>(command.action) || std::holds_alternative<core::WithdrawOperations>(command.action) ||
        std::holds_alternative<core::AcceptContract>(command.action) ||
        std::holds_alternative<core::StartContract>(command.action) ||
        std::holds_alternative<core::CancelContract>(command.action) ||
        std::holds_alternative<core::BuyData>(command.action) ||
        std::holds_alternative<core::StartResearch>(command.action) ||
        std::holds_alternative<core::StartTraining>(command.action) ||
        std::holds_alternative<core::StartEvaluation>(command.action) ||
        std::holds_alternative<core::DeployModel>(command.action) ||
        std::holds_alternative<core::AcceptBusinessOffer>(command.action) ||
        std::holds_alternative<core::TakeFinancing>(command.action) || std::holds_alternative<core::HireSpecialist>(command.action) ||
        std::holds_alternative<core::AddDelegationPolicy>(command.action) || std::holds_alternative<core::RenewComputeLease>(command.action) ||
        std::holds_alternative<core::ReviewDelegation>(command.action) ||
        std::holds_alternative<core::AppointExecutive>(command.action) || std::holds_alternative<core::VoteBoardMotion>(command.action) || std::holds_alternative<core::RespondCorporateDispute>(command.action) ||
        std::holds_alternative<core::BuildOwnedSite>(command.action) || std::holds_alternative<core::OrderOwnedHardware>(command.action) ||
        std::holds_alternative<core::HireSiteOperators>(command.action) || std::holds_alternative<core::ScheduleSiteAppointment>(command.action) ||
        std::holds_alternative<core::RetireOwnedHardware>(command.action) || std::holds_alternative<core::CloseOwnedSite>(command.action) ||
        std::holds_alternative<core::CancelSiteAppointment>(command.action) || std::holds_alternative<core::NoticeInitialLeaseTermination>(command.action);
    auto result = operate([&](auto& candidate) { return candidate.submit(command); }, irreversible);
    if (std::holds_alternative<core::AdvanceDay>(command.action) && !result.command.accepted() &&
        result.command.reason != runtime::RuntimeReason::stale_revision)
        result.halt_requested = true;
    return result;
}
OperationResult Campaign::respond_competitive_offer(std::uint64_t revision,std::string_view id,bool accept) {
    return operate([&](auto& candidate){return candidate.respondToCompetitiveOffer(revision,id,accept);},true);
}
OperationResult Campaign::propose_acquisition(std::uint64_t revision,std::string_view id,bool equity) {
    return operate([&](auto& candidate){return candidate.proposeAcquisition(revision,id,equity);},true);
}
OperationResult Campaign::transfer_subsidiary_funds(std::uint64_t revision,std::string_view id,core::Cents amount,bool funding) {
    return operate([&](auto& candidate){return candidate.transferSubsidiaryFunds(revision,id,amount,funding);},true);
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
