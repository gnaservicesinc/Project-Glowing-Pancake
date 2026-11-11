#include <pgp/core/romance.hpp>
#include <pgp/core/people.hpp>
#include <pgp/core/random.hpp>
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
namespace pgp::core {
void validate_romance_rules(const RomanceRules& r) {
    if(r.interval_days<7 || r.interval_days>30 || r.friendship<200 || r.friendship>850 || r.demisexual_friendship<r.friendship || r.demisexual_friendship>1000 ||
       r.dating_days<30 || r.dating_days>3650 || r.engagement_days<7 || r.engagement_days>3650 || r.engagement_affection<400 || r.engagement_affection>1000 ||
       r.marriage_affection<r.engagement_affection || r.marriage_affection>1000 || r.initial_affection<200 || r.initial_affection>600 || r.date_gain<1 || r.date_gain>100 ||
       r.date_cooldown<7 || r.date_cooldown>30 || r.proposal_cooldown<30 || r.proposal_cooldown>365 || r.minimum_dates<1 || r.minimum_dates>26 || r.city_gain<1 || r.city_gain>50 ||
       r.state_gain<1 || r.state_gain>r.city_gain || r.remote_decay<1 || r.remote_decay>30 || r.neglect_days<14 || r.neglect_days>90 ||
       r.strain_affection<0 || r.strain_affection>300 || r.strain_grace_days<14 || r.strain_grace_days>180)throw std::invalid_argument("Romance needs bounded trust, time, affection and consent rules.");
    for(const auto chance:{r.ask_acceptance_bp,r.proposal_acceptance_bp,r.npc_start_bp,r.npc_progress_bp,r.interested_chance_bp,r.public_orientation_chance_bp})
        if(chance<0 || chance>10000)throw std::invalid_argument("Romance chances must be basis-point values.");
}
void validate_romantic_profile(const RomanticProfile& p,int birth_year) {
    const auto& preferences=p.preferences;
    if(from_ordinal(p.born_on).year!=birth_year || preferences.genders.empty() || preferences.genders.size()>3 || preferences.minimum_friendship<200 || preferences.minimum_friendship>1000)
        throw std::invalid_argument("Romantic preferences need a birth record, explicit attraction and bounded trust.");
    std::set<std::string> unique;
    for(const auto& gender:preferences.genders)if((gender!="woman" && gender!="man" && gender!="nonbinary") || !unique.insert(gender).second)throw std::invalid_argument("Choose distinct supported romantic attraction categories.");
}
bool adult_on(const RomanticProfile& p,OrdinalDay day) {
    auto birth=from_ordinal(p.born_on);birth.year+=18;birth.day=std::min(birth.day,days_in_month(birth.year,birth.month));return to_ordinal(birth)<=day;
}
RomanticPreferences generate_romantic_preferences(std::string_view gender,std::string_view orientation,std::uint64_t seed,const RomanceRules& r) {
    SplitMix64 rng{seed^UINT64_C(0x524f4d414e544943)};RomanticPreferences p;
    p.interested=rng.bounded(10000)<static_cast<std::uint64_t>(r.interested_chance_bp);
    p.public_orientation=rng.bounded(10000)<static_cast<std::uint64_t>(r.public_orientation_chance_bp);
    p.minimum_friendship=orientation=="demisexual"?r.demisexual_friendship:r.friendship;
    // Romantic attraction is an explicit saved preference, separate from
    // sexual orientation. Asexual people can want romantic relationships.
    if(orientation=="straight" && (gender=="woman" || gender=="man"))p.genders={gender=="woman"?"man":"woman"};
    else if(orientation=="gay_or_lesbian" && (gender=="woman" || gender=="man"))p.genders={std::string{gender}};
    else if(orientation=="asexual" || orientation=="questioning")p.genders={std::array{"woman","man","nonbinary"}[rng.bounded(3)]};
    else p.genders={"woman","man","nonbinary"};
    if(p.genders.size()==1 && p.genders.front()!="nonbinary" && rng.bounded(4)==0)p.genders.push_back("nonbinary");
    return p;
}
void initialize_romantic_profile(PersonProfile& p,std::uint64_t seed,const RomanceRules& r) {
    std::uint64_t key=seed;for(char c:p.id)key=(key^static_cast<unsigned char>(c))*UINT64_C(1099511628211);
    SplitMix64 birthday{key^UINT64_C(0x4249525448444159)};const auto month=static_cast<unsigned>(birthday.bounded(12)+1);
    const auto day=static_cast<unsigned>(birthday.bounded(days_in_month(p.birth_year,month))+1);
    p.romantic=RomanticProfile{to_ordinal({p.birth_year,month,day}),generate_romantic_preferences(p.gender,p.orientation,key,r)};
}
bool mutual_romantic_interest(std::string_view first,const RomanticPreferences& a,std::string_view second,const RomanticPreferences& b,int friendship) {
    return a.interested && b.interested && friendship>=std::max(a.minimum_friendship,b.minimum_friendship) &&
        std::ranges::find(a.genders,second)!=a.genders.end() && std::ranges::find(b.genders,first)!=b.genders.end();
}
std::string romance_status(const RomanceBond& b) {
    if(b.ended_on)return b.married_on?"ex_spouse":"ex_partner";
    return b.married_on?"spouse":b.engaged_on?"engaged":"dating";
}
}
