#include <pgp/core/lab.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& named(const Rows& rows,std::string_view id) {
    const auto row=std::ranges::find_if(rows,[&](const auto& row) { return row.id==id; });
    if (row==rows.end()) invalid("Synthetic data refers to an unavailable recipe, checkpoint, report or source");
    return *row;
}
OrdinalDay current_day(const LabState& lab) {
    return lab.closed_through?advance_days(*lab.closed_through,Days{1}):lab.starts_on;
}
void require_identity(std::string_view id) {
    if (!id.starts_with("synthetic.") || id.size()>64 ||
        std::ranges::any_of(id,[](unsigned char ch) {
            return !(ch>='a' && ch<='z') && !(ch>='A' && ch<='Z') && !(ch>='0' && ch<='9') && ch!='.' && ch!='_' && ch!='-';
        })) invalid("Choose a distinct synthetic dataset identity with at most 64 simple characters");
}
bool uses_data(const LabState& lab,const LabModel& teacher,std::string_view anchor) {
    std::vector<std::string_view> pending(teacher.data_ids.begin(),teacher.data_ids.end());
    std::set<std::string_view> visited;
    while (!pending.empty()) {
        const auto id=pending.back();
        pending.pop_back();
        if (id==anchor) return true;
        if (!visited.insert(id).second) continue;
        if (visited.size()>lab_catalog_limit*lab_catalog_limit) invalid("Synthetic source lineage exceeds its bound");
        const auto generated=std::ranges::find(lab.generated,id,&GeneratedDataset::id);
        if (generated==lab.generated.end()) continue;
        pending.push_back(generated->verification_id);
        const auto& prior_teacher=named(lab.models,generated->teacher_id);
        for (const auto& source:prior_teacher.data_ids) pending.push_back(source);
    }
    return false;
}
bool reserved_on(const CapacityProject& work,OrdinalDay day) {
    return work.status!=ProjectStatus::cancelled || !work.stopped_on || *work.stopped_on>day;
}
struct SynthesisPlan {
    SyntheticPreview preview;
    const SyntheticRecipe* recipe;
    const LabModel* teacher;
    const DataOffer* verifier;
    const EvaluationPlan* evaluation;
};
std::uint64_t retained(std::uint64_t attempted,BasisPoints teacher,const DataOffer& verifier,BasisPoints diversity) {
    const std::uint64_t factors[]{attempted,static_cast<std::uint64_t>(std::min(teacher,verifier.quality).value()),
        static_cast<std::uint64_t>(diversity.value())};
    return mul_div_floor(factors,100'000'000).quotient;
}
SynthesisPlan plan(const LabState& lab,const CapacityState& capacity,OrdinalDay day,const GenerateDataset& request,
                   std::span<const GeneratedDataset> prior) {
    const auto& recipe=named(lab.catalog.synthesis,request.recipe_id);
    const auto& teacher=named(lab.models,request.teacher_id);
    const auto& evidence=named(lab.evaluations,request.evidence_id);
    const auto& evaluation=named(lab.catalog.evaluations,evidence.plan_id);
    const auto& verifier=named(lab.catalog.data,request.verification_id);
    if (teacher.created_on>day || teacher.consumed_work.value()==0 || request.attempted_units==0 ||
        request.attempted_units>recipe.maximum_units || request.allocation.daily_compute.value()==0 || request.allocation.staff==0)
        invalid("Choose a trained immutable teacher and positive generation resources within the recipe limit");
    if (!teacher.project_id.empty()) {
        const auto& work=named(capacity.projects,teacher.project_id);
        if (!work.stopped_on || *work.stopped_on>day ||
            (work.status==ProjectStatus::completed && *work.stopped_on==day))
            invalid("Save an immutable teacher checkpoint or finish its training before generation");
    }
    const auto& test=named(capacity.projects,evidence.project_id);
    if (!evidence.completed || evidence.model_id!=teacher.id || evidence.model_work!=teacher.consumed_work ||
        !evaluation.independent || !test.stopped_on || *test.stopped_on>=day ||
        evaluation.coverage[recipe.domain].value()==0 || evidence.lower[recipe.domain]<recipe.minimum_teacher)
        invalid("Choose an independent report of this frozen teacher covering the generation domain at the required lower bound");
    for (const auto& id:recipe.requires_research) {
        const auto research=std::ranges::find_if(lab.research,[&](const auto& row) {
            if (row.research_id!=id || !row.completed) return false;
            const auto& work=named(capacity.projects,row.project_id);
            return work.stopped_on && *work.stopped_on<day;
        });
        if (research==lab.research.end())
            invalid("Complete synthetic generation research before starting this job");
    }
    const auto license=std::ranges::find(lab.licenses,verifier.id,&DataLicense::offer_id);
    if (license==lab.licenses.end() || license->acquired_on>day ||
        verifier.coverage[recipe.domain].value()==0 || verifier.relevance[recipe.domain].value()==0 || verifier.quality.value()==0)
        invalid("License an external verification source covering the generation domain");
    if (uses_data(lab,teacher,verifier.id))
        invalid("The verification source must be independent of the teacher's full training lineage");

    std::uint64_t committed=0;
    for (const auto& job:prior) {
        const auto& other_recipe=named(lab.catalog.synthesis,job.recipe_id);
        if (job.verification_id!=verifier.id || other_recipe.domain!=recipe.domain ||
            !reserved_on(named(capacity.projects,job.project_id),day)) continue;
        const auto& other=named(lab.models,job.teacher_id);
        if (other.recipe_id==teacher.recipe_id && other.data_ids==teacher.data_ids && other.consumed_work==teacher.consumed_work)
            invalid("This teacher and verification source already have a generation job. Clones keep the same diversity");
        committed+=job.attempted_units;
    }
    const auto limit=mul_div_floor(verifier.unique_units,static_cast<std::uint64_t>(recipe.diversity.value()),10000).quotient;
    if (committed>limit || request.attempted_units>limit-committed)
        invalid("Generation would exceed the verification source's remaining diversity budget");
    SyntheticPreview quote;
    quote.diversity_remaining=limit-committed;
    quote.fee=recipe.fee;
    const auto& teacher_recipe=named(lab.catalog.recipes,teacher.recipe_id);
    const auto per_unit=checked_unit_add(teacher_recipe.inference_per_task,recipe.verification_per_unit);
    const auto inference=required_inference(request.attempted_units,per_unit);
    const auto minimum_days=Days{recipe.staff_days.value()/request.allocation.staff+
        (recipe.staff_days.value()%request.allocation.staff!=0?1:0)};
    quote.work=std::max(inference,integrate(request.allocation.daily_compute,minimum_days));
    if (quote.work.value()>1'000'000'000'000LL) invalid("Synthetic generation exceeds the finite work bound");
    quote.minimum_days=Days{quote.work.value()/request.allocation.daily_compute.value()+
        (quote.work.value()%request.allocation.daily_compute.value()!=0?1:0)};
    quote.retained_lower=retained(request.attempted_units,evidence.lower[recipe.domain],verifier,recipe.diversity);
    quote.retained_upper=retained(request.attempted_units,evidence.upper[recipe.domain],verifier,recipe.diversity);
    if (quote.retained_lower==0) invalid("Choose enough attempts to retain at least one verified example");
    quote.commercial_use=verifier.commercial_use;
    quote.open_weight_release=verifier.open_weight_release;
    for (const auto& id:teacher.data_ids) {
        const auto& source=lab_data(lab,id);
        quote.commercial_use=quote.commercial_use && source.commercial_use;
        quote.open_weight_release=quote.open_weight_release && source.open_weight_release;
    }
    return {quote,&recipe,&teacher,&verifier,&evaluation};
}
DataOffer verified_output(const LabState& lab,const GeneratedDataset& job,const SynthesisPlan& prepared) {
    const auto axis=prepared.recipe->domain;
    DataOffer output;
    output.id=job.id;
    output.name="Verified "+std::string(lab_domain_labels[axis])+" examples";
    output.provenance="Generated and filtered in the company Lab using the recorded teacher and external verification source.";
    output.rights=prepared.preview.open_weight_release?"Commercial training and weight redistribution with source attribution":
        prepared.preview.commercial_use?"Commercial training and deployment; weight redistribution unavailable":"Research use only";
    output.unique_units=retained(job.attempted_units,prepared.teacher->capability[axis],*prepared.verifier,prepared.recipe->diversity);
    output.quality=prepared.verifier->quality;
    output.coverage[axis]=std::min(prepared.evaluation->coverage[axis],prepared.verifier->coverage[axis]);
    output.relevance[axis]=prepared.verifier->relevance[axis];
    output.contamination=prepared.verifier->contamination;
    for (const auto& id:prepared.teacher->data_ids)
        output.contamination=std::max(output.contamination,lab_data(lab,id).contamination);
    output.commercial_use=prepared.preview.commercial_use;
    output.open_weight_release=prepared.preview.open_weight_release;
    return output;
}
GenerateDataset request_for(const GeneratedDataset& job,const CapacityProject& work) {
    return {job.id,job.recipe_id,job.teacher_id,job.evidence_id,job.verification_id,job.attempted_units,
        {work.source_id,work.requested,work.staff}};
}
}
void validate_synthetic_catalog(const LabCatalog& catalog) {
    if (catalog.synthesis.size()>lab_catalog_limit) invalid("Synthetic recipe catalog exceeds its bound");
    std::set<std::string_view> ids;
    for (const auto& row:catalog.synthesis) {
        if (row.id.empty() || row.id.size()>64 || row.name.empty() || row.name.size()>128 || !ids.insert(row.id).second ||
            row.domain>=lab_domain_count || row.fee.value()<0 || row.verification_per_unit.value()<=0 ||
            row.verification_per_unit.value()>1'000'000 || row.staff_days.value()<1 || row.staff_days.value()>365 ||
            row.maximum_units==0 || row.maximum_units>1'000'000'000 || row.diversity.value()==0 ||
            row.diversity.value()>5000 || row.minimum_teacher.value()==0 || row.requires_research.size()>16)
            invalid("Synthetic recipe needs bounded verification, work, diversity and teacher requirements");
        std::set<std::string_view> research;
        for (const auto& id:row.requires_research)
            if (!research.insert(id).second || std::ranges::find(catalog.research,id,&LabResearch::id)==catalog.research.end())
                invalid("Synthetic generation references unavailable or repeated research");
        // A domain has one shared diversity bound across all its generation recipes.
        for (const auto& other:catalog.synthesis)
            if (other.domain==row.domain && other.diversity!=row.diversity)
                invalid("Synthetic recipes for one domain must share the same diversity limit");
    }
}
SyntheticPreview preview_synthesis(const LabState& lab,const CapacityState& capacity,OrdinalDay day,const GenerateDataset& request) {
    if (day!=current_day(lab) || lab.generated.size()>=lab_catalog_limit) invalid("Generation needs the current morning and room in the dataset register");
    require_identity(request.id);
    if (std::ranges::find(lab.catalog.data,request.id,&DataOffer::id)!=lab.catalog.data.end() ||
        std::ranges::find(lab.generated,request.id,&GeneratedDataset::id)!=lab.generated.end())
        invalid("A dataset identity cannot be reused");
    return plan(lab,capacity,day,request,lab.generated).preview;
}
void generate_dataset(LabState& lab,AccountsState& accounts,CapacityState& capacity,OrdinalDay day,
                      const GenerateDataset& request,Cents protected_cash) {
    const auto quote=preview_synthesis(lab,capacity,day,request);
    const auto cash=accounts_report(accounts).cash;
    if (quote.fee>checked_unit_subtract(cash,protected_cash)) invalid("Synthetic generation would spend reserved or unavailable cash");
    const auto& teacher=named(lab.models,request.teacher_id);
    const auto& teacher_recipe=named(lab.catalog.recipes,teacher.recipe_id);
    CapacityProject work;
    work.id="lab/generate/"+request.id;
    work.kind=ProjectKind::data_generation;
    work.source_id=request.allocation.source_id;
    work.starts_on=day;
    work.requested=request.allocation.daily_compute;
    work.staff=request.allocation.staff;
    work.minimum_memory_mib=teacher_recipe.minimum_memory_mib;
    work.interconnect=teacher_recipe.interconnect;
    work.total_work=quote.work;
    work.cause="Generate and verify "+request.id;
    reserve_project(capacity,day,work);
    if (quote.fee.value()>0) record_cash(accounts,day,Cents{-quote.fee.value()},Account::expenses,work.cause,work.id);
    lab.generated.push_back({request.id,request.recipe_id,teacher.id,request.evidence_id,request.verification_id,
        work.id,day,teacher.consumed_work,request.attempted_units,{}});
}
void close_synthetic_data(LabState& lab,const CapacityState& capacity,OrdinalDay day) {
    for (std::size_t i=0;i<lab.generated.size();++i) {
        auto& job=lab.generated[i];
        const auto& work=named(capacity.projects,job.project_id);
        if (job.output || work.status!=ProjectStatus::completed) continue;
        const auto prepared=plan(lab,capacity,job.started_on,request_for(job,work),std::span(lab.generated).first(i));
        job.output=verified_output(lab,job,prepared);
        if (!work.stopped_on || *work.stopped_on!=day) invalid("Verified output must appear on its actual completion day");
    }
}
void validate_synthetic_data(const LabState& lab,const CapacityState& capacity,const AccountsState& accounts,bool deep) {
    if (lab.generated.size()>lab_catalog_limit) invalid("Synthetic dataset register exceeds its bound");
    std::set<std::string_view> ids;
    std::optional<OrdinalDay> prior;
    for (std::size_t i=0;i<lab.generated.size();++i) {
        const auto& job=lab.generated[i];
        require_identity(job.id);
        const auto& work=named(capacity.projects,job.project_id);
        if (!ids.insert(job.id).second || std::ranges::find(lab.catalog.data,job.id,&DataOffer::id)!=lab.catalog.data.end() ||
            job.started_on<lab.starts_on || job.started_on>current_day(lab) || (prior && *prior>job.started_on) ||
            work.kind!=ProjectKind::data_generation || work.id!="lab/generate/"+job.id || work.starts_on!=job.started_on)
            invalid("Synthetic job has an invalid identity, date or finite work owner");
        prior=job.started_on;
        const auto prepared=plan(lab,capacity,job.started_on,request_for(job,work),std::span(lab.generated).first(i));
        const auto& teacher_recipe=named(lab.catalog.recipes,prepared.teacher->recipe_id);
        if (job.teacher_work!=prepared.teacher->consumed_work || work.total_work!=prepared.preview.work ||
            work.minimum_memory_mib!=teacher_recipe.minimum_memory_mib || work.interconnect!=teacher_recipe.interconnect ||
            job.output.has_value()!=(work.status==ProjectStatus::completed) ||
            (job.output && *job.output!=verified_output(lab,job,prepared)))
            invalid("Synthetic output differs from its immutable teacher, verification and actual work");
        if (deep) {
            std::size_t payments=0;
            for (const auto& posting:accounts.postings) if (posting.key==work.id) {
                if (posting.day!=job.started_on || posting.credit!=Account::cash || posting.debit!=Account::expenses ||
                    posting.amount!=prepared.preview.fee) invalid("Synthetic generation fee differs from its accepted terms");
                ++payments;
            }
            if (payments!=(prepared.preview.fee.value()>0?1U:0U)) invalid("Synthetic generation is missing its unique paid fee");
        }
    }
}
} // namespace pgp::core
