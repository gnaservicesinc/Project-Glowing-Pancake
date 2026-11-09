#include <pgp/core/devices.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>
#include <tuple>
namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
const DeviceRules& rules(const SimulationState& s) {
    if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->catalog.workplace || !s.infrastructure->catalog.workplace->devices || !s.infrastructure->workplace->devices)
        invalid("This scenario has no personal device inventory.");
    return *s.infrastructure->catalog.workplace->devices;
}
const DeviceOffer& offer(const SimulationState& s,std::string_view id) {
    const auto& list=rules(s).offers;const auto it=std::ranges::find(list,id,&DeviceOffer::id);
    if(it==list.end()) invalid("The device model is no longer available.");return *it;
}
PersonalDevice& device(SimulationState& s,std::uint64_t id) {
    (void)rules(s);auto& rows=s.infrastructure->workplace->devices->records;const auto it=std::ranges::find(rows,id,&PersonalDevice::id);
    if(it==rows.end()) invalid("The selected device is not owned by this company.");return *it;
}
void pay(SimulationState& s,Cents cost,Cents protected_cash,std::string key,std::string cause) {
    if(checked_unit_subtract(s.cash,protected_cash)<cost) invalid("The device commitment exceeds available company funds.");
    if(cost>Cents{0}) record_cash(*s.accounts,s.date,Cents{-cost.value()},Account::expenses,std::move(cause),std::move(key));
    s.cash=accounts_report(*s.accounts).cash;
}
bool another_portable(const SimulationState& s,std::uint64_t id) {
    return std::ranges::any_of(s.infrastructure->workplace->devices->records,[&](const auto& row){return row.id!=id && offer(s,row.offer).portable && device_functioning(s,row);});
}
}
void validate_device_rules(const DeviceRules& r) {
    if(r.offers.empty() || r.offers.size()>4) invalid("Device catalog needs one to four models.");std::set<std::string> ids;
    const std::set<std::string> kinds{"phone","laptop","tablet","desktop"},apps{"office","people","facilities","lab","inbox","operations","civic","society","institutions","frontier","leadership","objectives"};
    bool phone=false;std::set<std::string> provided;
    for(const auto& d:r.offers) {
        if(!kinds.contains(d.id) || !ids.insert(d.id).second || d.name.empty() || d.name.size()>96 || d.name.find('\0')!=std::string::npos ||
            d.purchase<Cents{100} || d.purchase>Cents{1000000} || d.service<Cents{0} || d.service>Cents{100000} ||
            d.delivery<Days{0} || d.delivery>Days{30} || d.service_lead<Days{0} || d.service_lead>Days{7} || d.portable!=(d.id!="desktop") || d.apps.empty() || d.apps.size()>apps.size() || d.included_offices.size()>3)
            invalid("Device identity, cost, delivery or supported apps are invalid.");
        std::set<std::string> unique;for(const auto& app:d.apps) {if(!apps.contains(app) || !unique.insert(app).second) invalid("Device apps must be distinct built-in company applications.");provided.insert(app);}
        unique.clear();for(const auto& office:d.included_offices) if((office!="home" && office!="shared" && office!="leased") || !unique.insert(office).second) invalid("Device startup terms have an invalid office.");
        if(d.id=="phone") {
            phone=d.included_offices.size()==3 && d.service_lead==Days{0} && std::ranges::find(d.apps,"inbox")!=d.apps.end();
        }
    }
    if(provided!=apps) invalid("The device catalog must provide access to every built-in company application.");
    if(!phone) invalid("Each founding office needs an included phone with instant service and decision access.");
}
void initialize_devices(SimulationState& s) {
    if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->catalog.workplace->devices) return;
    auto& w=*s.infrastructure->workplace;w.devices=DeviceInventory{};
    for(const auto& d:s.infrastructure->catalog.workplace->devices->offers) if(!(s.infrastructure->catalog.workplace->headquarters && d.id=="desktop") && std::ranges::find(d.included_offices,w.opening_office)!=d.included_offices.end())
        w.devices->records.push_back({w.devices->records.size()+1,d.id,s.date,s.date,true,true,{},{}});
}
bool device_functioning(const SimulationState& s,const PersonalDevice& d) {
    return d.powered && !d.retired_on && d.ready_on<=s.date && (d.services.empty() || d.services.back().ready_on<=s.date);
}
bool device_available(const SimulationState& s,const PersonalDevice& d,bool headquarters) {return device_functioning(s,d) && (headquarters || offer(s,d.offer).portable);}
bool device_supports_app(const SimulationState& s,std::string_view kind,std::string_view app) {
    const auto& model=offer(s,kind);return std::ranges::find(model.apps,app)!=model.apps.end();
}
std::string resolve_app_device(const SimulationState& s,std::string_view preferred,std::string_view app,bool headquarters) {
    (void)rules(s);const auto& rows=s.infrastructure->workplace->devices->records;
    const auto usable=[&](std::string_view kind) {if(kind=="desktop" && s.infrastructure->catalog.workplace->headquarters) return headquarters && hq_computer_available(s) && device_supports_app(s,kind,app);return std::ranges::any_of(rows,[&](const auto& d){return d.offer==kind && device_available(s,d,headquarters) && device_supports_app(s,kind,app);});};
    if(usable(preferred)) return std::string{preferred};
    for(const auto kind:{"laptop","tablet","desktop","phone"}) if(usable(kind)) return kind;
    return {};
}
void buy_personal_device(SimulationState& s,std::string_view id,Cents protected_cash) {
    if(id=="desktop" && s.infrastructure->catalog.workplace->headquarters) {manage_headquarters(s,BuyHQEquipment{"desktop"},protected_cash);return;}
    const auto& model=offer(s,id);auto& rows=s.infrastructure->workplace->devices->records;
    if(rows.size()>=128) invalid("The bounded device purchase register is full.");
    const auto ready=advance_days(s.date,model.delivery);if(ready>=s.end_date) invalid("Device delivery would arrive after this campaign ends.");
    const auto next=rows.size()+1;pay(s,model.purchase,protected_cash,"device/purchase/"+std::to_string(next),"Device purchase: "+model.name);
    rows.push_back({next,model.id,s.date,ready,false,true,{},{}});
}
void service_personal_device(SimulationState& s,std::uint64_t id,Cents protected_cash) {
    auto& d=device(s,id);const auto& model=offer(s,d.offer);
    if(d.retired_on || d.ready_on>s.date || (!d.services.empty() && d.services.back().ready_on>s.date) || d.services.size()>=128)
        invalid("Choose a delivered device that is not retired or already being serviced.");
    if(model.portable && model.service_lead>Days{0} && device_functioning(s,d) && !another_portable(s,id))
        invalid("Keep one working portable device available while another is serviced.");
    const auto ready=advance_days(s.date,model.service_lead);if(ready>=s.end_date) invalid("Device service would finish after the campaign ends.");
    pay(s,model.service,protected_cash,"device/service/"+std::to_string(d.id)+"/"+std::to_string(d.services.size()+1),"Device service: "+model.name);
    d.services.push_back({s.date,ready});
}
void set_device_power(SimulationState& s,const SetDevicePower& action) {
    auto& d=device(s,action.id);if(d.retired_on || d.ready_on>s.date) invalid("Power controls require an owned, delivered device.");d.powered=action.powered;
}
void retire_personal_device(SimulationState& s,std::uint64_t id) {
    auto& d=device(s,id);const auto& model=offer(s,d.offer);
    if(d.retired_on || d.ready_on>s.date || (!d.services.empty() && d.services.back().ready_on>s.date)) invalid("Wait for delivery and any paid service before retiring this device.");
    if(model.portable && !another_portable(s,id)) invalid("Keep a working portable device for company decisions before retiring this one.");
    d.retired_on=s.date;d.powered=false;
}
void validate_devices(const SimulationState& s,bool deep) {
    const auto& w=*s.infrastructure->workplace;const auto& r=*s.infrastructure->catalog.workplace;
    if(w.devices.has_value()!=r.devices.has_value()) invalid("Device inventory differs from the authored equipment rules.");if(!w.devices) return;
    const auto& rows=w.devices->records;if(rows.empty() || rows.size()>128) invalid("Owned device history is empty or oversized.");
    std::set<std::tuple<std::string,OrdinalDay,Cents>> expenses;
    if(deep) for(const auto& p:s.accounts->postings) if(p.debit==Account::expenses && p.credit==Account::cash) expenses.emplace(p.key,p.day,p.amount);
    const auto paid=[&](std::string key,OrdinalDay day,Cents cost) {return cost==Cents{0} || expenses.contains({std::move(key),day,cost});};
    std::set<std::string> initial;
    for(std::size_t i=0;i<rows.size();++i) {
        const auto& d=rows[i];const auto& model=offer(s,d.offer);
        if(d.id!=i+1 || d.ordered_on<w.starts_on || d.ordered_on>s.date || d.ready_on!=advance_days(d.ordered_on,d.included?Days{0}:model.delivery) || d.ready_on>=s.end_date ||
            (d.retired_on && (*d.retired_on<d.ready_on || *d.retired_on>s.date || d.powered)) || d.services.size()>128)
            invalid("Device ownership, delivery, retirement or service dates are inconsistent.");
        if(d.included) {
            if(d.ordered_on!=w.starts_on || !initial.insert(d.offer).second || std::ranges::find(model.included_offices,w.opening_office)==model.included_offices.end()) invalid("Device was not included in the founder's office package.");
        } else if(deep && !paid("device/purchase/"+std::to_string(d.id),d.ordered_on,model.purchase)) invalid("A purchased device has no matching company expense.");
        auto next=d.ready_on;
        for(std::size_t j=0;j<d.services.size();++j) {
            const auto& service=d.services[j];if(service.ordered_on<next || service.ordered_on>s.date || service.ready_on!=advance_days(service.ordered_on,model.service_lead) || service.ready_on>=s.end_date || (d.retired_on && service.ready_on>*d.retired_on)) invalid("Device service overlaps delivery, another service or retirement.");
            if(deep && !paid("device/service/"+std::to_string(d.id)+"/"+std::to_string(j+1),service.ordered_on,model.service)) invalid("Device service has no matching expense.");next=service.ready_on;
        }
    }
    for(const auto& model:rules(s).offers) if(!(s.infrastructure->catalog.workplace->headquarters && model.id=="desktop") && std::ranges::find(model.included_offices,w.opening_office)!=model.included_offices.end() && !initial.contains(model.id)) invalid("The saved office package lost an included device.");
}
}
