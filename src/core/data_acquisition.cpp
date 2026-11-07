#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::size_t acquisition_limit=lab_catalog_limit;

[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }

const DataAcquisitionOffer& terms_for(const LabCatalog& catalog,std::string_view id) {
    const auto row=std::ranges::find(catalog.acquisition,id,[](const auto& offer){return offer.dataset.id;});
    if(row==catalog.acquisition.end()) invalid("Choose an offered data collection agreement");
    return *row;
}
const CapacityProject& project_for(const CapacityState& capacity,std::string_view id) {
    const auto row=std::ranges::find(capacity.projects,id,&CapacityProject::id);
    if(row==capacity.projects.end()) invalid("Data collection has no owning capacity project");
    return *row;
}
void require_feedback_source(const SimulationState& state,const FeedbackConsent& consent,OrdinalDay day) {
    if(!state.market) invalid("Product feedback requires an operating customer market");
    const auto& market=*state.market;
    const auto product=std::ranges::find(market.products,consent.product_id,&MarketProduct::id);
    const auto segment=std::ranges::find(market.segments,consent.segment_id,&MarketSegment::id);
    const auto* binding=market_binding(market,consent.product_id,day);
    if(product==market.products.end() || segment==market.segments.end() || !binding ||
        product->channel==ProductChannel::open_weights || !market_access(*product,*segment))
        invalid("Launch the named service for the consenting customer group before collecting feedback");
    const auto& work=project_for(*state.capacity,binding->project_id);
    if(work.starts_on>day || (work.stopped_on && *work.stopped_on<day))
        invalid("The consent window must start with an operating service");
}
DataAcquisitionPreview quote_terms(const DataAcquisitionOffer& terms,OrdinalDay day,const LabAllocation& allocation) {
    const auto rate=allocation.daily_compute;
    if(rate.value()<=0 || rate.value()>1'000'000'000 || allocation.staff==0 || allocation.staff>1'000'000)
        invalid("Choose positive bounded compute and staff for collection");
    const auto staff_time=terms.staff_days.value()/allocation.staff+
        (terms.staff_days.value()%allocation.staff!=0?1:0);
    const auto minimum=Days{std::max(terms.collection_days.value(),staff_time)};
    const auto work=std::max(terms.review_work,integrate(rate,minimum));
    return {terms.dataset.price,work,Days{work.value()/rate.value()+(work.value()%rate.value()!=0?1:0)},
        advance_days(day,terms.collection_days)};
}
bool precludes_retry(const DataAcquisition& job,const CapacityProject& project,OrdinalDay day) {
    return job.output.has_value() || !project.stopped_on || *project.stopped_on>day ||
        (project.status==ProjectStatus::completed && *project.stopped_on==day);
}
std::uint64_t settled_feedback(const MarketState& market,const FeedbackConsent& consent,
                               OrdinalDay starts,OrdinalDay ends) {
    std::set<std::string_view> contracts;
    for(const auto& row:market.contracts)
        if(row.product_id==consent.product_id && row.segment_id==consent.segment_id) contracts.insert(row.id);
    std::uint64_t tasks=0;
    for(auto day=starts;day<ends;day=advance_days(day,Days{1})) {
        const auto index=day.value()-market.starts_on.value();
        if(index<0 || static_cast<std::size_t>(index)>=market.days.size())
            invalid("Product feedback can use only settled work inside the accepted consent window");
        const auto& closed=market.days[static_cast<std::size_t>(index)];
        for(const auto& row:closed.contracts) if(contracts.contains(row.contract_id))
            tasks+=row.delivered_tasks;
        for(const auto& row:closed.discretionary)
            if(row.product_id==consent.product_id && row.segment_id==consent.segment_id && row.local_copy_project.empty())
                tasks+=row.tasks;
    }
    return tasks;
}
std::optional<DataOffer> deliver(const DataAcquisitionOffer& terms,std::uint64_t tasks) {
    auto output=terms.dataset;
    if(terms.feedback)
        output.unique_units=std::min(output.unique_units,
            mul_div_floor(tasks,static_cast<std::uint64_t>(terms.feedback->opted_in_share.value()),10000).quotient);
    if(output.unique_units==0) return std::nullopt;
    output.price=Cents{0};
    return output;
}
std::string project_id(std::size_t index) { return "lab/collect/"+std::to_string(index+1); }
} // namespace

void validate_acquisition_catalog(const LabCatalog& catalog) {
    if(catalog.acquisition.size()>acquisition_limit) invalid("Data collection catalog exceeds its offer bound");
    std::set<std::string_view> ids;
    std::set<std::pair<std::string_view,std::string_view>> groups;
    for(const auto& offer:catalog.acquisition) {
        const auto& data=offer.dataset;
        if(!data.id.starts_with("collected.") || data.id.size()>64 || data.name.empty() || data.name.size()>128 ||
            data.provenance.empty() || data.provenance.size()>256 || data.rights.empty() || data.rights.size()>256 ||
            !ids.insert(data.id).second || std::ranges::find(catalog.data,data.id,&DataOffer::id)!=catalog.data.end() ||
            data.unique_units==0 || data.unique_units>1'000'000'000 || data.price.value()<=0 ||
            data.price.value()>1'000'000'000 || data.quality.value()==0 ||
            offer.collection_days.value()<1 || offer.collection_days.value()>90 ||
            offer.staff_days.value()<1 || offer.staff_days.value()>365 || offer.review_work.value()<=0 ||
            offer.review_work.value()>1'000'000'000)
            invalid("Data collection needs distinct consent terms, paid work and a bounded corpus");
        bool useful=false;
        for(std::size_t i=0;i<lab_domain_count;++i) useful|=data.relevance[i].value()>0 && data.coverage[i].value()>0;
        if(!useful) invalid("A collected dataset needs measured coverage in a relevant domain");
        if(offer.feedback) {
            const auto& consent=*offer.feedback;
            if(consent.product_id.empty() || consent.product_id.size()>64 || consent.segment_id.empty() ||
                consent.segment_id.size()>64 || consent.opted_in_share.value()==0 || data.open_weight_release ||
                !groups.emplace(consent.product_id,consent.segment_id).second)
                invalid("Feedback requires one explicit opt-in agreement per service and buyer group, with no weight redistribution");
        }
    }
}

DataAcquisitionPreview preview_data_acquisition(const SimulationState& state,const StartDataAcquisition& request) {
    if(!state.lab || !state.capacity || !state.accounts || state.lab->acquisitions.size()>=acquisition_limit)
        invalid("No data collection slot is available");
    const auto& lab=*state.lab;
    const auto& terms=terms_for(lab.catalog,request.offer_id);
    for(const auto& job:lab.acquisitions)
        if(job.offer_id==request.offer_id && precludes_retry(job,project_for(*state.capacity,job.project_id),state.date))
            invalid("This agreement already has a collection job or a delivered corpus");
    if(terms.feedback) {
        require_feedback_source(state,*terms.feedback,state.date);
        const auto product=std::ranges::find(state.market->products,terms.feedback->product_id,&MarketProduct::id);
        const auto& work=project_for(*state.capacity,product->capacity_project_id);
        if(!product->available || work.status!=ProjectStatus::active || project_disrupted(work,state.date))
            invalid("The opted-in service needs to be operating when collection begins");
    }
    const auto result=quote_terms(terms,state.date,request.allocation);
    if(advance_days(state.date,result.minimum_days)>state.end_date)
        invalid("The collection cannot finish before the campaign ends at this allocation");
    return result;
}

void start_data_acquisition(SimulationState& state,const StartDataAcquisition& request,Cents protected_cash) {
    const auto quote=preview_data_acquisition(state,request);
    if(quote.fee>checked_unit_subtract(state.cash,protected_cash))
        invalid("Collection would spend reserved or unavailable cash");
    auto& lab=*state.lab;
    CapacityProject work;
    work.id=project_id(lab.acquisitions.size());
    work.kind=ProjectKind::data_acquisition;
    work.source_id=request.allocation.source_id;
    work.starts_on=state.date;
    work.requested=request.allocation.daily_compute;
    work.staff=request.allocation.staff;
    work.total_work=quote.work;
    work.cause="Collect and review "+request.offer_id;
    reserve_project(*state.capacity,state.date,work);
    record_cash(*state.accounts,state.date,Cents{-quote.fee.value()},Account::expenses,work.cause,work.id);
    state.cash=accounts_report(*state.accounts).cash;
    lab.acquisitions.push_back({request.offer_id,work.id});
}

void close_data_acquisitions(SimulationState& state) {
    for(auto& job:state.lab->acquisitions) {
        const auto& work=project_for(*state.capacity,job.project_id);
        if(work.status!=ProjectStatus::completed || work.stopped_on!=state.date) continue;
        const auto& terms=terms_for(state.lab->catalog,job.offer_id);
        if(terms.feedback) job.settled_deliveries=settled_feedback(*state.market,*terms.feedback,work.starts_on,
            advance_days(work.starts_on,terms.collection_days));
        job.output=deliver(terms,job.settled_deliveries.value_or(0));
    }
}

void validate_data_acquisitions(const SimulationState& state,bool deep) {
    if(!state.lab || state.lab->acquisitions.empty()) {
        if(deep && state.capacity && std::ranges::any_of(state.capacity->projects,[](const auto& row) {
            return row.kind==ProjectKind::data_acquisition;
        })) invalid("A collection project needs its owning Lab record");
        return;
    }
    const auto& lab=*state.lab;
    if(lab.acquisitions.size()>acquisition_limit) invalid("Data collection history exceeds its bound");
    if(!deep) return;
    std::set<std::string_view> projects;
    std::optional<OrdinalDay> prior;
    for(std::size_t i=0;i<lab.acquisitions.size();++i) {
        const auto& job=lab.acquisitions[i];
        const auto& terms=terms_for(lab.catalog,job.offer_id);
        const auto& work=project_for(*state.capacity,job.project_id);
        const auto quote=quote_terms(terms,work.starts_on,{work.source_id,work.requested,work.staff});
        if(work.kind!=ProjectKind::data_acquisition || work.id!=project_id(i) ||
            work.starts_on<lab.starts_on || work.starts_on>state.date || (prior && work.starts_on<*prior) ||
            work.total_work!=quote.work || work.minimum_memory_mib!=0 || work.interconnect!="general" ||
            work.cause!="Collect and review "+job.offer_id)
            invalid("Collection work differs from its accepted terms");
        projects.insert(work.id);
        prior=work.starts_on;
        for(std::size_t j=0;j<i;++j) {
            const auto& earlier=lab.acquisitions[j];
            if(earlier.offer_id==job.offer_id && precludes_retry(earlier,project_for(*state.capacity,earlier.project_id),work.starts_on))
                invalid("A dataset agreement cannot produce overlapping or duplicate work");
        }
        if(terms.feedback) require_feedback_source(state,*terms.feedback,work.starts_on);
        std::optional<std::uint64_t> tasks;
        std::optional<DataOffer> output;
        if(work.status==ProjectStatus::completed) {
            if(!work.stopped_on || *work.stopped_on<advance_days(work.starts_on,Days{quote.minimum_days.value()-1}))
                invalid("Collection finished before its actual minimum work and consent window");
            if(terms.feedback) tasks=settled_feedback(*state.market,*terms.feedback,work.starts_on,quote.collection_ends_before);
            output=deliver(terms,tasks.value_or(0));
        }
        if(job.output!=output || job.settled_deliveries!=tasks)
            invalid("Collected data differs from its completed work, consent or actual settled deliveries");
        const auto posting=std::ranges::find(state.accounts->postings,work.id,&Posting::key);
        if(posting==state.accounts->postings.end() || posting->day!=work.starts_on ||
            posting->amount!=quote.fee || posting->debit!=Account::expenses || posting->credit!=Account::cash)
            invalid("Collected data is missing its actual paid fee");
    }
    for(const auto& work:state.capacity->projects)
        if(work.kind==ProjectKind::data_acquisition && !projects.contains(work.id))
            invalid("A collection project needs its owning Lab record");
}
} // namespace pgp::core
