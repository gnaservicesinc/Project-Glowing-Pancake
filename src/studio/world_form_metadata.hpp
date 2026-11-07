#pragma once
#include <pgp/studio/world_forms.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <set>

namespace pgp::studio::world_detail {
using Json=nlohmann::json;
inline std::string escape(std::string_view v) {
    std::string out;for (char c:v) {if(c=='~') out+="~0";else if(c=='/') out+="~1";else out+=c;}return out;
}
inline std::string last(std::string_view path) {return std::string(path.substr(path.rfind('/')+1));}
inline std::string label(std::string v) {
    for(char& c:v) if(c=='_') c=' ';
    if(!v.empty()) v[0]=static_cast<char>(std::toupper(static_cast<unsigned char>(v[0])));
    return v;
}
inline std::string scalar(const Json& j) {return j.is_string()?j.get<std::string>():j.dump();}
inline std::string row_label(const Json& j,std::string key) {
    if(j.is_object()) {
        if(j.contains("name")) return scalar(j.at("name"))+(j.contains("id")?" · "+scalar(j.at("id")):"");
        if(j.contains("id")) return scalar(j.at("id"));
        if(j.contains("state_id")) return scalar(j.at("state_id"));
        if(j.contains("state")) return scalar(j.at("state"));
        if(j.contains("kind")) return label(scalar(j.at("kind")));
    }
    return label(std::move(key));
}
inline bool dynamic_map(std::string_view path) {
    return path.ends_with("/ownership_basis_points") || path.ends_with("/monthly_commitments_cents");
}
inline std::string units(std::string_view path) {
    if(path.ends_with("/scoring/weights") || path.find("/scoring/weights/")!=path.npos) return "score points; the five weights total 100";
    if(path.find("millicents")!=path.npos) return "thousandths of a constant-2014 cent per kWh";
    if(path.find("cents_per_1000_liters")!=path.npos) return "constant-2014 cents per 1,000 liters";
    if(path.find("per_kw_cents")!=path.npos) return "constant-2014 cents per kW";
    if(path.find("cents")!=path.npos) return "constant-2014 cents";
    if(path.ends_with("/first_agenda_day")) return "days since 2014-01-01";
    if(path.find("_bp")!=path.npos || path.find("basis_points")!=path.npos) return "basis points (100 bp = 1%)";
    if(path.find("milli_CU_per_day")!=path.npos) return "milli-CU per day (1,000 = 1 CU/day)";
    if(path.find("milli_CU")!=path.npos) return "milli-CU (1,000 = 1 CU)";
    if(path.find("days")!=path.npos) return "simulated days";
    if(path.find("watts")!=path.npos) return "watts";
    if(path.find("ml_per_kwh")!=path.npos) return "milliliters per kWh";
    if(path.find("ml_per_day")!=path.npos) return "milliliters per simulated day";
    if(path.find("grams_per_kwh")!=path.npos) return "grams per kWh";
    if(path.find("_mib")!=path.npos) return "MiB";
    if(path.find("pue_milli")!=path.npos) return "PUE × 1,000";
    return "whole units";
}
inline std::string help(std::string_view path) {
    const auto key=last(path);
    if(key=="cash_cents") return "Cash available at the scenario opening. Initial financing, obligations and committed bills remain separate.";
    if(key=="seed") return "Exact unsigned 64-bit root seed for the saved independent random streams. An inspection uses copied state.";
    if(key=="starts_on") return "Opening calendar date, YYYY-MM-DD. Stage dependent offer, incident and campaign dates together when moving the opening.";
    if(key=="ends_before") return "Exclusive end date, YYYY-MM-DD. Terms must fit the owning scenario window.";
    if(key=="team_people") return "Opening staff available to the shared project allocator. Dedicated site operators are separately hired.";
    if(key=="capacity_milli_CU_per_day") return "Capacity supplied by the opening compute lease; serving and project work share this supply.";
    if(key=="lease_energy_included") return "Whether the opening lease includes power in its quoted costs.";
    if(key.starts_with("default_policy_") || key=="default_reserve_floor_cents" || key=="default_monthly_ceiling_cents")
        return "Initial value offered by the management policy form. Player authorization, actual funds, staff and compute still limit every run.";
    if(key=="task_target") return "Actual delivered useful work needed for the work portion of the slice score. The other score components retain their own weights.";
    if(key=="opening_founder_share_bp") return "Must equal this founder’s actual opening ownership. Stage the ownership table and this baseline together.";
    if(key=="grid_lead_days") return "Delay from ordering a site until the grid connection is available. Construction, hardware, operators and inspection have separate readiness dates.";
    if(key=="first_agenda_day") return "Date this public docket opens. Committee, chamber, executive, review and agency dates follow the authored terms.";
    if(key=="permit_watts") return "Limit on newly authorized site reservations in this state while the rule is effective.";
    if(key=="evidence_axis") return "Independent evaluation domain index, from 0 to 5 in Lab profile order.";
    if(key=="rights") return "Describe the actual regional procurement rights and operating obligations granted by this role.";
    if(key=="public_work") return "Public tasks required in the last settled regional month. Reviews prorate the same obligation to their actual interval.";
    if(key=="points_bp") return "Maximum authority from a fully operating role. Hard authority is reduced by regional resistance.";
    if(key=="branches_after_day") return "Ordinal day since 2014-01-01 when the conditional era starts. Technology still depends on seeded conditions and research.";
    if(key=="asi_route") return "Every seeded path needs a feasible game ASI recipe, evidence and replication route when selected.";
    if(key=="refresh_review_age_days") return "Maximum age of a completed deployment methods experiment at refresh start and rollout.";
    if(key=="repeat_limit") return "Maximum successful results for this topic. Each attempt needs actual funding, staff and work.";
    if(key=="failure_bp") return "Setback chance for a first settled result if the safeguard was missing at the start.";
    if(key=="retry_credit_bp") return "Reusable experiment work after a settled setback. Staff-days still apply.";
    if(key=="safeguard") return "Optional research result that prevents this setback when completed before the attempt starts.";
    if(key=="start_lead_days") return "Days after signing until specialist payroll begins.";
    if(key=="onboarding_days") return "Paid days after joining before this specialist can take assignments.";
    if(key=="evaluation_credit_bp") return "Reduction in experiment work when explicitly assigned to one evaluation. The same fee and measured coverage apply.";
    if(key=="founder_birth_year" || key=="first_birth_year") return "Calendar birth year used for the charter's finite careers. Later cohorts are distinct people with separate identities.";
    if(key=="founder_board") return "The founder's existing board rights. The charter changes their personal chair and trust representation after succession.";
    if(key=="maximum_term_years" || key=="executive_retirement_age" || key=="chair_retirement_age") return "Career limit in calendar years. Planning notices precede the actual deadline and the caretaker charter preserves company obligations.";
    if(path.find("/identity_effects_bp/")!=path.npos) return "Signed audience confidence change when this public event occurs. Its effect fades over the authored memory window.";
    if(key=="map_sha256" || key=="map_asset_id") return "Identity of the prepared offline map asset. Retain its matching hash and provenance.";
    if(key=="assumptions") return "Visible authored-balance assumptions. Label fictional exposure and future parameters honestly.";
    if(path.find("/monthly_probability_bp/")!=path.npos) return "Monthly chance for this named hazard; all hazards together must leave a quiet outcome. Profiles are authored game balance.";
    return "This value belongs to the selected scenario section. Its owning content schema validates bounds, vocabulary and dependent references.";
}
inline std::vector<FormOption> choices(const DraftDocument& draft,const Json& doc,std::string_view scenario,std::string_view path,const Json& value) {
    std::vector<FormOption> out;
    auto add=[&](std::string id,std::string name={}) {
        if(std::none_of(out.begin(),out.end(),[&](const auto& o){return o.value==id;})) out.push_back({id,name.empty()?id:std::move(name),{}});
    };
    if(value.is_boolean()) {add("false","False");add("true","True");return out;}
    if(!value.is_string()) return out;
    const auto key=last(path);
    const auto parent=path.substr(0,path.rfind('/'));
    const auto array_key=last(parent);
    std::string definition_kind;
    if(key=="regional_markets") definition_kind="regional_markets";
    if(key=="resilience") definition_kind="site_resilience";
    if(key=="civic") definition_kind="us_civic";
    if(key=="society") definition_kind="regional_society";
    if(key=="authority") definition_kind="institutional_authority";
    if(key=="frontier") definition_kind="frontier_research";
    if(key=="refresh_research" || key=="safeguard" || (key=="research" && path.find("/catalog/experiments/")!=path.npos)) definition_kind="research";
    if(array_key=="flows") definition_kind="flow";
    if(parent.find("/prerequisites/")!=std::string_view::npos) definition_kind="research";
    if(array_key=="headquarters_choices") {add("US-CA","California");add("US-TX","Texas");add("US-WA","Washington");}
    if(!definition_kind.empty()) {
        if(key=="regional_markets" || key=="resilience" || key=="civic" || key=="society" || key=="authority" || key=="frontier") add("","Disabled");
        for(const auto& [id,d]:draft.collection().definitions) if(content::definition_kind(d)==definition_kind) add(id);
    }
    const auto* def=draft.collection().find(scenario);
    if(!def || !std::holds_alternative<content::ScenarioDefinition>(def->data)) return out;
    const auto& s=std::get<content::ScenarioDefinition>(def->data);
    if((parent.ends_with("/objectives") && key=="primary") || parent.ends_with("/objectives/secondary")) {
        const auto* future=content::frontier_catalog(draft.collection(),s);
        for(auto kind:core::objective_kinds) if(kind!=core::ObjectiveKind::asi || (future && future->asi_route))
            add(std::string(core::objective_key(kind)),std::string(core::objective_name(kind)));
    }
    if(key=="rule" && parent.find("/catalog/policies/")!=parent.npos)
        for(auto rule:{core::CivicRule::launch_evidence,core::CivicRule::site_power,core::CivicRule::final_closure}) {
            const std::string id(core::civic_key(rule));add(id,label(id));
        }
    if(key=="founder_board" && s.management && s.management->governance)
        for(const auto& member:s.management->governance->board) if(member.player) add(member.id,member.name);
    if(parent.find("/catalog/gates/")!=parent.npos) {
        if(key=="kind") for(const auto kind:{core::FrontierTechnology::data,core::FrontierTechnology::research,
                core::FrontierTechnology::recipe,core::FrontierTechnology::hardware}) {
            const auto id=std::string(core::frontier_key(kind));add(id,label(id));
        }
        if(key=="target") {
            // Offer all typed targets so kind and target can change in the
            // same validated transaction. Research may live in definitions.
            if(s.lab) {
                const auto lab=content::scenario_lab_catalog(draft.collection(),s);
                for(const auto& row:lab.data) add(row.id,row.name+" · Data");
                for(const auto& row:lab.research) add(row.id,row.id+" · Research");
                for(const auto& row:lab.recipes) add(row.id,row.name+" · Recipe");
            }
            if(s.infrastructure) for(const auto& row:s.infrastructure->hardware) add(row.id,row.name+" · Hardware");
        }
    }
    if(key=="id" && s.lab) {
        if(parent.find("/catalog/corpora/")!=parent.npos) for(const auto& row:s.lab->data) add(row.id,row.name);
        if(parent.find("/catalog/recipes/")!=parent.npos) for(const auto& row:s.lab->recipes) add(row.id,row.name);
    }
    if(key=="recipe_id" || key=="required_recipe" || array_key=="recipes") {
        if(s.lab) for(const auto& row:s.lab->recipes) add(row.id,row.name);
    }
    if(key=="data_id" || array_key=="data_ids") {
        if(s.lab) for(const auto& row:s.lab->data) add(row.id,row.name);
    }
    if(key=="state_id" || key=="state" || key=="from" || key=="to") {
        if(s.infrastructure) for(const auto& row:s.infrastructure->states) add(row.id,row.name);
    }
    if(key=="jurisdiction") {
        add("US","United States");
        if(s.infrastructure) for(const auto& row:s.infrastructure->states) add(row.id,row.name);
    }
    if(key=="issue" || (key=="priority" && path.find("/constituencies/")!=path.npos))
        for(std::size_t issue=0;issue<core::civic_issues;++issue) {
            const auto id=std::string(core::civic_key(static_cast<core::CivicIssue>(issue)));add(id,label(id));
        }
    if(key=="rule" && path.find("/catalog/policies/")!=path.npos)
        for(const auto rule:{core::CivicRule::launch_evidence,core::CivicRule::site_power}) {
            const auto id=std::string(core::civic_key(rule));add(id,label(id));
        }
    if(key=="source_id") add("lease.initial","Opening compute lease");
    if(key=="founder") for(const auto& [id,amount]:s.ownership) {(void)amount;add(id);}
    if(array_key=="research" || array_key=="requires_research" || array_key=="all_of" || array_key=="any_of") {
        for(const auto& [id,d]:draft.collection().definitions) if(std::holds_alternative<content::ResearchDefinition>(d.data)) add(id);
        if(s.lab && (path.find("/planning/")!=path.npos || array_key=="requires_research"))
            for(const auto& row:s.lab->research) add(row.id);
    }
    if(key=="kind" && path.find("/business_offers/")!=path.npos) for(const auto* id:{"compute_lease","recovery_pilot","enterprise_contract"}) add(id,label(id));
    if(key=="group") for(const auto* id:{"domestic","international","heavyweight"}) add(id,label(id));
    if(key=="region" && path.find("/planning/")!=path.npos)
        for(const auto* id:{"US","Canada","Europe","China","India","EastAsiaOceania","LatinAmerica","AfricaMiddleEast"}) add(id,label(id));
    if(key=="region_id" || array_key=="regions") {
        if(const auto* regions=content::regional_markets(draft.collection(),s))
            for(const auto& r:regions->regions) add(r.id,r.name);
    }
    if(key=="channel") for(const auto channel:{core::ProductChannel::legacy,core::ProductChannel::business,core::ProductChannel::api,
        core::ProductChannel::consumer,core::ProductChannel::licensed,core::ProductChannel::open_weights,core::ProductChannel::agent}) {
        const auto id=std::string(core::channel_key(channel));add(id,label(id));
    }
    if(key=="segment_id" || array_key=="public_services" || array_key=="science" || array_key=="industry")
        for(const auto& row:content::scenario_market_setup(draft.collection(),s).segments) add(row.id);
    if(key=="product_id" && path.find("/market/contracts/")!=path.npos) {
        if(s.market) for(const auto& row:s.market->offers) add(row.product.id);
    } else if(key=="pilot_product" || key=="product_id" || array_key=="services")
        for(const auto& row:content::scenario_services(draft.collection(),s)) add(row.product.id);
    if(key=="product_id" && s.market) for(const auto& row:s.market->offers) add(row.product.id);
    if(key=="publication_node" && s.lab) for(const auto& row:s.lab->research) add(row.id);
    if(key=="specialist_id" && s.management) for(const auto& row:s.management->specialists) add(row.id,row.name);
    if(key=="lease_offer_id") for(const auto& row:s.business_offers) if(row.kind==core::BusinessOfferKind::compute_lease) add(row.id,row.name);
    if(path.find("/covered/")!=path.npos) for(const auto kind:{core::SiteHazard::grid,core::SiteHazard::heat,core::SiteHazard::storm,core::SiteHazard::flood,core::SiteHazard::fire,core::SiteHazard::earthquake}) {
        const auto id=std::string(core::hazard_key(kind));add(id,label(id));
    }
    if(!out.empty()) add(scalar(value)); // Preserve a validated optional empty/reference value.
    (void)doc;
    return out;
}
} // namespace pgp::studio::world_detail
