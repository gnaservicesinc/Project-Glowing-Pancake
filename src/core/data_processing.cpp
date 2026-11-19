#include <pgp/core/lab.hpp>
#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }

void check_label(std::string_view value) {
    if (value.empty() || value.size() > 128 ||
        std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        invalid("Dataset or license identity must be nonempty and contain no control characters");
}

void require_day(const LabState& state, OrdinalDay day) {
    if (day != (state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on))
        invalid("Lab operation must name the current unclosed day");
}

void charge(AccountsState& accounts, OrdinalDay day, Cents amount, Cents protected_cash, const std::string& key, const std::string& cause) {
    if (amount.value() < 0 || amount > checked_unit_subtract(accounts_report(accounts).cash, protected_cash))
        invalid("Insufficient uncommitted cash for this lab commitment");
    if (amount.value() > 0) record_cash(accounts, day, Cents{-amount.value()}, Account::expenses, cause, key);
}

bool valid_operation(std::string_view op) {
    return op == "deduplicate" || op == "filter" || op == "correct" || op == "label" || op == "balance" || op == "review";
}

struct ProcessingEffect {
    std::uint64_t out_units = 0;
    BasisPoints quality{0};
    BasisPoints duplication{0};
    BasisPoints contamination{0};
    LabProfile coverage = empty_lab_profile;
    Cents fee{0};
    MilliCU work{0};
    Days staff_days{1};
};

ProcessingEffect compute_processing_effect(const DataOffer& src, std::string_view op) {
    ProcessingEffect e;
    e.out_units = src.unique_units;
    e.quality = src.quality;
    e.duplication = src.duplication_bp;
    e.contamination = src.contamination;
    e.coverage = src.coverage;
    const auto unit_blocks = std::max<std::uint64_t>(1, (src.unique_units + 99999) / 100000);
    const auto clamped_blocks = std::min<std::uint64_t>(unit_blocks, 200);
    if (op == "deduplicate") {
        const auto drop_bp = std::min<std::uint64_t>(3500, static_cast<std::uint64_t>(src.duplication_bp.value()) / 2 + 500);
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 10000 - drop_bp, 10000));
        e.duplication = BasisPoints{src.duplication_bp.value() / 4};
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 300)};
        e.contamination = BasisPoints{std::max<std::int64_t>(0, src.contamination.value() - 100)};
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 15000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 400)};
        e.staff_days = Days{std::max<std::int64_t>(2, static_cast<std::int64_t>(clamped_blocks / 4 + 2))};
    } else if (op == "filter") {
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 7500, 10000));
        e.duplication = BasisPoints{std::max<std::int64_t>(0, src.duplication_bp.value() - 300)};
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 600)};
        e.contamination = BasisPoints{std::max<std::int64_t>(0, src.contamination.value() - 250)};
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 20000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 550)};
        e.staff_days = Days{std::max<std::int64_t>(3, static_cast<std::int64_t>(clamped_blocks / 3 + 3))};
    } else if (op == "correct") {
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 9200, 10000));
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 450)};
        e.contamination = BasisPoints{std::max<std::int64_t>(0, src.contamination.value() - 150)};
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 40000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 350)};
        e.staff_days = Days{std::max<std::int64_t>(5, static_cast<std::int64_t>(clamped_blocks / 2 + 4))};
    } else if (op == "label") {
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 9600, 10000));
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 500)};
        for (std::size_t i = 0; i < lab_domain_count; ++i) {
            if (e.coverage[i].value() > 0)
                e.coverage[i] = BasisPoints{std::min<std::int64_t>(9800, e.coverage[i].value() + 400)};
        }
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 45000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 300)};
        e.staff_days = Days{std::max<std::int64_t>(6, static_cast<std::int64_t>(clamped_blocks / 2 + 5))};
    } else if (op == "balance") {
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 8200, 10000));
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 200)};
        for (std::size_t i = 0; i < lab_domain_count; ++i) {
            if (e.coverage[i].value() > 0)
                e.coverage[i] = BasisPoints{std::min<std::int64_t>(9800, e.coverage[i].value() + 550)};
        }
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 25000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 450)};
        e.staff_days = Days{std::max<std::int64_t>(4, static_cast<std::int64_t>(clamped_blocks / 3 + 3))};
    } else if (op == "review") {
        e.out_units = std::max<std::uint64_t>(1, mul_div_round_half_up(src.unique_units, 9000, 10000));
        e.quality = BasisPoints{std::min<std::int64_t>(9600, src.quality.value() + 250)};
        e.contamination = BasisPoints{std::max<std::int64_t>(0, src.contamination.value() - 350)};
        e.fee = Cents{static_cast<std::int64_t>(clamped_blocks * 30000)};
        e.work = MilliCU{static_cast<std::int64_t>(clamped_blocks * 250)};
        e.staff_days = Days{std::max<std::int64_t>(4, static_cast<std::int64_t>(clamped_blocks / 3 + 4))};
    } else {
        invalid("Unsupported dataset processing operation");
    }
    return e;
}

DataOffer materialize_processed_offer(const DataOffer& src, const std::string& output_id, std::string_view op) {
    const auto e = compute_processing_effect(src, op);
    DataOffer out = src;
    out.id = output_id;
    out.name = src.name + " (" + std::string{op} + ")";
    out.price = Cents{0};
    out.raw_units = src.raw_units > 0 ? src.raw_units : src.unique_units;
    out.unique_units = e.out_units;
    out.quality = e.quality;
    out.duplication_bp = e.duplication;
    out.contamination = e.contamination;
    out.coverage = e.coverage;
    // CRITICAL INVARIANT: Processing NEVER invents rights, NEVER raises provenance confidence, and NEVER clears controversy risk.
    out.commercial_use = src.commercial_use;
    out.open_weight_release = src.open_weight_release;
    out.provenance_confidence_bp = src.provenance_confidence_bp;
    out.third_party_controversy_risk = src.third_party_controversy_risk;
    out.source_path = src.source_path;
    out.supplier_or_publisher = src.supplier_or_publisher;
    out.underlying_history = src.underlying_history;
    out.collection_method = src.collection_method;
    out.parent_dataset_id = src.id;
    out.processing_step = std::string{op};
    return out;
}

bool dataset_id_exists(const LabState& state, std::string_view id) {
    return std::ranges::any_of(state.catalog.data, [&](const auto& d) { return d.id == id; }) ||
           std::ranges::any_of(state.generated, [&](const auto& d) { return d.id == id; }) ||
           std::ranges::any_of(state.acquisitions, [&](const auto& d) { return d.offer_id == id; }) ||
           std::ranges::any_of(state.processed, [&](const auto& d) { return d.id == id; }) ||
           std::ranges::any_of(state.creative_jobs, [&](const auto& d) { return d.id == id; });
}

std::size_t domain_index(std::string_view domain) {
    const auto it = std::ranges::find(lab_domain_names, domain);
    if (it == lab_domain_names.end()) invalid("Choose a valid dataset task domain");
    return static_cast<std::size_t>(it - lab_domain_names.begin());
}

std::uint32_t creative_team_average(const SimulationState& state, const std::vector<std::string>& ids) {
    if (!state.management || !state.management->people) invalid("Creative production requires company staff");
    if (ids.empty() || ids.size() > 16) invalid("Assign between 1 and 16 creative contributors");
    std::set<std::string_view> unique;
    std::uint64_t total = 0;
    for (const auto& id : ids) {
        if (!unique.insert(id).second) invalid("Each contributor may only be assigned once");
        const auto it = std::ranges::find_if(state.management->people->employees,
            [&](const auto& e) { return e.terms.profile.id == id && !e.departed_on; });
        if (it == state.management->people->employees.end())
            invalid("Assigned creative contributor is not an active employee");
        if (it->ready_on > state.date)
            invalid("Wait for onboarding before assigning this creative contributor");
        if (talent_rules(state)) {
            const auto why = software_worker_blocker(state, id);
            if (!why.empty()) throw std::invalid_argument(it->terms.profile.name + ": " + why);
        }
        const auto skill = it->terms.profile.creative;
        if (skill < 40)
            invalid("Assign Data Creative specialists or staff with creative production skill >= 40");
        total += skill;
    }
    return static_cast<std::uint32_t>(total / ids.size());
}

DataOffer materialize_creative_offer(const CreativeProductionJob& job) {
    const auto idx = domain_index(job.domain);
    DataOffer out;
    out.id = job.id;
    out.name = "In-House Creative Corpus · " + std::string{lab_domain_labels[idx]};
    out.provenance = "Created directly by salaried company Data Creative specialists";
    out.rights = "Full company ownership · commercial and open-weight cleared";
    out.price = Cents{0};
    out.raw_units = job.target_units;
    out.unique_units = job.target_units;
    // Highest attainable quality depends on actual creative skill — never automatic 100%.
    out.quality = BasisPoints{std::clamp<std::int64_t>(7400 + static_cast<std::int64_t>(job.average_creative_skill) * 24, 7600, 9700)};
    out.relevance[idx] = BasisPoints{9800};
    out.coverage[idx] = BasisPoints{std::clamp<std::int64_t>(7200 + static_cast<std::int64_t>(job.average_creative_skill) * 25, 7500, 9600)};
    // Secondary transfer to documents when crafting non-document creative corpora
    if (idx != 0) {
        out.relevance[0] = BasisPoints{3500};
        out.coverage[0] = BasisPoints{3000};
    }
    out.contamination = BasisPoints{0};
    out.duplication_bp = BasisPoints{0};
    out.commercial_use = true;
    out.open_weight_release = true;
    out.source_path = "in_house_creative";
    out.supplier_or_publisher = "In-house creative studio";
    out.underlying_history = "Original works authored specifically for training by company staff under work-for-hire contracts.";
    out.collection_method = "Salaried human authoring, domain review, and clean verification";
    out.provenance_confidence_bp = BasisPoints{10000};
    out.third_party_controversy_risk = false;
    return out;
}
} // namespace

DataProcessingPreview preview_data_processing(const LabState& state, const CapacityState& capacity, const ProcessDataset& request) {
    check_label(request.output_id);
    if (!valid_operation(request.operation)) invalid("Unsupported dataset processing operation");
    if (request.allocation.daily_compute.value() == 0 || request.allocation.staff == 0)
        invalid("Dataset processing needs positive staff and compute");
    const auto available = data_available_on(state, capacity, request.source_dataset_id);
    if (!available) invalid("Own or deliver the source dataset before processing it");
    const auto& src = lab_data(state, request.source_dataset_id);
    const auto eff = compute_processing_effect(src, request.operation);
    const auto compute_days = (eff.work.value() + request.allocation.daily_compute.value() - 1) / request.allocation.daily_compute.value();
    const auto staff_days = (eff.staff_days.value() + request.allocation.staff - 1) / request.allocation.staff;
    DataProcessingPreview p;
    p.fee = eff.fee;
    p.work = eff.work;
    p.minimum_days = Days{std::max<std::int64_t>(1, std::max<std::int64_t>(compute_days, staff_days))};
    p.estimated_unique_units = eff.out_units;
    p.estimated_quality = eff.quality;
    p.estimated_duplication = eff.duplication;
    p.estimated_contamination = eff.contamination;
    p.provenance_confidence = src.provenance_confidence_bp;
    p.commercial_use = src.commercial_use;
    p.open_weight_release = src.open_weight_release;
    p.third_party_controversy_risk = src.third_party_controversy_risk;
    p.explanation = "Processing improves corpus quality, duplication, coverage or contamination at the cost of volume, compute, staff and cash. "
                    "It preserves original provenance, rights and third-party controversy risk unchanged.";
    return p;
}

void start_data_processing(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                           const ProcessDataset& request, Cents protected_cash) {
    require_day(state, day);
    check_label(request.output_id);
    if (state.processed.size() >= lab_record_limit) invalid("Processed dataset record limit reached");
    if (dataset_id_exists(state, request.output_id)) invalid("That output dataset identity is already in use");
    const auto available = data_available_on(state, capacity, request.source_dataset_id);
    if (!available || *available > day) invalid("Source dataset must be owned and delivered before processing");
    const auto& src = lab_data(state, request.source_dataset_id);
    const auto eff = compute_processing_effect(src, request.operation);
    if (request.allocation.daily_compute.value() == 0 || request.allocation.staff == 0)
        invalid("Dataset processing needs positive staff and compute");
    CapacityProject work;
    work.id = "lab/process/" + request.output_id;
    work.source_id = request.allocation.source_id;
    work.kind = ProjectKind::research;
    work.starts_on = day;
    work.requested = request.allocation.daily_compute;
    work.staff = request.allocation.staff;
    work.total_work = eff.work;
    work.required_staff_days = static_cast<std::uint32_t>(eff.staff_days.value());
    work.cause = "Dataset processing (" + request.operation + "): " + request.output_id;
    reserve_project(capacity, day, work);
    charge(accounts, day, eff.fee, protected_cash, work.id, work.cause);
    state.processed.push_back({request.output_id, request.source_dataset_id, request.operation, work.id, day, eff.fee, std::nullopt});
}

void close_data_processing(LabState& state, const CapacityState& capacity, OrdinalDay /*day*/) {
    for (auto& job : state.processed) {
        if (job.output) continue;
        const auto it = std::ranges::find(capacity.projects, job.project_id, &CapacityProject::id);
        if (it == capacity.projects.end() || it->status != ProjectStatus::completed) continue;
        const auto& src = lab_data(state, job.source_dataset_id);
        job.output = materialize_processed_offer(src, job.id, job.operation);
    }
}

void validate_data_processing(const LabState& state, const CapacityState& capacity, const AccountsState& accounts, bool deep) {
    if (state.processed.size() > lab_record_limit) invalid("Processed dataset count exceeds bound");
    std::set<std::string_view> seen;
    for (const auto& job : state.processed) {
        check_label(job.id);
        if (!seen.insert(job.id).second || !valid_operation(job.operation))
            invalid("Duplicate or unsupported processed dataset record");
        const auto work = std::ranges::find(capacity.projects, job.project_id, &CapacityProject::id);
        if (work == capacity.projects.end() || work->id != "lab/process/" + job.id || work->starts_on != job.started_on)
            invalid("Processed dataset lost its backing capacity project");
        const auto& src = lab_data(state, job.source_dataset_id);
        const auto eff = compute_processing_effect(src, job.operation);
        if (job.fee != eff.fee || work->total_work != eff.work)
            invalid("Processed dataset fee or work differs from authored rules");
        if (deep && job.fee.value() > 0 && std::ranges::none_of(accounts.postings, [&](const auto& p) {
                return p.key == job.project_id && p.day == job.started_on && p.amount == job.fee && p.credit == Account::cash;
            })) invalid("Processed dataset payment is missing from accounts");
        if (work->status == ProjectStatus::completed) {
            if (!job.output || *job.output != materialize_processed_offer(src, job.id, job.operation))
                invalid("Completed processed dataset output differs from immutable lineage rules");
        } else if (job.output) {
            invalid("Unfinished dataset processing job cannot deliver an output corpus");
        }
    }
}

CreativeProductionPreview preview_creative_production(const SimulationState& state, const StartCreativeProduction& request) {
    if (!state.lab) invalid("This scenario has no lab system");
    check_label(request.output_id);
    (void)domain_index(request.domain);
    if (request.target_units < 10000 || request.target_units > 5'000'000)
        invalid("Creative corpus target size must be between 10,000 and 5,000,000 units");
    if (request.allocation.daily_compute.value() == 0) invalid("Creative ingestion requires positive verification compute");
    const auto avg = creative_team_average(state, request.allocation.researchers);
    const auto blocks = std::max<std::uint64_t>(1, (request.target_units + 24999) / 25000);
    // Deliberately slow and paid compared to scraping/torrents: 6 staff-days & $2,500 per 25k units.
    const auto staff_days = static_cast<std::int64_t>(blocks * 6);
    const auto staff_count = static_cast<std::int64_t>(request.allocation.researchers.size());
    const auto fee = Cents{static_cast<std::int64_t>(blocks * 250000)};
    const auto work = MilliCU{static_cast<std::int64_t>(blocks * 250)};
    const auto compute_days = (work.value() + request.allocation.daily_compute.value() - 1) / request.allocation.daily_compute.value();
    const auto labor_days = (staff_days + staff_count - 1) / staff_count;
    CreativeProductionPreview p;
    p.fee = fee;
    p.work = work;
    p.minimum_days = Days{std::max<std::int64_t>(1, std::max<std::int64_t>(compute_days, labor_days))};
    p.average_creative_skill = avg;
    p.estimated_quality = BasisPoints{std::clamp<std::int64_t>(7400 + static_cast<std::int64_t>(avg) * 24, 7600, 9700)};
    p.provenance_confidence = BasisPoints{10000};
    p.third_party_controversy_risk = false;
    p.explanation = "In-house creative production is slower and more expensive per unit, but delivers clean company-owned rights, zero third-party controversy risk, and quality scaled by your assigned creative staff.";
    return p;
}

void start_creative_production(SimulationState& state, const StartCreativeProduction& request, Cents protected_cash) {
    if (!state.lab || !state.accounts || !state.capacity) invalid("Lab, accounts and capacity are required");
    require_day(*state.lab, state.date);
    if (state.lab->creative_jobs.size() >= lab_record_limit) invalid("Creative production register is full");
    if (dataset_id_exists(*state.lab, request.output_id)) invalid("That output dataset identity is already in use");
    const auto preview = preview_creative_production(state, request);
    const auto blocks = std::max<std::uint64_t>(1, (request.target_units + 24999) / 25000);
    CapacityProject work;
    work.id = "lab/creative/" + request.output_id;
    work.source_id = request.allocation.source_id;
    work.kind = ProjectKind::research;
    work.starts_on = state.date;
    work.requested = request.allocation.daily_compute;
    work.staff = static_cast<std::uint32_t>(request.allocation.researchers.size());
    work.total_work = preview.work;
    work.required_staff_days = static_cast<std::uint32_t>(blocks * 6);
    work.cause = "In-house creative data production: " + request.output_id;
    reserve_project(*state.capacity, state.date, work);
    charge(*state.accounts, state.date, preview.fee, protected_cash, work.id, work.cause);
    state.lab->creative_jobs.push_back({request.output_id, request.domain, work.id, request.allocation.researchers,
        preview.average_creative_skill, state.date, preview.fee, request.target_units, std::nullopt});
}

void close_creative_production(LabState& state, const CapacityState& capacity, OrdinalDay /*day*/) {
    for (auto& job : state.creative_jobs) {
        if (job.output) continue;
        const auto it = std::ranges::find(capacity.projects, job.project_id, &CapacityProject::id);
        if (it == capacity.projects.end() || it->status != ProjectStatus::completed) continue;
        job.output = materialize_creative_offer(job);
    }
}

void validate_creative_production(const LabState& state, const CapacityState& capacity, const AccountsState& accounts, bool deep) {
    if (state.creative_jobs.size() > lab_record_limit) invalid("Creative production count exceeds bound");
    std::set<std::string_view> seen;
    for (const auto& job : state.creative_jobs) {
        check_label(job.id);
        (void)domain_index(job.domain);
        if (!seen.insert(job.id).second || job.creatives.empty() || job.creatives.size() > 16 || job.average_creative_skill < 40 || job.average_creative_skill > 100)
            invalid("Invalid creative production job metadata");
        const auto work = std::ranges::find(capacity.projects, job.project_id, &CapacityProject::id);
        if (work == capacity.projects.end() || work->id != "lab/creative/" + job.id || work->starts_on != job.started_on)
            invalid("Creative production job lost its capacity project");
        if (deep && job.fee.value() > 0 && std::ranges::none_of(accounts.postings, [&](const auto& p) {
                return p.key == job.project_id && p.day == job.started_on && p.amount == job.fee && p.credit == Account::cash;
            })) invalid("Creative production fee missing from accounts");
        if (work->status == ProjectStatus::completed) {
            if (!job.output || *job.output != materialize_creative_offer(job))
                invalid("Completed creative corpus differs from team skill and ownership rules");
        } else if (job.output) {
            invalid("Unfinished creative production job cannot deliver output");
        }
    }
}

void license_technology(LabState& state, AccountsState& accounts, OrdinalDay day, std::string_view offer_id, Cents protected_cash) {
    require_day(state, day);
    const auto offer = std::ranges::find(state.catalog.tech_licenses, offer_id, &TechLicenseOffer::id);
    if (offer == state.catalog.tech_licenses.end()) invalid("Technology license offer does not exist");
    if (state.tech_licenses.size() >= lab_catalog_limit) invalid("Technology license register is full");
    if (std::ranges::any_of(state.tech_licenses, [&](const auto& l) { return l.offer_id == offer->id && day < l.expires_on; }))
        invalid("This technology license is already active; renew it if you want to extend its term");
    const auto active_on = advance_days(day, offer->integration_days);
    const auto expires_on = advance_days(active_on, offer->term_days);
    charge(accounts, day, offer->upfront_fee, protected_cash, "lab/tech_license/" + offer->id + "/" + std::to_string(day.value()),
           "Technology license upfront fee: " + offer->licensor);
    if (offer->monthly_royalty.value() > 0) {
        add_recurring(accounts, {"lab/tech_royalty/" + offer->id + "/" + std::to_string(day.value()),
                                 offer->monthly_royalty, day, expires_on, "Technology license royalty: " + offer->licensor});
    }
    state.tech_licenses.push_back({offer->id, offer->research_id, offer->licensor, day, active_on, expires_on, false, 0});
}

void renew_tech_license(LabState& state, AccountsState& accounts, OrdinalDay day, std::string_view offer_id, Cents protected_cash) {
    require_day(state, day);
    const auto offer = std::ranges::find(state.catalog.tech_licenses, offer_id, &TechLicenseOffer::id);
    if (offer == state.catalog.tech_licenses.end() || !offer->renewable) invalid("This technology license cannot be renewed");
    auto it = std::find_if(state.tech_licenses.rbegin(), state.tech_licenses.rend(),
                           [&](const auto& l) { return l.offer_id == offer->id; });
    if (it == state.tech_licenses.rend()) invalid("Acquire a technology license before renewing it");
    if (day >= it->expires_on) invalid("The license expired. Acquire a new term and complete integration again.");
    const auto renewal_start = std::max(day, it->expires_on);
    const auto next_expiry = advance_days(renewal_start, offer->term_days);
    ++it->renewals;
    const auto fee = Cents{offer->upfront_fee.value() / 2};
    charge(accounts, day, fee, protected_cash,
           "lab/tech_license_renew/" + offer->id + "/" + std::to_string(it->starts_on.value()) + "/" + std::to_string(it->renewals),
           "Technology license renewal: " + offer->licensor);
    if (offer->monthly_royalty.value() > 0) {
        add_recurring(accounts, {"lab/tech_royalty_renew/" + offer->id + "/" + std::to_string(it->starts_on.value()) + "/" + std::to_string(it->renewals),
                                 offer->monthly_royalty, renewal_start, next_expiry, "Technology license royalty renewal: " + offer->licensor});
    }
    it->expires_on = next_expiry;
}

void validate_tech_licenses(const LabState& state, const AccountsState& accounts, bool deep) {
    if (state.tech_licenses.size() > lab_catalog_limit) invalid("Technology license list exceeds bound");
    for (const auto& lic : state.tech_licenses) {
        const auto offer = std::ranges::find(state.catalog.tech_licenses, lic.offer_id, &TechLicenseOffer::id);
        if (offer == state.catalog.tech_licenses.end() || lic.research_id != offer->research_id ||
            lic.licensor != offer->licensor || lic.grants_discovery_credit ||
            lic.active_on != advance_days(lic.starts_on, offer->integration_days) ||
            lic.expires_on < advance_days(lic.active_on, offer->term_days))
            invalid("Technology license record differs from catalog terms or forged discovery credit");
        if (deep && offer->upfront_fee.value() > 0 && std::ranges::none_of(accounts.postings, [&](const auto& p) {
                return p.key == "lab/tech_license/" + offer->id + "/" + std::to_string(lic.starts_on.value()) &&
                       p.day == lic.starts_on && p.amount == offer->upfront_fee && p.credit == Account::cash;
            })) invalid("Technology license upfront payment is missing from accounts");
    }
}

bool dataset_lineage_has_controversy(const LabState& state, std::string_view dataset_id) {
    std::string current{dataset_id};
    for (std::size_t depth = 0; depth < 32 && !current.empty(); ++depth) {
        const auto& d = lab_data(state, current);
        if (d.third_party_controversy_risk) return true;
        current = d.parent_dataset_id;
    }
    return false;
}

bool model_lineage_has_controversy(const LabState& state, std::string_view model_id) {
    std::string current{model_id};
    for (std::size_t depth = 0; depth < 64 && !current.empty(); ++depth) {
        const auto it = std::ranges::find(state.models, current, &LabModel::id);
        if (it == state.models.end()) break;
        for (const auto& did : it->data_ids)
            if (dataset_lineage_has_controversy(state, did)) return true;
        current = it->parent_id;
    }
    return false;
}

ProductTechnicalProfile product_technical_profile(const SimulationState& state, std::string_view model_id) {
    ProductTechnicalProfile prof;
    if (!state.lab || !state.capacity) return prof;
    const auto& lab = *state.lab;
    const auto mit = std::ranges::find(lab.models, model_id, &LabModel::id);
    if (mit == lab.models.end()) return prof;
    prof.is_proof_of_concept = mit->is_proof_of_concept;
    prof.clean_provenance = !model_lineage_has_controversy(lab, mit->id);
    prof.data_efficiency_bp = mit->data_efficiency_bp;
    prof.compute_efficiency_bp = mit->compute_efficiency_bp;
    std::int64_t peak = 0;
    for (const auto bp : mit->capability) peak = std::max(peak, bp.value());
    prof.quality_bp = static_cast<std::uint32_t>(peak);
    std::uint32_t lat = 0, rob = 0;
    std::set<std::string> mods, settings;
    for (const auto& topic : lab.catalog.research) {
        const auto& effects = topic.technical_effect;
        if (!effects.latency_bonus_bp && !effects.robustness_bonus_bp && effects.modality_unlock.empty() && effects.deployment_setting_unlock.empty()) continue;
        if (!has_research_access(lab, *state.capacity, topic.id, mit->created_on)) continue;
        lat = std::min<std::uint32_t>(3500, lat + topic.technical_effect.latency_bonus_bp);
        rob = std::min<std::uint32_t>(3500, rob + topic.technical_effect.robustness_bonus_bp);
        if (!topic.technical_effect.modality_unlock.empty()) mods.insert(topic.technical_effect.modality_unlock);
        if (!topic.technical_effect.deployment_setting_unlock.empty()) settings.insert(topic.technical_effect.deployment_setting_unlock);
    }
    prof.latency_bonus_bp = lat;
    prof.robustness_bonus_bp = rob;
    prof.modalities.assign(mods.begin(), mods.end());
    prof.deployment_settings.assign(settings.begin(), settings.end());
    return prof;
}
} // namespace pgp::core
