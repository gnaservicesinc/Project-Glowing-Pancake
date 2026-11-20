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
    if (terms.feedback) {
        output.source_path="user_feedback";
        output.collection_method="Consented service feedback";
    }
    if (output.supplier_or_publisher.empty()) output.supplier_or_publisher=output.name;
    if (output.underlying_history.empty()) output.underlying_history=output.provenance;
    if(terms.feedback)
        output.unique_units=std::min(output.unique_units,
            mul_div_floor(tasks,static_cast<std::uint64_t>(terms.feedback->opted_in_share.value()),10000).quotient);
    if(output.unique_units==0) return std::nullopt;
    output.price=Cents{0};
    return output;
}
std::string project_id(std::size_t index) { return "lab/collect/"+std::to_string(index+1); }
} // namespace

DataAcquisitionOffer collection_terms(const LabState& lab, std::string_view id,
                                     const std::vector<std::string>& sources, std::string_view name) {
    if (sources.empty()) {
        const auto& offer = terms_for(lab.catalog, id);
        if (offer.source) invalid("Select the source in the dataset builder.");
        return offer;
    }
    if (sources.size() > 8 || !id.starts_with("collected.mix.") || name.empty() || name.size() > 80 ||
        std::ranges::any_of(name, [](char c) { return static_cast<unsigned char>(c) < 32; }))
        invalid("Give the dataset a name and select up to eight distinct sources.");
    DataAcquisitionOffer result;
    auto& output = result.dataset;
    output.id = id;
    output.name = name;
    output.open_weight_release = true;
    output.source_path = "company_created";
    output.provenance = "Assembled by " + lab.owner + " from the recorded collection sources.";
    output.collection_method = "Collection and review";
    output.supplier_or_publisher = lab.owner;
    result.staff_days = Days{0};
    std::set<std::string_view> seen;
    std::uint64_t quality = 0, contamination = 0;
    std::array<std::uint64_t, lab_domain_count> relevance{}, coverage{};
    bool owns = true;
    for (const auto& key : sources) {
        const auto& terms = terms_for(lab.catalog, key);
        if (!terms.source || !seen.insert(key).second) invalid("Choose distinct data sources.");
        const auto& data = terms.dataset;
        if (!output.underlying_history.empty()) output.underlying_history += ", ";
        output.underlying_history += data.name;
        const auto units = data.unique_units;
        // Draw once per source and dataset identity, reproducible during quote, settlement and reload.
        SplitMix64 random{lab.research_stream_origin};
        for (const auto token : {id, std::string_view{key}}) for (const char c : token)
            random = SplitMix64{random.next() ^ static_cast<unsigned char>(c)};
        const auto low = static_cast<std::uint64_t>(terms.source->minimum_quality.value());
        quality += units * (low + random.bounded(static_cast<std::uint64_t>(data.quality.value()) - low + 1));
        contamination += units * static_cast<std::uint64_t>(data.contamination.value());
        for (std::size_t axis = 0; axis < coverage.size(); ++axis) {
            coverage[axis] += units * static_cast<std::uint64_t>(data.coverage[axis].value());
            relevance[axis] += units * static_cast<std::uint64_t>(data.relevance[axis].value());
        }
        output.unique_units += units;
        output.price = checked_unit_add(output.price, data.price);
        output.commercial_use &= data.commercial_use;
        output.open_weight_release &= data.open_weight_release;
        output.third_party_controversy_risk |= data.third_party_controversy_risk;
        output.provenance_confidence_bp = std::min(output.provenance_confidence_bp, data.provenance_confidence_bp);
        owns &= terms.source->transfers_ip;
        result.collection_days = std::max(result.collection_days, terms.collection_days);
        result.staff_days = checked_unit_add(result.staff_days, terms.staff_days);
        result.review_work = checked_unit_add(result.review_work, terms.review_work);
    }
    output.quality = BasisPoints{quality / output.unique_units};
    output.contamination = BasisPoints{contamination / output.unique_units};
    for (std::size_t axis = 0; axis < coverage.size(); ++axis) {
        output.coverage[axis] = BasisPoints{coverage[axis] / output.unique_units};
        output.relevance[axis] = BasisPoints{relevance[axis] / output.unique_units};
    }
    output.rights = owns ? "Company owns all source IP and may license this dataset."
                         : "Source permissions apply. The company cannot license the underlying dataset.";
    return result;
}

bool owns_dataset_ip(const LabState& lab, std::string_view id) {
    // Cleaning an owned corpus preserves IP. Following only completed parents also
    // keeps a buyer from transferring a purchased license by processing its copy.
    for (std::size_t depth=0; depth<=lab.processed.size(); ++depth) {
        const auto derived = std::ranges::find(lab.processed,id,&ProcessedDataset::id);
        if (derived == lab.processed.end()) break;
        if (!derived->output || depth == lab.processed.size()) return false;
        id = derived->source_dataset_id;
    }
    if (std::ranges::any_of(lab.creative_jobs, [&](const auto& job) { return job.id == id && job.output; })) return true;
    const auto job = std::ranges::find(lab.acquisitions, id, &DataAcquisition::offer_id);
    return job != lab.acquisitions.end() && job->output && !job->sources.empty() &&
        std::ranges::all_of(job->sources, [&](const auto& key) { const auto& source = terms_for(lab.catalog, key).source; return source && source->transfers_ip; });
}

void validate_acquisition_catalog(const LabCatalog& catalog) {
    if(catalog.acquisition.size()>acquisition_limit) invalid("Data collection catalog exceeds its offer bound");
    std::set<std::string_view> ids;
    std::set<std::pair<std::string_view,std::string_view>> groups;
    for(const auto& offer:catalog.acquisition) {
        const auto& data=offer.dataset;
        if(!data.id.starts_with("collected.") || data.id.size()>64 || data.name.empty() || data.name.size()>128 ||
            data.provenance.empty() || data.provenance.size()>256 || data.rights.empty() || data.rights.size()>256 ||
            !ids.insert(data.id).second || std::ranges::find(catalog.data,data.id,&DataOffer::id)!=catalog.data.end() ||
            data.unique_units==0 || data.unique_units>1'000'000'000 || (offer.source ? data.price.value()<0 : data.price.value()<=0) ||
            data.price.value()>1'000'000'000 || data.quality.value()==0 ||
            offer.collection_days.value()<1 || offer.collection_days.value()>90 ||
            offer.staff_days.value()<1 || offer.staff_days.value()>365 || offer.review_work.value()<=0 ||
            offer.review_work.value()>1'000'000'000)
            invalid("Data collection needs distinct consent terms, paid work and a bounded corpus");
        if (offer.source && (offer.feedback || (data.open_weight_release && !data.commercial_use) || offer.source->minimum_quality.value() == 0 ||
            offer.source->minimum_quality > data.quality || data.id.starts_with("collected.mix.")))
            invalid("Collection sources need a positive quality range and independent permissions.");
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
    if (!request.sources.empty() && request.offer_id != "collected.mix." + std::to_string(lab.acquisitions.size()+1))
        invalid("Refresh the dataset quote before commissioning the next collection.");
    const auto terms=collection_terms(lab,request.offer_id,request.sources,request.name);
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
    if (quote.fee.value()) record_cash(*state.accounts,state.date,Cents{-quote.fee.value()},Account::expenses,work.cause,work.id);
    state.cash=accounts_report(*state.accounts).cash;
    lab.acquisitions.push_back({request.offer_id,work.id,{},{},request.sources,request.name});
}

void close_data_acquisitions(SimulationState& state) {
    for(auto& job:state.lab->acquisitions) {
        const auto& work=project_for(*state.capacity,job.project_id);
        if(work.status!=ProjectStatus::completed || work.stopped_on!=state.date) continue;
        const auto terms=collection_terms(*state.lab,job.offer_id,job.sources,job.name);
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
        if (!job.sources.empty() && job.offer_id != "collected.mix." + std::to_string(i+1)) invalid("Unexpected dataset identity.");
        const auto terms=collection_terms(lab,job.offer_id,job.sources,job.name);
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
        if (!quote.fee.value()) {
            if (posting != state.accounts->postings.end()) invalid("A free source charged a license fee.");
            continue;
        }
        if(posting==state.accounts->postings.end() || posting->day!=work.starts_on ||
            posting->amount!=quote.fee || posting->debit!=Account::expenses || posting->credit!=Account::cash)
            invalid("Collected data is missing its actual paid fee");
    }
    for(const auto& work:state.capacity->projects)
        if(work.kind==ProjectKind::data_acquisition && !projects.contains(work.id))
            invalid("A collection project needs its owning Lab record");
}
} // namespace pgp::core
