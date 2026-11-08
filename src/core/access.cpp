#include <pgp/core/access.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
#include <tuple>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
bool present(const SimulationState& s) {return s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->access.has_value();}
const AccessRules& rules(const SimulationState& s) {if(!present(s) || !s.infrastructure->catalog.workplace->access) invalid("No liaison office plan is available in this scenario.");return *s.infrastructure->catalog.workplace->access;}
const LiaisonOfficeOffer& offer(const SimulationState& s,std::string_view id) {const auto& list=rules(s).offices;const auto it=std::ranges::find(list,id,&LiaisonOfficeOffer::id);if(it==list.end()) invalid("Choose an available overseas office.");return *it;}
std::string key(std::uint64_t id) {return "office.liaison/"+std::to_string(id);}
const RecurringObligation* obligation(const SimulationState& s,const LiaisonOffice& o) {const auto it=std::ranges::find(s.accounts->recurring,key(o.id),&RecurringObligation::id);return it==s.accounts->recurring.end()?nullptr:&*it;}
bool proof(const SimulationState& s,OrdinalDay day,std::string_view cause) {
    const auto& r=rules(s);
    if(cause=="staff") return s.capacity && available_staff_on(*s.capacity,day)>=r.productive_staff;
    if(cause=="revenue" && s.accounts) {
        if(day==s.date) return accounts_report(*s.accounts).revenue>=r.earned_revenue;
        Cents revenue{0};for(const auto& p:s.accounts->postings) if(p.day<=day) {
            if(p.credit==Account::revenue) revenue=checked_unit_add(revenue,p.amount);
            if(p.debit==Account::revenue) revenue=checked_unit_subtract(revenue,p.amount);
        }
        return revenue>=r.earned_revenue;
    }
    if(cause=="publications") return s.lab && std::ranges::count_if(s.lab->publications,[&](const auto& p){return p.day<=day;})>=r.publications;
    return false;
}
std::string qualifies(const SimulationState& s,OrdinalDay day) {
    for(const auto* cause:{"staff","revenue","publications"}) if(proof(s,day,cause)) return cause;return {};
}
}
void validate_access_rules(const AccessRules& r) {
    if(r.productive_staff<2 || r.productive_staff>1000 || r.publications<1 || r.publications>100 || r.earned_revenue<Cents{10000} || r.earned_revenue>Cents{100000000000} || r.offices.empty() || r.offices.size()>7) invalid("National access thresholds or overseas office catalog are out of bounds.");
    const std::set<std::string> regions{"region.canada","region.europe","region.china","region.india","region.asia_pacific","region.africa_middle_east","region.latin_america"};std::set<std::string> ids,used;
    for(const auto& o:r.offices) if(o.id.empty() || o.id.size()>48 || o.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._")!=std::string::npos || !ids.insert(o.id).second || !regions.contains(o.region) || !used.insert(o.region).second ||
        o.name.empty() || o.name.size()>96 || o.name.find('\0')!=std::string::npos || o.setup<Cents{10000} || o.setup>Cents{100000000} || o.monthly<Cents{10000} || o.monthly>Cents{10000000} || o.lead<Days{1} || o.lead>Days{365} || o.people<1 || o.people>32) invalid("An overseas office has invalid identity, region, setup, payroll or opening terms.");
}
void initialize_access(SimulationState& s,bool connected) {
    if(!s.infrastructure || !s.infrastructure->catalog.workplace || !s.infrastructure->catalog.workplace->access) return;
    s.infrastructure->workplace->access=AccessState{connected,{}, {}};prepare_access(s);
}
bool prepare_access(SimulationState& s) {
    if(!present(s)) return false;auto& a=*s.infrastructure->workplace->access;bool changed=false;
    if(!a.national) {
        const auto cause=a.connected_background?std::string{"background"}:qualifies(s,s.date);
        if(!cause.empty()) {
            NationalAccess n{s.date,cause};
            if(cause=="staff") {n.staff_additions=s.capacity->staff_additions.size();n.staff_departures=s.capacity->staff_departures.size();}
            if(cause=="revenue") n.postings=s.accounts->postings.size();
            if(cause=="publications") n.publications=s.lab->publications.size();
            a.national=std::move(n);changed=true;
        }
    }
    if(!a.first_overseas_opened) for(const auto& o:a.offices) if(liaison_office_open(s,o,s.date)) {a.first_overseas_opened=s.date;changed=true;break;}
    return changed;
}
bool national_access(const SimulationState& s) {return !present(s) || s.infrastructure->workplace->access->national.has_value();}
bool liaison_office_open(const SimulationState& s,const LiaisonOffice& o,OrdinalDay day) {
    const auto* rent=obligation(s,o);return o.ready_on<=day && (!o.closed_on || *o.closed_on>day) && rent && (!rent->ends_before || *rent->ends_before>day);
}
bool world_access(const SimulationState& s) {return !present(s) || s.infrastructure->workplace->access->first_overseas_opened.has_value();}
std::string regional_contact_block(const SimulationState& s,std::string_view region) {
    if(region=="region.us") return national_access(s)?"":"National contacts open through your founding background, productive staff, earned revenue or published research. Public information and responses to existing obligations remain available.";
    return world_access(s)?"":"World-level contacts open when your first overseas liaison office begins operating. Public regional information remains available.";
}
void open_liaison_office(SimulationState& s,std::string_view id,Cents protected_cash) {
    const auto& model=offer(s,id);auto& list=s.infrastructure->workplace->access->offices;
    if(list.size()>=64) invalid("The overseas office history is full.");
    for(const auto& o:list) if(offer(s,o.offer).region==model.region && !o.closed_on) {const auto* rent=obligation(s,o);if(rent && (!rent->ends_before || *rent->ends_before>s.date)) invalid("An office in this region is already opening or operating.");}
    const auto ready=advance_days(s.date,model.lead);if(ready>=s.end_date || checked_unit_subtract(s.cash,protected_cash)<model.setup) invalid("The office must open before the campaign ends and fit available company funds.");
    const auto next=list.size()+1;record_cash(*s.accounts,s.date,Cents{-model.setup.value()},Account::expenses,"Overseas office setup: "+model.name,key(next));
    add_recurring(*s.accounts,{key(next),model.monthly,ready,{},"Liaison office payroll and rent: "+model.name});
    list.push_back({next,model.id,s.date,ready,{}});s.cash=accounts_report(*s.accounts).cash;
}
void close_liaison_office(SimulationState& s,std::uint64_t id) {
    (void)rules(s);auto& list=s.infrastructure->workplace->access->offices;const auto it=std::ranges::find(list,id,&LiaisonOffice::id);
    if(it==list.end() || it->closed_on) invalid("Choose an owned office that has not closed.");
    schedule_recurring_end(*s.accounts,s.date,key(id),std::max(s.date,it->ready_on));it->closed_on=s.date;
}
void validate_access(const SimulationState& s,bool deep) {
    const auto& w=*s.infrastructure->workplace;const auto& catalog=*s.infrastructure->catalog.workplace;
    if(w.access.has_value()!=catalog.access.has_value()) invalid("Saved national and overseas access differs from its authored rules.");if(!w.access) return;
    const auto& a=*w.access;if(a.offices.size()>64) invalid("Overseas office history exceeds its bound.");
    if(a.connected_background && (!a.national || a.national->cause!="background")) invalid("Connected founders retain their opening national contacts.");
    if(a.national) {
        const auto& n=*a.national;if(n.day<w.starts_on || n.day>s.date || (n.cause!="background" && n.cause!="staff" && n.cause!="revenue" && n.cause!="publications")) invalid("National access has an invalid date or cause.");
        if(n.cause=="background" && (!a.connected_background || n.day!=w.starts_on)) invalid("National access was not supplied by the founding background.");
        if((n.cause!="staff" && (n.staff_additions || n.staff_departures)) || (n.cause!="revenue" && n.postings) || (n.cause!="publications" && n.publications)) invalid("National access has unrelated proof records.");
        if(deep && n.cause=="staff") {
            if(!s.capacity || n.staff_additions>s.capacity->staff_additions.size() || n.staff_departures>s.capacity->staff_departures.size()) invalid("National staffing proof exceeds the saved staffing register.");
            std::int64_t people=opening_staff_count(*s.capacity);
            for(std::size_t i=0;i<n.staff_additions;++i) {const auto& h=s.capacity->staff_additions[i];if(h.day>n.day) invalid("National staffing proof includes future hires.");people+=h.people;}
            for(std::size_t i=0;i<n.staff_departures;++i) {const auto& h=s.capacity->staff_departures[i];if(h.day>n.day) invalid("National staffing proof includes future departures.");people-=h.people;}
            if(people<rules(s).productive_staff) invalid("National access lacks its qualifying staff record.");
        }
        if(deep && n.cause=="revenue") {
            if(!s.accounts || n.postings>s.accounts->postings.size()) invalid("National revenue proof exceeds the ledger.");Cents revenue{0};
            for(std::size_t i=0;i<n.postings;++i) {const auto& p=s.accounts->postings[i];if(p.day>n.day) invalid("National revenue proof includes future earnings.");if(p.credit==Account::revenue)revenue=checked_unit_add(revenue,p.amount);if(p.debit==Account::revenue)revenue=checked_unit_subtract(revenue,p.amount);}
            if(revenue<rules(s).earned_revenue) invalid("National access lacks its qualifying earned revenue.");
        }
        if(deep && n.cause=="publications") {
            if(!s.lab || n.publications>s.lab->publications.size() || n.publications<rules(s).publications) invalid("National access lacks its qualifying publications.");
            for(std::size_t i=0;i<n.publications;++i) if(s.lab->publications[i].day>n.day) invalid("National publication proof includes future research.");
        }
    } else if(a.connected_background || (deep && !qualifies(s,s.date).empty())) invalid("The national access milestone is missing.");
    if(a.first_overseas_opened) {
        const auto day=*a.first_overseas_opened;
        if(day<w.starts_on || day>s.date || std::ranges::none_of(a.offices,[&](const auto& o){
            if(o.ready_on!=day || (o.closed_on && *o.closed_on<day)) return false;
            const auto* rent=obligation(s,o);if(!rent) return false;
            return !rent->ends_before || *rent->ends_before>day || (o.closed_on && *o.closed_on==day) ||
                std::ranges::any_of(s.receipts,[&](const auto& r){return r.cause=="withdraw_operations" && r.day==day;});
        })) invalid("World contacts lack an actual overseas office opening.");
    } else if(deep && std::ranges::any_of(a.offices,[&](const auto& o){return liaison_office_open(s,o,s.date);})) invalid("The first overseas opening milestone is missing.");
    std::set<std::tuple<std::string,OrdinalDay,Cents>> expenses;if(deep) for(const auto& p:s.accounts->postings) if(p.debit==Account::expenses && p.credit==Account::cash) expenses.emplace(p.key,p.day,p.amount);
    for(std::size_t i=0;i<a.offices.size();++i) {
        const auto& o=a.offices[i];const auto& model=offer(s,o.offer);const auto* rent=obligation(s,o);
        if(o.id!=i+1 || o.ordered_on<w.starts_on || o.ordered_on>s.date || o.ready_on!=advance_days(o.ordered_on,model.lead) || o.ready_on>=s.end_date || (o.closed_on && (*o.closed_on<o.ordered_on || *o.closed_on>s.date))) invalid("Overseas office identity or opening/closing dates are inconsistent.");
        if(!rent || rent->starts_on!=o.ready_on || rent->monthly_amount!=model.monthly) invalid("An overseas office lost its payroll and rent obligation.");
        if(o.closed_on && rent->ends_before!=std::optional{std::max(o.ready_on,*o.closed_on)}) invalid("A closed office must stop its future payroll and rent.");
        if(!o.closed_on && rent->ends_before && std::ranges::none_of(s.receipts,[&](const auto& r){return r.cause=="withdraw_operations" && std::max(r.day,o.ready_on)==*rent->ends_before;})) invalid("An overseas office's payroll cannot disappear without closure or withdrawal.");
        if(deep && !expenses.contains({key(o.id),o.ordered_on,model.setup})) invalid("An overseas office has no matching setup expense.");
        if(deep) for(std::size_t j=0;j<i;++j) {
            const auto& prior=a.offices[j];if(offer(s,prior.offer).region!=model.region || (prior.closed_on && *prior.closed_on<=o.ordered_on)) continue;const auto* old=obligation(s,prior);
            if(!old->ends_before || *old->ends_before>o.ordered_on) invalid("Overseas office commitments overlap in one region.");
        }
    }
}
}
