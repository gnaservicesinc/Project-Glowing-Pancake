#include "management_controller.hpp"
#include "flow_presentation.hpp"
#include <pgp/core/integer_codec.hpp>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pgp::bridge {
namespace {
QString text(std::string_view v) { return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size())); }
QString number(std::uint64_t v) { return QString::number(static_cast<qulonglong>(v)); }
QString money(core::Cents v) {
    const auto raw=v.value(); const auto magnitude=raw<0?static_cast<std::uint64_t>(-(raw+1))+1:static_cast<std::uint64_t>(raw);
    QString whole=number(magnitude/100);
    for (auto pos=whole.size()-3;pos>0;pos-=3) whole.insert(pos,QLatin1Char(','));
    return (raw<0?QStringLiteral("−$"):QStringLiteral("$"))+whole+QStringLiteral(".%1").arg(magnitude%100,2,10,QLatin1Char('0'));
}
QString percent(core::BasisPoints v) { return QStringLiteral("%1.%2%").arg(v.value()/100).arg(v.value()%100,2,10,QLatin1Char('0')); }
QString interval(const core::LabProfile& lower,const core::LabProfile& upper) {
    return QStringLiteral("Documents %1–%2 · scanned tables %3–%4").arg(percent(lower[0]),percent(upper[0]),percent(lower[1]),percent(upper[1]));
}
template<class Rows> const auto* find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    return it==rows.end()?nullptr:&*it;
}
std::int64_t integer(QString input,std::int64_t maximum) {
    static const QRegularExpression pattern{QStringLiteral("^[0-9]{1,12}$")};
    if (!pattern.match(input).hasMatch()) throw std::invalid_argument("Enter a positive whole number");
    bool ok=false; const auto value=input.toLongLong(&ok);
    if (!ok || value<=0 || value>maximum) throw std::invalid_argument("The entered amount exceeds its allowed range");
    return value;
}
std::int64_t compute(QString input) {
    static const QRegularExpression pattern{QStringLiteral("^([0-9]{1,9})(?:\\.([0-9]{1,3}))?$")};
    const auto match=pattern.match(input);
    if (!match.hasMatch()) throw std::invalid_argument("Compute uses CU with at most three decimal places");
    const auto value=match.captured(1).toLongLong()*1000+match.captured(2).leftJustified(3,QLatin1Char('0')).toLongLong();
    if (value<=0) throw std::invalid_argument("Compute must be positive");
    return value;
}
core::LabAllocation allocation(QString source,QString rate,QString staff) {
    return {source.toStdString(),core::MilliCUPerDay{compute(rate)},static_cast<std::uint32_t>(integer(staff,1'000'000))};
}
core::TrainingRequest request(QString id,QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent) {
    core::TrainingRequest result; result.model_id=id.toStdString(); result.recipe_id=recipe.toStdString();
    result.parent_id=parent.toStdString(); result.source_id=source.toStdString();
    for (const auto& value:data) result.data_ids.push_back(value.toStdString());
    result.daily_compute=core::MilliCUPerDay{compute(rate)}; result.staff=static_cast<std::uint32_t>(integer(staff,1'000'000));
    result.work=core::MilliCU{compute(work)}; return result;
}
QVariantMap option(std::string_view id,QString label) { return {{QStringLiteral("id"),text(id)},{QStringLiteral("label"),std::move(label)}}; }
QString research_status(const core::LabState& lab,const core::CapacityState& capacity,std::string_view id) {
    for (const auto& run:lab.research) if (run.research_id==id) {
        if (run.completed) return QStringLiteral("Completed");
        const auto* p=find(capacity.projects,run.project_id);
        return p && p->status==core::ProjectStatus::active?QStringLiteral("In progress"):QStringLiteral("Stopped; retry available");
    }
    return QStringLiteral("Available to research");
}
QString offer_terms(const core::BusinessOffer& offer) {
    return QStringLiteral("%1\n%2/month · setup %3 · %4 days\n%5\nAvailable %6 through %7 (exclusive). Accepted once.")
        .arg(text(offer.name),money(offer.monthly_amount),money(offer.setup_cost),QString::number(offer.term.value()),
            offer.kind==core::BusinessOfferKind::compute_lease ? cu(offer.capacity.value())+QStringLiteral(" CU/day; power included. Capacity expires at term end.") :
            number(offer.tasks_per_day)+QStringLiteral(" promised tasks/day · ")+readable(offer.product_id)+QStringLiteral(". Revenue follows actual delivery; no setup revenue."),
            text(core::iso_date(offer.available_from)),text(core::iso_date(offer.available_before)));
}
}
ManagementController::ManagementController(QObject* parent):QObject(parent),funding_(new StableListModel(this)),specialists_(new StableListModel(this)),
    policies_(new StableListModel(this)),decisions_(new StableListModel(this)),alerts_(new StableListModel(this)),data_(new StableListModel(this)),research_(new StableListModel(this)),
    models_(new StableListModel(this)),evidence_(new StableListModel(this)),offers_(new StableListModel(this)),products_(new StableListModel(this)),
    contracts_(new StableListModel(this)),rivals_(new StableListModel(this)),payables_(new StableListModel(this)) {}
bool ManagementController::available() const { return snapshot_ && state().lab && state().deployments; }
const core::SimulationState& ManagementController::state() const { return snapshot_->checkpoint.world.simulation; }
void ManagementController::update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace) {
    snapshot_=std::move(snapshot);
    std::vector<CampaignRow> data,research,models,evidence,offers,products,contracts,rivals,payables,funding,specialists,policies,decisions,alerts;
    const auto& s=state();
    if (s.lab) {
        const auto report=core::lab_report(*s.lab,*s.capacity);
        for (const auto& row:report.data_offers) {
            const bool owned=std::any_of(report.licenses.begin(),report.licenses.end(),[&](const auto& l){return l.offer_id==row.id;});
            data.push_back({text(row.id),text(row.name),money(row.price)+QStringLiteral(" · ")+number(row.unique_units)+QStringLiteral(" unique records"),
                QStringLiteral("%1\nRights: %2\nQuality %3 · contamination %4\nCoverage: documents %5 · scanned tables %6\nCommercial deployment: %7. Buying a license does not train an existing model.")
                .arg(text(row.provenance),text(row.rights),percent(row.quality),percent(row.contamination),percent(row.coverage[0]),percent(row.coverage[1]),row.commercial_use?QStringLiteral("permitted"):QStringLiteral("prohibited")),
                owned?QStringLiteral("Licensed"):QStringLiteral("Unlicensed"),!owned,{{QStringLiteral("owned"),owned}}});
        }
        for (const auto& row:s.lab->catalog.research) {
            QStringList prerequisites;
            for (const auto& id:row.all_of) prerequisites.push_back(readable(id));
            QStringList alternatives; for (const auto& id:row.any_of) alternatives.push_back(readable(id));
            if (!alternatives.empty()) prerequisites.push_back(QStringLiteral("one of: ")+alternatives.join(QStringLiteral(", ")));
            const auto status=research_status(*s.lab,*s.capacity,row.id);
            research.push_back({text(row.id),readable(row.id),money(row.cash)+QStringLiteral(" · ")+QString::number(row.staff_days.value())+QStringLiteral(" staff-days"),
                QStringLiteral("%1 CU of finite experiments plus %2 staff-days. Both must complete.\nPrerequisites: %3\nPaid work and staff time are not refunded when stopped.")
                    .arg(cu(row.experiment_work.value()),QString::number(row.staff_days.value()),prerequisites.isEmpty()?QStringLiteral("none"):prerequisites.join(QStringLiteral(", "))),status,
                status!=QStringLiteral("Completed") && status!=QStringLiteral("In progress")});
        }
        for (const auto& row:report.models) {
            const auto* actual=find(s.lab->models,row.id); QStringList mixture;
            for (const auto& id:actual->data_ids) { const auto* d=find(s.lab->catalog.data,id); mixture.push_back(d?text(d->name):readable(id)); }
            const auto* work=find(s.capacity->projects,actual->project_id);
            models.push_back({text(row.id),text(row.id),readable(row.recipe_id)+QStringLiteral(" · ")+cu(row.consumed_work.value())+QStringLiteral(" CU consumed"),
                QStringLiteral("Data: %1\nParent checkpoint: %2\n%3\nServing cost %4 CU/task. True capability remains unknown until measured. A clone copies consumed work; it adds no training.")
                    .arg(mixture.join(QStringLiteral(", ")),row.parent_id.empty()?QStringLiteral("none"):text(row.parent_id),work?text(work->cause):QString{},cu(find(s.lab->catalog.recipes,row.recipe_id)->inference_per_task.value())),
                row.training?QStringLiteral("Training"):QStringLiteral("Stopped checkpoint"),!row.training,
                {{QStringLiteral("projectId"),text(actual->project_id)},{QStringLiteral("training"),row.training}}});
        }
        for (const auto& row:report.evidence) {
            QStringList findings; for (const auto& finding:row.findings) findings.push_back(text(finding));
            evidence.push_back({text(row.id),text(row.model_id)+QStringLiteral(" · ")+readable(row.plan_id),row.completed?interval(row.lower,row.upper):QStringLiteral("Measurement in progress"),
                row.completed?QStringLiteral("%1\nCoverage: documents %2 · scanned tables %3\n%4\n%5\nResponses: add consented scans and train a fresh model; add paid human review; or promise document-only work.")
                    .arg(interval(row.lower,row.upper),percent(row.coverage[0]),percent(row.coverage[1]),row.independent?QStringLiteral("Independent held-out evidence"):QStringLiteral("Headline test: not independent qualification"),findings.isEmpty()?QStringLiteral("No measured subgroup finding in this test."):findings.join(QStringLiteral("\n"))):QStringLiteral("Wait for the finite evaluation project to complete."),
                row.completed?QStringLiteral("Measured"):QStringLiteral("Testing"),row.completed && row.independent,
                {{QStringLiteral("modelId"),text(row.model_id)},{QStringLiteral("completed"),row.completed},{QStringLiteral("independent"),row.independent}}});
        }
    }
    core::Cents protected_cash{0};for (const auto& reservation:s.reservations) protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
    if (s.business_offers) for (const auto& row:s.business_offers->catalog) {
        const auto it=std::find_if(s.business_offers->accepted.begin(),s.business_offers->accepted.end(),[&](const auto& a){return a.offer_id==row.id;});
        const bool eligible=core::business_offer_eligible(*s.business_offers,row,*s.accounts,*s.capacity,s.market,s.date,protected_cash);
        offers.push_back({text(row.id),text(row.name),money(row.monthly_amount)+QStringLiteral("/month · ")+QString::number(row.term.value())+QStringLiteral(" days"),offer_terms(row),
            it!=s.business_offers->accepted.end()?QStringLiteral("Accepted; ends ")+text(core::iso_date(it->expires_before)):eligible?QStringLiteral("Eligible"):QStringLiteral("Requirements unmet / outside offer window"),eligible});
    }
    if (s.market) {
        for (const auto& row:s.market->products) {
            const auto* work=find(s.capacity->projects,row.capacity_project_id);
            const core::DeploymentRecord* deployment=nullptr;
            if (s.deployments) for (const auto& d:s.deployments->records) if (d.service_id==row.id) deployment=&d;
            products.push_back({text(row.id),readable(row.id),work?number(work->tasks_per_day)+QStringLiteral(" tasks/day · ")+money(work->monthly_support_cost)+QStringLiteral(" support/month"):QString{},
                deployment?QStringLiteral("Model %1 · evidence %2\nControl: %3\n%4 useful tasks delivered\nSource %5 · %6 CU/day · %7 staff\nRetargeting a service preserves contracts and delivery history.")
                    .arg(text(deployment->model_id),text(deployment->evidence_id),deployment->human_review?QStringLiteral("Paid human review; lower throughput"):QStringLiteral("Measured model only"),number(work->useful_tasks),text(work->source_id),cu(work->requested.value()),QString::number(work->staff)):QStringLiteral("Engineering service"),
                work && work->status==core::ProjectStatus::active?QStringLiteral("Serving"):QStringLiteral("Stopped"),false,{{QStringLiteral("projectId"),text(row.capacity_project_id)}}});
        }
        for (const auto& row:s.market->contracts) {
            const bool active=row.status==core::ContractStatus::pending || row.status==core::ContractStatus::active;
            contracts.push_back({text(row.id),readable(row.product_id),number(row.tasks_per_day)+QStringLiteral(" tasks/day · ")+money(row.monthly_fee)+QStringLiteral("/month"),
                QStringLiteral("%1\nAccepted %2 · starts by %3 · ends %4 (exclusive)\nMissing task penalty %5 · cancellation %6\nService credits apply to undelivered promised work. Cancellation preserves consumed costs and settled invoices.")
                    .arg(text(row.cause),text(core::iso_date(row.accepted_on)),text(core::iso_date(row.start_by)),text(core::iso_date(row.ends_before)),money(row.missing_task_penalty),money(row.cancellation_penalty)),
                row.status==core::ContractStatus::active?QStringLiteral("Active"):row.status==core::ContractStatus::pending?QStringLiteral("Pending"):row.status==core::ContractStatus::completed?QStringLiteral("Completed"):row.status==core::ContractStatus::cancelled?QStringLiteral("Cancelled"):QStringLiteral("Missed"),active});
        }
    }
    if (snapshot_->checkpoint.world.competition) for (const auto& notice:snapshot_->checkpoint.world.competition->notices) {
        QString name=readable(notice.company_id);
        for (const auto& [id,definition]:program.collection.definitions) {
            (void)id; const auto* scenario=std::get_if<content::ScenarioDefinition>(&definition.data);
            if (!scenario) continue;
            for (const auto& rival:scenario->rivals) if (rival.id==notice.company_id) name=text(rival.name);
        }
        rivals.push_back({text(notice.company_id)+QLatin1Char(':')+text(notice.kind),name,text(core::iso_date(notice.day)),
            notice.response_due ? QStringLiteral("Heavyweight mobilization warning: public customer traction was observed. Earliest response %1. Build a useful, funded service before then; rival capability is not public.").arg(text(core::iso_date(*notice.response_due))) :
            notice.kind=="publication" ? QStringLiteral("The company published after consuming its own finite training budget. Publication does not reveal private capability or customer fit.") :
            notice.kind=="closure" ? QStringLiteral("The company closed after it could not service actual obligations.") : QStringLiteral("The company declined work that exceeded its available budget."),readable(notice.kind),false});
    }
    if (s.accounts) {
        for (const auto& row:s.accounts->invoices) if (row.kind==core::InvoiceKind::payable && row.settled<row.amount) {
            payables.push_back({text(row.id),text(row.cause),money(core::checked_unit_subtract(row.amount,row.settled)),QStringLiteral("Due ")+text(core::iso_date(row.due)),QStringLiteral("Unpaid invoice"),true,{{QStringLiteral("debt"),false}}});
        }
        for (const auto& row:s.accounts->debts) if (row.principal.value()>0) payables.push_back({text(row.id),QStringLiteral("Debt: ")+text(row.id),money(row.principal),QStringLiteral("Matures ")+text(core::iso_date(row.due)),QStringLiteral("Principal outstanding"),true,{{QStringLiteral("debt"),true}}});
    }
    if (s.management) {
        const auto& m=*s.management;
        for (const auto& offer:m.catalog.financing) {
            const bool taken=std::any_of(m.financing.begin(),m.financing.end(),[&](const auto& t){return t.offer_id==offer.id;});
            funding.push_back({text(offer.id),text(offer.name),money(offer.proceeds)+QStringLiteral(" cash now"),
                offer.term.value()==0?QStringLiteral("%1 proceeds in return for %2 ownership. Every existing holding is diluted proportionally. Offered once; no refund or reroll.").arg(money(offer.proceeds),percent(offer.acquired)):
                QStringLiteral("%1 proceeds · %2 monthly interest · principal due %3 days after drawing. Interest accrues through actual accounts; repay principal explicitly in Accounts. Offered once.").arg(money(offer.proceeds),percent(offer.monthly_interest),QString::number(offer.term.value())),taken?QStringLiteral("Used"):QStringLiteral("Available"),!taken});
        }
        for (const auto& offer:m.catalog.specialists) {
            const bool hired=std::any_of(m.specialists.begin(),m.specialists.end(),[&](const auto& h){return h.offer_id==offer.id;});
            specialists.push_back({text(offer.id),text(offer.name),money(offer.signing_cost)+QStringLiteral(" signing · ")+money(offer.monthly_salary)+QStringLiteral("/month"),
                QStringLiteral("Adds one available staff member today and a recurring salary. Past work keeps its actual staffing. Extra staff does not create compute or improve model capability for free."),hired?QStringLiteral("Hired"):QStringLiteral("Available"),!hired});
        }
        for (const auto& p:m.policies) policies.push_back({text(p.id),p.kind==core::DelegationKind::evaluation?QStringLiteral("Held-out evaluation policy"):QStringLiteral("Compute lease renewal policy"),
            QStringLiteral("%1 / %2 runs · %3 / %4 committed").arg(p.runs).arg(p.maximum_runs).arg(money(p.committed),money(p.budget)),
            QStringLiteral("Target %1 · next review %2\nReserve floor %3 · monthly ceiling %4\n%5 CU/day · %6 staff · %7-day evaluation interval\n%8\nSuspending preserves all spent/committed budget and prior decisions.")
                .arg(text(p.target_id),text(core::iso_date(p.next_review)),money(p.reserve_floor),money(p.monthly_ceiling),cu(p.allocation.daily_compute.value()),QString::number(p.allocation.staff),QString::number(p.interval.value()),p.blocked.empty()?QStringLiteral("No blocked action"):text(p.blocked)),p.enabled?QStringLiteral("Enabled"):p.runs>=p.maximum_runs?QStringLiteral("Run limit reached"):QStringLiteral("Suspended / blocked"),false,{{QStringLiteral("enabled"),p.enabled}}});
        for (std::size_t i=0;i<m.decisions.size();++i) {const auto& d=m.decisions[i];decisions.push_back({QStringLiteral("decision:%1").arg(static_cast<qulonglong>(i)),text(d.policy_id),text(core::iso_date(d.day))+QStringLiteral(" · ")+money(d.commitment),text(d.explanation)+(d.result_id.empty()?QString{}:QStringLiteral("\nResult: ")+text(d.result_id)),d.accepted?QStringLiteral("Committed"):QStringLiteral("Refused; policy suspended"),false});}
        for (const auto& n:m.notices) alerts.push_back({text(n.id),readable(n.kind),text(core::iso_date(n.day))+(n.deadline?QStringLiteral(" · deadline ")+text(core::iso_date(*n.deadline)):QString{}),
            alertDetail(n),n.acknowledged?QStringLiteral("Acknowledged"):n.required?QStringLiteral("Required before work"):QStringLiteral("Actionable / informational"),!n.acknowledged});
    }
    auto set=[&](StableListModel* model,std::vector<CampaignRow> rows){if (replace) model->beginCampaign(std::move(rows)); else model->setRows(std::move(rows));};
    set(data_,std::move(data));set(research_,std::move(research));set(models_,std::move(models));set(evidence_,std::move(evidence));set(offers_,std::move(offers));set(products_,std::move(products));set(contracts_,std::move(contracts));set(rivals_,std::move(rivals));set(payables_,std::move(payables));
    set(funding_,std::move(funding));set(specialists_,std::move(specialists));set(policies_,std::move(policies));set(decisions_,std::move(decisions));set(alerts_,std::move(alerts));
    emit changed();
}
QVariantList ManagementController::recipes() const {
    QVariantList result; if (!snapshot_ || !state().lab) return result;
    auto ordered=state().lab->catalog.recipes;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.requires_research.size()<b.requires_research.size();});
    for (const auto& row:ordered) {
        auto value=option(row.id,text(row.name));QStringList prerequisites;
        for (const auto& id:row.requires_research) prerequisites.push_back(readable(id));
        value.insert(QStringLiteral("detail"),QStringLiteral("Requires research: %1. Allowed budget %2–%3 CU. Serving cost %4 CU/task.").arg(prerequisites.isEmpty()?QStringLiteral("none"):prerequisites.join(QStringLiteral(", ")),cu(row.minimum_work.value()),cu(row.maximum_work.value()),cu(row.inference_per_task.value())));
        result.push_back(value);
    }
    return result;
}
QVariantList ManagementController::evaluations() const {
    QVariantList result; if (!snapshot_ || !state().lab) return result;
    auto ordered=state().lab->catalog.evaluations;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.requires_research.size()<b.requires_research.size();});
    for (const bool independent:{true,false}) for (const auto& row:ordered) if (row.independent==independent) {
        auto value=option(row.id,text(row.name)+QStringLiteral(" · ")+money(row.cash));
        QStringList prerequisites;for (const auto& id:row.requires_research) prerequisites.push_back(readable(id));
        value.insert(QStringLiteral("detail"),QStringLiteral("%1 CU of finite work · %2 fee\nCoverage documents %3 · scanned tables %4\nRequires %5").arg(cu(row.work.value()),money(row.cash),percent(row.coverage[0]),percent(row.coverage[1]),prerequisites.join(QStringLiteral(", "))));
        result.push_back(value);
    }
    return result;
}
QVariantList ManagementController::sources() const {
    QVariantList result; if (!snapshot_ || !state().capacity) return result;
    for (const auto& row:state().capacity->sources) {
        std::int64_t reserved=0;
        for (const auto& work:state().capacity->projects) if (work.source_id==row.id && work.status==core::ProjectStatus::active) reserved=core::checked_add(reserved,work.requested.value());
        result.push_back(option(row.id,readable(row.id)+QStringLiteral(" · ")+cu(std::max<std::int64_t>(0,row.effective.value()-reserved))+QStringLiteral(" CU/day free")));
    }
    return result;
}
QVariantList ManagementController::services() const {
    QVariantList result; if (!available()) return result;
    auto ordered=state().deployments->catalog;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.required_recipe.size()<b.required_recipe.size();});
    for (const auto& row:ordered) { auto item=option(row.product.id,readable(row.product.id));item.insert(QStringLiteral("reviewAllowed"),row.review_staff>0);result.push_back(item); }
    return result;
}
QVariantMap ManagementController::recommended() const {
    if (!available()) return {};
    const auto& s=state(); const auto& recipe=*std::min_element(s.lab->catalog.recipes.begin(),s.lab->catalog.recipes.end(),[](const auto& a,const auto& b){return a.requires_research.size()<b.requires_research.size();});
    const auto rate=s.capacity->sources.front().nominal.value();
    return {{QStringLiteral("trainingRate"),cu(rate*3/5)}, {QStringLiteral("trainingStaff"),QString::number(std::max<std::uint32_t>(1,s.capacity->available_staff>2?s.capacity->available_staff-2:1))},
        {QStringLiteral("trainingWork"),cu(core::checked_unit_add(recipe.compute_scale,recipe.compute_scale).value())},
        {QStringLiteral("testingRate"),cu(std::max<std::int64_t>(1,rate/5))},{QStringLiteral("researchStaff"),QString::number(std::min<std::uint32_t>(2,s.capacity->available_staff))},
        {QStringLiteral("testingStaff"),QStringLiteral("1")}};
}
QString ManagementController::briefing() const {
    if (!snapshot_ || !state().accounts) return {};
    const auto report=core::accounts_report(*state().accounts);
    const auto market=state().market?core::market_report(*state().market):core::MarketReport{};
    core::Cents monthly{0};
    for (const auto& term:state().accounts->recurring) if (term.starts_on<=state().date && (!term.ends_before || state().date<*term.ends_before)) monthly=core::checked_unit_add(monthly,term.monthly_amount);
    return QStringLiteral("Committed recurring costs %5/month\nCustomer charges %1 · %2 useful tasks delivered\nUnpaid invoices %3 · debt %4\nCosts accrue as time advances; revenue follows actual delivery.")
        .arg(money(market.customer_charges),number(market.delivered_tasks),money(report.payables),money(report.debt),money(monthly));
}
QString ManagementController::guide() const {
    if (!available()) return {};
    if (state().lab->licenses.empty()) return QStringLiteral("1 · License data in the Lab. Licensed documents are commercially cleared; inspect the coverage and rights before choosing a training mixture.");
    if (state().lab->models.empty()) return QStringLiteral("2 · Start held-out evaluation research (20 CU/day, 2 staff), then train a document baseline using the suggested allocation. The preview is an uncertain estimate, not a measurement. Press Play to do the finite work.");
    if (state().lab->evaluations.empty()) return QStringLiteral("3 · Wait for training and held-out research to complete. In Evaluate, run a held-out test of the stopped checkpoint (20 CU/day, 1 staff). Keep enough free cash for the test.");
    if (state().market->products.empty()) return QStringLiteral("4 · Read the measured subgroup finding. Documents can qualify while scanned tables fail. Deploy document-only now, add paid human review, or buy consented scans and train a fresh model. Only independent evidence can qualify a promise.");
    if (state().market->contracts.empty()) return QStringLiteral("5 · Compare the customer offers in the Office. The enterprise offer needs a running document service. A recovery pilot offers a smaller, shorter commitment. Accepting starts real delivery and obligations.");
    return QStringLiteral("6 · Keep the service funded while time advances to January 2017. Read public rival notices and deadlines in the inbox. Try a fresh model with consented scans, compare held-out evidence, and deploy the broader service if it qualifies. Save and load at any paused boundary.");
}
QString ManagementController::scoreText() const {
    if (!snapshot_ || !state().management) return {};
    const auto score=core::slice_score(state());const auto& rubric=state().management->catalog.scoring;
    return QStringLiteral("%1 slice score %2 / 100\nSurvive to the ending: %3 / %4\nDelivered useful work: %5 / %6 (%7 / %8 tasks)\nPositive operating profit: %9 / %10 (%11)\nDeployed independent domain evidence: %12 / %13\nRetained founder stake: %14 / %15 (%16)\nThis development-slice rubric does not award century objectives.")
        .arg(score.final?QStringLiteral("Final"):QStringLiteral("Provisional")).arg(score.total).arg(score.points[0]).arg(rubric.weights[0]).arg(score.points[1]).arg(rubric.weights[1])
        .arg(number(score.delivered_tasks),number(rubric.task_target)).arg(score.points[2]).arg(rubric.weights[2]).arg(money(score.profit)).arg(score.points[3]).arg(rubric.weights[3]).arg(score.points[4]).arg(rubric.weights[4]).arg(percent(score.founder_share));
}
QVariantMap ManagementController::policyDefaults() const {
    if (!snapshot_ || !state().management) return {};
    const auto& c=state().management->catalog;
    const auto input=[](core::Cents value){return QStringLiteral("%1.%2").arg(value.value()/100).arg(value.value()%100,2,10,QLatin1Char('0'));};
    return {{QStringLiteral("budget"),input(c.default_policy_budget)},{QStringLiteral("reserve"),input(c.default_reserve_floor)},
        {QStringLiteral("monthly"),input(c.default_monthly_ceiling)},{QStringLiteral("runs"),QString::number(c.default_policy_runs)},
        {QStringLiteral("interval"),QString::number(c.minimum_evaluation_interval.value())}};
}
QVariantList ManagementController::renewableLeases() const {
    QVariantList result;if (!snapshot_ || !state().business_offers) return result;
    for (const auto& offer:state().business_offers->catalog) if (offer.renewable && std::any_of(state().business_offers->accepted.begin(),state().business_offers->accepted.end(),[&](const auto& a){return a.offer_id==offer.id;}))
        result.push_back(option(offer.id,text(offer.name)+QStringLiteral(" · expires ")+text(core::iso_date(core::lease_expiry(*state().business_offers,offer.id)))+QStringLiteral(" · ceiling ")+money(core::lease_commitment_ceiling(offer))));
    return result;
}
QString ManagementController::alertDetail(const core::ManagementNotice& n) const {
    if (!snapshot_ || !state().management) return {};
    for (const auto& setback:state().management->catalog.setbacks) if (setback.id==n.subject) {
        return QStringLiteral("%1\nSource %2: reduced to %3 CU/day from %4 through %5 (exclusive), then nominal capacity returns.\nServing promises get priority; finite work may finish later. Pause/stop optional projects, choose narrower service scope, move a compatible service to another paid source, or wait for restoration. Acknowledging does not repair capacity or refund costs.")
            .arg(text(setback.name),readable(setback.source_id),cu(setback.reduced_capacity.value()),text(core::iso_date(setback.starts_on)),text(core::iso_date(setback.ends_before)));
    }
    if (n.kind=="policy_blocked") {for (const auto& p:state().management->policies) if (p.id==n.subject) return text(p.blocked)+QStringLiteral("\nThe action spent nothing. Inspect the decision log, free resources or change the plan before resuming.");}
    if (n.kind=="financing_due" && state().accounts) {
        const auto* debt=find(state().accounts->debts,"financing/"+n.subject);
        if (debt && debt->principal.value()==0) return QStringLiteral("This loan's principal has been repaid. Its original due date was %1; historical interest and invoices remain recorded.").arg(text(core::iso_date(*n.deadline)));
    }
    if (n.kind=="financing_due") return QStringLiteral("Debt principal is due on %1. Repay the outstanding amount in Accounts; interest and unpaid obligations remain real. Acknowledgment keeps the due date.").arg(text(core::iso_date(*n.deadline)));
    if (n.kind=="lease_due") return QStringLiteral("A paid compute lease expires on %1. Its source stops providing capacity unless renewed on that morning. Authorize a bounded policy beforehand or renew explicitly on the expiry date. Future monthly bills and setup fees apply.").arg(text(core::iso_date(*n.deadline)));
    return readable(n.kind);
}
QString ManagementController::ending() const {
    if (!snapshot_) return {};
    if (state().phase!=core::DayPhase::finished && state().phase!=core::DayPhase::closed) return {};
    return (state().phase==core::DayPhase::finished?QStringLiteral("The 2014–2017 campaign is complete.\n"):QStringLiteral("The company has closed.\n"))+briefing()+QStringLiteral("\nFinal cash ")+money(state().cash)+QStringLiteral("\n\n")+scoreText();
}
QVariantMap ManagementController::trainingPreview(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent) const {
    if (!available()) return {};
    try {
        const auto req=request(QStringLiteral("preview"),recipe,source,data,rate,staff,work,parent);
        const auto p=core::preview_training(*state().lab,req);
        std::int64_t reserved=0; const auto* supply=find(state().capacity->sources,req.source_id);
        for (const auto& row:state().capacity->projects) if (row.status==core::ProjectStatus::active && row.source_id==req.source_id) reserved=core::checked_add(reserved,row.requested.value());
        const auto capacity=core::capacity_report(*state().capacity);
        const bool free=supply && supply->effective.value()-reserved>=req.daily_compute.value() && capacity.available_staff-capacity.assigned_staff>=req.staff;
        core::Cents protected_cash{0};for (const auto& reservation:state().reservations) protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
        const auto horizon=core::Days{std::min<std::int64_t>(90,p.minimum_days.value())};
        const auto forecast=core::project_cash(*state().accounts,core::advance_days(state().date,horizon),protected_cash);
        const auto explanation=QStringLiteral("Earliest %1 days · unlicensed data %2\nEstimated %3\n%4\nBottlenecks: documents %5; scanned tables %6\n%7\n%8\n%9")
            .arg(QString::number(p.minimum_days.value()),money(p.unowned_data_cost),interval(p.lower,p.upper),text(p.explanation),text(p.bottlenecks[0]),text(p.bottlenecks[1]),
                p.licensed?QStringLiteral("All selected data licensed"):QStringLiteral("Buy each selected data license before training"),free?QStringLiteral("Requested compute and staff fit current free resources"):QStringLiteral("Reduce allocation or stop another project to free compute/staff"),
                QStringLiteral("Available cash after %2 forecast days: %1. Includes existing bills; excludes future customer sales and unpurchased licenses. Forecast capped at 90 days.").arg(money(forecast.available),QString::number(horizon.value())));
        return {{QStringLiteral("text"),explanation+(p.prerequisites_met?QString{}:QStringLiteral("\nComplete the selected recipe’s research prerequisites before training."))},{QStringLiteral("ready"),p.licensed && p.prerequisites_met && free}};
    } catch (const std::exception& e) { return {{QStringLiteral("text"),QString::fromUtf8(e.what())},{QStringLiteral("ready"),false}}; }
}
QString ManagementController::servicePreview(QString id,QString model,bool review) const {
    if (!available()) return {};
    for (const auto& row:state().deployments->catalog) if (row.product.id==id.toStdString()) {
        const auto* m=find(state().lab->models,model.toStdString());
        const auto* recipe=m?find(state().lab->catalog.recipes,m->recipe_id):nullptr;
        return QStringLiteral("Model lower bound requirements: documents ≥ %1; scanned tables ≥ %2\n%3 tasks/day · %4 staff · %5 support/month\n%6\n%7\nDeployment reserves real serving capacity. Existing customer promises survive replacement; consumed costs remain.")
            .arg(percent(row.required[0]),percent(row.required[1]),number(review?row.review_tasks_per_day:row.serving.tasks_per_day),QString::number(row.serving.staff+(review?row.review_staff:0)),
                money(core::checked_unit_add(row.serving.monthly_support_cost,review?row.review_monthly_cost:core::Cents{0})),
                recipe?cu(core::required_inference(review?row.review_tasks_per_day:row.serving.tasks_per_day,recipe->inference_per_task).value())+QStringLiteral(" CU/day using this checkpoint"):QStringLiteral("Choose a checkpoint to see serving compute"),
                QStringLiteral("Discretionary price %1/task. Review credits: documents %2; scanned tables %3. Controls change the service, not measured model capability.").arg(money(row.product.price_per_task),percent(review?row.review_credit[0]:core::BasisPoints{0}),percent(review?row.review_credit[1]:core::BasisPoints{0})));
    }
    return {};
}
bool ManagementController::editable() {
    if (!available()) return false;
    if (state().phase==core::DayPhase::finished || state().phase==core::DayPhase::closed) { emit noticeRaised(QStringLiteral("This campaign has ended."));return false; }
    if (!state().paused && !snapshot_->driver_halted) { emit noticeRaised(QStringLiteral("Pause time before changing a plan.")); return false; }
    return true;
}
QString ManagementController::nextId(QString prefix) const { return prefix+QString::number(static_cast<qulonglong>(state().revision+1)); }
void ManagementController::buyData(QString id) { if (editable()) emit actionRequested(core::BuyData{id.toStdString()}); }
void ManagementController::research(QString id,QString source,QString rate,QString staff) {
    if (!editable()) return; try { emit actionRequested(core::StartResearch{id.toStdString(),allocation(source,rate,staff)}); } catch (const std::exception& e) { emit noticeRaised(QString::fromUtf8(e.what())); }
}
void ManagementController::train(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent) {
    if (!editable()) return; try { emit actionRequested(core::StartTraining{request(nextId(QStringLiteral("model.")),recipe,source,data,rate,staff,work,parent)}); } catch (const std::exception& e) { emit noticeRaised(QString::fromUtf8(e.what())); }
}
void ManagementController::clone(QString id) { if (editable()) emit actionRequested(core::CloneModel{id.toStdString(),nextId(QStringLiteral("checkpoint.")).toStdString()}); }
void ManagementController::evaluate(QString model,QString plan,QString source,QString rate,QString staff) {
    if (!editable()) return; try { emit actionRequested(core::StartEvaluation{nextId(QStringLiteral("evaluation.")).toStdString(),model.toStdString(),plan.toStdString(),allocation(source,rate,staff)}); } catch (const std::exception& e) { emit noticeRaised(QString::fromUtf8(e.what())); }
}
void ManagementController::deploy(QString service,QString model,QString evidence,QString source,bool review) {
    if (editable()) emit actionRequested(core::DeployModel{service.toStdString(),model.toStdString(),evidence.toStdString(),source.toStdString(),review});
}
void ManagementController::acceptOffer(QString id) { if (editable()) emit actionRequested(core::AcceptBusinessOffer{id.toStdString()}); }
void ManagementController::cancelContract(QString id) { if (editable()) emit actionRequested(core::CancelContract{id.toStdString()}); }
void ManagementController::payInvoice(QString id) {
    if (!editable()) return; const auto* row=find(state().accounts->invoices,id.toStdString());
    if (row) emit actionRequested(core::SettleInvoice{row->id,core::checked_unit_subtract(row->amount,row->settled),nextId(QStringLiteral("payment.")).toStdString()});
}
void ManagementController::repayDebt(QString id) {
    if (!editable()) return; const auto* row=find(state().accounts->debts,id.toStdString());
    if (row) emit actionRequested(core::RepayDebt{row->id,row->principal,nextId(QStringLiteral("repayment.")).toStdString()});
}
}

namespace pgp::bridge {
namespace {
core::Cents policy_money(const QString& input) {
    static const QRegularExpression pattern{QStringLiteral("^([0-9]{1,10})(?:\\.([0-9]{1,2}))?$")};
    const auto match=pattern.match(input);if (!match.hasMatch()) throw std::invalid_argument("Enter dollars with at most two decimal places");
    return core::Cents{match.captured(1).toLongLong()*100+match.captured(2).leftJustified(2,QLatin1Char('0')).toLongLong()};
}
}
void ManagementController::takeFinancing(QString id) {if (editable()) emit actionRequested(core::TakeFinancing{id.toStdString()});}
void ManagementController::hireSpecialist(QString id) {if (editable()) emit actionRequested(core::HireSpecialist{id.toStdString()});}
void ManagementController::authorizeEvaluation(QString model,QString plan,QString source,QString rate,QString staff,QString budget,QString reserve,QString interval,QString runs) {
    if (!editable()) return;
    try {
        core::DelegationPolicy policy;policy.id=nextId(QStringLiteral("policy.")).toStdString();policy.target_id=model.toStdString();policy.plan_id=plan.toStdString();policy.kind=core::DelegationKind::evaluation;
        policy.allocation=allocation(source,rate,staff);policy.budget=policy_money(budget);policy.reserve_floor=policy_money(reserve);
        policy.interval=core::Days{integer(interval,365)};policy.maximum_runs=static_cast<std::uint32_t>(integer(runs,12));emit actionRequested(core::AddDelegationPolicy{std::move(policy)});
    } catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::authorizeRenewal(QString offer,QString budget,QString reserve,QString monthly,QString runs) {
    if (!editable() || !state().management) return;
    try {
        core::DelegationPolicy policy;policy.id=nextId(QStringLiteral("policy.")).toStdString();policy.target_id=offer.toStdString();policy.kind=core::DelegationKind::lease_renewal;
        policy.budget=policy_money(budget);policy.reserve_floor=policy_money(reserve);policy.monthly_ceiling=policy_money(monthly);policy.interval=state().management->catalog.minimum_evaluation_interval;
        policy.maximum_runs=static_cast<std::uint32_t>(integer(runs,12));emit actionRequested(core::AddDelegationPolicy{std::move(policy)});
    } catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::setPolicyEnabled(QString id,bool enabled) {if (editable()) emit actionRequested(core::SetDelegationEnabled{id.toStdString(),enabled});}
void ManagementController::renewLease(QString id) {if (editable()) emit actionRequested(core::RenewComputeLease{id.toStdString()});}
void ManagementController::acknowledgeAlert(QString id) {if (editable()) emit actionRequested(core::AcknowledgeManagementNotice{id.toStdString()});}
}
