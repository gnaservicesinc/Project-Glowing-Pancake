#include <pgp/content/founding.hpp>
#include <pgp/core/checked_arithmetic.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::content {
using Json=nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FoundingOption,id,name,detail,capital_cents,monthly_cents,fee_cents,shares_bp,prestige,buzz,family,leg,legal_form)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FormativeScene,id,title,date,text,choices)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FoundingOpeningRules,inherited_staff,cofounder_staff,ceo_equity_bp)
void to_json(Json& j,const FoundingCatalog& c) {
    j={{"families",c.families},{"legs",c.legs},{"routes",c.routes},{"backers",c.backers},{"offices",c.offices},{"researchers",c.researchers},{"scenes",c.scenes},
        {"formation_cents",c.formation_cents},{"compute_monthly_cents",c.compute_monthly_cents},{"founder_salary_cents",c.founder_salary_cents},
        {"prestige_finance_bp",c.prestige_finance_bp},{"buzz_finance_bp",c.buzz_finance_bp},{"maximum_finance_bonus_bp",c.maximum_finance_bonus_bp}};
    if(c.opening) j["opening"]=*c.opening;
}
void from_json(const Json& j,FoundingCatalog& c) {
    j.at("families").get_to(c.families);j.at("legs").get_to(c.legs);j.at("routes").get_to(c.routes);j.at("backers").get_to(c.backers);
    j.at("offices").get_to(c.offices);j.at("researchers").get_to(c.researchers);j.at("scenes").get_to(c.scenes);
    j.at("formation_cents").get_to(c.formation_cents);j.at("compute_monthly_cents").get_to(c.compute_monthly_cents);j.at("founder_salary_cents").get_to(c.founder_salary_cents);
    j.at("prestige_finance_bp").get_to(c.prestige_finance_bp);j.at("buzz_finance_bp").get_to(c.buzz_finance_bp);j.at("maximum_finance_bonus_bp").get_to(c.maximum_finance_bonus_bp);
    if(j.contains("opening")) c.opening=j.at("opening").get<FoundingOpeningRules>();
}
namespace {
[[noreturn]] void invalid(std::string why) {throw std::invalid_argument(std::move(why));}
Json parse(std::string_view bytes) {
    if(bytes.size()>65536) invalid("Founding data exceeds its size limit.");
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
    if(rows.empty() || rows.size()>16) invalid("Founding options need a bounded nonempty list.");
    std::set<std::string> ids;
    for(const auto& r:rows) {
        text(r.id,64);text(r.name);text(r.detail,2048);text(r.family,64,false);text(r.leg,64,false);text(r.legal_form,64,false);
        if(!ids.insert(r.id).second || r.shares_bp>5000 || r.prestige>1000 || r.buzz>1000) invalid("Invalid or duplicate founding option.");
        amount(r.capital_cents);amount(r.monthly_cents);amount(r.fee_cents);
    }
}
void shape(const FoundingSelection& s) {
    text(s.first_name,64);text(s.last_name,64);text(s.gender,64);
    core::validate_portrait(s.appearance);
    if(s.personal_investment_cents < -1 || s.personal_investment_cents>1000000000000) invalid("Choose a bounded personal investment.");
    if(s.gender!="woman" && s.gender!="man" && s.gender!="nonbinary" && s.gender!="unspecified") invalid("Choose a supported gender option.");
    if(s.mode!="campaign") invalid("New companies use the founder campaign.");
    if(s.sociability!="introverted" && s.sociability!="outgoing") invalid("Choose a sociability tendency.");
    if(s.activity!="athletic" && s.activity!="sedentary") invalid("Choose an activity tendency.");
    if(s.disposition!="deliberate" && s.disposition!="impulsive") invalid("Choose a decision-making tendency.");
    if(s.legal_form!="corporation" && s.legal_form!="nonprofit") invalid("Choose a corporation or nonprofit.");
    if(s.backers.size()>6 || s.choices.size()>16) invalid("Too many founding choices.");
    for(const auto* id:{&s.family,&s.leg,&s.route,&s.office,&s.researcher}) text(*id,64);
    for(const auto& [scene,choice]:s.choices) {text(scene,64);text(choice,64);}
    std::set<std::string> backers;
    for(const auto& id:s.backers) if(!backers.insert(id).second) invalid("A backer can contribute only once.");

}
}
FoundingCatalog load_founding_catalog(std::string_view bytes) {
    const auto j=parse(bytes);const auto c=j.get<FoundingCatalog>();
    if(Json(c)!=j) invalid("Unsupported fields in the founding catalog.");
    for(const auto* rows:{&c.families,&c.legs,&c.routes,&c.backers,&c.offices,&c.researchers}) options(*rows);
    amount(c.formation_cents);amount(c.compute_monthly_cents);amount(c.founder_salary_cents);
    if(c.prestige_finance_bp>1000 || c.buzz_finance_bp>1000 || c.maximum_finance_bonus_bp>5000) invalid("Founding reputation effects exceed their bounded terms.");
    if(c.opening && (c.opening->inherited_staff<2 || c.opening->inherited_staff>6 || c.opening->cofounder_staff<1 || c.opening->cofounder_staff>3 || c.opening->ceo_equity_bp>1500)) invalid("Opening company terms exceed their bounds.");
    if(c.scenes.size()<6 || c.scenes.size()>12) invalid("The introduction needs six to twelve formative scenes.");
    std::set<std::string> ids;
    for(const auto& scene:c.scenes) {
        text(scene.id,64);text(scene.title);text(scene.date,10);text(scene.text,4096);options(scene.choices);
        if(!ids.insert(scene.id).second) invalid("Duplicate formative scene.");
    }
    return c;
}
std::string canonical_founding_catalog(const FoundingCatalog& c) {return Json(c).dump();}
const FoundingOption& founding_option(const std::vector<FoundingOption>& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.id==id;});
    if(it==rows.end()) invalid("That founding option is unavailable in this campaign.");
    return *it;
}
std::string founding_ineligibility(const FoundingOption& option,const FoundingSelection& s) {
    if(!option.family.empty() && s.family!=option.family && !(option.family=="rich" && s.leg=="political")) return "Requires a wealthy family at 18.";
    if(!option.leg.empty() && s.leg!=option.leg) return "Requires the "+option.leg+" leg up.";
    if(!option.legal_form.empty() && s.legal_form!=option.legal_form) return "Available to a "+option.legal_form+" institution.";
    return {};
}
FoundingTerms founding_terms(const FoundingCatalog& c,const FoundingSelection& s,bool complete) {
    shape(s);FoundingTerms t;t.nonprofit=s.legal_form=="nonprofit";
    const auto& family=founding_option(c.families,s.family);const auto& leg=founding_option(c.legs,s.leg);
    if(leg.id=="wealthy" && s.family!="rich") invalid("Wealthy Elite requires rich parents.");
    const auto& route=founding_option(c.routes,s.route);const auto& office=founding_option(c.offices,s.office);
    const auto& researcher=founding_option(c.researchers,s.researcher);
    for(const auto* option:{&route,&office,&researcher}) if(const auto why=founding_ineligibility(*option,s);!why.empty()) invalid(why);
    if(s.researcher=="founder" && s.leg!="genius") invalid("Only Actual Genius can supply the first research assignment without hiring a researcher.");
    t.founder_researcher=s.researcher=="founder";
    const bool ceo=s.route=="hired_ceo";
    if(ceo && s.leg!="genius" && s.leg!="political" && s.leg!="wealthy") invalid("This lab considers leadership applications from an Actual Genius or a political or wealthy elite background.");
    {
        if(!c.opening) invalid("Complete your appearance and founding terms.");
        if(s.researcher!=(s.leg=="genius"?"founder":"none")) invalid("Research ability follows the biography; hiring takes place during play.");
        const bool funded=s.route=="investment" || s.route=="mission_director";
        if(s.route!="bootstrap" && s.route!="partnership" && !ceo && !funded) invalid("Choose a current company entry route.");
        if(!funded && !s.backers.empty()) invalid("This entry route does not solicit outside backers.");
        if(complete && funded && s.backers.empty()) invalid("Secure an investor or donor before signing this route.");
        if(s.office!=((ceo || s.route=="partnership")?"shared":"home")) invalid("The starting premises follow your company entry route.");
    }
    t.contributed_cents=core::checked_add(family.capital_cents,leg.capital_cents);
    if(s.leg=="political" && s.family!="rich") t.contributed_cents=core::checked_add(founding_option(c.families,"rich").capital_cents,leg.capital_cents);
    const auto personal=t.contributed_cents;
    t.personal_contribution_cents=ceo?0:s.personal_investment_cents<0?personal:s.personal_investment_cents;
    if(t.personal_contribution_cents>personal) invalid("Your personal investment exceeds your available savings.");
    t.personal_remaining_cents=core::checked_subtract(personal,t.personal_contribution_cents);
    t.contributed_cents=core::checked_add(t.personal_contribution_cents,route.capital_cents);
    t.partner_bp=t.nonprofit?0:route.shares_bp;
    t.paid_setup_cents=core::checked_add(c.formation_cents,core::checked_add(office.fee_cents,researcher.fee_cents));
    t.office_cents=office.monthly_cents;t.payroll_cents=researcher.monthly_cents;
    t.prestige=leg.prestige;t.buzz=leg.buzz;
    for(const auto& scene:c.scenes) {
        const auto choice=s.choices.find(scene.id);
        if(choice==s.choices.end()) {if(complete && s.mode=="campaign") invalid("Complete each formative chapter before founding the company.");continue;}
        const auto& r=founding_option(scene.choices,choice->second);
        t.prestige+=r.prestige;t.buzz+=r.buzz;
    }
    for(const auto& [id,choice]:s.choices) {
        (void)choice;if(std::none_of(c.scenes.begin(),c.scenes.end(),[&](const auto& scene){return scene.id==id;})) invalid("Unknown formative chapter.");
    }
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
        if(s.office=="home") {t.office_cents=0;t.paid_setup_cents=c.formation_cents;}
    }
    if(complete && t.contributed_cents<=t.paid_setup_cents) invalid("Secure funding sufficient to pay the disclosed formation expenses.");
    return t;
}
std::string canonical_founding_selection(const FoundingSelection& s) {
    shape(s);
    Json j={{"mode",s.mode},{"first_name",s.first_name},{"last_name",s.last_name},{"gender",s.gender},
        {"sociability",s.sociability},{"activity",s.activity},{"disposition",s.disposition},
        {"family",s.family},{"leg",s.leg},{"route",s.route},{"legal_form",s.legal_form},
        {"personal_investment_cents",s.personal_investment_cents},{"office",s.office},{"researcher",s.researcher},{"choices",s.choices},{"backers",s.backers}};
    j["appearance"]={{"kit",s.appearance.kit},{"parts",s.appearance.parts},{"style",s.appearance.style}};
    return j.dump();
}
FoundingSelection load_founding_selection(std::string_view bytes) {
    const auto j=parse(bytes);FoundingSelection s;
    s.mode=j.at("mode");s.first_name=j.at("first_name");s.last_name=j.at("last_name");s.gender=j.at("gender");
    s.sociability=j.at("sociability");s.activity=j.at("activity");s.disposition=j.at("disposition");
    s.family=j.at("family");s.leg=j.at("leg");s.route=j.at("route");s.legal_form=j.at("legal_form");
    s.personal_investment_cents=j.at("personal_investment_cents").get<std::int64_t>();
    s.office=j.at("office");s.researcher=j.at("researcher");s.choices=j.at("choices").get<decltype(s.choices)>();s.backers=j.at("backers").get<decltype(s.backers)>();
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
