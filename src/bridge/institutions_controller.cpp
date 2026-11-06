#include "institutions_controller.hpp"
#include "flow_presentation.hpp"
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/society.hpp>
#include <QStringList>
#include <algorithm>

namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString date(OrdinalDay day) {return text(iso_date(day));}
QString number(std::uint64_t value) {return QString::number(static_cast<qulonglong>(value));}
QString points(std::uint32_t value) {return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0'));}
QString money(Cents value) {
    auto whole=QString::number(value.value()/100);
    for(auto i=whole.size()-3;i>0;i-=3) whole.insert(i,u',');
    return QStringLiteral("$%1.%2").arg(whole).arg(value.value()%100,2,10,QLatin1Char('0'));
}
QVariantMap option(QString id,QString label) {return {{"id",std::move(id)},{"label",std::move(label)}};}
template<class Rows> const auto* named(const Rows& rows,std::string_view id) {
    using Row=typename Rows::value_type;
    const auto row=std::ranges::find(rows,id,&Row::id);
    return row==rows.end()?nullptr:&*row;
}
OrdinalDay response_date(const PublicCase& row,const AuthorityCatalog& catalog) {
    return advance_days(row.deadline,row.negotiated_on?catalog.extension:Days{0});
}
QString region_name(const runtime::WorldState& w,std::string_view region) {
    if(region.empty()) return QStringLiteral("Public record");
    return text(w.society->catalog.regions[society_region_index(w.society->catalog,region)].name);
}
QString case_title(PublicCaseKind kind) {
    switch(kind) {
        case PublicCaseKind::promise:return QStringLiteral("Unfulfilled public commitment");
        case PublicCaseKind::delivery:return QStringLiteral("Institutional delivery shortfall");
        case PublicCaseKind::resistance:return QStringLiteral("Coordinated regional resistance");
    }
    return {};
}
QString case_detail(const runtime::WorldState& w,const PublicCase& row) {
    const auto& a=*w.authority;
    QString cause;
    if(row.kind==PublicCaseKind::promise) {
        const auto& p=a.promises.at(row.subject-1);
        cause=p.kind==PublicPromiseKind::publication
            ?QStringLiteral("Publish the completed research %1 to fulfill the commitment.").arg(readable(p.subject))
            :QStringLiteral("%1 has delivered %2 of %3 promised public tasks in %4.")
                .arg(readable(p.subject),number(p.progress),number(p.target),region_name(w,p.region));
    } else {
        const auto& mandate=a.mandates.at(row.subject-1);
        const auto& offer=authority_offer(a.catalog,mandate.offer);
        cause=QStringLiteral("%1 in %2.").arg(text(offer.name),region_name(w,mandate.region));
        cause+=row.kind==PublicCaseKind::delivery
            ?QStringLiteral(" Restore administrative capacity, public delivery and qualifying evidence, or relinquish the agreement.")
            :QStringLiteral(" Fulfill a regional public commitment and maintain service through the next review, or relinquish the agreement.");
        if(row.kind==PublicCaseKind::resistance)
            cause+=QStringLiteral(" An unresolved case can revoke the role and suspend regional sales for %1 days.").arg(a.catalog.sanctions.value());
    }
    cause+=QStringLiteral("\nConcern on %1 · formal recommendation on %2 · response due %3.")
        .arg(date(row.opened_on),date(advance_days(row.opened_on,a.catalog.concern_period)),date(response_date(row,a.catalog)));
    if(row.testimony) cause+=QStringLiteral("\nCited report %1.").arg(text(a.testimonies.at(row.testimony-1).report));
    if(row.negotiated_on) cause+=QStringLiteral("\nFunded review began on %1.").arg(date(*row.negotiated_on));
    if(row.resolved_on) cause+=QStringLiteral("\nResolved on %1.").arg(date(*row.resolved_on));
    if(row.disclosed_on) cause+=QStringLiteral("\nDisclosed on %1.").arg(date(*row.disclosed_on));
    return cause;
}
QString executive_status(const CorporateDispute& row) {
    switch(row.status) {
        case CorporateDisputeStatus::concern:return QStringLiteral("Private concern");
        case CorporateDisputeStatus::formal:return row.negotiated_on?QStringLiteral("Negotiated response"):QStringLiteral("Formal recommendation");
        case CorporateDisputeStatus::resolved:return QStringLiteral("Resolved");
        case CorporateDisputeStatus::departed:return QStringLiteral("Departure and disclosure");
        default:return readable(row.kind);
    }
}
QString executive_detail(const SimulationState& company,const CorporateDispute& row) {
    const auto& terms=*company.management->catalog.governance;
    const auto* leader=named(terms.executives,row.subject);
    if(!leader) return {};
    auto description=QStringLiteral("%1 promised publication of %2. %3\nConcern on %4 · formal recommendation on %5 · publish before %6.")
        .arg(text(leader->name),readable(leader->publication_research),text(leader->priorities),date(row.opened_on),
            date(advance_days(row.opened_on,terms.concern_period)),date(row.deadline));
    if(row.negotiated_on) description+=QStringLiteral("\nFunded review began on %1.").arg(date(*row.negotiated_on));
    if(row.resolved_on) description+=QStringLiteral("\n%1 on %2.").arg(executive_status(row),date(*row.resolved_on));
    else description+=QStringLiteral("\nAn unresolved publication commitment leads to departure and disclosure.");
    return description;
}
const GovernanceState* governance(const SimulationState& company) {
    return company.management && company.management->governance?&*company.management->governance:nullptr;
}
QString public_key(std::uint64_t id) {return QStringLiteral("public/")+number(id);}
QString executive_key(std::string_view id) {return QStringLiteral("executive/")+text(id);}
}

InstitutionsController::InstitutionsController(QObject* parent)
    :QObject(parent),regions_(this),mandates_(this),promises_(this),cases_(this,true) {
    for(auto* model:{&regions_,&mandates_,&promises_,&cases_})
        connect(model,&StableListModel::selectedIdChanged,this,&InstitutionsController::clearReview);
}
bool InstitutionsController::available() const {return snapshot_ && world().authority.has_value();}
bool InstitutionsController::editable() const {
    return available() && company().phase==DayPhase::work_ready &&
        snapshot_->checkpoint.runtime.prepared_day==company().date &&
        (snapshot_->driver_halted || company().paused);
}
bool InstitutionsController::hasUpcoming() const {return available() && runtime::next_authority_boundary(world()).has_value();}
QString InstitutionsController::coverage() const {
    if(!available()) return {};
    const auto now=runtime::authority_snapshot(world(),company().date);
    auto result=QStringLiteral("Regions with at least 60/100 in either kind of authority cover %1% of population and %2% of economic output.")
        .arg(points(now.population_coverage),points(now.output_coverage));
    if(!authority().months.empty()) {
        const auto& last=authority().months.back();
        result+=QStringLiteral("\nLast settled check on %1 covered %2% of population and %3% of output.")
            .arg(date(last.month_end),points(last.population_coverage),points(last.output_coverage));
    }
    return result;
}
QString InstitutionsController::identity() const {
    if(!available()) return {};
    const auto scores=runtime::public_identity(world(),company().date);
    const auto baseline=authority().months.empty()?AudienceScores{5000,5000,5000,5000,5000,5000}:world().society->months.back().audiences;
    QStringList result;
    for(std::size_t i=0;i<scores.size();++i) {
        const auto label=scores[i]>baseline[i]?QStringLiteral("Confidence supported by public record"):
            scores[i]<baseline[i]?QStringLiteral("Confidence strained by public record"):QStringLiteral("Confidence follows recent delivery");
        result<<QStringLiteral("%1 · %2/100 · %3").arg(text(society_audience_name(i)),points(scores[i]),label);
    }
    const auto events=runtime::public_identity_events(world(),company().date);
    for(auto it=events.rbegin();it!=events.rend() && result.size()<12;++it)
        result<<QStringLiteral("%1 · %2 · %3").arg(date(it->day),text(it->cause),text(it->subject));
    return result.join(u'\n');
}
QVariantList InstitutionsController::roles() const {
    QVariantList result;
    if(!available()) return result;
    for(const auto& role:authority().catalog.offers) {
        auto entry=option(text(role.id),text(role.name));
        entry["terms"]=text(role.rights)+QStringLiteral("\n%1 up front and %2 per month (2014 dollars). Hire %3 staff and reserve %4 CU/day. Review after %5 days for a %6-day term. Requires %7 public tasks in the previous settled month and an independent lower bound of %8/100.")
            .arg(money(role.setup),money(role.monthly),number(role.staff),cu(role.compute.value()),QString::number(role.lead.value()),
                QString::number(role.term.value()),number(role.public_work),points(static_cast<std::uint32_t>(role.evidence_floor.value())));
        result.push_back(entry);
    }
    return result;
}
QVariantList InstitutionsController::services() const {
    QVariantList result;
    if(!available()) return result;
    for(const auto* row:runtime::public_service_deployments(world(),regions_.selectedId().toStdString()))
        result.push_back(option(text(row->service_id),readable(row->service_id)));
    return result;
}
QVariantList InstitutionsController::reports() const {
    QVariantList result;
    if(!available()) return result;
    for(const auto& row:company().lab->evaluations) {
        if(!runtime::society_evidence(company(),row.id,0,company().date)) continue;
        const bool disclosed=std::ranges::any_of(authority().testimonies,[&](const auto& t){return t.report==row.id;});
        result.push_back(option(text(row.id),QStringLiteral("%1 · lower bound %2/100%3")
            .arg(text(row.id),points(static_cast<std::uint32_t>(row.lower[0].value())),disclosed?QStringLiteral(" · disclosed"):QString{})));
    }
    return result;
}
QVariantList InstitutionsController::research() const {
    QVariantList result;
    if(!available()) return result;
    for(const auto& row:company().lab->research) {
        if(std::ranges::any_of(company().lab->publications,[&](const auto& p){return p.research_id==row.research_id;})) continue;
        result.push_back(option(text(row.research_id),readable(row.research_id)));
    }
    return result;
}
QVariantList InstitutionsController::sources() const {
    QVariantList result;
    if(!available()) return result;
    for(const auto& row:company().capacity->sources) if(company_capacity(row).value()>0)
        result.push_back(option(text(row.id),QStringLiteral("%1 · %2 CU/day").arg(text(row.id),cu(company_capacity(row).value()))));
    return result;
}
void InstitutionsController::update(CampaignSnapshotPtr snapshot,bool replace) {
    if(replace || (pending_ && reviewed_revision_!=snapshot->checkpoint.world.simulation.revision)) {
        pending_.reset();
        review_text_.clear();
    }
    snapshot_=std::move(snapshot);
    updateRows(replace);
    emit changed();
}
void InstitutionsController::updateRows(bool replace) {
    std::vector<CampaignRow> regions,mandates,promises,cases;
    if(available()) {
        const auto& a=authority();
        const auto day=company().date;
        const auto score=runtime::authority_snapshot(world(),day);
        for(std::size_t i=0;i<society_regions;++i) {
            const auto& region=a.catalog.regions[i];
            const auto& profile=world().society->catalog.regions[i];
            auto detail=QStringLiteral("Soft authority %1/100 · hard authority %2/100 · resistance %3/100.")
                .arg(points(score.soft[i]),points(score.hard[i]),points(score.resistance[i]));
            if(!world().society->months.empty()) detail+=QStringLiteral(" Last settled public delivery by your company: %1 tasks.")
                .arg(number(world().society->months.back().outcomes[i].player_public));
            regions.push_back({text(region.id),text(profile.name),text(region.issuer),detail,QStringLiteral("Regional authority")});
        }
        for(auto it=a.mandates.rbegin();it!=a.mandates.rend();++it) {
            const auto& m=*it;
            const auto& role=authority_offer(a.catalog,m.offer);
            const auto& issuer=a.catalog.regions[society_region_index(world().society->catalog,m.region)];
            const auto status=runtime::mandate_status(world(),m,day);
            const bool operating=status=="operating";
            const bool restricted=!m.directives.empty() && m.directives.back().enabled;
            const bool change=operating && (m.directives.empty() || day>=advance_days(m.directives.back().day,a.catalog.directive_spacing));
            auto detail=QStringLiteral("%1\n%2 · %3 from %4 · independent report %5. Decision on %6. Term ends before %7.")
                .arg(text(role.rights),text(issuer.issuer),readable(m.service),text(m.source),text(a.testimonies.at(m.testimony-1).report),
                    date(m.decision_on),date(m.ends_before));
            if(!m.reviews.empty()) {
                const auto& review=m.reviews.back();
                detail+=QStringLiteral("\nReview on %1 · %2/%3 CU worked · %4/%5 public tasks · evidence %6/100 · consent %7/100 · authority before resistance %8/100.")
                    .arg(date(review.day),cu(static_cast<std::int64_t>(review.work)),cu(static_cast<std::int64_t>(review.required_work)),
                        number(review.public_work),number(review.required_public),points(static_cast<std::uint32_t>(review.evidence.value())),
                        points(static_cast<std::uint32_t>(review.consent.value())),points(static_cast<std::uint32_t>(review.score.value())));
            }
            if(m.staff_released_on) detail+=QStringLiteral("\nContract team released on %1.").arg(date(*m.staff_released_on));
            else detail+=QStringLiteral("\n%1 staff · %2 CU/day · %3 per month.").arg(number(role.staff),cu(role.compute.value()),money(role.monthly));
            if(operating) detail+=restricted?QStringLiteral("\nEvidence requirement is active for public buyers."):QStringLiteral("\nRegional public procurement is open under ordinary rules.");
            mandates.push_back({number(m.id),text(role.name),region_name(world(),m.region),detail,readable(status),false,
                {{"directive",change},{"restricted",restricted},{"relinquish",operating || status=="commissioning" || status=="interrupted"}}});
        }
        for(auto it=a.promises.rbegin();it!=a.promises.rend();++it) {
            const auto& p=*it;
            const auto status=p.met_on?QStringLiteral("Fulfilled"):p.closed_on?QStringLiteral("Disclosed as broken"):day>=p.due_on?QStringLiteral("In dispute"):QStringLiteral("Open");
            auto detail=p.kind==PublicPromiseKind::publication?QStringLiteral("Publish the completed research by %1.").arg(date(p.due_on)):
                QStringLiteral("Deliver %1 public tasks in %2 by %3. Actual deliveries so far: %4.")
                    .arg(number(p.target),region_name(world(),p.region),date(p.due_on),number(p.progress));
            if(p.met_on) detail+=QStringLiteral(" Fulfilled on %1.").arg(date(*p.met_on));
            if(p.closed_on) detail+=QStringLiteral(" Response closed on %1.").arg(date(*p.closed_on));
            promises.push_back({number(p.id),readable(p.subject),region_name(world(),p.region),detail,status});
        }
        for(auto it=a.cases.rbegin();it!=a.cases.rend();++it) {
            const auto& row=*it;
            const bool open=!row.resolved_on && !row.disclosed_on;
            const bool formal=day>=advance_days(row.opened_on,a.catalog.concern_period) && day<row.deadline;
            cases.push_back({public_key(row.id),case_title(row.kind),region_name(world(),row.region),case_detail(world(),row),
                readable(public_case_phase(row,day,a.catalog)),!row.acknowledged,
                {{"cite",open && !row.testimony},{"negotiate",open && row.testimony && formal && !row.negotiated_on}}});
        }
        if(const auto* board=governance(company())) for(const auto& row:board->disputes) if(row.kind=="publication") {
            const auto* leader=named(company().management->catalog.governance->executives,row.subject);
            cases.push_back({executive_key(row.id),leader?text(leader->name):readable(row.subject),QStringLiteral("Executive publication commitment"),
                executive_detail(company(),row),executive_status(row),!row.acknowledged,
                {{"negotiate",row.status==CorporateDisputeStatus::formal && !row.negotiated_on && day<row.deadline}}});
        }
    }
    const auto set=[&](StableListModel& model,std::vector<CampaignRow> rows) {
        if(replace) model.beginCampaign(std::move(rows));
        else model.setRows(std::move(rows));
        if(model.selectedId().isEmpty() && model.rowCount()) model.setSelectedId(model.idAt(0));
    };
    set(regions_,std::move(regions));
    set(mandates_,std::move(mandates));
    set(promises_,std::move(promises));
    set(cases_,std::move(cases));
}
const PublicCase* InstitutionsController::selectedPublicCase() const {
    if(!available() || !cases_.selectedId().startsWith(QStringLiteral("public/"))) return nullptr;
    const auto id=cases_.selectedId().sliced(7).toULongLong();
    return id && id<=authority().cases.size()?&authority().cases[id-1]:nullptr;
}
const AuthorityMandate* InstitutionsController::selectedMandate() const {
    if(!available()) return nullptr;
    const auto id=mandates_.selectedId().toULongLong();
    return id && id<=authority().mandates.size()?&authority().mandates[id-1]:nullptr;
}
QString InstitutionsController::requiredCaseText() const {
    if(available()) for(const auto& row:authority().cases) if(!row.acknowledged)
        return case_title(row.kind)+QStringLiteral("\n")+case_detail(world(),row);
    return {};
}
void InstitutionsController::selectRequiredCase() {
    if(!available()) return;
    for(const auto& row:authority().cases) if(!row.acknowledged) {
        cases_.setSelectedId(public_key(row.id));
        return;
    }
    if(const auto* board=governance(company())) for(const auto& row:board->disputes) if(row.kind=="publication" && !row.acknowledged) {
        cases_.setSelectedId(executive_key(row.id));
        return;
    }
}
void InstitutionsController::review(CommandAction action,const std::function<QString(const runtime::WorldState&)>& describe) {
    clearReview();
    if(!editable()) return;
    try {
        // An explicit review uses the same command and checks as confirmation.
        // The candidate is local to this call and never reaches the campaign.
        auto candidate=world();
        if(!runtime::submit_authority_action(candidate,action)) throw std::invalid_argument("Choose an institutional action.");
        review_text_=describe(candidate);
        pending_=std::move(action);
        reviewed_revision_=company().revision;
        emit changed();
        emit reviewReady();
    } catch(const std::exception& error) {
        emit noticeRaised(text(error.what()));
    }
}
void InstitutionsController::reviewMandate(QString role,QString service,QString source) {
    if(!editable()) return;
    const auto region=regions_.selectedId().toStdString();
    const auto service_id=service.toStdString();
    const auto deployments=runtime::public_service_deployments(world(),region);
    const auto deployment=std::ranges::find_if(deployments,[&](const auto* row){return row->service_id==service_id;});
    if(deployment==deployments.end()) {
        emit noticeRaised(QStringLiteral("Deploy a service with public customers in this region."));
        return;
    }
    const auto testimony=std::ranges::find_if(authority().testimonies,[&](const auto& row){return row.report==(*deployment)->evidence_id;});
    if(testimony==authority().testimonies.end()) {
        emit noticeRaised(QStringLiteral("Disclose this service's independent evaluation under Public record before applying."));
        return;
    }
    RequestInstitutionalMandate request{role.toStdString(),region,service_id,source.toStdString(),{},testimony->id,0};
    try {
        const auto& offer=authority_offer(authority().catalog,request.offer);
        if(offer.kind==AuthorityKind::delegation) {
            const auto match=std::find_if(authority().promises.rbegin(),authority().promises.rend(),[&](const auto& p){
                return p.kind==PublicPromiseKind::public_work && p.region==region && p.met_on;
            });
            if(match==authority().promises.rend()) throw std::invalid_argument("Fulfill a public-service commitment in this region before applying for delegation.");
            request.promise=match->id;
        }
        if(offer.kind==AuthorityKind::infrastructure && company().infrastructure) {
            const auto hardware=std::ranges::find_if(company().infrastructure->hardware,[&](const auto& row){
                return row.source_id==request.source && row.commissioned_on && !row.retired_on;
            });
            if(hardware!=company().infrastructure->hardware.end()) request.site=hardware->site_id;
        }
        review(request,[&](const auto& candidate){
            const auto& m=candidate.authority->mandates.back();
            const auto& issuer=authority().catalog.regions[society_region_index(world().society->catalog,m.region)];
            return QStringLiteral("%1\n%2\nPay %3 up front and %4 per month (2014 dollars). Hire %5 staff and reserve %6 CU/day on %7.\n%8 · independent report %9.\nDecision on %10 · if granted, expires before %11.")
                .arg(text(offer.name),text(issuer.issuer),money(offer.setup),money(offer.monthly),number(offer.staff),cu(offer.compute.value()),
                    m.source=="lease.initial"?QStringLiteral("the opening lease"):text(m.source),readable(service_id),text(testimony->report),date(m.decision_on),date(m.ends_before));
        });
    } catch(const std::exception& error) {
        emit noticeRaised(text(error.what()));
    }
}
void InstitutionsController::reviewTestimony(QString report,bool forCase) {
    const auto* row=forCase?selectedPublicCase():nullptr;
    if(forCase && !row) return;
    const auto id=row?row->id:0;
    review(SubmitPublicTestimony{report.toStdString(),id},[&](const auto&){
        return id?QStringLiteral("Cite the independent report %1 in public case %2. The underlying response and deadline still apply.").arg(report,number(id)):
            QStringLiteral("Enter the completed independent report %1 in the public record. Investors, researchers and other audiences can react differently.").arg(report);
    });
}
void InstitutionsController::reviewPromise(QString subject,bool publication) {
    const auto kind=publication?PublicPromiseKind::publication:PublicPromiseKind::public_work;
    const auto region=publication?std::string{}:regions_.selectedId().toStdString();
    review(MakePublicPromise{kind,region,subject.toStdString()},[&](const auto& candidate){
        const auto& p=candidate.authority->promises.back();
        return publication?QStringLiteral("Commit publicly to publish %1 by %2. A missed date opens a public response case.").arg(readable(p.subject),date(p.due_on)):
            QStringLiteral("Commit publicly to deliver %1 tasks with %2 in %3 by %4. Only settled public deliveries after the commitment count.")
                .arg(number(p.target),readable(p.subject),region_name(world(),p.region),date(p.due_on));
    });
}
void InstitutionsController::reviewNegotiation(QString source) {
    if(!editable()) return;
    const auto describe=[&](OrdinalDay due) {
        const auto& terms=authority().catalog;
        return QStringLiteral("Pay %1 in 2014 dollars and reserve %2 existing staff and %3 CU/day on %4 for a formal review. The extended response is due %5. Fulfill the underlying obligation to resolve the case.")
            .arg(money(terms.remedy_fee),number(terms.remedy_staff),cu(terms.remedy_compute.value()),source,date(due));
    };
    if(const auto* row=selectedPublicCase()) {
        review(NegotiatePublicCase{row->id,source.toStdString()},[&](const auto& candidate){
            return describe(response_date(candidate.authority->cases.at(row->id-1),authority().catalog));
        });
    } else if(cases_.selectedId().startsWith(QStringLiteral("executive/"))) {
        const auto id=cases_.selectedId().sliced(10).toStdString();
        review(NegotiateExecutiveDispute{id,source.toStdString()},[&](const auto& candidate){
            return describe(named(governance(candidate.simulation)->disputes,id)->deadline);
        });
    }
}
void InstitutionsController::reviewProcurement() {
    const auto* m=selectedMandate();
    if(!m) return;
    const bool enable=m->directives.empty() || !m->directives.back().enabled;
    review(DirectRegionalProcurement{m->id,enable},[&](const auto&){
        return enable?QStringLiteral("Require an independent lower bound of %1/100 for all suppliers serving public customers in %2. Signed obligations still incur credits and penalties when delivery is blocked.")
            .arg(points(static_cast<std::uint32_t>(authority().catalog.procurement_floor.value())),region_name(world(),m->region)):
            QStringLiteral("Withdraw this agreement's evidence requirement for public procurement in %1. Other agreements and existing laws still apply.").arg(region_name(world(),m->region));
    });
}
void InstitutionsController::reviewRelinquishment() {
    const auto* m=selectedMandate();
    if(!m) return;
    review(RelinquishInstitutionalMandate{m->id},[&](const auto&){
        return QStringLiteral("Relinquish %1 in %2. Release its contract team and future payroll today and end its decision rights.")
            .arg(text(authority_offer(authority().catalog,m->offer).name),region_name(world(),m->region));
    });
}
void InstitutionsController::acknowledgeCase() {
    if(!editable()) return;
    if(const auto* row=selectedPublicCase();row && !row->acknowledged) emit actionRequested(AcknowledgePublicCase{row->id});
    else if(cases_.selectedId().startsWith(QStringLiteral("executive/")))
        emit actionRequested(RespondCorporateDispute{cases_.selectedId().sliced(10).toStdString(),false});
}
void InstitutionsController::confirmAction() {
    if(!pending_ || !editable()) return;
    if(company().revision!=reviewed_revision_) {
        clearReview();
        emit noticeRaised(QStringLiteral("The campaign changed. Review the current terms before confirming."));
        return;
    }
    auto action=std::move(*pending_);
    clearReview();
    emit actionRequested(std::move(action));
}
void InstitutionsController::clearReview() {
    pending_.reset();
    review_text_.clear();
    emit changed();
}
}
