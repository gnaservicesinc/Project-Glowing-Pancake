#include <pgp/core/devices.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <map>
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
    for(const auto& d:s.infrastructure->catalog.workplace->devices->offers) if(!(s.infrastructure->catalog.workplace->headquarters && d.id=="desktop") && std::ranges::find(d.included_offices,w.opening_office)!=d.included_offices.end()) {
        PersonalDevice rec{w.devices->records.size()+1,d.id,s.date,s.date,true,true,{},{},false,d.portable?"protagonist":"company"};
        w.devices->records.push_back(std::move(rec));
    }
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
std::vector<PhysicalDeviceInfo> physical_devices(const SimulationState& s,bool headquarters) {
    std::vector<PhysicalDeviceInfo> out;
    if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->catalog.workplace ||
       !s.infrastructure->catalog.workplace->devices || !s.infrastructure->workplace->devices) return out;
    const auto& offers=s.infrastructure->catalog.workplace->devices->offers;
    const auto find_offer=[&](std::string_view id) -> const DeviceOffer* {
        const auto it=std::ranges::find(offers,id,&DeviceOffer::id);
        return it==offers.end()?nullptr:&*it;
    };
    // Count how many physical units of each offer kind exist so names stay unambiguous ("Laptop #1", "Laptop #2").
    std::map<std::string,std::size_t> kind_totals, kind_seen;
    for(const auto& d:s.infrastructure->workplace->devices->records) ++kind_totals[d.offer];
    for(const auto& d:s.infrastructure->workplace->devices->records) {
        const auto* model=find_offer(d.offer);
        if(!model) continue;
        const auto idx=++kind_seen[d.offer];
        const bool servicing=!d.services.empty() && d.services.back().ready_on>s.date;
        const bool in_transit=d.ready_on>s.date;
        const bool avail=device_available(s,d,headquarters);
        PhysicalDeviceInfo info;
        info.device_id="personal/"+std::to_string(d.id);
        info.kind=d.offer;
        info.numeric_id=d.id;
        info.name=kind_totals[d.offer]>1 ? (model->name+" #"+std::to_string(idx)) : model->name;
        info.owner=!d.owner.empty() ? d.owner : (d.included && model->portable ? "protagonist" : "company");
        info.portable=model->portable;
        info.powered=d.powered;
        info.available=avail;
        info.can_power=!d.retired_on && !in_transit;
        info.can_service=!d.retired_on && !in_transit && !servicing;
        info.can_retire=!d.retired_on && !in_transit && !servicing;
        info.apps=model->apps;
        if(d.retired_on) {
            info.location="retired";
            info.unavailability_reason="This device was retired on "+iso_date(*d.retired_on)+". Its workspace is preserved as read-only history; open another available device to continue working.";
        } else if(in_transit) {
            info.location="in transit";
            info.unavailability_reason="This device is in transit and scheduled for delivery on "+iso_date(d.ready_on)+".";
        } else if(servicing) {
            info.location="in service";
            info.unavailability_reason="This device is in service until "+iso_date(d.services.back().ready_on)+". Its open apps and drafts are preserved while offline.";
        } else if(!d.powered) {
            info.location=model->portable ? "with you" : "headquarters";
            info.unavailability_reason="This device is powered off. Power it back on to resume its preserved apps and drafts, or use another available device.";
        } else if(!headquarters && !model->portable) {
            info.location="headquarters";
            info.unavailability_reason="This workstation stays at headquarters while you are away. Its open apps remain waiting at HQ; use a portable device while traveling.";
        } else {
            info.location=model->portable ? "with you" : "headquarters";
        }
        out.push_back(std::move(info));
    }
    // Include every actual physical HQ computer individually (never collapsing by kind).
    if(s.infrastructure->workplace->headquarters && s.infrastructure->catalog.workplace->headquarters) {
        const auto& hq=*s.infrastructure->workplace->headquarters;
        const auto& hq_eq=s.infrastructure->catalog.workplace->headquarters->equipment;
        const auto* desktop_offer=find_offer("desktop");
        const auto* laptop_offer=find_offer("laptop");
        const auto hq_id=hq.premises.empty()?0U:hq.premises.back().id;
        const bool has_powered_rack=std::ranges::any_of(hq.items,[&](const auto& server) {
            const auto seq=std::ranges::find(hq_eq,server.offer,&HQEquipmentOffer::id);
            return seq!=hq_eq.end() && seq->kind=="rack" && server.powered && server.delivered_on<=s.date && server.premises==hq_id && hq_id!=0;
        });
        std::map<std::string,std::size_t> hq_totals, hq_seen;
        for(const auto& item:hq.items) {
            const auto eq=std::ranges::find(hq_eq,item.offer,&HQEquipmentOffer::id);
            if(eq==hq_eq.end() || !hq_desk_device(eq->kind)) continue;
            // Employee-issued laptops belong to that employee; only protagonist-usable machines enter the switcher.
            if(hq_portable(eq->kind) && !item.person.empty() && item.person!="founder") continue;
            ++hq_totals[eq->id];
        }
        for(const auto& item:hq.items) {
            const auto eq=std::ranges::find(hq_eq,item.offer,&HQEquipmentOffer::id);
            if(eq==hq_eq.end() || !hq_desk_device(eq->kind)) continue;
            if(hq_portable(eq->kind) && !item.person.empty() && item.person!="founder") continue;
            const auto idx=++hq_seen[eq->id];
            const bool portable_hq=hq_portable(eq->kind);
            const bool issued_to_founder=portable_hq && item.person=="founder";
            const bool in_transit=item.delivered_on>s.date;
            const bool placed=issued_to_founder || (item.premises==hq_id && hq_id!=0);
            bool desk_mounted=portable_hq || item.mount!=0;
            if(item.mount) {
                const auto desk=std::ranges::find(hq.items,item.mount,&HQEquipment::id);
                desk_mounted=desk!=hq.items.end() && desk->premises==hq_id && desk->delivered_on<=s.date;
            }
            const bool needs_rack=(eq->kind=="thin_client" || eq->kind=="research_laptop") && !has_powered_rack;
            const bool ready=!in_transit && placed && desk_mounted && !needs_rack && item.powered && (portable_hq || headquarters);
            PhysicalDeviceInfo info;
            info.device_id="hq/"+std::to_string(item.id);
            info.kind=portable_hq ? "laptop" : "desktop";
            info.numeric_id=item.id;
            info.name=hq_totals[eq->id]>1 ? (eq->name+" #"+std::to_string(idx)) : eq->name;
            info.owner="company";
            info.portable=portable_hq;
            info.powered=item.powered;
            info.available=ready;
            info.can_power=!in_transit && placed;
            info.can_service=false;
            info.can_retire=false;
            if(const auto* app_offer=portable_hq ? laptop_offer : desktop_offer) info.apps=app_offer->apps;
            if(in_transit) {
                info.location="in transit";
                info.unavailability_reason="Ordered company hardware scheduled for HQ delivery on "+iso_date(item.delivered_on)+".";
            } else if(!placed || !desk_mounted) {
                info.location="storage";
                info.unavailability_reason="This HQ computer is in storage or not mounted on a furnished desk. Place it in Arrange headquarters to use it.";
            } else if(needs_rack) {
                info.location="headquarters";
                info.unavailability_reason="This machine requires a powered HQ compute rack to operate. Place and power on a rack in Arrange headquarters.";
            } else if(!item.powered) {
                info.location=portable_hq ? "with you" : "headquarters";
                info.unavailability_reason="This workstation is powered off. Power it on to resume its preserved apps and drafts.";
            } else if(!headquarters && !portable_hq) {
                info.location="headquarters";
                info.unavailability_reason="This HQ workstation stays at headquarters while you are traveling. Its open apps remain preserved until you return.";
            } else {
                info.location=portable_hq ? "with you" : "headquarters";
            }
            out.push_back(std::move(info));
        }
    }
    return out;
}
std::optional<PhysicalDeviceInfo> inspect_physical_device(const SimulationState& s,std::string_view device_id,bool headquarters) {
    for(auto& info:physical_devices(s,headquarters))
        if(info.device_id==device_id) return info;
    return std::nullopt;
}
bool physical_device_supports_app(const SimulationState& s,std::string_view device_id,std::string_view app,bool headquarters) {
    const auto info=inspect_physical_device(s,device_id,headquarters);
    if(!info || !info->available) return false;
    if(app=="devices" || app=="personal") return true;
    const auto canonical=[](std::string_view a) -> std::string_view {
        if(a=="research" || a=="data" || a=="models") return "lab";
        if(a=="accounts" || a=="products") return "office";
        if(a=="messages") return "inbox";
        if(a=="world") return "society";
        return a;
    };
    const auto target=canonical(app);
    return std::ranges::find(info->apps,target)!=info->apps.end();
}
std::string resolve_physical_app_device(const SimulationState& s,std::string_view preferred_device_id,std::string_view app,bool headquarters) {
    if(!preferred_device_id.empty() && physical_device_supports_app(s,preferred_device_id,app,headquarters))
        return std::string{preferred_device_id};
    const auto all=physical_devices(s,headquarters);
    for(const std::string_view preferred_kind:{"laptop","desktop","tablet","phone"}) {
        for(const auto& info:all) {
            if(info.kind==preferred_kind && info.available && physical_device_supports_app(s,info.device_id,app,headquarters))
                return info.device_id;
        }
    }
    return {};
}
void buy_personal_device(SimulationState& s,std::string_view id,Cents protected_cash) {
    if(id=="desktop" && s.infrastructure->catalog.workplace->headquarters) {manage_headquarters(s,BuyHQEquipment{"desktop"},protected_cash);return;}
    const auto& model=offer(s,id);auto& rows=s.infrastructure->workplace->devices->records;
    if(rows.size()>=128) invalid("The bounded device purchase register is full.");
    const auto ready=advance_days(s.date,model.delivery);if(ready>=s.end_date) invalid("Device delivery would arrive after this campaign ends.");
    const auto next=rows.size()+1;pay(s,model.purchase,protected_cash,"device/purchase/"+std::to_string(next),"Device purchase: "+model.name);
    rows.push_back({next,model.id,s.date,ready,false,true,{},{},false,"company"});
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
        if(d.id!=i+1 || d.ordered_on<w.starts_on || d.ordered_on>s.date || (d.ready_on<(d.playtest_delivery?d.ordered_on:advance_days(d.ordered_on,d.included?Days{0}:model.delivery)) || d.ready_on>advance_days(d.ordered_on,d.included?Days{0}:model.delivery) || (d.playtest_delivery && (d.included || d.ready_on>s.date))) || d.ready_on>=s.end_date ||
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
