#include <pgp/core/people.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
const PeopleRules& rules(const SimulationState& s) {
    if(!s.management || !s.management->catalog.people || !s.management->people || !s.capacity || !s.accounts)
        invalid("This company has no HQ recruitment office.");
    return *s.management->catalog.people;
}
std::uint32_t draw(SplitMix64& rng,std::uint32_t upper) {return static_cast<std::uint32_t>(rng.bounded(upper));}
Applicant applicant(SimulationState& s,bool opening) {
    auto& p=*s.management->people;const auto& r=rules(s);
    SplitMix64 rng{p.stream};
    PersonProfile person;
    person.id="employee/"+std::to_string(p.next_id++);
    person.name=r.first_names[draw(rng,static_cast<std::uint32_t>(r.first_names.size()))]+" "+r.last_names[draw(rng,static_cast<std::uint32_t>(r.last_names.size()))];
    // Identity draws are fixed-count and do not alter quality by gender/pronouns.
    const auto gender=draw(rng,3);
    person.gender=std::array{"woman","man","nonbinary"}[gender];
    person.pronouns=std::array{"she/her","he/him","they/them"}[gender];
    const auto age=22+draw(rng,34);
    person.birth_year=from_ordinal(s.date).year-static_cast<int>(age);
    person.experience=draw(rng,age-20);
    person.education=std::array{"Apprenticeship and independent study","Bachelor's degree","Master's degree","Doctorate"}[draw(rng,4)];
    person.role=std::array{"Researcher","Software engineer","Evaluation researcher"}[draw(rng,3)];
    person.applied_on=s.date;
    person.portrait=make_portrait(p.stream,person.id);
    person.preferred_mission=std::array{"research","public benefit","commercial"}[draw(rng,3)];
    person.mission_fit=(person.preferred_mission==p.mission?70U:40U)+draw(rng,31);
    const auto publications=s.lab?static_cast<std::uint32_t>(std::min<std::size_t>(s.lab->publications.size(),10)):0;
    const auto quality=static_cast<std::int64_t>(r.base_skill)+
        (p.offered_pay.value()-10000)*r.pay_skill_weight/10000+
        std::min<std::uint32_t>(10,p.founding_prestige+publications)*r.prestige_skill_weight+
        static_cast<std::int64_t>(person.mission_fit)*r.mission_skill_weight/100;
    const auto skill=[&]{return static_cast<std::uint32_t>(std::clamp(quality+static_cast<std::int64_t>(draw(rng,2*r.skill_jitter+1))-r.skill_jitter,INT64_C(1),INT64_C(100)));};
    person.research=skill();person.engineering=skill();person.evaluation=skill();
    // Bios are optional. No empty layout slot is needed for an absent biography.
    if(draw(rng,3)==0) person.bio="Drawn to "+person.preferred_mission+" work and a team where their contributions can be seen.";
    const auto salary=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(r.monthly_salary.value()),static_cast<std::uint64_t>(p.offered_pay.value()),10000)};
    Applicant result{person,opening?Cents{0}:salary,opening?Cents{0}:r.signing_cost,
        BasisPoints{r.maximum_evaluation_credit.value()*person.evaluation/100},{}};
    if(!opening && r.recruitment) initialize_recruitment(result,*r.recruitment,rng);
    for(const auto& group:r.trait_groups) result.profile.traits.push_back(group[draw(rng,static_cast<std::uint32_t>(group.size()))]);
    p.stream=rng.state();return result;
}
void recruit(SimulationState& s) {
    auto& p=*s.management->people;
    const auto& r=rules(s);
    std::erase_if(p.applicants,[&](const auto& a){const auto status=recruitment_status(a,s.date);return status!="waiting" && status!="counter" && status!="agreed";});
    for(auto i=p.applicants.size();i<r.candidates;++i) p.applicants.push_back(applicant(s,false));
    p.next_recruitment=advance_days(s.date,r.recruitment_interval);
}
void profile_valid(const PersonProfile& p,OrdinalDay now) {
    validate_portrait(p.portrait);
    if(p.traits.size()>16) invalid("Too many personal traits.");
    std::set<std::string> unique;
    for(const auto& trait:p.traits) if(trait.empty() || trait.size()>80 || trait.find('\0')!=trait.npos || !unique.insert(trait).second) invalid("Traits must have unique bounded names.");
    const auto year=from_ordinal(p.applied_on).year;
    if(!p.id.starts_with("employee/") || p.id.size()>64 || p.name.empty() || p.name.size()>128 ||
        p.gender.empty() || p.gender.size()>32 || p.pronouns.empty() || p.pronouns.size()>64 || p.education.size()>128 || p.role.size()>64 || p.bio.size()>512 ||
        p.birth_year>year-18 || p.birth_year<year-80 || p.experience>static_cast<unsigned>(year-p.birth_year-18) ||
        p.research>100 || p.engineering>100 || p.evaluation>100 || p.mission_fit>100 || p.applied_on>now ||
        (p.preferred_mission!="research" && p.preferred_mission!="public benefit" && p.preferred_mission!="commercial"))
        invalid("Employee profile has an invalid identity, age, experience or skill.");
    for(const auto* value:{&p.id,&p.name,&p.gender,&p.pronouns,&p.education,&p.role,&p.bio})
        if(value->find('\0')!=std::string::npos) invalid("Employee text cannot contain null characters.");
}
}
void validate_people_rules(const PeopleRules& r) {
    if(r.dismissal && r.dismissal->severance_months>6) invalid("Dismissal severance must be between zero and six months of salary.");
    if(r.recruitment) validate_recruitment_rules(*r.recruitment);
    std::set<std::pair<std::string,std::string>> audience_pairs;
    const auto identity=[](std::string_view value){return value=="woman" || value=="man" || value=="nonbinary";};
    for(const auto& row:r.founder_audiences) {
        if(!identity(row.founder) || !identity(row.audience) || !audience_pairs.emplace(row.founder,row.audience).second ||
            row.salary_bp < -500 || row.salary_bp>500 || row.mood < -5 || row.mood>5 ||
            (!r.recruitment && row.salary_bp) || (!r.mood && row.mood) || (!row.salary_bp && !row.mood))
            invalid("Founder audience rules require unique gender pairs and bounded reactions.");
    }
    if(r.trait_groups.size()>12) invalid("Too many trait categories.");
    std::set<std::string> trait_names;
    for(const auto& group:r.trait_groups) {
        if(group.empty() || group.size()>16) invalid("Trait groups need bounded choices.");
        for(const auto& trait:group) if(trait.empty() || trait.size()>80 || trait.find('\0')!=trait.npos || !trait_names.insert(trait).second) invalid("Trait IDs must be distinct across categories.");
    }
    if(r.mood) {
        const auto& mood=*r.mood;
        if(mood.pay>50 || mood.company_prestige>30 || mood.ceo_prestige>30 || mood.role>30 ||
           mood.coworkers>30 || mood.amenity_cap>30 || mood.temperaments.size()>16 || mood.amenities.size()>7)
            invalid("Workplace mood weights exceed their bounds.");
        for(const auto& [name,points]:mood.temperaments)
            if(!trait_names.contains("Temperament: "+name) || points < -15 || points>15)
                invalid("Temperament effects must use authored traits and bounded points.");
        for(const auto& [kind,points]:mood.amenities)
            if(!hq_amenity_kind(kind) || points==0 || points>mood.amenity_cap)
                invalid("Amenity mood effects need a usable furnishing and bounded points.");
    }
    if(r.first_names.empty() || r.first_names.size()>512 || r.last_names.empty() || r.last_names.size()>512 ||
        r.maximum_hq<1 || r.maximum_hq>256 || r.candidates<1 || r.candidates>12 ||
        r.recruitment_interval<Days{7} || r.recruitment_interval>Days{365} || r.onboarding>Days{90} ||
        r.monthly_salary<Cents{10000} || r.monthly_salary>Cents{10000000} || r.signing_cost>Cents{10000000} || r.signing_cost<Cents{0} ||
        r.base_skill>100 || r.skill_jitter>30 || r.pay_skill_weight>50 || r.prestige_skill_weight>10 || r.mission_skill_weight>30 || r.maximum_evaluation_credit>BasisPoints{2500})
        invalid("HQ recruitment rules exceed their authored bounds.");
    for(const auto* names:{&r.first_names,&r.last_names}) for(const auto& name:*names)
        if(name.empty() || name.size()>48 || name.find('\0')!=std::string::npos) invalid("Name pool entry is empty or oversized.");
}
void initialize_people(SimulationState& s,std::uint64_t seed,std::string founder_name,std::string founder_pronouns,bool founder_researcher,std::uint32_t prestige,std::string mission) {
    if(!s.management || !s.management->catalog.people) return;
    validate_people_rules(*s.management->catalog.people);
    if(!s.capacity || !s.accounts || s.capacity->available_staff>s.management->catalog.people->maximum_hq)
        invalid("Opening HQ staff exceed the detailed roster capacity.");
    PeopleState state;SplitMix64 derivation{seed ^ UINT64_C(0x50454f504c455631)};
    state.stream=derivation.next();state.starts_on=s.date;state.founding_prestige=std::min(10U,prestige);
    state.mission=std::move(mission);
    s.management->people=std::move(state);
    for(std::uint32_t i=0;i<s.capacity->available_staff;++i) {
        auto terms=applicant(s,true);
        if(i==0 && founder_researcher && !founder_name.empty()) {
            terms.profile.name=std::move(founder_name);terms.profile.pronouns=std::move(founder_pronouns);
            terms.profile.gender="Founder identity";terms.profile.role="Founder / researcher";
            terms.profile.birth_year=1997;terms.profile.experience=0;terms.profile.education="Founder's formative experience";terms.profile.bio.clear();
        }
        s.management->people->employees.push_back({std::move(terms),s.date,s.date,true,{}});
    }
    initialize_individual_payroll(s);
    for(const auto& offer:s.management->catalog.specialists) {
        auto person=applicant(s,true).profile;person.name=offer.name;person.role="Evaluation specialist";
        s.management->people->known_people.emplace(offer.id,std::move(person));
    }
    if(s.management->catalog.governance) for(const auto& offer:s.management->catalog.governance->executives) {
        auto person=applicant(s,true).profile;person.name=offer.name;person.role=offer.role;person.bio=offer.priorities;
        s.management->people->known_people.emplace(offer.id,std::move(person));
    }
    recruit(s);
}
void set_recruiting_terms(SimulationState& s,const SetRecruitingTerms& action) {
    (void)rules(s);
    if(action.pay<SalaryRate{7500} || action.pay>SalaryRate{15000} ||
        (action.mission!="research" && action.mission!="public benefit" && action.mission!="commercial"))
        invalid("Choose pay between 75% and 150% and a supported recruiting mission.");
    s.management->people->offered_pay=action.pay;s.management->people->mission=action.mission;
    // Changing recruiting terms affects the next cohort, not existing discussions.
}
void hire_employee(SimulationState& s,std::string_view id,Cents protected_cash,std::uint32_t desk) {
    const auto& r=rules(s);auto& p=*s.management->people;
    const auto it=std::ranges::find(p.applicants,id,[](const auto& row){return row.profile.id;});
    if(it==p.applicants.end()) invalid("This applicant is no longer available. Review the current cohort.");
    if(p.employees.size()>=r.maximum_hq) invalid("HQ has reached its individual staff limit. Branch operators are hired with their facilities.");
    Applicant accepted=*it;
    if(r.recruitment) {
        if(recruitment_status(*it,s.date)!="agreed") invalid("Agree compensation with the applicant before hiring.");
        if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->headquarters || !desk) invalid("Choose a delivered, furnished desk for this person.");
        const auto& items=s.infrastructure->workplace->headquarters->items;
        const auto seat=std::ranges::find(items,desk,&HQEquipment::id);
        if(seat==items.end() || !seat->person.empty()) invalid("Choose an unassigned furnished desk.");
        const auto terms=agreed_employment(*it);accepted.salary=terms.first;accepted.signing=terms.second;accepted.recruitment->hired_on=s.date;
    }
    if(checked_unit_subtract(s.cash,protected_cash)<accepted.signing) invalid("Available cash does not cover the signing cost.");
    const auto ready=advance_days(s.date,r.onboarding);
    if(ready>=s.end_date) invalid("Onboarding would finish after this campaign's work ends.");
    if(accepted.signing>Cents{0}) record_cash(*s.accounts,s.date,Cents{-accepted.signing.value()},Account::expenses,"Employee recruitment: "+accepted.profile.name,accepted.profile.id);
    add_recurring(*s.accounts,{accepted.profile.id,accepted.salary,s.date,std::nullopt,"Employee salary: "+accepted.profile.name});
    p.employees.push_back({std::move(accepted),s.date,ready,false,{}});p.applicants.erase(it);
    if(r.recruitment) manage_headquarters(s,AssignHQDesk{desk,p.employees.back().terms.profile.id},protected_cash);
    s.cash=accounts_report(*s.accounts).cash;
    if(ready==s.date) {const auto& employee=p.employees.back();add_staff_capacity(*s.capacity,s.date,employee.terms.profile.id,1,employee.terms.evaluation_credit);}
}
void prepare_people(SimulationState& s) {
    if(!s.management || !s.management->people) return;
    auto& p=*s.management->people;
    for(const auto& employee:p.employees) if(!employee.opening && !employee.departed_on && employee.ready_on==s.date &&
        std::ranges::none_of(s.capacity->staff_additions,[&](const auto& row){return row.id==employee.terms.profile.id;}))
        add_staff_capacity(*s.capacity,s.date,employee.terms.profile.id,1,employee.terms.evaluation_credit);
    prepare_recruitment(s);
    if(s.date>=p.next_recruitment && (rules(s).recruitment || s.capacity->available_staff>0)) recruit(s);
}
void withdraw_people(SimulationState& s) {
    if(!s.management || !s.management->people) return;
    for(auto& employee:s.management->people->employees) if(!employee.departed_on) employee.departed_on=s.date;
    s.management->people->applicants.clear();
}
const FounderAudienceRule* founder_audience_on(const SimulationState& s,std::string_view audience,OrdinalDay day) {
    if(!s.management || !s.management->people || !s.succession) return nullptr;
    const auto& people=*s.management->people;
    if(people.founder_gender.empty()) return nullptr;
    const auto& rows=s.management->catalog.people->founder_audiences;
    const auto rule=std::ranges::find_if(rows,[&](const auto& row){return row.founder==people.founder_gender && row.audience==audience;});
    if(rule==rows.end()) return nullptr;
    const auto& terms=s.succession->terms;
    if(std::ranges::none_of(terms,[&](const auto& term){return term.person=="founder" && term.starts_on<=day && (!term.ended_on || day<*term.ended_on);}))
        return nullptr;
    return &*rule;
}
void validate_people(const SimulationState& s,bool deep) {
    if(!s.management) return;
    const auto& m=*s.management;
    if(m.people.has_value()!=m.catalog.people.has_value()) invalid("HQ people and recruitment rules must agree.");
    if(!m.people) return;
    const auto& r=rules(s);const auto& p=*m.people;
    if(p.employees.size()>r.maximum_hq || p.applicants.size()>r.candidates || p.next_id==0 || p.next_id>1000000 ||
        p.offered_pay<SalaryRate{7500} || p.offered_pay>SalaryRate{15000} || p.starts_on>s.date || p.next_recruitment<p.starts_on ||
        p.next_recruitment>advance_days(s.date,r.recruitment_interval) || p.founding_prestige>10 ||
        (!p.founder_gender.empty() && p.founder_gender!="woman" && p.founder_gender!="man" && p.founder_gender!="nonbinary" && p.founder_gender!="unspecified") ||
        (p.mission!="research" && p.mission!="public benefit" && p.mission!="commercial")) invalid("HQ roster or recruitment state is outside its bounds.");
    std::set<std::string> ids;
    const auto check=[&](const Applicant& a){profile_valid(a.profile,s.date);if(!ids.insert(a.profile.id).second || a.salary<Cents{0} || a.salary>Cents{r.recruitment?30000000:15000000} ||
        a.signing<Cents{0} || a.signing>Cents{r.signing_cost.value()*(r.recruitment?r.recruitment->maximum_signing_multiple:1)} || a.evaluation_credit!=BasisPoints{r.maximum_evaluation_credit.value()*a.profile.evaluation/100}) invalid("Applicant identity, pay or evaluation evidence changed.");};
    for(const auto& a:p.applicants) {check(a);if(r.recruitment.has_value()!=a.recruitment.has_value()) invalid("Current applicants need their compensation discussions.");validate_recruitment(a,s,false);}
    const auto known=[&](const auto& offer) {
        const auto it=p.known_people.find(offer.id);
        if(it==p.known_people.end() || it->second.name!=offer.name) invalid("Authored hiring offer has lost its stable identity.");
        profile_valid(it->second,s.date);if(!ids.insert(it->second.id).second) invalid("Duplicate person identity.");
    };
    for(const auto& offer:m.catalog.specialists) known(offer);
    if(m.catalog.governance) for(const auto& offer:m.catalog.governance->executives) known(offer);
    if(p.known_people.size()!=m.catalog.specialists.size()+(m.catalog.governance?m.catalog.governance->executives.size():0)) invalid("Unknown saved hiring identity.");
    std::size_t opening=0,added=0;
    for(const auto& employee:p.employees) {
        check(employee.terms);
        if(employee.joined_on<p.starts_on || employee.joined_on>s.date || employee.ready_on<employee.joined_on || employee.terms.profile.applied_on>employee.joined_on ||
            (employee.departed_on && (*employee.departed_on<employee.joined_on || *employee.departed_on>s.date))) invalid("Employee hiring dates are inconsistent.");
        if(employee.opening) {++opening;if(employee.ready_on!=p.starts_on || employee.joined_on!=p.starts_on || employee.terms.salary!=Cents{0}) invalid("Opening staff must retain their existing payroll.");continue;}
        const auto minimum_salary=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(r.monthly_salary.value()),7500,10000)};
        const auto maximum_salary=Cents{mul_div_round_half_up(static_cast<std::uint64_t>(r.monthly_salary.value()),15000,10000)};
        if(r.recruitment) {
            if(!employee.terms.recruitment || employee.terms.recruitment->hired_on!=employee.joined_on) invalid("A hire needs its signed compensation history.");
            validate_recruitment(employee.terms,s,true);
        } else if(employee.terms.salary<minimum_salary || employee.terms.salary>maximum_salary || employee.terms.signing!=r.signing_cost) invalid("Employee pay is outside the authored offer range.");
        const auto& id=employee.terms.profile.id;
        const auto salary=std::ranges::find(s.accounts->recurring,id,&RecurringObligation::id);
        const auto hire=std::ranges::find(s.capacity->staff_additions,id,&StaffAddition::id);
        const bool ready=employee.ready_on<=s.date && (!employee.departed_on || *employee.departed_on>=employee.ready_on);
        if(ready) ++added;
        if(salary==s.accounts->recurring.end() || salary->monthly_amount!=employee.terms.salary || salary->starts_on!=employee.joined_on || salary->ends_before!=employee.departed_on ||
            employee.ready_on!=advance_days(employee.joined_on,r.onboarding) || ready!=(hire!=s.capacity->staff_additions.end()) ||
            (ready && (hire->day!=employee.ready_on || hire->people!=1 || hire->evaluation_credit!=employee.terms.evaluation_credit))) invalid("Employee payroll, onboarding and available capacity differ.");
    }
    if(opening!=opening_staff_count(*s.capacity) || static_cast<std::size_t>(std::ranges::count_if(s.capacity->staff_additions,[](const auto& row){return row.id.starts_with("employee/");}))!=added)
        invalid("Individual HQ people do not reconcile with their staffing capacity.");
    validate_people_contacts(s);
    if(deep) {validate_employee_departures(s);validate_contact_funding(s);}
}
}
