#include "civic_controller.hpp"
#include "flow_presentation.hpp"

#include <QStringList>
#include <algorithm>

namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QString date(OrdinalDay value) { return text(iso_date(value)); }
QString number(std::uint64_t value) { return QString::number(static_cast<qulonglong>(value)); }
QString money(Cents amount) {
    return QStringLiteral("$%1.%2 (2014)").arg(amount.value() / 100)
        .arg(amount.value() % 100, 2, 10, QLatin1Char('0'));
}
QString stage(CivicStage value, bool federal) {
    switch (value) {
    case CivicStage::agenda: return QStringLiteral("Committee hearing scheduled");
    case CivicStage::committee: return QStringLiteral("Committee");
    case CivicStage::house: return federal ? QStringLiteral("House vote") : QStringLiteral("State assembly vote");
    case CivicStage::senate: return QStringLiteral("Senate vote");
    case CivicStage::executive: return federal ? QStringLiteral("Presidential decision") : QStringLiteral("Governor decision");
    case CivicStage::review: return QStringLiteral("Procedural review");
    case CivicStage::agency: return QStringLiteral("Agency implementation");
    case CivicStage::effective: return QStringLiteral("In force");
    case CivicStage::rejected: return QStringLiteral("Closed without implementation");
    case CivicStage::expired: return QStringLiteral("Expired");
    }
    return {};
}
QString policy_terms(const CivicPolicy& policy) {
    if(policy.rule==CivicRule::final_closure)
        return QStringLiteral("A final federal closure ends institutional play if the order takes effect after review. Review the dated hearing and evidence options before the decision.");
    if (policy.rule == CivicRule::site_power)
        return QStringLiteral("New site permits up to %1 kW in %2. Earlier authorized reservations keep their terms.")
            .arg(number(policy.permit_watts / 1000), text(policy.jurisdiction));
    auto terms = QStringLiteral("New %1 launches offered to US customers require an independent %2 lower bound of %3 bp. Earlier deployments keep their terms.")
        .arg(readable(channel_key(policy.channel)), readable(lab_domain_names[policy.evidence_axis]), QString::number(policy.evidence_floor.value()));
    if (policy.exempt_reviewed_service) terms += QStringLiteral(" The funded human-review scope is exempt.");
    return terms;
}
QString docket_detail(const CivicPolicy& policy, const CivicDocket& docket) {
    QStringList lines{text(policy.explanation), policy_terms(policy),
        QStringLiteral("%1 · %2 · opened %3").arg(text(policy.jurisdiction), readable(civic_key(policy.issue)), date(docket.opened_on))};
    if (docket.stage == CivicStage::agency) lines << QStringLiteral("Effective from %1 after the agency queue.").arg(date(*docket.next_on));
    else if (docket.stage == CivicStage::effective || docket.stage == CivicStage::expired)
        lines << QStringLiteral("Effective %1 · expires before %2").arg(date(*docket.effective_from), date(*docket.expires_before));
    else if (docket.next_on) lines << QStringLiteral("%1 on %2").arg(stage(docket.stage, policy.jurisdiction == "US"), date(*docket.next_on));
    if (policy.review.value() > 0)
        lines << QStringLiteral("Procedural review requires %1 bp support in each legislative vote.").arg(QString::number(policy.review_floor.value()));
    for (const auto& vote : docket.decisions)
        lines << QStringLiteral("%1 · %2 · %3/%4 in favor · %5")
            .arg(date(vote.day), stage(vote.stage, policy.jurisdiction == "US"), number(vote.yes), number(vote.seats),
                vote.passed ? QStringLiteral("advanced") : QStringLiteral("stopped"));
    for (const auto& record : docket.submissions)
        lines << QStringLiteral("%1 · %2 · %3 bp · %4").arg(date(record.day),
            record.support ? QStringLiteral("support") : QStringLiteral("oppose"), QString::number(record.influence.value()),
            record.evidence.empty() ? money(record.fee) : text(record.evidence));
    return lines.join(u'\n');
}
} // namespace

CivicController::CivicController(QObject* parent) : QObject(parent), dockets_(this) {
    connect(&dockets_, &StableListModel::selectedIdChanged, this, [this] {
        pending_.reset();
        review_text_.clear();
        emit changed();
    });
}
const core::SimulationState& CivicController::state() const { return snapshot_->checkpoint.world.simulation; }
bool CivicController::available() const { return snapshot_ && state().civic.has_value(); }
bool CivicController::editable() const { return available() && state().phase == core::DayPhase::work_ready; }
QString CivicController::assumptions() const { return available() ? text(state().civic->catalog.assumptions) : QString{}; }
const core::CivicDocket* CivicController::selected() const {
    if (!available()) return nullptr;
    bool ok = false;
    const auto id = dockets_.selectedId().toULongLong(&ok);
    const auto& rows = state().civic->dockets;
    return ok && id > 0 && id <= rows.size() ? &rows[static_cast<std::size_t>(id - 1)] : nullptr;
}
std::optional<core::CivicAdvocacyTerms> CivicController::terms() const {
    const auto* docket = selected();
    return docket && editable() ? civic_advocacy_terms(*state().civic, *docket, state().date) : std::nullopt;
}
bool CivicController::canAdvocate() const {
    const auto price = terms();
    if (!price) return false;
    auto funds = state().cash;
    for (const auto& row : state().reservations) funds = checked_unit_subtract(funds, row.reserved);
    return funds >= price->fee;
}
QString CivicController::advocacy() const {
    const auto price = terms();
    if (!price) return QStringLiteral("Paid submissions are closed or exhausted for this docket.");
    return QStringLiteral("%1 · %2 bp change to the vote score · %3 remaining. Each payment yields a smaller change.")
        .arg(money(price->fee), QString::number(price->influence.value()), number(price->remaining));
}
QVariantList CivicController::evidence() const {
    QVariantList values;
    if (const auto* docket = selected(); docket && editable())
        for (const auto& id : civic_evidence_options(state(), *docket)) values.push_back(text(id));
    return values;
}
void CivicController::update(CampaignSnapshotPtr snapshot, bool replace) {
    const auto& incoming = snapshot->checkpoint.world.simulation;
    const bool rebuild = replace || !available() || !incoming.civic || incoming.civic->dockets != state().civic->dockets;
    const bool stale = pending_ && incoming.revision != reviewed_revision_;
    snapshot_ = std::move(snapshot);
    if (replace || stale) {
        pending_.reset();
        review_text_.clear();
    }
    if (replace) {
        jurisdiction_ = QStringLiteral("all");
        jurisdictions_ = {QVariantMap{{"id", "all"}, {"label", "All jurisdictions"}},
            QVariantMap{{"id", "US"}, {"label", "United States"}}};
        if (available()) for (const auto& state : this->state().civic->catalog.constituencies)
            jurisdictions_.push_back(QVariantMap{{"id", text(state.id)}, {"label", text(state.name)}});
        emit catalogChanged();
    }
    if (rebuild) update_rows(replace);
    emit changed();
}
void CivicController::update_rows(bool replace) {
    std::vector<CampaignRow> rows;
    if (available()) {
        const auto& civic = *state().civic;
        for (auto it = civic.dockets.rbegin(); it != civic.dockets.rend(); ++it) {
            const auto& docket = *it;
            const auto& policy = civic_policy(civic.catalog, docket.policy_id);
            if (jurisdiction_ != QStringLiteral("all") && jurisdiction_ != text(policy.jurisdiction)) continue;
            const auto next = docket.next_on ? QStringLiteral(" · %1").arg(date(*docket.next_on)) : QString{};
            rows.push_back({number(docket.id), text(policy.title), text(policy.jurisdiction) + next,
                docket_detail(policy, docket), stage(docket.stage, policy.jurisdiction == "US")});
        }
    }
    if (replace) dockets_.beginCampaign(std::move(rows)); else dockets_.setRows(std::move(rows));
    if (dockets_.selectedId().isEmpty() && dockets_.rowCount() > 0) dockets_.setSelectedId(dockets_.idAt(0));
}
void CivicController::setJurisdiction(const QString& jurisdiction) {
    if (jurisdiction_ == jurisdiction || std::none_of(jurisdictions_.begin(), jurisdictions_.end(),
        [&](const auto& row) { return row.toMap().value("id") == jurisdiction; })) return;
    jurisdiction_ = jurisdiction;
    update_rows(false);
    emit changed();
}
QString CivicController::institutions() const {
    if (!available()) return {};
    const auto& civic = *state().civic;
    const auto bodies = civic_institutions(civic, state().date);
    const auto& blocs = civic.catalog.blocs;
    const auto where = std::find_if(civic.catalog.constituencies.begin(), civic.catalog.constituencies.end(),
        [&](const auto& row) { return text(row.id) == jurisdiction_; });
    QStringList lines;
    if (where == civic.catalog.constituencies.end()) {
        lines << QStringLiteral("President · %1").arg(text(blocs[bodies.president].name));
        const auto senate = civic_senate(bodies);
        for (std::size_t i = 0; i < blocs.size(); ++i)
            lines << QStringLiteral("%1 · House %2/435 · Senate %3/100").arg(text(blocs[i].name), number(bodies.house[i]), number(senate[i]));
    } else {
        const auto index = static_cast<std::size_t>(where - civic.catalog.constituencies.begin());
        lines << QStringLiteral("%1 executive · %2 · salient issue %3")
            .arg(text(where->name), text(blocs[bodies.governors[index]].name), readable(civic_key(where->priority)));
        for (std::size_t i = 0; i < blocs.size(); ++i)
            lines << QStringLiteral("%1 · assembly %2/100").arg(text(blocs[i].name), number(bodies.assemblies[index][i]));
    }
    return lines.join(u'\n');
}
bool CivicController::hasUpcoming() const {
    return available() && state().civic->next_boundary && *state().civic->next_boundary > state().date &&
        *state().civic->next_boundary < state().end_date;
}
QString CivicController::calendar() const {
    if (!available()) return {};
    const auto& civic = *state().civic;
    QStringList lines;
    if (civic.next_boundary) lines << QStringLiteral("Next civic boundary · %1").arg(date(*civic.next_boundary));
    if (const auto next = next_civic_election(advance_days(state().date, Days{1})))
        lines << QStringLiteral("Next ballot · %1").arg(date(*next));
    if (!civic.elections.empty()) {
        const auto& election = civic.elections.back();
        lines << QStringLiteral("Counted %1 · chambers seated %2 · executives seated %3")
            .arg(date(election.day), date(civic_seating(election.day, false)), date(civic_seating(election.day, true)));
        lines << QStringLiteral("Settled domestic uptake %1 bp · executive record %2 bp")
            .arg(QString::number(election.adoption.value()), QString::number(election.performance.value()));
        if (std::any_of(election.electors.begin(), election.electors.end(), [](auto seats) { return seats > 0; })) {
            for (std::size_t i = 0; i < civic_blocs; ++i)
                lines << QStringLiteral("%1 · %2 presidential electors").arg(text(civic.catalog.blocs[i].name), number(election.electors[i]));
        }
    }
    return lines.join(u'\n');
}
void CivicController::reviewAdvocacy(bool support) {
    const auto price = terms();
    const auto* docket = selected();
    if (!docket || !price || !canAdvocate()) return;
    const auto& policy = civic_policy(state().civic->catalog, docket->policy_id);
    pending_ = core::FundPolicyAdvocacy{docket->id, support};
    reviewed_revision_ = state().revision;
    review_text_ = QStringLiteral("%1 %2. Pay %3 from unreserved cash for a disclosed %4 bp change in subsequent vote scores. The vote is still decided by the institutions.")
        .arg(support ? QStringLiteral("Support") : QStringLiteral("Oppose"), text(policy.title), money(price->fee),
            QString::number(price->influence.value()));
    emit changed();
}
void CivicController::confirmAdvocacy() {
    if (!pending_) return;
    const auto request = *pending_;
    const bool valid = editable() && state().revision == reviewed_revision_ && canAdvocate() && selected() && selected()->id == request.docket;
    clearReview();
    if (valid) emit actionRequested(request);
    else emit noticeRaised(QStringLiteral("The docket or available cash changed. Review the current terms."));
}
void CivicController::clearReview() {
    pending_.reset();
    review_text_.clear();
    emit changed();
}
void CivicController::discloseEvidence(const QString& id, bool support) {
    if (const auto* docket = selected(); docket && evidence().contains(id))
        emit actionRequested(core::SubmitPolicyEvidence{docket->id, id.toStdString(), support});
}
} // namespace pgp::bridge
