#include "society_controller.hpp"
#include <pgp/runtime/society.hpp>
#include <QStringList>
#include <algorithm>
#include <set>

namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString date(OrdinalDay day) {return text(iso_date(day));}
QString score(std::uint32_t value) {return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0'));}
QString count(std::uint64_t value) {return QString::number(static_cast<qulonglong>(value));}
QString money(Cents value) {return QStringLiteral("$%1.%2 (2014)").arg(value.value()/100).arg(value.value()%100,2,10,QLatin1Char('0'));}
}
SocietyController::SocietyController(QObject* parent):QObject(parent),regions_(this),map_regions_(this) {
    connect(&regions_,&StableListModel::selectedIdChanged,this,&SocietyController::clearReview);
}
bool SocietyController::available() const {return snapshot_ && world().society.has_value();}
bool SocietyController::editable() const {return available() && world().simulation.phase==DayPhase::work_ready &&
    (snapshot_->driver_halted || world().simulation.paused);}
QString SocietyController::assumptions() const {
    if(!available()) return {};
    auto result=text(society().catalog.assumptions)+QStringLiteral(" Welfare uses a %1-month lag. Components have equal weight within each index. Scores are 0–100.").arg(society().catalog.lag_months);
    if(const auto& d=society().catalog.development) {
        const auto value=[](BasisPoints n){return score(static_cast<std::uint32_t>(n.value()));};
        result+=QStringLiteral(" Each region gains %1 points after a month with at least %2% of public demand, %3% of total demand, and independent quality at least %4/100. Other months erode %5 points. The gain is capped at %6 points.")
            .arg(value(d->monthly_gain),value(d->public_coverage),value(d->useful_coverage),value(d->evidence_floor),value(d->monthly_decay),value(d->maximum_gain));
    }
    return result;
}
QString SocietyController::global() const {
    if(!available()) return {};
    const auto global=global_welfare(society()),minimum=minimum_welfare(society());
    QStringList lines;
    for(std::size_t i=0;i<global.size();++i) lines<<QStringLiteral("%1 · population-weighted %2 · lowest region %3")
        .arg(text(welfare_index_name(i)),score(global[i]),score(minimum[i]));
    lines<<(society().months.empty()?QStringLiteral("No settled regional month yet."):
        QStringLiteral("Latest observation · %1. %2 monthly observations retained.").arg(date(society().months.back().month_end),count(society().months.size())));
    return lines.join(u'\n');
}
QString SocietyController::forum() const {
    if(!available()) return {};
    const auto& state=society();const auto& terms=state.catalog.forum;
    QStringList lines{ text(terms.title),QStringLiteral("%1 of 8 members must support. %2 access requires an independent %3 lower bound ≥ %4/100 after each accepting member's implementation date.")
        .arg(terms.quorum).arg(text(channel_key(terms.channel)),text(lab_domain_names[terms.evidence_axis]),score(static_cast<std::uint32_t>(terms.evidence_floor.value())))};
    if(state.rounds.empty()) lines<<QStringLiteral("Next agenda · %1").arg(date(terms.first_agenda));
    else {
        const auto& round=state.rounds.back();
        lines<<QStringLiteral("Round %1 · hearing opened %2 · ballot %3 · expires before %4")
            .arg(count(round.id),date(round.opened_on),date(round.ballot_on),date(round.expires_before));
        if(round.members.empty()) lines<<QStringLiteral("Members have not voted yet. A paid disclosure changes only the selected member's vote score.");
        for(const auto& member:round.members)
            lines<<QStringLiteral("%1 · %2 · score %3 · %4").arg(text(member.region),member.supports?QStringLiteral("supports"):QStringLiteral("declines"),
                score(static_cast<std::uint32_t>(member.score.value())),member.effective_from?QStringLiteral("effective %1").arg(date(*member.effective_from)):QStringLiteral("no implementation"));
        for(const auto& row:round.endorsements) lines<<QStringLiteral("Disclosed %1 · %2 · %3 · %4").arg(date(row.day),text(row.region),text(row.evidence),money(row.paid));
    }
    lines<<QStringLiteral("To restore access, complete the required independent evaluation in Lab and redeploy using that report. Promised work still incurs its signed credits and penalties while blocked.");
    return lines.join(u'\n');
}
QString SocietyController::audiences() const {
    if(!available()) return {};
    const AudienceScores initial{5000,5000,5000,5000,5000,5000};
    const auto& scores=society().months.empty()?initial:society().months.back().audiences;
    QStringList lines;
    std::set<std::string> services;
    for(const auto& row:society().campaigns) if(text(row.region)==regions_.selectedId() && row.ends_before>world().simulation.date) services.insert(row.service);
    for(std::size_t i=0;i<scores.size();++i) {
        QStringList awareness;
        for(const auto& service:services) {
            const auto value=campaign_awareness(society(),regions_.selectedId().toStdString(),service,world().simulation.date,static_cast<std::uint32_t>(i));
            if(value.value()>0) awareness<<QStringLiteral("%1 awareness %2 bp").arg(text(service),QString::number(value.value()));
        }
        lines<<QStringLiteral("%1 · reputation %2/100%3").arg(text(society_audience_name(i)),score(scores[i]),
            awareness.empty()?QString{}:QStringLiteral(" · ")+awareness.join(QStringLiteral("; ")));
    }
    lines<<QStringLiteral("Reputation follows independently measured delivered work and promise reliability. Public-service and science delivery affect the relevant audiences.");
    return lines.join(u'\n');
}
QString SocietyController::media() const {
    if(!available()) return {};
    const auto& terms=society().catalog.media;
    QStringList lines{QStringLiteral("%1 per campaign · %2 days · at most %3 per calendar month. Repeat campaigns to the same region, service and audience yield smaller awareness gains. Customer awareness affects choice; purchases still need demand, evidence, price and capacity.")
        .arg(money(terms.fee)).arg(terms.duration.value()).arg(terms.monthly_limit)};
    const auto region=regions_.selectedId().toStdString();
    for(auto it=society().campaigns.rbegin();it!=society().campaigns.rend();++it) if(it->region==region) {
        lines<<QStringLiteral("%1 · %2 · %3 · +%4 awareness bp · %5 to before %6 · %7")
            .arg(count(it->id),text(it->service),text(society_audience_name(it->audience)),QString::number(it->awareness.value()),date(it->starts_on),date(it->ends_before),money(it->paid));
        if(lines.size()>=9) break;
    }
    return lines.join(u'\n');
}
QVariantList SocietyController::services() const {
    QVariantList result;
    if(!available() || !world().simulation.deployments) return result;
    std::set<std::string> seen;
    const auto& state=world().simulation;
    for(auto it=state.deployments->records.rbegin();it!=state.deployments->records.rend();++it)
        if(seen.insert(it->service_id).second && (it->region_id.empty() || text(it->region_id)==regions_.selectedId()) &&
            runtime::society_evidence(state,it->evidence_id,society().catalog.forum.evidence_axis,state.date))
            result.push_back(QVariantMap{{"id",text(it->service_id)},{"label",text(it->service_id)+QStringLiteral(" · ")+text(it->evidence_id)},{"evidence",text(it->evidence_id)}});
    return result;
}
QVariantList SocietyController::evidence() const {
    QVariantList result;
    if(available() && world().simulation.lab) for(const auto& row:world().simulation.lab->evaluations)
        if(runtime::society_evidence(world().simulation,row.id,society().catalog.forum.evidence_axis,world().simulation.date)) result.push_back(text(row.id));
    return result;
}
bool SocietyController::hasUpcoming() const {
    if(!available()) return false;
    const auto next=next_society_boundary(society(),world().simulation.date);
    return next && *next<world().simulation.end_date;
}
void SocietyController::update(CampaignSnapshotPtr snapshot,bool replace) {
    const bool rebuild=replace || !available() || !snapshot->checkpoint.world.society ||
        snapshot->checkpoint.world.society->months.size()!=society().months.size();
    const bool stale=pending_ && snapshot->checkpoint.world.simulation.revision!=reviewed_revision_;
    snapshot_=std::move(snapshot);
    if(replace || stale) {pending_.reset();review_text_.clear();}
    if(rebuild) updateRows(replace);
    emit changed();
}
void SocietyController::updateRows(bool replace) {
    std::vector<CampaignRow> rows;
    if(available()) for(std::size_t i=0;i<society_regions;++i) {
        const auto& profile=society().catalog.regions[i];
        const auto& components=society().months.empty()?profile.initial:society().months.back().components[i];
        const auto indices=welfare_indices(components);
        QStringList detail;
        for(std::size_t j=0;j<indices.size();++j) detail<<QStringLiteral("%1 · %2/100").arg(text(welfare_index_name(j)),score(indices[j]));
        for(std::size_t j=0;j<components.size();++j) detail<<QStringLiteral("%1 · %2/100").arg(text(welfare_component_name(j)),score(components[j]));
        if(!society().months.empty()) {
            const auto& outcome=society().months.back().outcomes[i];
            detail<<QStringLiteral("Latest settled month · %1/%2 tasks delivered · public services %3/%4 · science %5 · industry %6 · missed promises %7/%8 · compute %9 milli-CU · supplier concentration %10 bp · measured lower bound %11/100")
                .arg(count(outcome.delivered),count(outcome.demand),count(outcome.public_work),count(outcome.public_demand),count(outcome.science_work),count(outcome.industrial_work),count(outcome.missed),count(outcome.promised),count(outcome.compute_milli),QString::number(outcome.concentration.value()),score(static_cast<std::uint32_t>(outcome.quality.value())));
        }
        rows.push_back({text(profile.id),text(profile.name),QStringLiteral("Population weight %1 · output weight %2").arg(profile.population_weight).arg(profile.output_weight),detail.join(u'\n'),QStringLiteral("Regional profile")});
    }
    if(replace) map_regions_.beginCampaign(rows);else map_regions_.setRows(rows);
    if(replace) regions_.beginCampaign(std::move(rows));else regions_.setRows(std::move(rows));
    if(regions_.selectedId().isEmpty() && regions_.rowCount()) regions_.setSelectedId(regions_.idAt(0));
}
void SocietyController::reviewMedia(const QString& service,int audience) {
    clearReview();if(!editable() || audience<0) return;
    try {
        const auto options=services();
        const auto found=std::find_if(options.begin(),options.end(),[&](const auto& row){return row.toMap().value("id")==service;});
        if(found==options.end()) throw std::invalid_argument("Deploy and independently evaluate a service for this region first.");
        BuyMediaCampaign request{regions_.selectedId().toStdString(),service.toStdString(),found->toMap().value("evidence").toString().toStdString(),static_cast<std::uint32_t>(audience)};
        const auto quote=runtime::preview_media_campaign(world(),request);
        review_text_=QStringLiteral("Pay %1 for %2 in %3. %4 gains %5 awareness bp until before %6. This payment does not change capability, measured evidence, reputation or welfare.")
            .arg(money(quote.paid),service,text(quote.region),text(society_audience_name(quote.audience)),QString::number(quote.awareness.value()),date(quote.ends_before));
        pending_=request;reviewed_revision_=world().simulation.revision;emit changed();
    } catch(const std::exception& error) {emit noticeRaised(text(error.what()));}
}
void SocietyController::reviewEndorsement(const QString& report,bool support) {
    clearReview();if(!editable()) return;
    if(const auto block=core::regional_contact_block(world().simulation,regions_.selectedId().toStdString());!block.empty()) {emit noticeRaised(text(block));return;}
    const auto& state=society();
    if(state.rounds.empty() || state.rounds.back().ballot_on<=world().simulation.date || !evidence().contains(report) ||
        std::any_of(state.rounds.back().endorsements.begin(),state.rounds.back().endorsements.end(),[&](const auto& row){return text(row.region)==regions_.selectedId();})) {
        emit noticeRaised(QStringLiteral("Choose an undisclosed member and a completed independent report during the open hearing."));return;
    }
    pending_=EndorseForumStandard{state.rounds.back().id,regions_.selectedId().toStdString(),report.toStdString(),support};
    reviewed_revision_=world().simulation.revision;
    review_text_=QStringLiteral("Pay %1 and disclose %2 to %3 in %4 of the standard. This changes the member's vote score by %5 bp; quorum and individual implementation dates still apply.")
        .arg(money(state.catalog.forum.endorsement_fee),report,regions_.selectedId(),support?QStringLiteral("support"):QStringLiteral("opposition"),QString::number(state.catalog.forum.endorsement_shift.value()));emit changed();
}
void SocietyController::confirmAction() {
    if(!pending_) return;
    const auto action=*pending_;const bool valid=editable() && world().simulation.revision==reviewed_revision_;
    clearReview();if(valid) emit actionRequested(action);else emit noticeRaised(QStringLiteral("The campaign changed. Review current terms again."));
}
void SocietyController::clearReview() {pending_.reset();review_text_.clear();emit changed();}
}
