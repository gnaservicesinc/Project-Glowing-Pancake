#pragma once
#include <pgp/core/calendar.hpp>
#include <pgp/core/mission_conflict.hpp>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace pgp::core {
struct SimulationState;
struct PersonProfile;
struct SocialRules {
    int interval_days=7,weekly_gain=10,shared_gain=2,friction=3,interaction_gain=18;
    int outgoing_threshold=40,private_threshold=400,remote_decay=2,mood_cap=6;
    int city_chance_bp=8000,state_chance_bp=4000,remote_chance_bp=200,remote_ceiling=200;
    int city_gain=12,state_gain=6,remote_gain=1,remote_breakthrough_bp=10,referral_days=90;
    std::array<int,5> thresholds{80,200,400,650,850};
    std::optional<MissionConflictRules> mission_conflict;
    auto operator<=>(const SocialRules&) const=default;
};
struct SocialLink {
    std::string first,second;
    int affinity=0,encounters=0;
    OrdinalDay met_on{0},last_contact{0};
    std::optional<OrdinalDay> last_invited;
    std::string memory;
    bool known=false;
    auto operator<=>(const SocialLink&) const=default;
};
struct SocialState {
    std::uint64_t stream=0,industry_stream=0;
    OrdinalDay next_meeting{0},next_industry_meeting{0};
    std::optional<OrdinalDay> last_outing;
    std::map<std::string,OrdinalDay> referrals;
    std::vector<std::string> founder_traits;
    std::vector<SocialLink> links;
    std::map<std::string,std::vector<std::string>> known_traits;
    std::vector<std::string> hostile_peers{};
    std::optional<MissionConflictState> mission_conflict;
    auto operator<=>(const SocialState&) const=default;
};
struct Socialize {std::string person,activity;};
const SocialRules* social_rules(const SimulationState&);
void validate_social_rules(const SocialRules&);
void initialize_social(SimulationState&,std::uint64_t seed);
void prepare_social(SimulationState&);
void prepare_industry_social(SimulationState&);
std::string social_distance(const SimulationState&,std::string_view,std::string_view);
std::string social_referral_blocker(const SimulationState&,std::string_view);
std::string social_introduction(const SimulationState&,std::string_view);
void socialize(SimulationState&,const Socialize&);
void note_social_contact(SimulationState&,std::string_view person);
void validate_social(const SimulationState&);
std::string social_id(const SimulationState&,std::string_view);
const SocialLink* social_link(const SimulationState&,std::string_view,std::string_view);
std::string friendship_name(const SimulationState&,int affinity,bool coworker);
std::vector<std::string> visible_traits(const SimulationState&,const PersonProfile&,std::string_view identity={});
std::string social_blocker(const SimulationState&,std::string_view);
int friendship_mood(const SimulationState&,std::string_view);
}
