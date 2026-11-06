#include "management_controller.hpp"
#include "lab_inputs.hpp"
#include "flow_presentation.hpp"
#include "heavyweight_presentation.hpp"
#include "governance_presentation.hpp"
#include <pgp/core/integer_codec.hpp>
#include <pgp/runtime/scenario.hpp>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <map>
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
QString money_input(core::Cents value) {
    return QStringLiteral("%1.%2").arg(value.value()/100).arg(value.value()%100,2,10,QLatin1Char('0'));
}
QString percent(core::BasisPoints v) { return QStringLiteral("%1.%2%").arg(v.value()/100).arg(v.value()%100,2,10,QLatin1Char('0')); }
QString channel_name(core::ProductChannel channel) {
    using core::ProductChannel;
    switch (channel) {
    case ProductChannel::legacy: return QStringLiteral("Document service");
    case ProductChannel::business: return QStringLiteral("Business contracts");
    case ProductChannel::api: return QStringLiteral("Developer API");
    case ProductChannel::consumer: return QStringLiteral("Consumer application");
    case ProductChannel::licensed: return QStringLiteral("Licensed / on-premise");
    case ProductChannel::open_weights: return QStringLiteral("Open weights");
    case ProductChannel::agent: return QStringLiteral("Agent service");
    }
    return {};
}
QString interval(const core::LabProfile& lower,const core::LabProfile& upper) {
    return QStringLiteral("Documents %1–%2 · scanned tables %3–%4").arg(percent(lower[0]),percent(upper[0]),percent(lower[1]),percent(upper[1]));
}
template<class Rows> const auto* find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    return it==rows.end()?nullptr:&*it;
}
std::vector<CampaignRow> regional_rows(const runtime::WorldState& world,const content::RegionalMarkets& catalog) {
    const auto& market=*world.simulation.market;
    const auto day=market.closed_through.value_or(world.simulation.date);
    struct Totals {std::uint64_t industry=0,own=0;core::MilliCU buyer_compute{0};};
    std::map<std::string,Totals,std::less<>> delivery;
    const auto add=[&](const core::MarketState& seller,bool own) {
        if (seller.closed_through!=day) return;
        const auto& settled=seller.days.back();
        for (const auto& row:settled.discretionary) {
            auto& segment=delivery[row.segment_id];
            segment.industry+=row.tasks;if (own) segment.own+=row.tasks;
            segment.buyer_compute=core::checked_unit_add(segment.buyer_compute,row.customer_compute);
        }
        for (const auto& row:settled.contracts) {
            const auto* contract=find(seller.contracts,row.contract_id);
            auto& segment=delivery[contract->segment_id];
            segment.industry+=row.delivered_tasks;if (own) segment.own+=row.delivered_tasks;
        }
    };
    add(market,true);
    if (world.competition) for (const auto& rival:world.competition->companies)
        if (rival.simulation.market) add(*rival.simulation.market,false);
    std::vector<CampaignRow> rows;
    for (const auto& region:catalog.regions) {
        std::uint64_t demand=0,useful=0,own=0;
        QStringList breakdown;
        for (const auto& segment:market.segments) if (segment.region_id==region.id) {
            const auto potential=core::effective_market_demand(market,segment,day);
            const auto& delivered=delivery[segment.id];
            demand+=potential;useful+=delivered.industry;own+=delivered.own;
            breakdown.push_back(QStringLiteral("%1 · %2 of %3 useful tasks · yours %4\n%5 customer budget · %6 / %7 CU on local hardware%8")
                .arg(readable(segment.id),number(delivered.industry),number(potential),number(delivered.own),money(segment.daily_budget),
                    cu(delivered.buyer_compute.value()),cu(segment.customer_compute.value()),segment.requires_local?QStringLiteral(" · local processing required"):QString{}));
        }
        const auto details=text(region.description)+QStringLiteral("\n\n")+breakdown.join(QStringLiteral("\n\n"));
        rows.push_back({text(region.id),text(region.name),QStringLiteral("%1 useful tasks · %2 yours · %3 outside")
            .arg(number(useful),number(own),number(demand-useful)),details,market.closed_through?text(core::iso_date(day)):QStringLiteral("Opening estimates")});
    }
    return rows;
}
using namespace lab_inputs;
core::TrainingRequest request(QString id,QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent) {
    core::TrainingRequest result; result.model_id=id.toStdString(); result.recipe_id=recipe.toStdString();
    result.parent_id=parent.toStdString(); result.source_id=source.toStdString();
    for (const auto& value:data) result.data_ids.push_back(value.toStdString());
    result.daily_compute=core::MilliCUPerDay{compute(rate)}; result.staff=static_cast<std::uint32_t>(integer(staff,1'000'000));
    result.work=core::MilliCU{compute(work)}; return result;
}
QVariantMap option(std::string_view id,QString label) { return {{QStringLiteral("id"),text(id)},{QStringLiteral("label"),std::move(label)}}; }
core::PricingBounds pricing_bounds(const QString& floor,const QString& ceiling,const QString& step,const QString& low,const QString& high) {
    const core::PricingBounds b{policy_money(floor),policy_money(ceiling),policy_money(step),
        core::BasisPoints{low==QStringLiteral("0")?0:integer(low,99)*100},core::BasisPoints{integer(high,100)*100}};
    if (b.floor>=b.ceiling || b.ceiling.value()>core::market_price_limit || b.step.value()<=0 ||
        b.step.value()>b.ceiling.value()-b.floor.value() || b.low_utilization>=b.high_utilization)
        throw std::invalid_argument("Choose an ordered price range, a positive step within it and ordered utilization percentages");
    return b;
}
QString research_status(const core::LabState& lab,const core::CapacityState& capacity,const core::LabResearch& topic) {
    std::uint32_t completed=0;
    bool stopped=false;
    for (const auto& run:lab.research) if (run.research_id==topic.id) {
        if (run.completed) {++completed;continue;}
        const auto* p=find(capacity.projects,run.project_id);
        if (p && p->status==core::ProjectStatus::active) return QStringLiteral("In progress");
        if (p && p->status==core::ProjectStatus::paused) return QStringLiteral("Paused");
        stopped=true;
    }
    if (completed>=topic.repeat_limit) return QStringLiteral("Completed");
    if (completed>0) return QStringLiteral("Repeat available · %1 of %2 complete").arg(completed).arg(topic.repeat_limit);
    return stopped?QStringLiteral("Stopped; retry available"):QStringLiteral("Available to research");
}
QString offer_terms(const core::BusinessOffer& offer) {
    return QStringLiteral("%1\n%2/month · setup %3 · %4 days\n%5\nAvailable %6 through %7 (exclusive). Accepted once.")
        .arg(text(offer.name),money(offer.monthly_amount),money(offer.setup_cost),QString::number(offer.term.value()),
            offer.kind==core::BusinessOfferKind::compute_lease ? cu(offer.capacity.value())+QStringLiteral(" CU/day; power included. Capacity expires at term end.") :
            number(offer.tasks_per_day)+QStringLiteral(" promised tasks/day · ")+readable(offer.product_id)+QStringLiteral(". Revenue follows actual delivery; no setup revenue."),
            text(core::iso_date(offer.available_from)),text(core::iso_date(offer.available_before)));
}
std::optional<core::ResearchInsight> public_insight(const runtime::WorldState& world,std::string_view id) {
    if (!world.competition || !world.competition->setup || world.competition->setup->publication_credit.value()==0) return {};
    std::optional<core::ResearchInsight> result;
    for (const auto& rival:world.competition->companies) if (rival.planning && rival.planning->entered_on) for (const auto& publication:rival.simulation.lab->publications)
        if (publication.research_id==id && (!result || std::tie(publication.day,rival.definition_id)<std::tie(result->published_on,result->publisher)))
            result=core::ResearchInsight{std::string(id),rival.definition_id,publication.day,world.simulation.date,world.competition->setup->publication_credit};
    return result;
}
}
ManagementController::ManagementController(QObject* parent):QObject(parent),funding_(new StableListModel(this)),specialists_(new StableListModel(this)),
    policies_(new StableListModel(this)),decisions_(new StableListModel(this)),alerts_(new StableListModel(this)),data_(new StableListModel(this)),research_(new StableListModel(this)),
    models_(new StableListModel(this)),evidence_(new StableListModel(this)),offers_(new StableListModel(this)),products_(new StableListModel(this)),
    contracts_(new StableListModel(this)),rivals_(new StableListModel(this)),payables_(new StableListModel(this)),heavyweights_(new StableListModel(this)),competitive_offers_(new StableListModel(this)),corporate_finance_(new StableListModel(this)),executives_(new StableListModel(this)),
    board_motions_(new StableListModel(this)),corporate_disputes_(new StableListModel(this)),merger_targets_(new StableListModel(this)),charters_(new StableListModel(this)),regional_markets_(new StableListModel(this)) {}
bool ManagementController::available() const { return snapshot_ && state().lab && state().deployments; }
bool ManagementController::plannedCompetition() const {return snapshot_ && snapshot_->checkpoint.world.competition && snapshot_->checkpoint.world.competition->setup.has_value();}
QString ManagementController::competitionSummary() const {
    if (!plannedCompetition()) return QStringLiteral("Introductory roster. Public notices describe finite rival training and heavyweight warnings.");
    const auto& competition=*snapshot_->checkpoint.world.competition;const auto& setup=*competition.setup;
    const auto entered=std::count_if(competition.companies.begin(),competition.companies.end(),[](const auto& row){return row.planning->entered_on.has_value();});
    return QStringLiteral("%1 US startups · %2 international startups · %3 heavyweights. %4 companies have entered; others are scheduled or watching. One shared customer task/budget pool; contracts keep priority. Operating companies review one feasible strategy action every %8 days. Public research takes %5 days to adopt and reduces later research work by %6; cash costs and prerequisites remain. %7")
        .arg(setup.domestic).arg(setup.international).arg(setup.heavyweights).arg(entered).arg(setup.publication_delay.value()).arg(percent(setup.publication_credit),
            setup.catchup_cash.value()>0?QStringLiteral("One optional catch-up round per rival: %1 for %2 ownership.").arg(money(setup.catchup_cash),percent(setup.catchup_acquired)):QStringLiteral("Catch-up funding is disabled.")).arg(setup.review_interval.value());
}
const core::SimulationState& ManagementController::state() const { return snapshot_->checkpoint.world.simulation; }
void ManagementController::update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace) {
    program_=&program;if (replace) {acquisition_preview_revision_.reset();decision_page_=0;}
    snapshot_=std::move(snapshot);
    decision_page_=std::clamp(decision_page_,0,std::max(0,decisionPageCount()-1));
    std::vector<CampaignRow> data,research,models,evidence,offers,products,contracts,rivals,payables,funding,specialists,policies,decisions,alerts;
    const auto& s=state();
    const auto& world=snapshot_->checkpoint.world;
    const auto* definition=program.collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    const auto* regions=scenario?content::regional_markets(program.collection,*scenario):nullptr;
    if (replace) {
        launch_regions_.clear();
        if (regions) {
            launch_regions_.push_back(option("",QStringLiteral("All eligible regions")));
            for (const auto& region:regions->regions) launch_regions_.push_back(option(region.id,text(region.name)));
        }
    }
    if (s.lab) {
        const auto report=core::lab_report(*s.lab,*s.capacity);
        for (const auto& row:report.data_offers) {
            const bool owned=std::any_of(report.licenses.begin(),report.licenses.end(),[&](const auto& l){return l.offer_id==row.id;});
            data.push_back({text(row.id),text(row.name),money(row.price)+QStringLiteral(" · ")+number(row.unique_units)+QStringLiteral(" unique records"),
                QStringLiteral("%1\nRights: %2\nQuality %3 · contamination %4\nCoverage: documents %5 · scanned tables %6\nCommercial deployment: %7. Buying a license does not train an existing model.")
                .arg(text(row.provenance),text(row.rights),percent(row.quality),percent(row.contamination),percent(row.coverage[0]),percent(row.coverage[1]),row.commercial_use?QStringLiteral("permitted"):QStringLiteral("prohibited")),
                owned?QStringLiteral("Licensed"):QStringLiteral("Unlicensed"),!owned,{{QStringLiteral("owned"),owned}}});
        }
        const auto title_for=[&](const std::string& id) {
            const auto title=program.collection.messages.find(id+".title");
            return title!=program.collection.messages.end()?text(title->second):readable(id);
        };
        for (const auto& row:s.lab->catalog.research) {
            QStringList prerequisites;
            for (const auto& id:row.all_of) prerequisites.push_back(title_for(id));
            QStringList alternatives; for (const auto& id:row.any_of) alternatives.push_back(title_for(id));
            if (!alternatives.empty()) prerequisites.push_back(QStringLiteral("one of: ")+alternatives.join(QStringLiteral(", ")));
            const auto status=research_status(*s.lab,*s.capacity,row);
            const bool completed=std::any_of(s.lab->research.begin(),s.lab->research.end(),[&](const auto& run){return run.research_id==row.id && run.completed;});
            const bool published=std::any_of(s.lab->publications.begin(),s.lab->publications.end(),[&](const auto& publication){return publication.research_id==row.id;});
            const auto insight=public_insight(snapshot_->checkpoint.world,row.id);
            const bool has_insight=std::any_of(s.lab->insights.begin(),s.lab->insights.end(),[&](const auto& value){return value.research_id==row.id;});
            const bool can_adopt=insight && !has_insight && !completed && status!=QStringLiteral("In progress") &&
                s.date>=core::advance_days(insight->published_on,snapshot_->checkpoint.world.competition->setup->publication_delay);
            research.push_back({text(row.id),title_for(row.id),money(row.cash)+QStringLiteral(" · ")+QString::number(row.staff_days.value())+QStringLiteral(" staff-days"),
                QStringLiteral("%1 CU of finite experiments plus %2 staff-days. Both must complete.\nPrerequisites: %3\nPaid work and staff time are not refunded when stopped.")
                    .arg(cu(row.experiment_work.value()),QString::number(row.staff_days.value()),prerequisites.isEmpty()?QStringLiteral("none"):prerequisites.join(QStringLiteral(", "))),status,
                status!=QStringLiteral("Completed") && status!=QStringLiteral("In progress"),{{QStringLiteral("completed"),completed},{QStringLiteral("published"),published},{QStringLiteral("canAdopt"),can_adopt}}});
            if (const auto description=program.collection.messages.find(row.id+".detail"); description!=program.collection.messages.end())
                research.back().detail=text(description->second)+QStringLiteral("\n\n")+research.back().detail;
            research.back().detail+=published?QStringLiteral("\nPublished: other companies can adopt this result after the disclosed delay."):QStringLiteral("\nThis result remains private until explicitly published.");
            if (insight) research.back().detail+=QStringLiteral("\nPublic result from %1 · published %2 · adoption available %3 · %4 work credit. Cash and prerequisites are unchanged.")
                .arg(readable(insight->publisher),text(core::iso_date(insight->published_on)),text(core::iso_date(core::advance_days(insight->published_on,snapshot_->checkpoint.world.competition->setup->publication_delay))),percent(insight->credit));
            if (has_insight) research.back().detail+=QStringLiteral("\nPublic work credit has been adopted for this company's later research.");
        }
        for (const auto& row:report.models) {
            const auto* actual=find(s.lab->models,row.id); QStringList mixture;
            for (const auto& id:actual->data_ids) { const auto* d=find(s.lab->catalog.data,id); mixture.push_back(d?text(d->name):readable(id)); }
            const auto* work=find(s.capacity->projects,actual->project_id);
            models.push_back({text(row.id),text(row.id),readable(row.recipe_id)+QStringLiteral(" · ")+cu(row.consumed_work.value())+QStringLiteral(" CU consumed"),
                QStringLiteral("Data: %1\nParent checkpoint: %2\n%3\nServing cost %4 CU/task. True capability remains unknown until measured. A clone copies consumed work; it adds no training.")
                    .arg(mixture.join(QStringLiteral(", ")),row.parent_id.empty()?QStringLiteral("none"):text(row.parent_id),work?text(work->cause):QString{},cu(find(s.lab->catalog.recipes,row.recipe_id)->inference_per_task.value())),
                row.paused?QStringLiteral("Paused training"):row.training?QStringLiteral("Training"):QStringLiteral("Immutable checkpoint"),!row.training && row.consumed_work.value()>0,
                {{QStringLiteral("projectId"),text(actual->project_id)},{QStringLiteral("training"),row.training},{QStringLiteral("paused"),row.paused},{QStringLiteral("checkpointReady"),row.consumed_work.value()>0}}});
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
    const auto protected_cash=core::checked_unit_subtract(s.cash,core::discretionary_cash(s));
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
                    .arg(text(deployment->model_id),text(deployment->evidence_id),deployment->human_review?QStringLiteral("Paid human review; lower throughput"):QStringLiteral("Measured model only"),number(work->useful_tasks),work->customer_inference?QStringLiteral("Customer hardware"):text(work->source_id),cu(work->requested.value()),QString::number(work->staff)):QStringLiteral("Engineering service"),
                work && work->status==core::ProjectStatus::active?QStringLiteral("Serving"):QStringLiteral("Stopped"),false,{{QStringLiteral("projectId"),text(row.capacity_project_id)},{QStringLiteral("priceCents"),QString::number(row.price_per_task.value())},{QStringLiteral("freeWeights"),row.channel==core::ProductChannel::open_weights}}});
            const auto* region=regions?find(regions->regions,row.region_id):nullptr;
            products.back().detail+=QStringLiteral("\n%1 · %2").arg(channel_name(row.channel),region?text(region->name):QStringLiteral("All eligible regions"));
            products.back().detail+=QStringLiteral("\nCurrent price: %1 per task. Price changes preserve past invoices and signed contract prices.").arg(money(row.price_per_task));
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
        const auto response_detail=heavyweight_notice_detail(snapshot_->checkpoint.world,program,notice);
        rivals.push_back({text(notice.company_id)+QLatin1Char(':')+text(notice.kind)+QLatin1Char(':')+text(notice.subject),name,text(core::iso_date(notice.day)),
            !response_detail.isEmpty()?response_detail:notice.response_due ? QStringLiteral("Heavyweight mobilization warning: public customer traction was observed. Earliest response %1. Build a useful, funded service before then; rival capability is not public.").arg(text(core::iso_date(*notice.response_due))) :
            notice.kind=="publication" ? (notice.subject.empty()?QStringLiteral("The company published after consuming its own finite training budget. Publication does not reveal private capability or customer fit."):QStringLiteral("Published completed research: %1. Adoption needs the disclosed delay and your own research work; private checkpoints remain hidden.").arg(readable(notice.subject))) :
            notice.kind=="entry" ? QStringLiteral("The company entered with its authored finite capital, staff and lease commitments. Future results and private budgets remain hidden.") :
            notice.kind=="closure" ? QStringLiteral("The company closed after it could not service actual obligations.") : QStringLiteral("The company declined work that exceeded its available budget."),readable(notice.kind),false});
    }
    if (plannedCompetition()) for (const auto& release:snapshot_->checkpoint.world.competition->releases) {
        if (release.company_id==snapshot_->checkpoint.world.company_id) continue;
        const auto company=std::find_if(snapshot_->checkpoint.world.competition->companies.begin(),snapshot_->checkpoint.world.competition->companies.end(),[&](const auto& row){return row.definition_id==release.company_id;});
        if (company==snapshot_->checkpoint.world.competition->companies.end()) continue;
        const auto& definition=runtime::rival_definition(program,release.company_id);const auto& rival=company->simulation;
        const auto* product=find(rival.market->products,release.product_id);if (!product) continue;
        rivals.push_back({QStringLiteral("release:")+text(release.company_id)+QLatin1Char(':')+text(release.product_id),text(definition.name)+QStringLiteral(" · ")+readable(release.product_id),
            money(product->price_per_task)+QStringLiteral(" per task"),QStringLiteral("Public customer deliveries began %1. %2 · %3. This quote reveals the price of a released service; private model capability, cash and future decisions stay hidden.")
                .arg(text(core::iso_date(release.day)),text(rival.succession?rival.succession->terms.back().name:definition.planning->leader),text(definition.planning->region)),rival.phase==core::DayPhase::closed?QStringLiteral("Closed provider"):(company->response && company->response->retired_on)?QStringLiteral("Withdrawn program"):QStringLiteral("Public service"),false});
    }
    if (snapshot_->checkpoint.world.competition) for (const auto& company:snapshot_->checkpoint.world.competition->companies) {
        if (!company.simulation.frontier) continue;
        const auto& frontier=*company.simulation.frontier;
        const auto asi=core::earliest_frontier_qualification(frontier,true);
        const auto agi=core::earliest_frontier_qualification(frontier,false);
        if (!agi && !asi) continue;
        const auto& definition=runtime::rival_definition(program,company.definition_id);
        const auto day=asi?*asi:*agi;
        rivals.push_back({QStringLiteral("frontier:")+text(company.definition_id),text(definition.name),text(core::iso_date(day)),
            QStringLiteral("Independent assessment of a deployed system completed on %1. Qualification follows paid held-out panels, observed useful work and, for game ASI, independent physical replication.")
                .arg(text(core::iso_date(day))),asi?QStringLiteral("Game ASI independently recorded"):QStringLiteral("Game AGI independently recorded"),false});
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
            if (offer.public_announcement) funding.back().detail+=tr(" Acceptance publicly announces this financing round; its private spending and balances remain undisclosed.");
        }
        for (const auto& offer:m.catalog.specialists) {
            const bool hired=std::any_of(m.specialists.begin(),m.specialists.end(),[&](const auto& h){return h.offer_id==offer.id;});
            specialists.push_back({text(offer.id),text(offer.name),money(offer.signing_cost)+QStringLiteral(" signing · ")+money(offer.monthly_salary)+QStringLiteral("/month"),
                QStringLiteral("Adds one available staff member today and a recurring salary. Past work keeps its actual staffing. Extra staff does not create compute or improve model capability for free."),hired?QStringLiteral("Hired"):QStringLiteral("Available"),!hired});
        }
        for (const auto& p:m.policies) {
            if (p.kind==core::DelegationKind::operating_reserve) {
                policies.push_back({text(p.id),tr("Operating cash reserve"),tr("%1 target").arg(money(p.reserve_floor)),
                    p.enabled?operatingReserveText():tr("Suspended. Resume this policy to restore its original target."),
                    p.enabled?tr("Protecting cash"):tr("Suspended"),false,{{QStringLiteral("enabled"),p.enabled}}});
                continue;
            }
            if (p.kind==core::DelegationKind::hardware_replacement) {
                const auto& infrastructure=*s.infrastructure;
                const auto* family=find(infrastructure.catalog.hardware,p.plan_id);
                const auto* rack=find(infrastructure.hardware,p.target_id);
                const auto* site=find(infrastructure.sites,rack->site_id);
                const auto status=p.runs==2?tr("Replaced"):!p.enabled?tr("Suspended"):
                    p.runs==0?tr("Order scheduled"):tr("Replacement ordered");
                auto detail=tr("%1 at %2 → %3\nPurchase ceiling %4 · maintenance ceiling %5/month · cash floor %6")
                    .arg(text(p.target_id),text(site->name),text(family->name),money(p.budget),money(p.monthly_ceiling),money(p.reserve_floor));
                if (p.runs<2) detail+=tr("\nNext eligible review %1").arg(text(core::iso_date(p.next_review)));
                if (!p.blocked.empty()) detail+=QLatin1Char('\n')+text(p.blocked);
                policies.push_back({text(p.id),tr("Replace obsolete hardware"),tr("%1 paid · %2 of 2 actions").arg(money(p.committed)).arg(p.runs),
                    std::move(detail),status,false,{{QStringLiteral("enabled"),p.enabled}}});
                continue;
            }
            if (p.kind==core::DelegationKind::pricing && p.pricing) {
                policies.push_back({text(p.id),QStringLiteral("Bounded product pricing"),QStringLiteral("%1 / %2 reviews · %3 to %4").arg(p.runs).arg(p.maximum_runs).arg(money(p.pricing->floor),money(p.pricing->ceiling)),
                    QStringLiteral("Product %1 · next review %2 · every %3 days\nStep %4 · low utilization %5 · high utilization %6\nReserve floor %7 · no spending authority\n%8\nEach review uses up to seven settled days. Manual price changes suspend this policy. Counters and prior decisions are retained.")
                        .arg(text(p.target_id),text(core::iso_date(p.next_review)),QString::number(p.interval.value()),money(p.pricing->step),percent(p.pricing->low_utilization),percent(p.pricing->high_utilization),money(p.reserve_floor),p.blocked.empty()?QStringLiteral("No blocked action"):text(p.blocked)),
                    p.enabled?QStringLiteral("Enabled"):p.runs>=p.maximum_runs?QStringLiteral("Review limit reached"):QStringLiteral("Suspended / blocked"),false,{{QStringLiteral("enabled"),p.enabled}}});
                continue;
            }
            policies.push_back({text(p.id),p.kind==core::DelegationKind::evaluation?QStringLiteral("Held-out evaluation policy"):QStringLiteral("Compute lease renewal policy"),
            QStringLiteral("%1 / %2 runs · %3 / %4 committed").arg(p.runs).arg(p.maximum_runs).arg(money(p.committed),money(p.budget)),
            QStringLiteral("Target %1 · next review %2\nReserve floor %3 · monthly ceiling %4\n%5 CU/day · %6 staff · %7-day evaluation interval\n%8\nSuspending preserves all spent/committed budget and prior decisions.")
                .arg(text(p.target_id),text(core::iso_date(p.next_review)),money(p.reserve_floor),money(p.monthly_ceiling),cu(p.allocation.daily_compute.value()),QString::number(p.allocation.staff),QString::number(p.interval.value()),p.blocked.empty()?QStringLiteral("No blocked action"):text(p.blocked)),p.enabled?QStringLiteral("Enabled"):p.runs>=p.maximum_runs?QStringLiteral("Run limit reached"):QStringLiteral("Suspended / blocked"),false,{{QStringLiteral("enabled"),p.enabled}}});
        }
        decisions=policyDecisionRows();
        for (const auto& n:m.notices) alerts.push_back({text(n.id),readable(n.kind),text(core::iso_date(n.day))+(n.deadline?QStringLiteral(" · deadline ")+text(core::iso_date(*n.deadline)):QString{}),
            alertDetail(n),n.acknowledged?QStringLiteral("Acknowledged"):n.required?QStringLiteral("Required before work"):QStringLiteral("Actionable / informational"),!n.acknowledged});
    }
    auto set=[&](StableListModel* model,std::vector<CampaignRow> rows){if (replace) model->beginCampaign(std::move(rows)); else model->setRows(std::move(rows));};
    set(regional_markets_,regions?regional_rows(world,*regions):std::vector<CampaignRow>{});
    set(data_,std::move(data));set(research_,std::move(research));set(models_,std::move(models));set(evidence_,std::move(evidence));set(offers_,std::move(offers));set(products_,std::move(products));set(contracts_,std::move(contracts));set(rivals_,std::move(rivals));set(payables_,std::move(payables));
    set(funding_,std::move(funding));set(specialists_,std::move(specialists));set(policies_,std::move(policies));set(decisions_,std::move(decisions));set(alerts_,std::move(alerts));
    auto response=heavyweight_models(snapshot_->checkpoint,program);set(heavyweights_,std::move(response.responses));set(competitive_offers_,std::move(response.offers));
    auto corporate=governance_models(snapshot_->checkpoint,program);ownership_text_=std::move(corporate.ownership);board_text_=std::move(corporate.board);
    set(charters_,std::move(corporate.charters));set(corporate_finance_,std::move(corporate.financing));set(executives_,std::move(corporate.executives));set(board_motions_,std::move(corporate.motions));set(corporate_disputes_,std::move(corporate.disputes));set(merger_targets_,std::move(corporate.targets));
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
        result.push_back(option(row.id,readable(row.id)+QStringLiteral(" · ")+cu(std::max<std::int64_t>(0,core::company_capacity(row).value()-reserved))+QStringLiteral(" CU/day free")));
    }
    return result;
}
QVariantList ManagementController::services() const {
    QVariantList result; if (!available()) return result;
    auto ordered=state().deployments->catalog;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.required_recipe.size()<b.required_recipe.size();});
    for (const auto& row:ordered) {
        const auto channel=row.product.channel;
        auto label=readable(row.product.id);
        if (channel!=core::ProductChannel::legacy) label+=QStringLiteral(" · ")+channel_name(channel);
        auto item=option(row.product.id,std::move(label));
        item.insert(QStringLiteral("reviewAllowed"),row.review_staff>0);
        item.insert(QStringLiteral("reviewRequired"),channel==core::ProductChannel::agent);
        item.insert(QStringLiteral("customerHardware"),core::customer_hosted(channel));
        item.insert(QStringLiteral("existing"),find(state().market->products,row.product.id)!=nullptr);
        result.push_back(item);
    }
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
    auto monthly=core::infrastructure_monthly_fixed_cost(state(),state().date);
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
    return {{QStringLiteral("budget"),money_input(c.default_policy_budget)},{QStringLiteral("reserve"),money_input(c.default_reserve_floor)},
        {QStringLiteral("monthly"),money_input(c.default_monthly_ceiling)},{QStringLiteral("runs"),QString::number(c.default_policy_runs)},
        {QStringLiteral("interval"),QString::number(c.minimum_evaluation_interval.value())}};
}
QVariantList ManagementController::renewableLeases() const {
    QVariantList result;if (!snapshot_ || !state().business_offers) return result;
    for (const auto& offer:state().business_offers->catalog) if (offer.renewable && std::any_of(state().business_offers->accepted.begin(),state().business_offers->accepted.end(),[&](const auto& a){return a.offer_id==offer.id;}))
        result.push_back(option(offer.id,text(offer.name)+QStringLiteral(" · expires ")+text(core::iso_date(core::lease_expiry(*state().business_offers,offer.id)))+QStringLiteral(" · ceiling ")+money(core::lease_commitment_ceiling(offer))));
    return result;
}
QVariantList ManagementController::pricedProducts() const {
    QVariantList result;if (!snapshot_ || !state().market || !state().capacity) return result;
    const auto input=[](std::int64_t value){return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0'));};
    for (const auto& p:state().market->products) {
        const auto* project=find(state().capacity->projects,p.capacity_project_id);
        if (!p.available || p.channel==core::ProductChannel::open_weights || !project || project->status!=core::ProjectStatus::active) continue;
        auto row=option(p.id,readable(p.id)+QStringLiteral(" · current quote ")+money(p.price_per_task));
        row.insert(QStringLiteral("floor"),input(p.price_per_task.value()/2));
        row.insert(QStringLiteral("ceiling"),input(std::min(core::market_price_limit,std::max<std::int64_t>(100,p.price_per_task.value()*2))));
        row.insert(QStringLiteral("step"),QStringLiteral("0.05"));
        result.push_back(std::move(row));
    }
    return result;
}
QString ManagementController::operatingReserveText() const {
    if (!snapshot_) return {};
    const auto target=core::operating_reserve_target(state());
    if (target.value()==0) return tr("No operating reserve is enabled.");
    const auto protected_amount=std::min(target,core::uncommitted_cash(state()));
    auto result=tr("%1 of %2 protected · %3 available for new spending.")
        .arg(money(protected_amount),money(target),money(core::discretionary_cash(state())));
    if (protected_amount<target)
        result+=tr(" New cash replenishes the reserve after existing commitments.");
    return result;
}
QVariantList ManagementController::replacementRacks() const {
    QVariantList result;
    if (!snapshot_ || !state().infrastructure) return result;
    const auto& infrastructure=*state().infrastructure;
    for (const auto& rack:infrastructure.hardware) {
        if (!rack.commissioned_on || rack.retired_on) continue;
        const auto* family=find(infrastructure.catalog.hardware,rack.family_id);
        const auto* site=find(infrastructure.sites,rack.site_id);
        const auto eligible=core::advance_days(rack.delivered_on,family->useful_life);
        if (eligible>=state().end_date) continue;
        result.push_back(option(rack.id,tr("%1 · %2 at %3 · eligible %4")
            .arg(text(rack.id),text(family->name),text(site->name),text(core::iso_date(eligible)))));
    }
    return result;
}
QVariantList ManagementController::replacementFamilies(QString rack) const {
    QVariantList result;
    if (!snapshot_ || !state().infrastructure || rack.isEmpty()) return result;
    const auto& infrastructure=*state().infrastructure;
    const auto id=rack.toStdString();
    const auto* original=find(infrastructure.hardware,id);
    if (!original || original->retired_on) return result;
    for (const auto& family:infrastructure.catalog.hardware) {
        try {
            const auto terms=core::hardware_replacement_terms(state(),id,family.id);
            auto row=option(family.id,tr("%1 · %2 to buy · %3/month maintenance")
                .arg(text(family.name),money(terms.purchase),money(terms.monthly_maintenance)));
            row.insert(QStringLiteral("purchase"),money_input(terms.purchase));
            row.insert(QStringLiteral("maintenance"),money_input(terms.monthly_maintenance));
            if (family.id==original->family_id) result.push_front(std::move(row));
            else result.push_back(std::move(row));
        } catch (const std::invalid_argument&) {
            // Only compatible families are offered for this incumbent.
        }
    }
    return result;
}
void ManagementController::authorizeOperatingReserve(QString target) {
    if (!editable()) return;
    try {
        core::DelegationPolicy policy;
        policy.id=nextId(QStringLiteral("policy.")).toStdString();
        policy.kind=core::DelegationKind::operating_reserve;
        policy.reserve_floor=policy_money(target);
        policy.interval=core::Days{0};
        emit actionRequested(core::AddDelegationPolicy{std::move(policy)});
    } catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::authorizeHardwareReplacement(QString rack,QString family,QString purchase,QString monthly,QString reserve) {
    if (!editable()) return;
    try {
        core::DelegationPolicy policy;
        policy.id=nextId(QStringLiteral("policy.")).toStdString();
        policy.kind=core::DelegationKind::hardware_replacement;
        policy.target_id=rack.toStdString();
        policy.plan_id=family.toStdString();
        policy.budget=policy_money(purchase);
        policy.monthly_ceiling=policy_money(monthly);
        policy.reserve_floor=policy_money(reserve);
        policy.interval=core::Days{1};
        policy.maximum_runs=2;
        emit actionRequested(core::AddDelegationPolicy{std::move(policy)});
    } catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
int ManagementController::decisionPageCount() const {
    if (!snapshot_ || !state().management) return 0;
    return static_cast<int>((state().management->decisions.size()+127)/128);
}
QString ManagementController::decisionPageLabel() const {
    if (!decisionPageCount()) return QStringLiteral("No policy decisions yet");
    return QStringLiteral("Page %1 of %2 · newest first · 128 decisions per page").arg(decision_page_+1).arg(decisionPageCount());
}
std::vector<CampaignRow> ManagementController::policyDecisionRows() const {
    std::vector<CampaignRow> rows;if (!snapshot_ || !state().management) return rows;
    const auto& history=state().management->decisions;
    const auto end=history.size()-std::min(history.size(),static_cast<std::size_t>(decision_page_)*128);
    const auto begin=end>128?end-128:0;
    rows.reserve(end-begin);
    for (auto i=end;i>begin;) {
        --i;const auto& d=history[i];auto detail=text(d.explanation);
        const auto* policy=find(state().management->policies,d.policy_id);
        auto status=d.overridden?tr("Player override · suspended"):d.accepted?tr("Committed"):tr("Refused · suspended");
        if (policy && policy->kind==core::DelegationKind::operating_reserve) {
            status=d.result_id=="reserve_enabled"?tr("Reserve enabled"):tr("Reserve suspended");
            detail+=tr("\nAuthorized target %1").arg(money(policy->reserve_floor));
        } else if (!d.result_id.empty()) {
            detail+=tr("\nResult: %1").arg(text(d.result_id));
            if (policy && policy->kind==core::DelegationKind::hardware_replacement)
                status=d.result_id==policy->target_id?tr("Old rack retired"):tr("Replacement ordered");
        }
        if (d.price_event_cursor && *d.price_event_cursor>0 && *d.price_event_cursor<=state().market->price_events.size()) {
            const auto& e=state().market->price_events[static_cast<std::size_t>(*d.price_event_cursor-1)];
            status=tr("Price reviewed");
            detail+=QStringLiteral("\n%1: %2 → %3 per task. No cash spent; existing contracts retain their terms.").arg(readable(e.product_id),money(e.before),money(e.after));
        }
        // StableListModel sorts identities; a fixed inverse cursor keeps newest first.
        const auto identity=QStringLiteral("decision:%1").arg(static_cast<qulonglong>(core::management_decision_limit-i),5,10,QLatin1Char('0'));
        rows.push_back({identity,text(d.policy_id),text(core::iso_date(d.day))+QStringLiteral(" · ")+money(d.commitment),std::move(detail),std::move(status),false});
    }
    return rows;
}
void ManagementController::setDecisionPage(int page) {
    const auto next=std::clamp(page,0,std::max(0,decisionPageCount()-1));
    if (next==decision_page_) return;
    decision_page_=next;decisions_->beginCampaign(policyDecisionRows());emit changed();
}
QString ManagementController::pricingPreview(QString product,QString floor,QString ceiling,QString step,QString low,QString high) const {
    if (!snapshot_ || !state().market) return QStringLiteral("Start a company to authorize pricing");
    try {
        core::DelegationPolicy p;p.kind=core::DelegationKind::pricing;p.target_id=product.toStdString();p.pricing=pricing_bounds(floor,ceiling,step,low,high);
        const auto* target=find(state().market->products,p.target_id);
        if (!target) return QStringLiteral("Launch a paid product before authorizing pricing");
        if (target->price_per_task<p.pricing->floor || target->price_per_task>p.pricing->ceiling) return QStringLiteral("The current price must be inside the proposed range");
        const auto preview=core::pricing_review(state(),p,state().date,target->price_per_task);
        return QStringLiteral("Current settled evidence: %1\nProposed quote on this evidence: %2 → %3 per task. The scheduled review will use the settled days available then.")
            .arg(text(preview.explanation),money(target->price_per_task),money(preview.quote));
    } catch (const std::exception& error) {return QString::fromUtf8(error.what());}
}
void ManagementController::authorizePricing(QString product,QString floor,QString ceiling,QString step,QString low,QString high,QString interval,QString runs,QString reserve) {
    if (!editable()) return;
    try {
        core::DelegationPolicy p;p.id=nextId(QStringLiteral("policy.")).toStdString();p.target_id=product.toStdString();p.kind=core::DelegationKind::pricing;
        p.pricing=pricing_bounds(floor,ceiling,step,low,high);p.reserve_floor=policy_money(reserve);
        p.interval=core::Days{integer(interval,365)};p.maximum_runs=static_cast<std::uint32_t>(integer(runs,core::pricing_review_limit));
        emit actionRequested(core::AddDelegationPolicy{std::move(p)});
    } catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
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
        const auto p=runtime::preview_campaign_training(*program_,snapshot_->checkpoint.world,req);
        std::int64_t reserved=0; const auto* supply=find(state().capacity->sources,req.source_id);
        for (const auto& row:state().capacity->projects) if (row.status==core::ProjectStatus::active && row.source_id==req.source_id) reserved=core::checked_add(reserved,row.requested.value());
        const auto capacity=core::capacity_report(*state().capacity);
        const auto* recipe_rules=find(state().lab->catalog.recipes,req.recipe_id);
        const bool compatible=supply && recipe_rules && supply->memory_mib>=recipe_rules->minimum_memory_mib && supply->interconnect==recipe_rules->interconnect;
        const bool free=compatible && core::company_capacity(*supply).value()-reserved>=req.daily_compute.value() && capacity.available_staff-capacity.assigned_staff>=req.staff;
        core::Cents protected_cash{0};for (const auto& reservation:state().reservations) protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
        const auto horizon=core::Days{std::min<std::int64_t>(90,p.minimum_days.value())};
        const auto forecast=core::project_company_cash(state(),core::advance_days(state().date,horizon),protected_cash);
        const auto* parent_model=parent.isEmpty()?nullptr:find(state().lab->models,req.parent_id);
        const auto* parent_work=parent_model && !parent_model->project_id.empty()?find(state().capacity->projects,parent_model->project_id):nullptr;
        const bool frozen_parent=parent.isEmpty() || (parent_model && parent_model->consumed_work.value()>0 && (!parent_work || parent_work->status==core::ProjectStatus::completed || parent_work->status==core::ProjectStatus::cancelled));
        const auto explanation=QStringLiteral("Earliest %1 days · unlicensed data %2\nEstimated %3\n%4\nBottlenecks: documents %5; scanned tables %6\n%7\n%8\n%9")
            .arg(QString::number(p.minimum_days.value()),money(p.unowned_data_cost),interval(p.lower,p.upper),text(p.explanation),text(p.bottlenecks[0]),text(p.bottlenecks[1]),
                p.licensed?QStringLiteral("All selected data licensed"):QStringLiteral("Buy each selected data license before training"),!compatible?QStringLiteral("Select a source with the recipe’s memory and interconnect class"):free?QStringLiteral("Requested compute and staff fit current free resources"):QStringLiteral("Reduce allocation or stop another project to free compute/staff"),
                QStringLiteral("Available cash after %2 forecast days: %1. Includes existing bills; excludes future customer sales and unpurchased licenses. Forecast capped at 90 days.").arg(money(forecast.available),QString::number(horizon.value())));
        return {{QStringLiteral("text"),explanation+(frozen_parent?QString{}:QStringLiteral("\nSave an immutable checkpoint of the running or paused parent before continuing."))+(p.prerequisites_met?QString{}:QStringLiteral("\nComplete the selected recipe’s research prerequisites before training."))},{QStringLiteral("ready"),p.licensed && p.prerequisites_met && free && frozen_parent}};
    } catch (const std::exception& e) { return {{QStringLiteral("text"),QString::fromUtf8(e.what())},{QStringLiteral("ready"),false}}; }
}
QString ManagementController::servicePreview(QString id,QString model,bool review) const {
    if (!available()) return {};
    for (const auto& row:state().deployments->catalog) if (row.product.id==id.toStdString()) {
        const auto* m=find(state().lab->models,model.toStdString());
        const auto* recipe=m?find(state().lab->catalog.recipes,m->recipe_id):nullptr;
        QStringList prerequisites;
        for (const auto& research:row.required_research) prerequisites.push_back(readable(research));
        const auto hardware=core::customer_hosted(row.product.channel)?QStringLiteral("Uses the customer's shared local hardware."):QStringLiteral("Reserves serving capacity on the selected source.");
        const auto rights=row.product.channel==core::ProductChannel::open_weights?QStringLiteral("\nWeight release is free and requires redistribution rights across the training lineage."):QString{};
        return QStringLiteral("Model lower bounds · documents ≥ %1 · scanned tables ≥ %2\n%3 tasks/day · %4 staff · %5 support/month\n%6\n%7\n%8%9%10")
            .arg(percent(row.required[0]),percent(row.required[1]),number(review?row.review_tasks_per_day:row.serving.tasks_per_day),QString::number(row.serving.staff+(review?row.review_staff:0)),
                money(core::checked_unit_add(row.serving.monthly_support_cost,review?row.review_monthly_cost:core::Cents{0})),
                recipe?cu(core::required_inference(review?row.review_tasks_per_day:row.serving.tasks_per_day,recipe->inference_per_task).value())+QStringLiteral(" CU/day using this checkpoint"):QStringLiteral("Choose a checkpoint to see serving compute"),
                QStringLiteral("Discretionary price %1/task · review credits %2 documents and %3 scanned tables.").arg(money(row.product.price_per_task),percent(review?row.review_credit[0]:core::BasisPoints{0}),percent(review?row.review_credit[1]:core::BasisPoints{0})),
                hardware,prerequisites.isEmpty()?QString{}:QStringLiteral("\nRequired research · %1.").arg(prerequisites.join(QStringLiteral(", "))),rights);
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
void ManagementController::publishResearch(QString id) {if (editable()) emit actionRequested(core::PublishResearch{id.toStdString()});}
void ManagementController::acquirePublication(QString id) {
    if (!editable()) return;
    if (const auto insight=public_insight(snapshot_->checkpoint.world,id.toStdString())) emit actionRequested(core::AcquireResearchInsight{*insight});
}
void ManagementController::setProductPrice(QString id,QString cents) {
    if (!editable()) return;
    try {emit actionRequested(core::SetMarketPrice{id.toStdString(),core::Cents{cents==QStringLiteral("0")?0:integer(cents,core::market_price_limit)}});}
    catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::train(QString recipe,QString source,QStringList data,QString rate,QString staff,QString work,QString parent) {
    if (!editable()) return; try { emit actionRequested(core::StartTraining{request(nextId(QStringLiteral("model.")),recipe,source,data,rate,staff,work,parent)}); } catch (const std::exception& e) { emit noticeRaised(QString::fromUtf8(e.what())); }
}
void ManagementController::checkpoint(QString id) {if(editable()) emit actionRequested(core::CheckpointModel{id.toStdString(),nextId(QStringLiteral("checkpoint.")).toStdString()});}
void ManagementController::clone(QString id) { if (editable()) emit actionRequested(core::CloneModel{id.toStdString(),nextId(QStringLiteral("checkpoint.")).toStdString()}); }
void ManagementController::evaluate(QString model,QString plan,QString source,QString rate,QString staff) {
    if (!editable()) return; try { emit actionRequested(core::StartEvaluation{nextId(QStringLiteral("evaluation.")).toStdString(),model.toStdString(),plan.toStdString(),allocation(source,rate,staff)}); } catch (const std::exception& e) { emit noticeRaised(QString::fromUtf8(e.what())); }
}
void ManagementController::deploy(QString service,QString model,QString evidence,QString source,bool review,QString region) {
    if (editable()) emit actionRequested(core::DeployModel{service.toStdString(),model.toStdString(),evidence.toStdString(),source.toStdString(),review,region.toStdString()});
}
void ManagementController::acceptOffer(QString id) { if (editable()) emit actionRequested(core::AcceptBusinessOffer{id.toStdString()}); }
void ManagementController::respondCompetitiveOffer(QString id,bool accept) {if (editable()) emit competitiveResponseRequested(std::move(id),accept);}
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

namespace pgp::bridge {
bool ManagementController::governanceAvailable() const {return snapshot_ && state().management && state().management->governance.has_value();}
void ManagementController::negotiateFinance(QString id) {if (editable()) emit actionRequested(core::NegotiateCorporateFinance{id.toStdString()});}
void ManagementController::appointExecutive(QString id,bool hybrid) {if (editable()) emit actionRequested(core::AppointExecutive{id.toStdString(),hybrid});}
void ManagementController::proposeCharter(QString id) {if(editable()) emit actionRequested(core::ProposeCharter{id.toStdString()});}
void ManagementController::proposePool(QString bp) {
    if (!editable()) return;
    try {emit actionRequested(core::ProposePoolExpansion{core::BasisPoints{integer(bp,2000)}});}
    catch (const std::exception& e) {emit noticeRaised(QString::fromUtf8(e.what()));}
}
void ManagementController::voteBoard(QString id,bool support) {if (editable()) emit actionRequested(core::VoteBoardMotion{id.toStdString(),support});}
void ManagementController::respondDispute(QString id,bool waiver) {if (editable()) emit actionRequested(core::RespondCorporateDispute{id.toStdString(),waiver});}
QString ManagementController::acquisitionPreview(QString id,bool equity) {
    acquisition_preview_revision_.reset();if (!editable() || !program_) return {};
    try {
        const auto result=acquisition_terms(snapshot_->checkpoint,*program_,id.toStdString(),equity);
        acquisition_preview_revision_=state().revision;acquisition_preview_target_=id;acquisition_preview_equity_=equity;return result;
    } catch (const std::exception& e) {emit noticeRaised(QString::fromUtf8(e.what()));return {};}
}
void ManagementController::proposeAcquisition(QString id,bool equity) {
    if (!editable()) return;
    if (!acquisition_preview_revision_ || *acquisition_preview_revision_!=state().revision || acquisition_preview_target_!=id || acquisition_preview_equity_!=equity) {
        emit noticeRaised(tr("The company changed after due diligence. Review the current seller counteroffer again."));return;
    }
    acquisition_preview_revision_.reset();emit acquisitionRequested(std::move(id),equity);
}
}
namespace pgp::bridge {
void ManagementController::transferSubsidiaryFunds(QString id,QString dollars,bool funding) {
    if (!editable()) return;
    try {const auto amount=policy_money(dollars);if (amount.value()<=0) throw std::invalid_argument("Group transfer must be positive");emit groupTransferRequested(std::move(id),amount,funding);}
    catch (const std::exception& e) {emit noticeRaised(QString::fromUtf8(e.what()));}
}
}
