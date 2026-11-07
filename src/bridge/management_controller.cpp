#include "management_controller.hpp"
#include "lab_inputs.hpp"
#include "flow_presentation.hpp"
#include "heavyweight_presentation.hpp"
#include "governance_presentation.hpp"
#include <pgp/core/company_finance.hpp>
#include <pgp/core/integer_codec.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/society.hpp>
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
CampaignRow funding_support_row(const core::SimulationState& state,const core::FundingSupportOffer& offer) {
    const auto status=core::funding_support_status(state,offer);
    const bool grant=offer.kind==core::FundingSupportKind::research_grant;
    const auto date=[](core::OrdinalDay day){return text(core::iso_date(day));};
    QString detail;
    if (grant) {
        QStringList topics;
        for (const auto& target:offer.targets) topics.push_back(readable(target));
        detail=QObject::tr("Covers up to %1 of each eligible research fee. Eligible topics are %2. "
                           "The grant pays its share when work starts. Stopping the project keeps the original charges.")
            .arg(percent(offer.coverage),topics.join(QStringLiteral(", ")));
    } else {
        const auto& target=offer.targets.front();
        const auto bill=target=="compute_lease"?QObject::tr("the opening cloud lease"):
            target=="founder_compute"?QObject::tr("the founder compute lease"):readable(target.substr(std::string_view{"offer.lease/"}.size()));
        detail=QObject::tr("Covers up to %1 of the daily bill for %2. The credit lowers the payable before cash settlement. "
                           "Cash forecasts use the gross lease terms.")
            .arg(percent(offer.coverage),bill);
    }
    detail+=QObject::tr("\nClaim from %1 up to %2. One award starts the morning after you claim it and lasts %3 days. "
                        "The award is reserved for its eligible expenses and any unused amount expires.")
        .arg(date(status.opens_on),date(status.claim_before),QString::number(offer.use_days.value()));
    QString caption;
    if (status.expires_before) {
        if (state.date<*status.active_on) caption=QObject::tr("Starts next morning");
        else if (status.unused.value()==0) caption=QObject::tr("Fully used");
        else if (state.date>=*status.expires_before) caption=QObject::tr("Expired");
        else caption=QObject::tr("Active");
        detail+=QObject::tr("\nActive from %1 up to %2. Applied %3. %4 %5.")
            .arg(date(*status.active_on),date(*status.expires_before),money(core::checked_unit_subtract(offer.limit,status.unused)),
                 money(status.unused),state.date>=*status.expires_before?QObject::tr("expired unused"):QObject::tr("remaining"));
        const auto& uses=state.management->funding_support.uses;
        const auto latest=std::find_if(uses.rbegin(),uses.rend(),[&](const auto& use){return use.offer_id==offer.id;});
        if (latest!=uses.rend()) detail+=QObject::tr("\nLatest application %1 on %2 to %3.")
            .arg(money(latest->amount),date(latest->day),text(latest->target));
    } else if (status.claimable) caption=QObject::tr("Available to claim");
    else if (state.date<status.opens_on) caption=QObject::tr("Opens %1").arg(date(status.opens_on));
    else if (state.date>=status.claim_before) caption=QObject::tr("Claim window closed");
    else caption=QObject::tr("Unavailable for this campaign or lease");
    return {text(offer.id),text(offer.name),QObject::tr("%1 in eligible fee support").arg(money(offer.limit)),
        std::move(detail),std::move(caption),status.claimable,{}};
}
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
core::StageServiceRollout rollout_request(const QString& service,const QString& model,const QString& evidence,
    const QString& source,bool review,const QString& share,const QString& days) {
    return {{service.toStdString(),model.toStdString(),evidence.toStdString(),source.toStdString(),review},
        core::BasisPoints{integer(share,core::service_trial_share_limit/100)*100},
        core::Days{integer(days,core::service_trial_days_limit)}};
}
core::Days service_term(const QString& days) { return core::Days{integer(days,core::service_term_max_days)}; }
QString compatibility_terms(const core::CompatibilityProfile& profile) {
    QStringList floors;
    if (profile.quality[0].value()>0) floors.push_back(QObject::tr("documents ≥ %1").arg(percent(profile.quality[0])));
    if (profile.quality[1].value()>0) floors.push_back(QObject::tr("scanned tables ≥ %1").arg(percent(profile.quality[1])));
    auto detail=QObject::tr("Keep at least %1 tasks/day of serving capacity and these measured quality floors · %2.")
        .arg(number(profile.tasks_per_day),floors.join(QStringLiteral(" · ")));
    if (profile.human_review) detail+=QObject::tr(" Keep paid human review.");
    if (profile.customer_compute_per_task.value()>0) detail+=QObject::tr(" Buyer hardware stays within %1 MiB, %2 interconnect and %3 CU per task.")
        .arg(number(profile.customer_memory_mib),text(profile.customer_interconnect),cu(profile.customer_compute_per_task.value()));
    return detail;
}
void add_service_support(CampaignRow& row,const core::SimulationState& state,const core::MarketProduct& product,bool live) {
    row.fields.insert(QStringLiteral("live"),live);
    if (const auto* term=core::market_compatibility(*state.market,product.id,state.date)) {
        row.fields.insert(QStringLiteral("compatibilityActive"),true);
        row.detail+=QObject::tr("\nCompatibility commitment ends %1. %2")
            .arg(text(core::iso_date(term->ends_before)),compatibility_terms(core::compatibility_profile(state,*term)));
    } else {
        const auto latest=std::find_if(state.market->compatibility.rbegin(),state.market->compatibility.rend(),
            [&](const auto& term){return term.product_id==product.id;});
        if (latest!=state.market->compatibility.rend())
            row.detail+=QObject::tr("\nLast compatibility commitment ended on %1.").arg(text(core::iso_date(latest->ends_before)));
    }
    if (const auto& notice=product.retirement; notice) {
        const auto date=text(core::iso_date(notice->retires_on));
        const auto announced=text(core::iso_date(notice->announced_on));
        const bool retired=state.date>=notice->retires_on;
        row.fields.insert(QStringLiteral("retirementAnnounced"),true);
        row.fields.insert(QStringLiteral("retired"),retired);
        row.status=retired?QObject::tr("Retired"):QObject::tr("Retires %1").arg(date);
        row.detail+=retired?QObject::tr("\nRetired on %1 after notice on %2. Release and customer history remain available.").arg(date,announced):
            QObject::tr("\nRetirement announced on %1. Serving ends on %2. Discretionary adoption is limited to existing share. Signed work continues through its term.").arg(announced,date);
    }
}
void add_release_details(CampaignRow& row,const core::SimulationState& state,const core::MarketProduct& product) {
    const auto& releases=*state.deployments;
    const auto current=std::ranges::find(releases.records,product.capacity_project_id,&core::DeploymentRecord::project_id);
    if (current==releases.records.end()) return;
    row.detail.prepend(QObject::tr("Release %1\n").arg(current-releases.records.begin()+1));
    if (current->restored_release>0)
        row.detail+=QObject::tr("\nRestored release %1.").arg(number(current->restored_release));
    if (const auto prior=core::rollback_release(releases,*state.market,product.id); prior>0) {
        const auto& previous=releases.records[static_cast<std::size_t>(prior-1)];
        row.fields.insert(QStringLiteral("rollbackTarget"),QObject::tr("Release %1 · %2 · report %3")
            .arg(number(prior),text(previous.model_id),text(previous.evidence_id)));
    }
    const auto latest=std::find_if(releases.rollouts.rbegin(),releases.rollouts.rend(),[&](const auto& rollout) {
        return rollout.candidate.service_id==product.id;
    });
    if (latest==releases.rollouts.rend()) return;
    const auto index=static_cast<std::size_t>(std::prev(latest.base())-releases.rollouts.begin());
    const auto& route=state.market->trials.at(index);
    const auto* work=find(state.capacity->projects,route.project_id);
    if (!work) return;
    row.detail+=QObject::tr("\nStaged checkpoint %1 · report %2 · %3 tasks served.")
        .arg(text(latest->candidate.model_id),text(latest->candidate.evidence_id),number(work->useful_tasks));
    if (latest->resolved_on) {
        row.detail+=latest->promoted_release>0?QObject::tr(" Promoted as release %1.").arg(number(latest->promoted_release)):
            QObject::tr(" Trial discarded.");
        return;
    }
    row.fields.insert(QStringLiteral("trialPending"),true);
    row.fields.insert(QStringLiteral("trialCanPromote"),work->useful_tasks>0);
    row.fields.insert(QStringLiteral("trialCheckpoint"),text(latest->candidate.model_id));
    if (route.stopped_on) row.detail+=QObject::tr(" Trial ended on %1 and released its staff and compute. Choose whether to promote or discard it.")
        .arg(text(core::iso_date(*route.stopped_on)));
    else row.detail+=QObject::tr(" Up to %1 of discretionary orders through the day before %2. Production serves signed contracts.")
        .arg(percent(route.share),text(core::iso_date(route.ends_before)));
}
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
    auto terms=QStringLiteral("%1\n%2/month · setup %3 · %4 days\n%5\nAvailable %6 through %7 (exclusive). Accepted once.")
        .arg(text(offer.name),money(offer.monthly_amount),money(offer.setup_cost),QString::number(offer.term.value()),
            offer.kind==core::BusinessOfferKind::compute_lease ? cu(offer.capacity.value())+QStringLiteral(" CU/day; power included. Capacity expires at term end.") :
            number(offer.tasks_per_day)+QStringLiteral(" promised tasks/day · ")+readable(offer.product_id)+QStringLiteral(". Revenue follows actual delivery; no setup revenue."),
            text(core::iso_date(offer.available_from)),text(core::iso_date(offer.available_before)));
    if (offer.customer_advance.value()>0)
        terms+=QObject::tr("\nThe customer pays %1 when you accept. The advance adds cash immediately. Earned service charges use it first. Any unused balance is due back when the contract ends or you cancel it.")
            .arg(money(offer.customer_advance));
    return terms;
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
QString ManagementController::publicListingText() const {
    if (!snapshot_) return {};
    const auto listed=core::public_listing_date(state());
    if (!listed) return {};
    return tr("Publicly listed on %1. Quarterly filings report the parent company's settled accounts.")
        .arg(text(core::iso_date(*listed)));
}
int ManagementController::publicFilingCount() const {
    if (!governanceAvailable()) return 0;
    return static_cast<int>(state().management->governance->public_filings.size());
}
QString ManagementController::publicFiling(int newestOffset) const {
    const auto count=publicFilingCount();
    if (newestOffset<0 || newestOffset>=count) return tr("The first filing will appear after the current calendar quarter closes.");
    const auto& filing=state().management->governance->public_filings[static_cast<std::size_t>(count-1-newestOffset)];
    return tr("Published %1 · results from %2 up to publication\nRevenue %3 · expenses %4 · profit %5\nQuarter-end cash %6")
        .arg(text(core::iso_date(filing.ends_before)),text(core::iso_date(filing.starts_on)),money(filing.revenue),
             money(filing.expenses),money(filing.profit()),money(filing.ending_cash));
}
QVariantMap ManagementController::listingPreview(QString offer) const {
    if (!governanceAvailable()) return {};
    const auto& rules=state().management->catalog.governance->public_listing;
    if (!rules || offer!=text(rules->offer_id)) return {};
    const auto row=corporate_finance_->row(offer);
    const auto qualification=core::listing_qualification(state(),state().date);
    QString detail;
    if (qualification.old_enough) {
        detail=tr("Actual settled revenue %1 · profit %2. %3")
            .arg(money(qualification.revenue),money(qualification.profit),
                 qualification.qualified?tr("Financial thresholds met."):tr("Financial thresholds still open."));
    } else detail=tr("The company reaches the minimum age on %1.").arg(text(core::iso_date(qualification.eligible_on)));
    const auto window=state().management->catalog.governance->vote_window;
    const bool time=core::advance_days(state().date,window)<state().end_date;
    if (!time) detail+=tr("\nThe campaign has no remaining board response window.");
    detail+=QStringLiteral("\n\n")+row.value(QStringLiteral("detail")).toString();
    return {{QStringLiteral("detail"),std::move(detail)},
            {QStringLiteral("ready"),qualification.qualified && time && row.value(QStringLiteral("actionable")).toBool()}};
}
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
            const auto cash_fee=core::research_cash_fee(s,row.id);
            research.push_back({text(row.id),title_for(row.id),money(cash_fee)+QStringLiteral(" · ")+QString::number(row.staff_days.value())+QStringLiteral(" staff-days"),
                QStringLiteral("%1 CU of finite experiments plus %2 staff-days. Both must complete.\nPrerequisites: %3\nPaid work and staff time are not refunded when stopped.")
                    .arg(cu(row.experiment_work.value()),QString::number(row.staff_days.value()),prerequisites.isEmpty()?QStringLiteral("none"):prerequisites.join(QStringLiteral(", "))),status,
                status!=QStringLiteral("Completed") && status!=QStringLiteral("In progress"),{{QStringLiteral("completed"),completed},{QStringLiteral("published"),published},{QStringLiteral("canAdopt"),can_adopt}}});
            if (cash_fee<row.cash) research.back().detail+=tr("\nGross fee %1. The grant pays %2 and the company pays %3 if this research starts today.")
                .arg(money(row.cash),money(core::checked_unit_subtract(row.cash,cash_fee)),money(cash_fee));
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
        auto summary=money(row.monthly_amount)+QStringLiteral("/month · ")+QString::number(row.term.value())+QStringLiteral(" days");
        const bool advance=row.customer_advance.value()>0;
        if (advance) summary+=tr(" · %1 advance").arg(money(row.customer_advance));
        offers.push_back({text(row.id),text(row.name),std::move(summary),offer_terms(row),
            it!=s.business_offers->accepted.end()?QStringLiteral("Accepted; ends ")+text(core::iso_date(it->expires_before)):eligible?QStringLiteral("Eligible"):QStringLiteral("Requirements unmet / outside offer window"),eligible,
            {{QStringLiteral("customerAdvance"),advance}}});
    }
    if (s.market) {
        for (const auto& row:s.market->products) {
            const auto* work=find(s.capacity->projects,row.capacity_project_id);
            const bool live=row.available && work && work->status==core::ProjectStatus::active;
            const bool retired=row.retirement && s.date>=row.retirement->retires_on;
            const core::DeploymentRecord* deployment=nullptr;
            if (s.deployments) for (const auto& d:s.deployments->records) if (d.service_id==row.id) deployment=&d;
            const auto subtitle=retired?tr("Serving resources released"):work?tr("%1 tasks/day · %2 support/month")
                .arg(number(work->tasks_per_day),money(work->monthly_support_cost)):QString{};
            QString detail=tr("Engineering service");
            if (deployment && work) {
                detail=tr("Model %1 · report %2\n%3 · %4 useful tasks delivered on this release")
                    .arg(text(deployment->model_id),text(deployment->evidence_id),
                         deployment->human_review?tr("Paid human review"):tr("Measured model only"),number(work->useful_tasks));
                if (!retired) detail+=tr("\nServing source %1 · %2 CU/day · %3 staff")
                    .arg(work->customer_inference?tr("Customer hardware"):text(work->source_id),cu(work->requested.value()),QString::number(work->staff));
            }
            products.push_back({text(row.id),readable(row.id),subtitle,std::move(detail),live?tr("Serving"):tr("Stopped"),false,
                {{QStringLiteral("projectId"),text(row.capacity_project_id)},{QStringLiteral("priceCents"),QString::number(row.price_per_task.value())},
                 {QStringLiteral("freeWeights"),row.channel==core::ProductChannel::open_weights}}});
            const auto* region=regions?find(regions->regions,row.region_id):nullptr;
            products.back().detail+=QStringLiteral("\n%1 · %2").arg(channel_name(row.channel),region?text(region->name):QStringLiteral("All eligible regions"));
            products.back().detail+=(retired?tr("\nLast quote was %1 per task."):
                tr("\nCurrent price is %1 per task. Price changes preserve past invoices and signed contract prices.")).arg(money(row.price_per_task));
            if (deployment) {
                add_release_details(products.back(),s,row);
                add_service_support(products.back(),s,row,live);
            }
        }
        for (const auto& row:s.market->contracts) {
            const bool active=row.status==core::ContractStatus::pending || row.status==core::ContractStatus::active;
            contracts.push_back({text(row.id),readable(row.product_id),number(row.tasks_per_day)+QStringLiteral(" tasks/day · ")+money(row.monthly_fee)+QStringLiteral("/month"),
                QStringLiteral("%1\nAccepted %2 · starts by %3 · ends %4 (exclusive)\nMissing task penalty %5 · cancellation %6\nService credits apply to undelivered promised work. Cancellation preserves consumed costs and settled invoices.")
                    .arg(text(row.cause),text(core::iso_date(row.accepted_on)),text(core::iso_date(row.start_by)),text(core::iso_date(row.ends_before)),money(row.missing_task_penalty),money(row.cancellation_penalty)),
                row.status==core::ContractStatus::active?QStringLiteral("Active"):row.status==core::ContractStatus::pending?QStringLiteral("Pending"):row.status==core::ContractStatus::completed?QStringLiteral("Completed"):row.status==core::ContractStatus::cancelled?QStringLiteral("Cancelled"):QStringLiteral("Missed"),active});
            if (row.customer_advance.value()>0) {
                auto& detail=contracts.back().detail;
                detail+=tr("\nReceived %1 in advance. %2 has paid for delivered work.")
                    .arg(money(row.customer_advance),money(row.advance_applied));
                if (const auto refund=core::customer_refund(row)) {
                    const auto index=s.accounts->invoices.find(refund->id);
                    const auto paid=index?s.accounts->invoices[*index].settled:core::Cents{0};
                    detail+=tr("\nRefund of %1 due %2. Paid %3, outstanding %4.")
                        .arg(money(refund->amount),text(core::iso_date(refund->due)),money(paid),
                             money(core::checked_unit_subtract(refund->amount,paid)));
                } else if (active && row.unearned_advance().value()>0) {
                    detail+=tr("\nUnused advance %1. Cancelling today makes this amount due back today.")
                        .arg(money(row.unearned_advance()));
                }
            }
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
        for (const auto& offer:m.catalog.support_offers) funding.push_back(funding_support_row(s,offer));
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
        const auto* current=find(state().market->products,row.product.id);
        const auto* work=current?find(state().capacity->projects,current->capacity_project_id):nullptr;
        item.insert(QStringLiteral("existing"),current!=nullptr);
        item.insert(QStringLiteral("live"),current && current->available && work && work->status==core::ProjectStatus::active);
        item.insert(QStringLiteral("trialPending"),core::pending_rollout(*state().deployments,row.product.id)!=nullptr);
        item.insert(QStringLiteral("retirementAnnounced"),current && current->retirement.has_value());
        item.insert(QStringLiteral("retired"),current && current->retirement && state().date>=current->retirement->retires_on);
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
    auto briefing=QStringLiteral("Committed recurring costs %5/month\nCustomer charges %1 · %2 useful tasks delivered\nUnpaid invoices %3 · debt %4\nCosts accrue as time advances; revenue follows actual delivery.")
        .arg(money(market.customer_charges),number(market.delivered_tasks),money(report.payables),money(report.debt),money(monthly));
    if (report.customer_advances.value()>0)
        briefing+=tr("\nUnused customer advances %1").arg(money(report.customer_advances));
    return briefing;
}
QVariantMap ManagementController::financialStatements() const {
    using namespace core;
    if (!snapshot_ || !state().accounts) return {};
    const auto& s=state();
    const auto report=accounts_report(*s.accounts);
    const auto other_assets=checked_unit_subtract(report.assets,report.accumulated_depreciation);
    const auto assets=checked_unit_add(checked_unit_add(report.cash,report.receivables),other_assets);
    QVariantMap result{
        {QStringLiteral("date"),text(iso_date(s.date))},
        {QStringLiteral("assets"),tr("Cash %1\nReceivables %2\nOther assets %3\nAccumulated depreciation %4\nTotal assets %5")
            .arg(money(report.cash),money(report.receivables),money(report.assets),money(report.accumulated_depreciation),money(assets))},
        {QStringLiteral("liabilities"),tr("Unpaid bills %1\nDebt principal %2\nUnused customer advances %3\nNet assets after liabilities %4")
            .arg(money(report.payables),money(report.debt),money(report.customer_advances),money(book_net_assets(report)))},
        {QStringLiteral("equity"),tr("Recorded equity %1\nCumulative profit or loss %2\nCash for new spending after reservations and operating reserve %3")
            .arg(money(report.equity),money(report.profit),money(discretionary_cash(s)))}
    };
    const auto period=recent_company_period(s);
    if (!period) return result;
    result.insert(QStringLiteral("period"),tr("%1 through %2")
        .arg(text(iso_date(period->first)),text(iso_date(period->last))));
    auto profit=tr("Revenue %1\nExpenses %2\nProfit or loss %3")
        .arg(money(period->revenue),money(period->expenses),money(checked_unit_subtract(period->revenue,period->expenses)));
    if (s.accounts->taxes) profit+=tr("\nCorporate tax included in expenses %1").arg(money(period->corporate_tax_expense));
    result.insert(QStringLiteral("profit"),profit);
    const auto flow=[&](const CashMovement& movement) {
        return tr("%1 in · %2 out · %3 net").arg(money(movement.received),money(movement.paid),money(movement.net()));
    };
    result.insert(QStringLiteral("cash"),tr("Starting cash %1\nOperations · %2\nInvesting · %3\nFinancing · %4\nEnding cash %5")
        .arg(money(period->opening_cash),flow(period->operations),flow(period->investing),flow(period->financing),money(period->closing_cash)));
    auto units=tr("No product tasks delivered in this period.");
    if (period->delivered_tasks>0) {
        const auto tasks=static_cast<std::int64_t>(period->delivered_tasks);
        units=tr("%1 product tasks delivered\nEarned product revenue %2 per task\nService support and penalties %3 per task\nContribution before shared costs %4 per task")
            .arg(number(period->delivered_tasks),money(Cents{period->product_revenue.value()/tasks}),
                 money(Cents{period->service_cost.value()/tasks}),
                 money(Cents{checked_unit_subtract(period->product_revenue,period->service_cost).value()/tasks}));
        units+=tr("\nShared costs include compute leases, site costs, payroll and research.");
    }
    const auto wholesale=runtime::wholesale_sales(snapshot_->checkpoint.world,period->first,period->last);
    if (wholesale.supplied.value()>0) {
        const auto per_cu=Cents{mul_div_floor(static_cast<std::uint64_t>(wholesale.cash.value()),1000,
            static_cast<std::uint64_t>(wholesale.supplied.value())).quotient};
        units+=tr("\nWholesale supplied %1 CU across the period. Buyers used %2 CU. Earned %3 per CU supplied.")
            .arg(cu(wholesale.supplied.value()),cu(wholesale.used.value()),money(per_cu));
    }
    result.insert(QStringLiteral("units"),units);
    return result;
}
bool ManagementController::corporateTaxAvailable() const {
    return snapshot_ && state().accounts && state().accounts->taxes.has_value();
}
QVariantMap ManagementController::corporateTax() const {
    using namespace core;
    if (!corporateTaxAvailable()) return {};
    const auto& accounts=*state().accounts;
    const auto& tax=*accounts.taxes;
    const auto& policy=tax.policy;
    auto terms=tr("Fictional corporate tax · %1\nEarned revenue enters the base. Ordinary booked costs, depreciation and interest are deductible. "
        "Subsidiary dividends are exempt. A bill comes due %2 days after quarter end.")
        .arg(percent(policy.rate)).arg(policy.payment_delay.value());
    if (policy.loss_carry_quarters>0)
        terms+=tr("\nLosses carry for up to %1 following quarters, up to %2 in total. They can offset up to %3 of a profitable quarter’s earnings.")
            .arg(policy.loss_carry_quarters).arg(money(policy.loss_carry_cap),percent(policy.loss_offset));
    if (const auto until=last_research_credit_day(tax))
        terms+=tr("\nThrough %1, the tax credit covers %2 of net Lab research fees, up to %3 each quarter. The credit can reduce the bill to zero.")
            .arg(text(iso_date(*until)),percent(policy.research_credit),money(policy.research_credit_cap));
    const auto current=open_tax_base(accounts);
    const auto balance=tr("Taxable income before deductions %1\nBooked deductions %2\nQualifying research fees %3\nUnused loss carry %4")
        .arg(money(current.income),money(current.deductions),money(current.research_fees),money(tax_loss_balance(tax)));
    QVariantMap result{{QStringLiteral("terms"),terms},{QStringLiteral("current"),balance}};
    if (tax.assessments.empty()) return result;
    const auto& last=tax.assessments.back();
    auto assessment=tr("%1 through %2\nIncome %3 · deductions %4\nLosses used %5 · added %6 · expired %7\nTaxable earnings %8")
        .arg(text(iso_date(last.first)),text(iso_date(last.through)),money(last.base.income),money(last.base.deductions),
            money(last.loss_used),money(last.loss_added),money(last.loss_expired),money(last.taxable));
    assessment+=tr("\nTax before credit %1 · research credit %2 · assessed tax %3")
        .arg(money(last.gross_tax),money(last.credit),money(last.bill));
    if (const auto row=accounts.invoices.find(tax_bill_id(last.through))) {
        const auto& invoice=accounts.invoices[*row];
        assessment+=tr("\nDue %1 · paid %2 · outstanding %3").arg(text(iso_date(invoice.due)),money(invoice.settled),
            money(checked_unit_subtract(invoice.amount,invoice.settled)));
    }
    result.insert(QStringLiteral("latest"),assessment);
    return result;
}
QVariantMap ManagementController::cashScenarios() const {
    using namespace core;
    if (!snapshot_ || !state().accounts) return {};
    const auto& s=state();
    if (s.phase==DayPhase::closed || s.phase==DayPhase::finished)
        return {{QStringLiteral("error"),tr("Cash scenarios are available while the company can still operate.")}};
    try {
        const auto wholesale=runtime::forecast_wholesale_cash(snapshot_->checkpoint.world);
        const auto forecast=forecast_company_finances(s,wholesale);
        const auto point=[&](const CashProjection& value) {
            auto line=tr("Through %1\nCash %2 · unreserved %3\nOverdue bills %4 · due principal unpaid %5")
                .arg(text(iso_date(value.through)),money(value.cash),money(value.available),
                     money(value.overdue_payables),money(value.due_principal));
            if (s.accounts->taxes) line+=tr("\nProjected corporate tax paid %1").arg(money(value.corporate_tax_paid));
            return line;
        };
        const std::array<QString,3> labels{tr("Weak demand"),tr("Recent demand"),tr("Strong demand")};
        QVariantList cases;
        for (std::size_t i=0;i<forecast.cases.size();++i) {
            const auto& row=forecast.cases[i];
            cases.push_back(QVariantMap{
                {QStringLiteral("title"),tr("%1 · %2% of recent discretionary tasks").arg(labels[i]).arg(row.demand_percent)},
                {QStringLiteral("sales"),tr("%1 discretionary tasks/day · %2 per day at current prices")
                    .arg(number(row.tasks_per_day),money(row.discretionary_sales_per_day))},
                {QStringLiteral("near"),point(row.cash.near_term)},
                {QStringLiteral("year"),point(row.cash.year)},
                {QStringLiteral("risk"),row.cash.first_shortfall
                    ?tr("First projected unpaid obligation on %1.").arg(text(iso_date(*row.cash.first_shortfall)))
                    :tr("Projected cash covers obligations through both dates.")}
            });
        }
        const auto basis=forecast.sample_days==0?tr("Discretionary sales enter the scenarios after the first settled market day."):
            tr("Discretionary sales use %1 settled market days and fit today’s serving capacity.").arg(forecast.sample_days);
        auto assumptions=basis+tr("\nSigned contracts use recent delivery capped at today’s capacity. New contracts use today’s capacity. "
            "Earned charges first use any advance, and an unused balance is refunded at term end. Wholesale uses recent receipts through signed terms.\n"
            "Existing bills, gross lease terms and the owned-site cost ceiling apply. %1 in reservations stays held. Due principal uses remaining cash after bills. "
            "New purchases, renewals, and unexpected changes are outside these scenarios.").arg(money(forecast.reserved_cash));
        if (s.accounts->taxes) assumptions+=tr("\nCorporate tax uses projected earned income and the current quarterly policy.");
        if (s.market && std::ranges::any_of(s.market->products,[&](const auto& product) {
            return product.retirement && product.retirement->retires_on>s.date;
        })) assumptions+=tr("\nScheduled service retirements end projected discretionary sales and serving support on their published dates.");
        return {{QStringLiteral("date"),text(iso_date(s.date))},{QStringLiteral("assumptions"),assumptions},
                {QStringLiteral("cases"),cases}};
    } catch (const std::exception& error) {
        return {{QStringLiteral("error"),tr("Cash scenarios are unavailable for this snapshot. %1").arg(QString::fromUtf8(error.what()))}};
    }
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
QVariantMap ManagementController::rolloutPreview(QString service,QString model,QString evidence,QString source,
    bool review,QString share,QString days) const {
    if (!available()) return {};
    try {
        const auto request=rollout_request(service,model,evidence,source,review,share,days);
        const auto work=core::preview_rollout(state(),request);
        const auto& society=snapshot_->checkpoint.world.society;
        const auto blocked=runtime::society_launch_block(society?&*society:nullptr,state(),request.candidate,state().date);
        if (!blocked.empty()) return {{QStringLiteral("detail"),text(blocked)}};
        const auto resources=work.customer_inference?tr("Buyer hardware"):tr("%1 CU/day on %2").arg(cu(work.requested.value()),source);
        const auto detail=tr("Try %1 with report %2 for %3 days. The candidate serves up to %4 of eligible discretionary orders and at most %5 tasks per day. The production release serves all signed contracts.\n\n"
            "%6 · %7 staff · %8 monthly support rate while active. Starting the trial rechecks available resources. Decide whether to promote or discard it in Office → Services.")
            .arg(model,evidence,days,percent(request.share),number(work.tasks_per_day),resources,QString::number(work.staff),money(work.monthly_support_cost));
        return {{QStringLiteral("ready"),true},{QStringLiteral("detail"),detail}};
    } catch (const std::exception& error) {
        return {{QStringLiteral("detail"),QString::fromUtf8(error.what())}};
    }
}
QVariantMap ManagementController::compatibilityQuote(QString service,QString days) const {
    if (!available()) return {};
    try {
        const auto term=core::preview_compatibility(state(),{service.toStdString(),service_term(days)});
        const auto detail=tr("Publish a commitment for %1 through the day before %2. Keep the service operating and apply these terms to each replacement. Established customers favor supported integrations while the commitment is active.\n\n%3")
            .arg(readable(term.product_id),text(core::iso_date(term.ends_before)),compatibility_terms(core::compatibility_profile(state(),term)));
        return {{QStringLiteral("ready"),true},{QStringLiteral("detail"),detail}};
    } catch (const std::exception& error) { return {{QStringLiteral("detail"),QString::fromUtf8(error.what())}}; }
}
QVariantMap ManagementController::retirementQuote(QString service,QString days) const {
    if (!available()) return {};
    try {
        const auto notice=core::preview_retirement(state(),{service.toStdString(),service_term(days)});
        const auto detail=tr("Publish an end date of %1 for %2. Discretionary adoption is limited to the latest installed share from today. Signed work stays protected, and new deals must end by this date.\n\nOn that morning the service stops, releasing its staff, compute and support costs. The retirement date is binding, and the release and customer history stay available.")
            .arg(text(core::iso_date(notice.retires_on)),readable(service.toStdString()));
        return {{QStringLiteral("ready"),true},{QStringLiteral("detail"),detail}};
    } catch (const std::exception& error) { return {{QStringLiteral("detail"),QString::fromUtf8(error.what())}}; }
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
void ManagementController::stageRollout(QString service,QString model,QString evidence,QString source,bool review,QString share,QString days) {
    if (!editable()) return;
    try { emit actionRequested(rollout_request(service,model,evidence,source,review,share,days)); }
    catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::finishRollout(QString service,bool promote) {
    if (editable()) emit actionRequested(core::FinishServiceRollout{service.toStdString(),promote});
}
void ManagementController::rollbackService(QString service) {
    if (editable()) emit actionRequested(core::RollbackService{service.toStdString()});
}
void ManagementController::commitCompatibility(QString service,QString days) {
    if (!editable()) return;
    try { emit actionRequested(core::CommitServiceCompatibility{service.toStdString(),service_term(days)}); }
    catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
}
void ManagementController::announceRetirement(QString service,QString days) {
    if (!editable()) return;
    try { emit actionRequested(core::AnnounceServiceRetirement{service.toStdString(),service_term(days)}); }
    catch (const std::exception& error) {emit noticeRaised(QString::fromUtf8(error.what()));}
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
