#include <pgp/content/founding.hpp>
#include <pgp/core/checked_arithmetic.hpp>
#include <pgp/core/objectives.hpp>
#include <pgp/core/people.hpp>
#include <pgp/core/random.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <tuple>
#include <stdexcept>

namespace pgp::content {
using Json=nlohmann::json;
void to_json(Json& j,const FoundingOption& o) {
    j={{"id",o.id},{"name",o.name},{"detail",o.detail},{"capital_cents",o.capital_cents},{"monthly_cents",o.monthly_cents},
       {"fee_cents",o.fee_cents},{"shares_bp",o.shares_bp},{"prestige",o.prestige},{"buzz",o.buzz},
       {"family",o.family},{"leg",o.leg},{"legal_form",o.legal_form}};
    if(!o.requires_leg.empty())j["requires_leg"]=o.requires_leg;
    if(!o.requires_trait.empty())j["requires_trait"]=o.requires_trait;
    if(!o.excludes_trait.empty())j["excludes_trait"]=o.excludes_trait;
    if(!o.combo_leg.empty())j["combo_leg"]=o.combo_leg;
    if(o.prestige_delta)j["prestige_delta"]=o.prestige_delta;
    if(o.buzz_delta)j["buzz_delta"]=o.buzz_delta;
    if(o.combo_prestige_delta)j["combo_prestige_delta"]=o.combo_prestige_delta;
    if(o.combo_buzz_delta)j["combo_buzz_delta"]=o.combo_buzz_delta;
    if(o.best_friends_delta)j["best_friends_delta"]=o.best_friends_delta;
    if(o.min_bullies)j["min_bullies"]=o.min_bullies;
    if(o.max_bullies)j["max_bullies"]=o.max_bullies;
    if(o.delay_days)j["delay_days"]=o.delay_days;
    if(o.genius_delay_days)j["genius_delay_days"]=o.genius_delay_days;
    if(o.student_loan_cents)j["student_loan_cents"]=o.student_loan_cents;
    if(o.research_bonus)j["research_bonus"]=o.research_bonus;
    if(o.research_speed_bp)j["research_speed_bp"]=o.research_speed_bp;
    if(o.grants_doctorate)j["grants_doctorate"]=o.grants_doctorate;
    if(!o.education_label.empty())j["education_label"]=o.education_label;
    if(!o.profile_value.empty())j["profile_value"]=o.profile_value;
}
void from_json(const Json& j,FoundingOption& o) {
    j.at("id").get_to(o.id);j.at("name").get_to(o.name);j.at("detail").get_to(o.detail);
    j.at("capital_cents").get_to(o.capital_cents);j.at("monthly_cents").get_to(o.monthly_cents);j.at("fee_cents").get_to(o.fee_cents);
    j.at("shares_bp").get_to(o.shares_bp);j.at("prestige").get_to(o.prestige);j.at("buzz").get_to(o.buzz);
    j.at("family").get_to(o.family);j.at("leg").get_to(o.leg);j.at("legal_form").get_to(o.legal_form);
    if(j.contains("requires_leg"))j.at("requires_leg").get_to(o.requires_leg);
    if(j.contains("requires_trait"))j.at("requires_trait").get_to(o.requires_trait);
    if(j.contains("excludes_trait"))j.at("excludes_trait").get_to(o.excludes_trait);
    if(j.contains("combo_leg"))j.at("combo_leg").get_to(o.combo_leg);
    if(j.contains("prestige_delta"))j.at("prestige_delta").get_to(o.prestige_delta);
    if(j.contains("buzz_delta"))j.at("buzz_delta").get_to(o.buzz_delta);
    if(j.contains("combo_prestige_delta"))j.at("combo_prestige_delta").get_to(o.combo_prestige_delta);
    if(j.contains("combo_buzz_delta"))j.at("combo_buzz_delta").get_to(o.combo_buzz_delta);
    if(j.contains("best_friends_delta"))j.at("best_friends_delta").get_to(o.best_friends_delta);
    if(j.contains("min_bullies"))j.at("min_bullies").get_to(o.min_bullies);
    if(j.contains("max_bullies"))j.at("max_bullies").get_to(o.max_bullies);
    if(j.contains("delay_days"))j.at("delay_days").get_to(o.delay_days);
    if(j.contains("genius_delay_days"))j.at("genius_delay_days").get_to(o.genius_delay_days);
    if(j.contains("student_loan_cents"))j.at("student_loan_cents").get_to(o.student_loan_cents);
    if(j.contains("research_bonus"))j.at("research_bonus").get_to(o.research_bonus);
    if(j.contains("research_speed_bp"))j.at("research_speed_bp").get_to(o.research_speed_bp);
    if(j.contains("grants_doctorate"))j.at("grants_doctorate").get_to(o.grants_doctorate);
    if(j.contains("education_label"))j.at("education_label").get_to(o.education_label);
    if(j.contains("profile_value"))j.at("profile_value").get_to(o.profile_value);
}
void to_json(Json& j,const FormativeScene& s) {
    j={{"id",s.id},{"title",s.title},{"date",s.date},{"text",s.text},{"choices",s.choices}};
    if(!s.pool.empty())j["pool"]=s.pool;if(!s.requires_choice.empty())j["requires_choice"]=s.requires_choice;
    if(!s.section.empty())j["section"]=s.section;
    if(!s.event_kind.empty())j["event_kind"]=s.event_kind;
    if(!s.genius_date.empty())j["genius_date"]=s.genius_date;
    if(!s.prompt.empty())j["prompt"]=s.prompt;
    if(!s.requires_leg.empty())j["requires_leg"]=s.requires_leg;
    if(!s.requires_trait.empty())j["requires_trait"]=s.requires_trait;
    if(!s.excludes_trait.empty())j["excludes_trait"]=s.excludes_trait;
    if(!s.excludes_choice.empty())j["excludes_choice"]=s.excludes_choice;
    if(s.max_selections!=1)j["max_selections"]=s.max_selections;
    if(!s.cap_reduced_when_not.empty())j["cap_reduced_when_not"]=s.cap_reduced_when_not;
    if(!s.profile_field.empty())j["profile_field"]=s.profile_field;
}
void from_json(const Json& j,FormativeScene& s) {
    j.at("id").get_to(s.id);j.at("title").get_to(s.title);j.at("date").get_to(s.date);j.at("text").get_to(s.text);j.at("choices").get_to(s.choices);
    if(j.contains("pool"))j.at("pool").get_to(s.pool);if(j.contains("requires_choice"))j.at("requires_choice").get_to(s.requires_choice);
    if(j.contains("section"))j.at("section").get_to(s.section);
    if(j.contains("event_kind"))j.at("event_kind").get_to(s.event_kind);
    if(j.contains("genius_date"))j.at("genius_date").get_to(s.genius_date);
    if(j.contains("prompt"))j.at("prompt").get_to(s.prompt);
    if(j.contains("requires_leg"))j.at("requires_leg").get_to(s.requires_leg);
    if(j.contains("requires_trait"))j.at("requires_trait").get_to(s.requires_trait);
    if(j.contains("excludes_trait"))j.at("excludes_trait").get_to(s.excludes_trait);
    if(j.contains("excludes_choice"))j.at("excludes_choice").get_to(s.excludes_choice);
    if(j.contains("max_selections"))j.at("max_selections").get_to(s.max_selections);
    if(j.contains("cap_reduced_when_not"))j.at("cap_reduced_when_not").get_to(s.cap_reduced_when_not);
    if(j.contains("profile_field"))j.at("profile_field").get_to(s.profile_field);
}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FormativePool,minimum,maximum)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FoundingOpeningRules,inherited_staff,cofounder_staff,ceo_equity_bp)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FoundingLoan,lender,principal_cents,minimum_investment_cents,monthly_interest_bp,term_days)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FoundingPartner,id,name,pronouns,gender,letter,access,objective,traits,capital_cents,monthly_salary_cents,cash_floor_cents,shares_bp,seats,maximum_dilution_bp)
void to_json(Json& j,const FoundingCatalog& c) {
    j={{"families",c.families},{"legs",c.legs},{"routes",c.routes},{"backers",c.backers},{"offices",c.offices},{"researchers",c.researchers},{"scenes",c.scenes},
        {"formation_cents",c.formation_cents},{"compute_monthly_cents",c.compute_monthly_cents},{"founder_salary_cents",c.founder_salary_cents},
        {"prestige_finance_bp",c.prestige_finance_bp},{"buzz_finance_bp",c.buzz_finance_bp},{"maximum_finance_bonus_bp",c.maximum_finance_bonus_bp}};
    if(!c.trait_rewards.empty())j["trait_rewards"]=c.trait_rewards;
    if(!c.skill_rewards.empty())j["skill_rewards"]=c.skill_rewards;
    if(!c.scene_pools.empty())j["scene_pools"]=c.scene_pools;
    if(c.opening) j["opening"]=*c.opening;
    if(c.startup_loan) j["startup_loan"]=*c.startup_loan;
    if(!c.partners.empty()) j["partners"]=c.partners;
    if(c.queer_community_buzz) j["queer_community_buzz"]=c.queer_community_buzz;
    if(c.queer_community_prestige) j["queer_community_prestige"]=c.queer_community_prestige;
}
void from_json(const Json& j,FoundingCatalog& c) {
    j.at("families").get_to(c.families);j.at("legs").get_to(c.legs);j.at("routes").get_to(c.routes);j.at("backers").get_to(c.backers);
    j.at("offices").get_to(c.offices);j.at("researchers").get_to(c.researchers);j.at("scenes").get_to(c.scenes);
    j.at("formation_cents").get_to(c.formation_cents);j.at("compute_monthly_cents").get_to(c.compute_monthly_cents);j.at("founder_salary_cents").get_to(c.founder_salary_cents);
    j.at("prestige_finance_bp").get_to(c.prestige_finance_bp);j.at("buzz_finance_bp").get_to(c.buzz_finance_bp);j.at("maximum_finance_bonus_bp").get_to(c.maximum_finance_bonus_bp);
    if(j.contains("trait_rewards"))j.at("trait_rewards").get_to(c.trait_rewards);
    if(j.contains("skill_rewards"))j.at("skill_rewards").get_to(c.skill_rewards);
    if(j.contains("scene_pools"))j.at("scene_pools").get_to(c.scene_pools);
    if(j.contains("opening")) c.opening=j.at("opening").get<FoundingOpeningRules>();
    if(j.contains("startup_loan")) c.startup_loan=j.at("startup_loan").get<FoundingLoan>();
    if(j.contains("partners")) c.partners=j.at("partners").get<std::vector<FoundingPartner>>();
    if(j.contains("queer_community_buzz")) c.queer_community_buzz=j.at("queer_community_buzz").get<std::uint32_t>();
    if(j.contains("queer_community_prestige")) c.queer_community_prestige=j.at("queer_community_prestige").get<std::uint32_t>();
}
namespace {
[[noreturn]] void invalid(std::string why) {throw std::invalid_argument(std::move(why));}
Json parse(std::string_view bytes, std::size_t limit = 65536) {
    if(bytes.size()>limit) invalid("Founding data exceeds its size limit.");
    std::vector<std::set<std::string>> keys;
    return Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if(depth>12) invalid("Founding data is nested too deeply.");
        if(event==Json::parse_event_t::object_start) keys.emplace_back();
        if(event==Json::parse_event_t::object_end) keys.pop_back();
        if(event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid("Duplicate founding field.");
        if(event==Json::parse_event_t::value && value.is_number_float()) invalid("Founding values must use exact integers.");
        return true;
    });
}
void text(std::string_view s,std::size_t maximum=128,bool required=true) {
    if(s.size()>maximum || (required && s.find_first_not_of(' ')==s.npos) ||
       std::ranges::any_of(s,[](unsigned char c){return c<32 || c==127;})) invalid("Enter valid text within the displayed length limit.");
    (void)Json(s).dump();
}
void amount(std::int64_t value) {
    if(value<0 || value>100000000000LL) invalid("Founding money is outside its supported range.");
}
void options(const std::vector<FoundingOption>& rows) {
    if(rows.empty() || rows.size()>32) invalid("Founding options need a bounded nonempty list.");
    std::set<std::string> ids;
    for(const auto& r:rows) {
        text(r.id,64);text(r.name);text(r.detail,2048);text(r.family,64,false);text(r.leg,64,false);text(r.legal_form,64,false);
        text(r.requires_leg,64,false);text(r.requires_trait,80,false);text(r.excludes_trait,80,false);text(r.combo_leg,64,false);text(r.education_label,128,false);
        if(!ids.insert(r.id).second || r.shares_bp>5000 || r.prestige>1000 || r.buzz>1000 ||
           r.prestige_delta < -100 || r.prestige_delta > 100 || r.buzz_delta < -100 || r.buzz_delta > 100 ||
           r.combo_prestige_delta < -100 || r.combo_prestige_delta > 100 || r.combo_buzz_delta < -100 || r.combo_buzz_delta > 100 ||
           r.best_friends_delta > 20 || r.min_bullies > r.max_bullies || r.max_bullies > 16 ||
           r.delay_days > 7300 || r.genius_delay_days > 7300 || r.student_loan_cents < 0 || r.student_loan_cents > 100000000000LL ||
           r.research_bonus > 60 || r.research_speed_bp > 5000) invalid("Invalid or duplicate founding option.");
        amount(r.capital_cents);amount(r.monthly_cents);amount(r.fee_cents);
    }
}
void shape(const FoundingSelection& s) {
    text(s.first_name,64);text(s.last_name,64);text(s.gender,64);
    core::validate_portrait(s.appearance);
    if(s.personal_investment_cents < -1 || s.personal_investment_cents>1000000000000) invalid("Choose a bounded personal investment.");
    if(s.gender!="woman" && s.gender!="man" && s.gender!="nonbinary") invalid("Choose a supported gender option.");
    if(s.birth_sex!="female" && s.birth_sex!="male" && s.birth_sex!="indeterminate") invalid("Choose a supported sex at birth.");
    if(std::ranges::find(founder_orientations,s.orientation)==founder_orientations.end()) invalid("Choose a supported orientation option.");
    if(s.mode!="campaign") invalid("New companies use the founder campaign.");
    if(s.sociability!="introverted" && s.sociability!="outgoing") invalid("Choose a sociability tendency.");
    if(s.activity!="athletic" && s.activity!="sedentary") invalid("Choose an activity tendency.");
    if(s.disposition!="deliberate" && s.disposition!="impulsive") invalid("Choose a decision-making tendency.");
    if(s.legal_form!="corporation" && s.legal_form!="nonprofit") invalid("Choose a corporation or nonprofit.");
    if(s.backers.size()>6 || s.choices.size()>128) invalid("Too many founding choices.");
    for(const auto* id:{&s.family,&s.route,&s.office,&s.researcher}) text(*id,64);
    if(s.advantages.size()>32) invalid("Too many early advantages.");
    std::set<std::string> advantages;
    for(const auto& id:s.advantages) {
        text(id,64);
        if(id=="wealthy") invalid("Wealthy Elite follows the family background.");
        if(id=="political" && s.family!="rich") invalid("Child of Political Elite requires a wealthy family.");
        if(!advantages.insert(id).second) invalid("Each early advantage can be selected only once.");
    }
    text(s.partner,64,false);
    for(const auto& [scene,choice]:s.choices) {text(scene,64);text(choice,256);}
    std::set<std::string> backers;
    for(const auto& id:s.backers) if(!backers.insert(id).second) invalid("A backer can contribute only once.");

}
}
bool FoundingSelection::has_advantage(std::string_view id) const {
    return id=="wealthy"?family=="rich":std::ranges::find(advantages,id)!=advantages.end();
}

namespace {
std::pair<std::string_view, std::string_view> split_condition(std::string_view cond) {
    const auto slash=cond.find('/');
    return slash==std::string_view::npos?std::pair{cond,std::string_view{}}:std::pair{cond.substr(0,slash),cond.substr(slash+1)};
}
bool has_matching_trait(const std::vector<std::string>& traits,std::string_view target) {
    return std::ranges::any_of(traits,[&](const auto& trait){return core::trait_matches(trait,target);});
}
bool scene_choice_matches(const FoundingSelection& s,std::string_view cond) {
    const auto [parent,expected]=split_condition(cond);
    const auto found=s.choices.find(parent);
    if(found==s.choices.end())return false;
    const auto tokens=parse_formative_choices(found->second);
    if(expected=="*")return !tokens.empty();
    return std::ranges::find(tokens,expected)!=tokens.end();
}
std::uint64_t pool_salt(std::string_view pool) {
    std::uint64_t hash=UINT64_C(0xcbf29ce484222325);
    for(char ch:pool){hash^=static_cast<unsigned char>(ch);hash*=UINT64_C(0x100000001b3);}
    return hash;
}
template<class T>
void seeded_shuffle(std::vector<T>& rows, core::SplitMix64& rng) {
    for (auto remaining=rows.size();remaining>1;--remaining)
        std::swap(rows[remaining-1],rows[static_cast<std::size_t>(rng.bounded(remaining))]);
}
void append_scene_traits(const FoundingCatalog& c,const FormativeScene& scene,const FoundingSelection& s,std::vector<std::string>& traits_so_far) {
    const auto found=s.choices.find(scene.id);
    if(found==s.choices.end())return;
    for(const auto token:parse_formative_choices(found->second)) {
        const auto it=c.trait_rewards.find(scene.id+"/"+std::string(token));
        if(it==c.trait_rewards.end())continue;
        for(const auto& trait:it->second)
            if(std::ranges::find(traits_so_far,trait)==traits_so_far.end())traits_so_far.push_back(trait);
    }
}
struct FormativePath {
    std::vector<const FormativeScene*> scenes;
    std::vector<std::string> traits_before_stop;
};
FormativePath resolve_formative_path(const FoundingCatalog& c,const FoundingSelection& s,std::string_view stop_before_scene={}) {
    std::map<std::string,std::vector<const FormativeScene*>> pools;
    for(const auto& scene:c.scenes) if(!scene.pool.empty()) pools[scene.pool].push_back(&scene);
    std::set<std::string> selected;
    core::SplitMix64 legacy_rng{s.story_seed};
    for(auto& [pool,rows]:pools) {
        core::SplitMix64 rng{s.story_seed ^ pool_salt(pool)};
        const auto rule=c.scene_pools.find(pool);
        FormativePool limits;
        if(rule!=c.scene_pools.end()) limits=rule->second;
        else if(pool.starts_with("rand_")) {
            // Earlier authored collections used named conventions for sample size.
            limits.minimum=rows.size()>=4?2:1;
            limits.maximum=std::min<std::size_t>(rows.size(),pool=="rand_elementary"||pool=="rand_middle"||pool=="rand_high"?3:2);
        } else {
            selected.insert(rows[static_cast<std::size_t>(legacy_rng.bounded(rows.size()))]->id);
            continue;
        }
        seeded_shuffle(rows,rng);
        const auto count=limits.minimum==limits.maximum?limits.minimum:
            limits.minimum+static_cast<std::size_t>(rng.bounded(limits.maximum-limits.minimum+1));
        for(std::size_t i=0;i<count;++i)selected.insert(rows[i]->id);
    }
    const auto effective_date=[&](const FormativeScene* scene)->const std::string& {
        return (s.has_advantage("genius") && !scene->genius_date.empty())?scene->genius_date:scene->date;
    };
    std::vector<const FormativeScene*> sorted;for(const auto& scene:c.scenes)sorted.push_back(&scene);
    std::ranges::sort(sorted,[&](const auto* a,const auto* b){return std::tie(effective_date(a),a->id)<std::tie(effective_date(b),b->id);});
    FormativePath path;std::set<std::string> active;
    std::vector<std::string> traits_so_far{"Upbringing: "+s.family};
    for(const auto* scene:sorted) {
        if(!scene->pool.empty() && !selected.contains(scene->id))continue;
        if(!scene->requires_leg.empty() && !s.has_advantage(scene->requires_leg))continue;
        if(!scene->requires_trait.empty() && !has_matching_trait(traits_so_far,scene->requires_trait))continue;
        if(!scene->excludes_trait.empty() && has_matching_trait(traits_so_far,scene->excludes_trait))continue;
        if(!scene->requires_choice.empty()) {
            const auto [parent,_]=split_condition(scene->requires_choice);
            if(!active.contains(std::string(parent)) || !scene_choice_matches(s,scene->requires_choice))continue;
        }
        if(!scene->excludes_choice.empty()) {
            const auto [parent,_]=split_condition(scene->excludes_choice);
            if(active.contains(std::string(parent)) && scene_choice_matches(s,scene->excludes_choice))continue;
        }
        if(!stop_before_scene.empty() && scene->id==stop_before_scene)path.traits_before_stop=traits_so_far;
        active.insert(scene->id);
        path.scenes.push_back(scene);
        append_scene_traits(c,*scene,s,traits_so_far);
    }
    return path;
}
}
std::vector<std::string_view> parse_formative_choices(std::string_view value) {
    std::vector<std::string_view> tokens;
    std::size_t start=0;
    while(start<value.size()) {
        const auto comma=value.find(',',start);
        const auto part=value.substr(start,comma==std::string_view::npos?std::string_view::npos:comma-start);
        if(!part.empty())tokens.push_back(part);
        if(comma==std::string_view::npos)break;
        start=comma+1;
    }
    return tokens;
}
std::uint32_t scene_max_selections(const FormativeScene& scene,const FoundingSelection& s) {
    std::uint32_t cap=std::max<std::uint32_t>(1,scene.max_selections);
    if(!scene.cap_reduced_when_not.empty() && cap>1) {
        const auto [parent,_]=split_condition(scene.cap_reduced_when_not);
        if(s.choices.contains(parent) && !scene_choice_matches(s,scene.cap_reduced_when_not))--cap;
    }
    return cap;
}
std::string option_ineligibility(const FoundingOption& option,const FoundingSelection& s,const std::vector<std::string>& traits_so_far) {
    if(const auto base=founding_ineligibility(option,s);!base.empty())return base;
    if(!option.requires_leg.empty() && !s.has_advantage(option.requires_leg))
        return option.requires_leg=="charisma"?"Requires the Charisma advantage.":
               option.requires_leg=="genius"?"Requires the Actual Genius advantage.":
               "Requires the "+option.requires_leg+" advantage.";
    if(!option.requires_trait.empty() && !has_matching_trait(traits_so_far,option.requires_trait))
        return "Requires the "+option.requires_trait+" trait.";
    if(!option.excludes_trait.empty() && has_matching_trait(traits_so_far,option.excludes_trait))
        return option.excludes_trait=="Addict"?"Closed because of the Addict trait.":
               "Unavailable with the "+option.excludes_trait+" trait.";
    return {};
}
std::vector<std::string> formative_traits_before(const FoundingCatalog& c,const FoundingSelection& s,std::string_view scene_id) {
    return resolve_formative_path(c,s,scene_id).traits_before_stop;
}
FoundingCatalog load_founding_catalog(std::string_view bytes) {
    const auto j=parse(bytes,262144);const auto c=j.get<FoundingCatalog>();
    if(Json(c)!=j) invalid("Unsupported fields in the founding catalog.");
    for(const auto* rows:{&c.families,&c.legs,&c.routes,&c.backers,&c.offices,&c.researchers}) options(*rows);
    amount(c.formation_cents);amount(c.compute_monthly_cents);amount(c.founder_salary_cents);
    if(c.prestige_finance_bp>1000 || c.buzz_finance_bp>1000 || c.maximum_finance_bonus_bp>5000 || c.queer_community_buzz>20 || c.queer_community_prestige>10) invalid("Founding reputation effects exceed their bounded terms.");
    if(c.opening && (c.opening->inherited_staff<2 || c.opening->inherited_staff>6 || c.opening->cofounder_staff<1 || c.opening->cofounder_staff>3 || c.opening->ceo_equity_bp>1500)) invalid("Opening company terms exceed their bounds.");
    if(!c.partners.empty()) {
        const auto& route=founding_option(c.routes,"partnership");
        if(!c.opening || c.opening->cofounder_staff!=1 || c.partners.size()>8 || route.capital_cents!=0 || route.shares_bp!=0)
            invalid("Named partnership terms require one working cofounder and no generic contribution.");
        std::set<std::string> ids{"alder"};
        for(const auto& backer:c.backers) ids.insert(backer.id);
        for(const auto& partner:c.partners) {
            text(partner.id,56);text(partner.name);text(partner.pronouns,64);text(partner.letter,512);
            amount(partner.capital_cents);amount(partner.monthly_salary_cents);amount(partner.cash_floor_cents);
            if(!ids.insert(partner.id).second || !core::objective_kind(partner.objective) ||
                (partner.gender!="woman" && partner.gender!="man" && partner.gender!="nonbinary") ||
                (partner.access!="technical" && partner.access!="connected") ||
                partner.capital_cents<=0 || partner.monthly_salary_cents<=0 || partner.monthly_salary_cents>10000000 ||
                partner.cash_floor_cents>1000000000 || !partner.shares_bp || partner.shares_bp>5000 ||
                !partner.seats || partner.seats>3 || partner.maximum_dilution_bp>3000 || partner.traits.empty() || partner.traits.size()>16)
                invalid("A cofounder needs a distinct identity, mission, funded terms and bounded board rights.");
            std::set<std::string> traits;
            for(const auto& trait:partner.traits) {
                text(trait,80);
                if(!traits.insert(trait).second) invalid("Cofounder traits must be distinct.");
            }
        }
    }
    const auto loan_route=std::ranges::find(c.routes,std::string_view{"bank_loan"},&FoundingOption::id);
    if((loan_route!=c.routes.end())!=c.startup_loan.has_value()) invalid("The startup loan must have both an entry route and lending terms.");
    if(c.startup_loan) {
        const auto& loan=*c.startup_loan;
        text(loan.lender,64);amount(loan.principal_cents);amount(loan.minimum_investment_cents);
        if(!c.opening || loan.principal_cents<=0 || loan.minimum_investment_cents<=0 ||
           loan.monthly_interest_bp<1 || loan.monthly_interest_bp>500 || loan.term_days<90 || loan.term_days>3650 ||
           loan_route->capital_cents || loan_route->shares_bp || loan_route->fee_cents || loan_route->monthly_cents)
            invalid("A startup loan must have bounded debt terms without an equity contribution.");
    }
    if(c.scenes.size()<6 || c.scenes.size()>128) invalid("The introduction needs six to one hundred twenty-eight formative scenes.");
    std::set<std::string> ids;
    std::map<std::string,std::size_t,std::less<>> pool_sizes;
    for(const auto& scene:c.scenes) {
        text(scene.id,64);text(scene.title);text(scene.date,10);text(scene.text,4096);options(scene.choices);
        text(scene.section,32,false);text(scene.event_kind,32,false);
        if(!scene.genius_date.empty())text(scene.genius_date,10);
        text(scene.prompt,256,false);text(scene.requires_leg,64,false);
        text(scene.requires_trait,80,false);text(scene.excludes_trait,80,false);text(scene.excludes_choice,128,false);text(scene.cap_reduced_when_not,128,false);
        if(scene.max_selections==0 || scene.max_selections>8) invalid("Formative multi-select limit must stay between 1 and 8.");
        if(!scene.section.empty() && scene.section!="elementary" && scene.section!="middle" && scene.section!="high" &&
           scene.section!="graduation" && scene.section!="undergrad" && scene.section!="grad")
            invalid("Unknown school section.");
        if(!scene.event_kind.empty() && scene.event_kind!="static" && scene.event_kind!="conditional" && scene.event_kind!="random")
            invalid("Unknown school event kind.");
        if(!ids.insert(scene.id).second) invalid("Duplicate formative scene.");
        if(!scene.pool.empty()) ++pool_sizes[scene.pool];
        if(!scene.profile_field.empty() && (scene.max_selections!=1 ||
           (scene.profile_field!="sociability" && scene.profile_field!="activity" && scene.profile_field!="disposition")))
            invalid("A formative profile field needs a supported tendency and a single choice.");
        for(const auto& option:scene.choices) {
            const auto& value=option.profile_value;
            if(value.empty()) continue;
            if((scene.profile_field=="sociability" && (value=="outgoing" || value=="introverted")) ||
               (scene.profile_field=="activity" && (value=="athletic" || value=="sedentary")) ||
               (scene.profile_field=="disposition" && (value=="deliberate" || value=="impulsive"))) continue;
            invalid("A formative profile value must match its tendency.");
        }
    }
    for(const auto& [id,limits]:c.scene_pools) {
        text(id,48);
        if(!pool_sizes.contains(id) || limits.minimum<1 || limits.minimum>limits.maximum || limits.maximum>pool_sizes.at(id))
            invalid("A formative pool needs a valid draw range within its candidate count.");
    }
    const auto validate_ref=[&](const FormativeScene& scene, std::string_view cond, bool allow_wildcard) {
        const auto [parent_id,choice_id]=split_condition(cond);
        if(parent_id.empty() || choice_id.empty())invalid("A condition needs scene/choice.");
        const auto parent=std::ranges::find(c.scenes,parent_id,&FormativeScene::id);
        if(parent==c.scenes.end() || parent->date>=scene.date)invalid("A formative condition must follow an earlier scene.");
        if(!(allow_wildcard && choice_id=="*"))(void)founding_option(parent->choices,choice_id);
        return parent;
    };
    for(const auto& scene:c.scenes) {
        if(!scene.pool.empty() && !c.scene_pools.contains(scene.pool) && scene.pool!="activity" && scene.pool!="camp" &&
           !(scene.pool.starts_with("rand_") && scene.pool.size()<=48))
            invalid("Unknown formative scene pool.");
        if(!scene.requires_choice.empty()) {
            const auto parent=validate_ref(scene,scene.requires_choice,true);
            (void)parent;
            if(!scene.pool.empty() && !c.scene_pools.contains(scene.pool) && !scene.pool.starts_with("rand_"))
                invalid("Follow-ups inherit selection from their parent chain.");
        }
        if(!scene.excludes_choice.empty()) (void)validate_ref(scene,scene.excludes_choice,false);
        if(!scene.cap_reduced_when_not.empty()) (void)validate_ref(scene,scene.cap_reduced_when_not,false);
    }
    for(const auto* table:{&c.skill_rewards,&c.trait_rewards}) {
    if(table->size()>512)invalid("Too many formative rewards.");
    for(const auto& [key,rewards]:*table) {
        const auto slash=key.find('/');if(slash==key.npos || rewards.empty() || rewards.size()>3)invalid("Formative rewards need a scene/choice and one to three rewards.");
        const auto scene=std::ranges::find(c.scenes,key.substr(0,slash),&FormativeScene::id);
        if(scene==c.scenes.end())invalid("Formative reward names an unknown scene.");
        (void)founding_option(scene->choices,key.substr(slash+1));
        std::set<std::string> unique;for(const auto& id:rewards){text(id,table==&c.trait_rewards?80:64);if(!unique.insert(id).second)invalid("Duplicate formative reward.");}
    }
    }
    return c;
}
std::vector<const FormativeScene*> formative_scenes(const FoundingCatalog& c,const FoundingSelection& s) {
    return resolve_formative_path(c,s).scenes;
}
std::vector<const FoundingOption*> formative_options(const FormativeScene& scene,const FoundingSelection& s) {
    std::vector<const FoundingOption*> choices;
    choices.reserve(scene.choices.size());
    for(const auto& option:scene.choices) choices.push_back(&option);
    core::SplitMix64 rng{s.story_seed ^ pool_salt(scene.id) ^ UINT64_C(0x43484f49434553)};
    seeded_shuffle(choices,rng);
    return choices;
}
std::vector<std::string> formative_skills(const FoundingCatalog& c,const FoundingSelection& s) {
    std::set<std::string> result;
    for(const auto* scene:formative_scenes(c,s)) {
        const auto choice=s.choices.find(scene->id);if(choice==s.choices.end())continue;
        for(const auto token:parse_formative_choices(choice->second))
            if(const auto it=c.skill_rewards.find(scene->id+"/"+std::string(token));it!=c.skill_rewards.end())
                result.insert(it->second.begin(),it->second.end());
    }
    return {result.begin(),result.end()};
}
std::vector<std::string> formative_traits(const FoundingCatalog& c,const FoundingSelection& s) {
    std::vector<std::string> result;
    for(const auto* scene:formative_scenes(c,s)) append_scene_traits(c,*scene,s,result);
    return result;
}
std::string canonical_founding_catalog(const FoundingCatalog& c) {return Json(c).dump();}
const FoundingOption& founding_option(const std::vector<FoundingOption>& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.id==id;});
    if(it==rows.end()) invalid("That founding option is unavailable in this campaign.");
    return *it;
}
std::string founding_ineligibility(const FoundingOption& option,const FoundingSelection& s) {
    if(!option.family.empty() && s.family!=option.family) return option.family=="rich"?"Requires a wealthy family.":"Requires the "+option.family+" family background.";
    if(!option.leg.empty() && !s.has_advantage(option.leg)) return "Requires the "+option.leg+" advantage.";
    if(!option.legal_form.empty() && s.legal_form!=option.legal_form) return "Available to a "+option.legal_form+" institution.";
    return {};
}
const FoundingPartner* founding_partner(const FoundingCatalog& catalog,std::string_view id) {
    const auto found=std::ranges::find(catalog.partners,id,&FoundingPartner::id);
    return found==catalog.partners.end()?nullptr:&*found;
}
std::string partner_ineligibility(const FoundingPartner& partner,const FoundingSelection& selection) {
    if(partner.access=="technical" && !selection.has_advantage("genius")) return "Requires the Actual Genius advantage.";
    if(partner.access=="connected" && !selection.has_advantage("wealthy")) return "Requires a wealthy family.";
    return {};
}
bool founding_ceo_eligible(const FoundingSelection& s) {
    return s.has_advantage("wealthy") || s.has_advantage("genius");
}
std::int64_t founding_savings(const FoundingCatalog& c,const FoundingSelection& s) {
    auto savings=founding_option(c.families,s.family).capital_cents;
    for(const auto& option:c.legs) if(s.has_advantage(option.id))
        savings=core::checked_add(savings,option.capital_cents);
    return savings;
}
bool transgender_founder(const FoundingSelection& s) {
    return (s.birth_sex=="female" && s.gender!="woman") || (s.birth_sex=="male" && s.gender!="man");
}
bool queer_founder(const FoundingSelection& s) {
    return transgender_founder(s) || s.gender=="nonbinary" ||
        (s.orientation!="straight" && s.orientation!="questioning");
}
FoundingTerms founding_formative_terms(const FoundingCatalog& c,const FoundingSelection& s,bool complete) {
    FoundingTerms t;
    const bool community=queer_founder(s);
    std::int64_t prestige_signed=community?c.queer_community_prestige:0;
    std::int64_t buzz_signed=community?c.queer_community_buzz:0;
    for(const auto& option:c.legs) if(s.has_advantage(option.id)) {
        prestige_signed+=option.prestige;
        buzz_signed+=option.buzz;
    }
    std::uint32_t research_speed_discount_bp=0;
    std::vector<std::string> traits_so_far{"Upbringing: "+s.family};
    std::vector<std::string> credentials;
    for(const auto* selected_scene:formative_scenes(c,s)) {
        const auto& scene=*selected_scene;
        const auto choice=s.choices.find(scene.id);
        if(choice==s.choices.end()) {if(complete && s.mode=="campaign") invalid("Complete each formative chapter before founding the company.");continue;}
        const auto tokens=parse_formative_choices(choice->second);
        const auto max_sel=scene_max_selections(scene,s);
        if(tokens.empty() || tokens.size()>max_sel) invalid("Choose within the allowed selection limit for this chapter.");
        std::set<std::string_view> seen_tokens;
        core::SplitMix64 bully_rng{s.story_seed ^ UINT64_C(0x42554C4C49455331) ^ pool_salt(scene.id)};
        for(const auto token:tokens) {
            if(!seen_tokens.insert(token).second) invalid("Duplicate selection in formative chapter.");
            const auto& r=founding_option(scene.choices,token);
            if(const auto why=option_ineligibility(r,s,traits_so_far);!why.empty()) invalid(why);
            const bool combo=!r.combo_leg.empty() && s.has_advantage(r.combo_leg);
            prestige_signed+=combo?r.combo_prestige_delta:(r.prestige_delta!=0?r.prestige_delta:static_cast<std::int32_t>(r.prestige));
            buzz_signed+=combo?r.combo_buzz_delta:(r.buzz_delta!=0?r.buzz_delta:static_cast<std::int32_t>(r.buzz));
            t.best_friends+=r.best_friends_delta;
            if(r.max_bullies>0) {
                const auto lo=std::min(r.min_bullies,r.max_bullies);
                const auto span=r.max_bullies-lo+1;
                t.bully_count+=lo+(span>1?static_cast<std::uint32_t>(bully_rng.bounded(span)):0U);
            }
            t.entry_delay_days+=(s.has_advantage("genius")?r.genius_delay_days:r.delay_days);
            t.student_loan_cents=core::checked_add(t.student_loan_cents,r.student_loan_cents);
            if(r.grants_doctorate) ++t.doctorate_count;
            t.research_bonus+=r.research_bonus;
            research_speed_discount_bp+=r.research_speed_bp;
            if(!r.education_label.empty() && std::ranges::find(credentials,r.education_label)==credentials.end())
                credentials.push_back(r.education_label);
        }
        append_scene_traits(c,scene,s,traits_so_far);
    }
    for(const auto& [id,choice]:s.choices) {
        (void)choice;if(std::none_of(c.scenes.begin(),c.scenes.end(),[&](const auto& scene){return scene.id==id;})) invalid("Unknown formative chapter.");
    }
    t.prestige=static_cast<std::uint32_t>(std::max<std::int64_t>(0,prestige_signed));
    t.buzz=static_cast<std::uint32_t>(std::max<std::int64_t>(0,buzz_signed));
    t.research_work_scale_bp=10000U-std::min<std::uint32_t>(5000U,research_speed_discount_bp);
    for(const auto& cred:credentials) {
        const auto next_size=t.education_label.empty()?cred.size():t.education_label.size()+3+cred.size();
        if(next_size<=128) {
            if(!t.education_label.empty()) t.education_label+=" · ";
            t.education_label+=cred;
        }
    }
    return t;
}

FoundingTerms founding_terms(const FoundingCatalog& c,const FoundingSelection& s,bool complete) {
    shape(s);
    for(const auto& id:s.advantages) {
        const auto& option=founding_option(c.legs,id);
        if(const auto why=founding_ineligibility(option,s);!why.empty()) invalid(why);
    }
    const auto& route=founding_option(c.routes,s.route);const auto& office=founding_option(c.offices,s.office);
    const auto& researcher=founding_option(c.researchers,s.researcher);
    for(const auto* option:{&route,&office}) if(const auto why=founding_ineligibility(*option,s);!why.empty()) invalid(why);
    if(researcher.id!="founder") if(const auto why=founding_ineligibility(researcher,s);!why.empty()) invalid(why);
    auto t=founding_formative_terms(c,s,complete);
    t.nonprofit=s.legal_form=="nonprofit";
    const bool can_research=s.has_advantage("genius") || t.doctorate_count>0;
    if(s.researcher=="founder" && !can_research) invalid("Only Actual Genius or a doctoral graduate can supply the first research assignment without hiring a researcher.");
    t.founder_researcher=s.researcher=="founder";
    const bool ceo=s.route=="hired_ceo";
    const bool borrowing=s.route=="bank_loan";
    const auto* partner=founding_partner(c,s.partner);
    if(!s.partner.empty() && (s.route!="partnership" || !partner)) invalid("Choose a known cofounder for the partnership route.");
    if(complete && s.route=="partnership" && !c.partners.empty() && !partner) invalid("Agree terms with a cofounder before signing.");
    if(partner) if(const auto reason=partner_ineligibility(*partner,s);!reason.empty()) invalid(reason);
    if(ceo && !founding_ceo_eligible(s)) invalid("This lab considers leadership applications from an Actual Genius or a wealthy family.");
    {
        if(!c.opening) invalid("Complete your appearance and founding terms.");
        if(complete && s.researcher!=(can_research?"founder":"none")) invalid("Research ability follows the biography; hiring takes place during play.");
        const bool funded=s.route=="investment" || s.route=="mission_director";
        if(s.route!="bootstrap" && s.route!="partnership" && !ceo && !funded && !borrowing) invalid("Choose a current company entry route.");
        if(!funded && !s.backers.empty()) invalid("This entry route does not solicit outside backers.");
        if(complete && funded && s.backers.empty()) invalid("Secure an investor or donor before signing this route.");
        if(s.office!=((ceo || s.route=="partnership")?"shared":"home")) invalid("The starting premises follow your company entry route.");
    }
    const auto personal=founding_savings(c,s);
    t.personal_contribution_cents=ceo?0:s.personal_investment_cents<0?personal:s.personal_investment_cents;
    if(t.personal_contribution_cents>personal) invalid("Your personal investment exceeds your available savings.");
    if(borrowing) {
        if(!c.startup_loan) invalid("Startup lending is unavailable.");
        if(complete && s.personal_investment_cents<c.startup_loan->minimum_investment_cents)
            invalid("The bank requires a larger explicit personal investment before it can approve this loan.");
        t.startup_loan=c.startup_loan;
    }
    t.personal_remaining_cents=core::checked_subtract(personal,t.personal_contribution_cents);
    t.contributed_cents=core::checked_add(t.personal_contribution_cents,partner?partner->capital_cents:route.capital_cents);
    t.partner_bp=t.nonprofit?0:partner?partner->shares_bp:route.shares_bp;
    t.paid_setup_cents=core::checked_add(c.formation_cents,core::checked_add(office.fee_cents,researcher.fee_cents));
    t.office_cents=office.monthly_cents;t.payroll_cents=researcher.monthly_cents;
    std::uint32_t sold=t.partner_bp;
    for(const auto& id:s.backers) {
        const auto& backer=founding_option(c.backers,id);
        if(const auto why=founding_ineligibility(backer,s);!why.empty()) invalid(why);
        const auto premium=std::min(c.maximum_finance_bonus_bp,t.prestige*c.prestige_finance_bp+t.buzz*c.buzz_finance_bp);
        const auto offered=core::mul_div_floor(static_cast<std::uint64_t>(backer.capital_cents),10000+premium,10000).quotient;
        t.contributed_cents=core::checked_add(t.contributed_cents,static_cast<std::int64_t>(offered));
        if(!t.nonprofit) {sold+=backer.shares_bp;t.backer_shares.emplace(id,backer.shares_bp);}
    }
    if(sold>8500) invalid("These terms leave too little founding control; choose fewer equity backers.");
    t.founder_bp=t.nonprofit?0:10000-sold;
    {
        // A nonprofit founder must secure the named gift; the entry route
        // itself is not an unrequested endowment.
        if(s.route=="mission_director") t.contributed_cents=core::checked_subtract(t.contributed_cents,route.capital_cents);
        if(ceo) {
            t.contributed_cents=route.capital_cents;
            t.partner_bp=10000-c.opening->ceo_equity_bp;t.founder_bp=c.opening->ceo_equity_bp;
            t.paid_setup_cents=0;
        }
        t.opening_staff=(t.founder_researcher?1U:0U)+(ceo?c.opening->inherited_staff:s.route=="partnership"?c.opening->cofounder_staff:0U);
        const auto paid_staff=t.opening_staff-(t.founder_researcher?1U:0U);
        t.payroll_cents=static_cast<std::int64_t>(core::mul_div_floor(static_cast<std::uint64_t>(founding_option(c.researchers,"first_hire").monthly_cents),paid_staff,1).quotient);
        if(partner)t.payroll_cents=partner->monthly_salary_cents;
        if(s.office=="home") {t.office_cents=0;t.paid_setup_cents=c.formation_cents;}
    }
    const auto borrowed=t.startup_loan?t.startup_loan->principal_cents:0;
    if(complete && core::checked_add(t.contributed_cents,borrowed)<=t.paid_setup_cents) invalid("Secure funding sufficient to pay the disclosed formation expenses.");
    return t;
}
std::string canonical_founding_selection(const FoundingSelection& s) {
    shape(s);
    Json j={{"mode",s.mode},{"first_name",s.first_name},{"last_name",s.last_name},{"gender",s.gender},
        {"sociability",s.sociability},{"activity",s.activity},{"disposition",s.disposition},
        {"family",s.family},{"advantages",s.advantages},{"route",s.route},{"legal_form",s.legal_form},
        {"personal_investment_cents",s.personal_investment_cents},{"office",s.office},{"researcher",s.researcher},{"choices",s.choices},{"backers",s.backers}};
    j["appearance"]={{"kit",s.appearance.kit},{"parts",s.appearance.parts},{"style",s.appearance.style}};
    if(!s.partner.empty()) j["partner"]=s.partner;
    if(s.birth_sex!="indeterminate") j["birth_sex"]=s.birth_sex;
    j["orientation"]=s.orientation;
    j["story_seed"]=s.story_seed;
    return j.dump();
}
FoundingSelection load_founding_selection(std::string_view bytes) {
    const auto j=parse(bytes);FoundingSelection s;
    s.mode=j.at("mode");s.first_name=j.at("first_name");s.last_name=j.at("last_name");s.gender=j.at("gender");
    s.sociability=j.at("sociability");s.activity=j.at("activity");s.disposition=j.at("disposition");
    s.family=j.at("family");s.advantages=j.at("advantages").get<decltype(s.advantages)>();s.route=j.at("route");s.legal_form=j.at("legal_form");
    s.personal_investment_cents=j.at("personal_investment_cents").get<std::int64_t>();
    s.office=j.at("office");s.researcher=j.at("researcher");s.choices=j.at("choices").get<decltype(s.choices)>();s.backers=j.at("backers").get<decltype(s.backers)>();
    if(j.contains("partner")) s.partner=j.at("partner").get<std::string>();
    if(j.contains("birth_sex")) s.birth_sex=j.at("birth_sex").get<std::string>();
    if(j.contains("orientation")) s.orientation=j.at("orientation").get<std::string>();
    s.story_seed=j.at("story_seed").get<std::uint64_t>();
    {
        const auto& a=j.at("appearance");core::PortraitRecipe p;
        if(a.at("kit")!=1 || a.at("parts").size()!=p.parts.size()) invalid("Unsupported portrait kit.");
        for(std::size_t i=0;i<p.parts.size();++i) {
            const auto& v=a.at("parts").at(i);if(!v.is_number_integer() || v<0 || v>=core::portrait_part_counts[i]) invalid("Invalid portrait part.");p.parts[i]=v.get<std::uint8_t>();
        }
        p.style=a.at("style").get<decltype(p.style)>();core::validate_portrait(p);
        s.appearance=p;
    }
    shape(s);if(Json::parse(canonical_founding_selection(s))!=j) invalid("Unsupported or out-of-range founding fields.");return s;
}
}
