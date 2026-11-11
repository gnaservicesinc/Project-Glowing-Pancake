#pragma once
#include <pgp/core/calendar.hpp>
#include <pgp/core/paged_history.hpp>
#include <map>
#include <set>
#include <cstdint>
#include <string_view>
#include <optional>
#include <string>
#include <vector>
namespace pgp::core {
struct PersonProfile;
struct RomanticPreferences {
    bool interested=true,public_orientation=false;
    std::vector<std::string> genders;
    int minimum_friendship=400;
    auto operator<=>(const RomanticPreferences&) const=default;
};
struct RomanticProfile {
    OrdinalDay born_on{0};
    RomanticPreferences preferences;
    auto operator<=>(const RomanticProfile&) const=default;
};
struct RomanceRules {
    int interval_days=7,friendship=400,demisexual_friendship=650;
    int dating_days=180,engagement_days=90,engagement_affection=650,marriage_affection=750;
    int initial_affection=400,date_gain=35,date_cooldown=7,proposal_cooldown=90,minimum_dates=3;
    int ask_acceptance_bp=7500,proposal_acceptance_bp=9000,npc_start_bp=250,npc_progress_bp=2500;
    int city_gain=20,state_gain=12,remote_decay=8,neglect_days=28,strain_affection=150,strain_grace_days=30;
    int interested_chance_bp=8500,public_orientation_chance_bp=2500;
    auto operator<=>(const RomanceRules&) const=default;
};
struct RomanceBond {
    std::uint32_t id=0;
    std::string first,second;
    OrdinalDay started_on{0},last_time_together{0};
    std::optional<OrdinalDay> engaged_on,married_on,ended_on,strained_on,last_proposal;
    int affection=0,dates=0;
    bool known=false;
    std::string memory,end_reason;
    auto operator<=>(const RomanceBond&) const=default;
};
struct RomanticReply {
    std::string person,action,response;
    OrdinalDay day{0};
    std::uint32_t relationship=0;
    bool accepted=false;
    auto operator<=>(const RomanticReply&) const=default;
};
inline constexpr std::size_t romance_history_limit=16384;
struct RomanceState {
    RomanticProfile founder;
    std::uint64_t stream=0;
    std::uint32_t next_id=1;
    OrdinalDay next_review{0};
    std::optional<OrdinalDay> last_action;
    std::set<std::string> known_identities;
    std::map<std::uint32_t,RomanceBond> active;
    PagedHistory<RomanceBond,16> history;
    PagedHistory<RomanticReply,16> replies;
    auto operator<=>(const RomanceState&) const=default;
};
void validate_romance_rules(const RomanceRules&);
void validate_romantic_profile(const RomanticProfile&,int birth_year);
[[nodiscard]] bool adult_on(const RomanticProfile&,OrdinalDay);
[[nodiscard]] RomanticPreferences generate_romantic_preferences(std::string_view gender,std::string_view orientation,std::uint64_t seed,const RomanceRules&);
void initialize_romantic_profile(PersonProfile&,std::uint64_t seed,const RomanceRules&);
[[nodiscard]] bool mutual_romantic_interest(std::string_view,const RomanticPreferences&,std::string_view,const RomanticPreferences&,int friendship);
[[nodiscard]] std::string romance_status(const RomanceBond&);
}
