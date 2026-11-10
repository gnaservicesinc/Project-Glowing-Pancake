#include <pgp/content/collection.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::content {
namespace {
core::Cents scaled(core::Cents value,std::uint32_t percent) {
    return core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(value.value()),percent,100).quotient};
}
}
const CampaignDifficulty* scenario_difficulty(const ScenarioDefinition& s,std::string_view id) {
    if(id.empty()) return nullptr;
    const auto it=std::find_if(s.difficulties.begin(),s.difficulties.end(),[&](const auto& d){return d.id==id;});
    if(it==s.difficulties.end()) throw std::invalid_argument("This scenario does not offer that difficulty");
    return &*it;
}
std::optional<core::ManagementCatalog> scenario_management(const ScenarioDefinition& s,std::string_view difficulty,const FoundingSelection* founding) {
    auto result=s.management;const auto* d=scenario_difficulty(s,difficulty);
    if(!result) return result;
    if(d) {
        for(auto& offer:result->financing) offer.proceeds=scaled(offer.proceeds,d->financing_percent);
        if(result->governance) for(auto& offer:result->governance->financing) offer.proceeds=scaled(offer.proceeds,d->financing_percent);
    }
    if(founding) {
        if(!s.founding) throw std::invalid_argument("This scenario has no founder introduction.");
        const auto terms=founding_terms(*s.founding,*founding);
        if(!difficulty.empty()) throw std::invalid_argument("Campaign conditions follow the founding choices, without a difficulty setting.");
        // New founders have no opening compute source. The prototype's
        // scheduled lease interruptions cannot refer to an unbought lease.
        result->setbacks.clear();
        const auto premium=std::min(s.founding->maximum_finance_bonus_bp,
            terms.prestige*s.founding->prestige_finance_bp+terms.buzz*s.founding->buzz_finance_bp);
        const auto reputation_offer=[&](auto& offer) {
            if(offer.acquired.value()>0) offer.proceeds=core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(offer.proceeds.value()),10000+premium,10000).quotient};
        };
        for(auto& offer:result->financing) reputation_offer(offer);
        if(result->governance) for(auto& offer:result->governance->financing) reputation_offer(offer);
        result->scoring.opening_founder_share=core::BasisPoints{terms.founder_bp};
        if(result->succession) {
            result->succession->founder_birth_year=1997;
            result->succession->personal_cash=core::Cents{terms.personal_remaining_cents};
            result->succession->founder_salary=core::Cents{s.founding->founder_salary_cents};
            result->succession->chair_salary=core::Cents{s.founding->founder_salary_cents/2};
        }
        if(result->governance) {
            auto& board=result->governance->board;
            board.erase(std::remove_if(board.begin(),board.end(),[&](const auto& b) {
                return !b.player && b.id!="board.independent" && !(b.id=="board.cofounder" && terms.partner_bp>0);
            }),board.end());
            if(terms.partner_bp>0)
                for(auto& member:board) if(member.id=="board.cofounder") member.name=founding->route=="hired_ceo"?"Existing company owners":"Founding partner";
            for(const auto& id:founding->backers) {
                const auto& backer=founding_option(s.founding->backers,id);
                board.push_back({"board."+id,backer.name,1,false,core::Cents{0},core::BasisPoints{1500}});
            }
            auto& governance=*result->governance;
            std::erase_if(governance.financing,[&](const auto& offer){return terms.nonprofit?offer.acquired.value()>0:offer.donation;});
            if(governance.contact_finance) {
                std::erase_if(governance.contact_finance->offers,[&](const auto& offer){return std::ranges::none_of(governance.financing,[&](const auto& finance){return finance.id==offer.terms;});});
                if(governance.contact_finance->offers.empty())governance.contact_finance.reset();
            }
        }
        if(terms.nonprofit) {
            result->corporate_tax.reset();
            std::erase_if(result->financing,[](const auto& f){return f.term.value()==0;});
            if(result->governance) {
                auto& g=*result->governance;
                g.public_listing.reset();
                for(auto& e:g.executives) {e.hybrid_grant=core::BasisPoints{0};e.hybrid_salary=e.cash_salary;}
            }
        }
    }
    core::validate_management_catalog(*result);
    return result;
}
std::map<std::string,core::BasisPoints,std::less<>> scenario_ownership(const ScenarioDefinition& s,const FoundingSelection* founding) {
    if(!founding) return s.ownership;
    if(!s.founding) throw std::invalid_argument("This scenario has no founding terms.");
    const auto t=founding_terms(*s.founding,*founding);
    std::map<std::string,core::BasisPoints,std::less<>> result;
    result.emplace("founder_trust",core::BasisPoints{t.founder_bp});
    result.emplace("option_pool",core::BasisPoints{0});
    if(t.nonprofit) result.emplace("institution_endowment",core::BasisPoints{10000});
    if(t.partner_bp>0) result.emplace(founding->route=="hired_ceo"?"existing_owners":"cofounder",core::BasisPoints{t.partner_bp});
    for(const auto& [id,shares]:t.backer_shares) if(shares>0) result.emplace(id,core::BasisPoints{shares});
    return result;
}
std::optional<CompetitionSetup> scenario_competition(const ScenarioDefinition& s,std::string_view difficulty) {
    auto result=s.competition;const auto* d=scenario_difficulty(s,difficulty);
    if(result && d) result->review_interval=d->rival_review_interval;
    return result;
}
ScenarioOpening scenario_opening(const ScenarioDefinition& s,std::string_view background,std::string_view owner,std::string_view difficulty,const FoundingSelection* founding) {
    const auto* d=scenario_difficulty(s,difficulty);
    ScenarioOpening result{s.cash,s.team_people,s.monthly_commitments,s.business_offers,{}};
    if(s.leased_capacity.value()>0) result.capacity_sources.push_back({"lease.initial","cloud.pool.initial",std::string(owner),
        "supplier.compute.initial",core::CapacitySourceKind::leased,s.leased_capacity,s.leased_capacity,0,"general",s.lease_energy_included});
    if(founding) {
        if(!s.founding || !background.empty()) throw std::invalid_argument("Choose the new founding terms without an old background package.");
        const auto terms=founding_terms(*s.founding,*founding);
        if(!difficulty.empty()) throw std::invalid_argument("Campaign conditions follow the founding choices, without a difficulty setting.");
        result.cash=core::Cents{terms.contributed_cents};
        result.setup_cost=core::Cents{terms.paid_setup_cents};
        if(terms.startup_loan) {
            const auto& loan=*terms.startup_loan;
            const auto due=core::advance_days(s.starts_on,core::Days{loan.term_days});
            if(due>=s.ends_before) throw std::invalid_argument("The startup loan must mature within the campaign.");
            result.startup_debt=core::Debt{std::string{startup_debt_id},core::Cents{loan.principal_cents},
                core::BasisPoints{loan.monthly_interest_bp},s.starts_on,due,0,{}};
        }
        result.team_people=terms.opening_staff;
        result.capacity_sources.clear();
        result.monthly_commitments.clear();
        result.monthly_commitments.emplace("office_and_admin",core::Cents{terms.office_cents});
        result.monthly_commitments.emplace("compute_lease",core::Cents{0});
        result.monthly_commitments.emplace("payroll_with_benefits",core::Cents{terms.payroll_cents});
        return result;
    }
    if(background.empty()) {if(d) result.cash=scaled(result.cash,d->starting_cash_percent);return result;}
    const auto it=std::find_if(s.backgrounds.begin(),s.backgrounds.end(),[&](const auto& b){return b.id==background;});
    if(it==s.backgrounds.end()) throw std::invalid_argument("This scenario does not offer that founder background");
    const auto& b=*it;
    result.cash=core::checked_unit_add(result.cash,b.cash_adjustment);
    const auto staff=core::checked_add(result.team_people,b.staff_adjustment);
    if(result.cash.value()<0 || staff<1 || staff>1000000) throw std::invalid_argument("Founder package exceeds opening cash or staff bounds");
    result.team_people=static_cast<std::uint32_t>(staff);
    if(b.monthly_payroll_adjustment.value()!=0) {
        const auto base=result.monthly_commitments.find("payroll_with_benefits");
        const auto payroll=core::checked_unit_add(base==result.monthly_commitments.end()?core::Cents{0}:base->second,b.monthly_payroll_adjustment);
        if(payroll.value()<0) throw std::invalid_argument("Founder package makes opening payroll negative");
        result.monthly_commitments.insert_or_assign("payroll_with_benefits",payroll);
    }
    if(b.independent_capacity.value()>0) {
        result.capacity_sources.push_back({"lease.founder","cloud.pool.founder",std::string(owner),"supplier.founder_contact",
            core::CapacitySourceKind::leased,b.independent_capacity,b.independent_capacity,0,"general",true});
        result.monthly_commitments.insert_or_assign("founder_compute",b.monthly_compute_cost);
    }
    for(auto& offer:result.business_offers) if(offer.kind!=core::BusinessOfferKind::compute_lease && offer.available_from==s.starts_on) {
        offer.available_from=core::advance_days(offer.available_from,b.commercial_delay);
        if(offer.available_from>=offer.available_before) throw std::invalid_argument("Founder contact delay consumes an offer window");
    }
    for(const auto& offer:b.contacts) {
        if(offer.available_from<s.starts_on || offer.available_before>s.ends_before) throw std::invalid_argument("Founder contact window is outside the campaign");
        result.business_offers.push_back(offer);
    }
    std::sort(result.business_offers.begin(),result.business_offers.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    core::validate_business_offer_catalog(result.business_offers);
    (void)core::opening_capacity(result.capacity_sources,result.team_people,s.starts_on,std::string(owner));
    if(d) result.cash=scaled(result.cash,d->starting_cash_percent);
    return result;
}
}
