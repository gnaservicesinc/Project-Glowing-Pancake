#include <pgp/core/headquarters.hpp>
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
bool enabled(const SimulationState& s) {return s.infrastructure && s.infrastructure->workplace && s.infrastructure->catalog.workplace && s.infrastructure->catalog.workplace->headquarters;}
const HeadquartersRules& rules(const SimulationState& s) {if(!enabled(s)) invalid("This company has no headquarters property market.");return *s.infrastructure->catalog.workplace->headquarters;}
HeadquartersState& hq(SimulationState& s) {if(!enabled(s) || !s.infrastructure->workplace->headquarters) invalid("Headquarters records are unavailable.");return *s.infrastructure->workplace->headquarters;}
const HeadquartersState& hq(const SimulationState& s) {return hq(const_cast<SimulationState&>(s));}
template<class Rows,class Id> const auto& find(const Rows& rows,const Id& id) {const auto it=std::ranges::find(rows,id,[](const auto& r){return r.id;});if(it==rows.end()) invalid("That property, item or contractor is no longer available.");return *it;}
HQEquipment& item(SimulationState& s,std::uint32_t id) {auto& rows=hq(s).items;auto it=std::ranges::find(rows,id,&HQEquipment::id);if(it==rows.end()) invalid("Choose an owned item.");return *it;}
const HQEquipmentOffer& offer(const SimulationState& s,const HQEquipment& i) {return find(rules(s).equipment,i.offer);}
Cents times(Cents value,std::uint64_t n) {return Cents{mul_div_round_half_up(static_cast<std::uint64_t>(value.value()),n,1)};}
Cents fraction(Cents value,std::uint64_t n,std::uint64_t d) {return Cents{mul_div_round_half_up(static_cast<std::uint64_t>(value.value()),n,d)};}
std::string key(std::string_view prefix,std::uint32_t id) {return std::string{prefix}+std::to_string(id);}
void pay(SimulationState& s,Cents cost,Account account,std::string cause,std::string id) {if(cost.value()!=0) record_cash(*s.accounts,s.date,Cents{-cost.value()},account,std::move(cause),std::move(id));s.cash=accounts_report(*s.accounts).cash;}
void affordable(const SimulationState& s,Cents cost,Cents protected_cash) {if(checked_unit_subtract(s.cash,protected_cash)<cost) invalid("Available funds do not cover the complete commitment.");}
const HQMarket& market(const SimulationState& s,const HQPremises& p) {return find(rules(s).markets,p.market);}
std::uint32_t area(int w,int d) {return static_cast<std::uint32_t>(w*d);}
bool working(const SimulationState& s,const HQEquipment& i) {
    if(i.delivered_on>s.date || !i.powered) return false;
    const auto& model=offer(s,i);
    const bool issued=hq_portable(model.kind) && !i.person.empty();
    if(issued && i.person!="founder" && (!s.management || !s.management->people || std::ranges::none_of(s.management->people->employees,[&](const auto& e){return e.terms.profile.id==i.person && !e.departed_on;}))) return false;
    if(!issued && i.premises!=current_headquarters(s).id) return false;
    if(model.kind=="thin_client" || model.kind=="research_laptop") {
        const bool rack=std::ranges::any_of(hq(s).items,[&](const auto& server) {
            return offer(s,server).kind=="rack" && server.powered && server.delivered_on<=s.date && server.premises==current_headquarters(s).id;
        });
        if(!rack) return false;
    }
    if(i.mount) {const auto& desk=find(hq(s).items,i.mount);return desk.premises==i.premises && desk.delivered_on<=s.date && offer(s,desk).kind=="desk";}
    return true;
}
Cents exit_fee(const SimulationState& s) {const auto& p=current_headquarters(s);if(p.kind!="lease") return Cents{0};const auto& m=market(s,p);return s.date<advance_days(p.entered_on,Days{m.minimum_lease_days})?times(p.rent,m.early_exit_months):Cents{0};}
void move(SimulationState& s,HQPremises next,Cents protected_cash) {
    const auto old=current_headquarters(s);const auto fee=exit_fee(s);
    affordable(s,checked_unit_subtract(checked_unit_add(next.deposit,fee),old.deposit),protected_cash);
    if(old.deposit.value()>0) pay(s,Cents{-old.deposit.value()},Account::assets,"Returned office security deposit",key("hq.deposit.return/",old.id));
    pay(s,fee,Account::expenses,"Office lease early-exit fee",key("hq.exit/",old.id));
    pay(s,next.deposit,Account::assets,"Office security deposit held by "+next.lessor,key("hq.deposit/",next.id));
    for(auto& term:s.accounts->recurring) if(!term.ends_before && (term.id=="office_and_admin" || term.id==key("hq.rent/",old.id))) term.ends_before=s.date;
    hq(s).premises.back().left_on=s.date;
    if(next.rent.value()>0) add_recurring(*s.accounts,{key("hq.rent/",next.id),next.rent,s.date,std::nullopt,"Office rent: "+next.lessor+", "+next.address});
    for(auto& owned:hq(s).items) {owned.premises=0;owned.mount=0;if(!hq_portable(offer(s,owned).kind)) owned.person.clear();}
    const auto state=market(s,next).state;
    hq(s).premises.push_back(std::move(next));s.infrastructure->executive_base=state;
}
HQPremises leased(const SimulationState& s,const LeaseHQ& request) {
    const auto& m=find(rules(s).markets,request.market);const auto& p=find(rules(s).plans,request.plan);
    const auto rent=times(m.rent_per_cell,area(p.width,p.depth));
    return {static_cast<std::uint32_t>(hq(s).premises.size()+1),"lease",m.id,p.id,m.landlord,m.street+" / "+p.name,p.width,p.depth,p.rooms,rent,times(rent,m.deposit_months),s.date,{},0};
}
HQPremises home_premises(const SimulationState& s) {
    const auto home=headquarters_home(s);
    return {static_cast<std::uint32_t>(hq(s).premises.size()+1),"home",hq(s).home_market,home.plan.id,"Your parents",home.plan.name,
        home.plan.width,home.plan.depth,home.plan.rooms,Cents{0},Cents{0},s.date,{},0};
}
void sync_capacity(SimulationState& s) {
    for(const auto& i:hq(s).items) {
        const auto& model=offer(s,i);if(model.compute.value()==0 || i.delivered_on>s.date) continue;
        const auto id=key("hq.equipment/",i.id);const auto effective=working(s,i)?model.compute:MilliCUPerDay{0};
        auto it=std::ranges::find(s.capacity->sources,id,&CapacitySource::id);
        if(it==s.capacity->sources.end()) {
            if(s.capacity->sources.size()>=capacity_source_limit) invalid("The company's capacity register is full.");
            const auto& owner=s.capacity->owner;
            s.capacity->sources.push_back({id,"physical/"+owner+"/"+id,owner,owner,CapacitySourceKind::owned,model.compute,effective,model.memory_mib,"general",false,i.delivered_on});
        } else it->effective=effective;
    }
}
}
bool hq_portable(std::string_view kind) {return kind=="laptop" || kind=="research_laptop";}
bool hq_desk_device(std::string_view kind) {return kind=="desktop" || kind=="workstation" || kind=="thin_client" || hq_portable(kind);}
bool hq_laptop_ready(const SimulationState& s,std::string_view person) {
    return enabled(s) && s.infrastructure->workplace->headquarters && !person.empty() && std::ranges::any_of(hq(s).items,[&](const auto& i) {
        return i.person==person && hq_portable(offer(s,i).kind) && working(s,i);
    });
}
std::string hq_placement_blocker(const SimulationState& s,const PlaceHQEquipment& a) {
    const auto& items=hq(s).items;
    const auto found=std::ranges::find(items,a.item,&HQEquipment::id);
    if(found==items.end()) return "Choose an owned item.";
    const auto& i=*found;const auto& e=offer(s,i);const auto& p=current_headquarters(s);
    if(i.delivered_on>s.date) return "Wait for delivery before placing this item.";
    if(a.mount) {
        const auto desk=std::ranges::find(items,a.mount,&HQEquipment::id);
        if(!hq_desk_device(e.kind) || desk==items.end() || offer(s,*desk).kind!="desk" || desk->premises!=p.id || desk->delivered_on>s.date) return "Choose a delivered desk in this office.";
        if(std::ranges::any_of(items,[&](const auto& other){return other.id!=i.id && other.mount==a.mount;})) return "That desk already has a computer.";
        return {};
    }
    if(hq_desk_device(e.kind)) return "Choose a free desk for this computer.";
    const int w=a.rotated?e.depth:e.width,d=a.rotated?e.width:e.depth;
    // Bound coordinates before addition to keep arbitrary command inputs safe.
    if(a.x<0 || a.y<0 || a.x>p.width-w || a.y>p.depth-d) return "The whole object must fit inside the floor.";
    if(std::ranges::none_of(p.rooms,[&](const auto& r){return a.x>=r.x && a.y>=r.y && a.x+w<=r.x+r.width && a.y+d<=r.y+r.depth;})) return "The object crosses a room wall.";
    for(const auto& other:items) {
        if(other.id==i.id || other.mount || other.premises!=p.id) continue;
        const auto& model=offer(s,other);const int ow=other.rotated?model.depth:model.width,od=other.rotated?model.width:model.depth;
        if(a.x<other.x+ow && a.x+w>other.x && a.y<other.y+od && a.y+d>other.y) return "This space is occupied. Move the other object first.";
    }
    return {};
}
void validate_hq_plan(int width,int depth,const std::vector<HQRoom>& rooms,int maximum_edge) {
    if(width<3 || depth<3 || width>maximum_edge || depth>maximum_edge || rooms.empty() || rooms.size()>16) invalid("Choose a floor plan within the plot, with one to sixteen rooms.");
    std::vector<bool> covered(static_cast<std::size_t>(width*depth));
    for(const auto& r:rooms) {
        if(r.x<0 || r.y<0 || r.width<2 || r.depth<2 || r.x+r.width>width || r.y+r.depth>depth) invalid("Rooms must fit inside the building and be at least two metres wide and deep.");
        for(int y=r.y;y<r.y+r.depth;++y) for(int x=r.x;x<r.x+r.width;++x) {
            const auto index=static_cast<std::size_t>(y*width+x);if(covered[index]) invalid("Rooms cannot overlap.");covered[index]=true;
        }
    }
    if(std::ranges::find(covered,false)!=covered.end()) invalid("The rooms must cover the complete building floor.");
}
void validate_headquarters_rules(const HeadquartersRules& r) {
    if(r.maximum_edge<3 || r.maximum_edge>40 || r.maximum_items<4 || r.maximum_items>128 || r.maximum_properties<1 || r.maximum_properties>16 || r.building_tax_annual_bp>1000 || r.markets.empty() || r.markets.size()>16 || r.plans.empty() || r.plans.size()>8 || r.equipment.empty() || r.equipment.size()>48 || r.contractors.size()!=3) invalid("Headquarters catalogs exceed their supported bounds.");
    validate_hq_plan(r.home_width,r.home_depth,{{0,0,r.home_width,r.home_depth}},r.maximum_edge);
    const auto text=[](const std::string& v){return !v.empty() && v.size()<=128 && v.find('\0')==v.npos;};
    const auto ids=[&](const auto& rows){std::set<std::string> seen;for(const auto& row:rows) if(!text(row.id) || !seen.insert(row.id).second) invalid("Headquarters catalog identities must be unique.");};
    if(!r.homes.empty()) {
        if(r.homes.size()!=3) invalid("Provide poor, middle and rich family home plans.");
        std::set<std::string> families;
        for(const auto& home:r.homes) {
            if((home.family!="poor" && home.family!="middle" && home.family!="rich") || !families.insert(home.family).second || !text(home.plan.id) || !text(home.plan.name)) invalid("Home plans need distinct family backgrounds and names.");
            validate_hq_plan(home.plan.width,home.plan.depth,home.plan.rooms,r.maximum_edge);
            if(home.plan.rooms.size()<3 || home.room_names.size()!=home.plan.rooms.size()) invalid("Every home needs at least three named free workspaces.");
            for(const auto& name:home.room_names) if(!text(name)) invalid("Name each free home workspace.");
        }
    }
    ids(r.markets);ids(r.plans);ids(r.equipment);ids(r.contractors);
    for(const auto& m:r.markets) if((m.state!="US-CA" && m.state!="US-TX" && m.state!="US-WA") || !text(m.city) || !text(m.agent) || !text(m.landlord) || !text(m.street) || m.rent_per_cell<Cents{100} || m.land_per_cell<Cents{10000} || m.monthly_land_tax_per_cell<Cents{1} || m.deposit_months>6 || m.minimum_lease_days>1095 || m.early_exit_months>6) invalid("Property listings need actual counterparties and bounded prices and lease terms.");
    for(const auto& p:r.plans) {if(!text(p.name)) invalid("Name the floor plan.");validate_hq_plan(p.width,p.depth,p.rooms,r.maximum_edge);}
    std::set<std::string> kinds;
    for(const auto& e:r.equipment) {kinds.insert(e.kind);if(!text(e.name) || !text(e.supplier) || e.price<Cents{100} || e.price>Cents{100000000} || e.delivery<Days{0} || e.delivery>Days{90} || e.width<1 || e.depth<1 || e.width>4 || e.depth>4 || e.compute<MilliCUPerDay{0} || e.compute>MilliCUPerDay{1000000} || ((e.kind=="thin_client" || e.kind=="research_laptop") && e.compute.value()!=0) || e.watts>10000 || e.memory_mib>16777216 || (!hq_desk_device(e.kind) && e.kind!="desk" && e.kind!="rack" && e.kind!="bench" && e.kind!="evaluation" && e.kind!="plant" && e.kind!="aquarium" && e.kind!="water" && e.kind!="microwave" && e.kind!="table" && e.kind!="cabinet" && e.kind!="vending")) invalid("Equipment needs physical dimensions, a supplier, a price and bounded delivery/output.");}
    if(!kinds.contains("desk") || !kinds.contains("desktop")) invalid("The equipment shop must supply desks and computers.");
    for(const auto& c:r.contractors) if(!text(c.name) || c.cost_per_cell<Cents{10000} || c.cost_per_cell>Cents{10000000} || c.base_days<1 || c.base_days>365 || c.cells_per_day<1 || c.cells_per_day>100 || c.delay_bp>10000 || c.overrun_bp>10000 || c.maximum_delay_days>90 || c.maximum_overrun_bp>5000) invalid("Contractor bids need bounded costs, duration and disclosed risks.");
}
const HQPremises& current_headquarters(const SimulationState& s) {const auto& p=hq(s).premises;if(p.empty()) invalid("The company has no current premises.");return p.back();}
HQHomePlan headquarters_home(const HeadquartersRules& r,std::string_view family) {
    if(!r.homes.empty()) {
        const auto id=family.empty()?std::string_view{"middle"}:family;
        const auto found=std::ranges::find(r.homes,id,&HQHomePlan::family);
        if(found==r.homes.end()) invalid("The family home plan is unavailable.");
        return *found;
    }
    HQHomePlan result{std::string{family},{"home","Parents' house / free workspaces",r.home_width,r.home_depth,{}},{}};
    if(r.home_width>=9 && r.home_width%3==0) {
        for(int x=0;x<r.home_width;x+=r.home_width/3) result.plan.rooms.push_back({x,0,r.home_width/3,r.home_depth});
        result.room_names={"Garage","Basement","Shed"};
    } else {result.plan.rooms={{0,0,r.home_width,r.home_depth}};result.room_names={"Garage"};}
    return result;
}
HQHomePlan headquarters_home(const SimulationState& s) {return headquarters_home(rules(s),hq(s).home_family);}
HQQuote preview_hq_home(const SimulationState& s) {
    if(current_headquarters(s).kind=="home") invalid("The company already operates from your parents' home.");
    if(hq(s).premises.size()>=64) invalid("The headquarters move register is full.");
    (void)home_premises(s);
    return {Cents{0},Cents{0},Cents{0},exit_fee(s),current_headquarters(s).deposit,s.date};
}
HQQuote preview_hq_lease(const SimulationState& s,const LeaseHQ& request) {
    const auto p=leased(s,request);const auto& old=current_headquarters(s);
    return {p.deposit,p.deposit,p.rent,exit_fee(s),old.deposit,s.date};
}
HQQuote preview_hq_move(const SimulationState& s,const MoveToOwnedHQ& request) {
    const auto& b=find(hq(s).construction,request.construction);
    if(!b.completed_on) invalid("Construction must be completed before moving in.");
    return {Cents{0},Cents{0},Cents{0},exit_fee(s),current_headquarters(s).deposit,s.date};
}
HQQuote preview_hq_land(const SimulationState& s,const BuyHQLand& request) {
    const auto& m=find(rules(s).markets,request.market);const auto& p=find(rules(s).plans,request.plan);const auto cells=area(p.width,p.depth);
    return {times(m.land_per_cell,cells),times(m.land_per_cell,cells),times(m.monthly_land_tax_per_cell,cells),Cents{0},Cents{0},s.date};
}
HQQuote preview_hq_build(const SimulationState& s,const BuildHQ& request) {
    const auto& lot=find(hq(s).land,request.land);const auto& plot=find(rules(s).plans,lot.plan);const auto& c=find(rules(s).contractors,request.contractor);
    validate_hq_plan(request.width,request.depth,request.rooms,rules(s).maximum_edge);
    if(request.width>plot.width || request.depth>plot.depth) invalid("The building cannot extend beyond your land.");
    if(std::ranges::any_of(hq(s).construction,[&](const auto& b){return b.land==request.land;})) invalid("That plot already has a building contract.");
    const auto cells=area(request.width,request.depth);const auto price=times(c.cost_per_cell,cells);const auto maximum=checked_unit_add(price,fraction(price,c.maximum_overrun_bp,10000));
    const auto days=c.base_days+(cells+c.cells_per_day-1)/c.cells_per_day;
    return {price,maximum,fraction(maximum,rules(s).building_tax_annual_bp,120000),Cents{0},Cents{0},advance_days(s.date,Days{days})};
}
void initialize_headquarters(SimulationState& s,std::uint64_t seed,std::string_view home_family) {
    if(!enabled(s)) return;
    auto& w=*s.infrastructure->workplace;w.headquarters=HeadquartersState{};auto& h=*w.headquarters;
    SplitMix64 stream{seed ^ UINT64_C(0x4845414451554152)};h.stream=stream.next();
    const auto& r=rules(s);const auto it=std::ranges::find(r.markets,s.infrastructure->executive_base,&HQMarket::state);
    if(it==r.markets.end()) invalid("The opening headquarters has no local property market.");
    h.home_family=home_family;
    const auto home_market=home_family.empty()?it:std::ranges::find(r.markets,std::string{"US-CA"},&HQMarket::state);
    if(home_market==r.markets.end()) invalid("The family home needs a California property market.");
    h.home_market=home_market->id;
    if(w.opening_office=="home") {
        h.premises.push_back(home_premises(s));
    }
    else {
        const auto& p=find(r.plans,std::string{"studio"});Cents rent{0};
        for(const auto& term:s.accounts->recurring) if(term.id=="office_and_admin") rent=term.monthly_amount;
        h.premises.push_back({1,"lease",it->id,p.id,it->landlord,it->street+" / inherited office",p.width,p.depth,p.rooms,rent,Cents{0},s.date,{},0});
        std::vector<std::string> people;
        if(s.management && s.management->people) for(const auto& e:s.management->people->employees) people.push_back(e.terms.profile.role=="Founder / researcher"?"founder":e.terms.profile.id);
        if(w.nonresearch_founder) people.insert(people.begin(),"founder");
        const auto& desk=find(r.equipment,std::string{"desk"});const auto& computer=find(r.equipment,std::string{"desktop"});
        for(std::size_t n=0;n<people.size();++n) {
            const auto id=static_cast<std::uint32_t>(h.items.size()+1);
            h.items.push_back({id,desk.id,people[n],s.date,s.date,Cents{0},true,true,false,1,0,static_cast<int>(n%3)*3,static_cast<int>(n/3)*3});
            h.items.push_back({id+1,computer.id,{},s.date,s.date,Cents{0},true,true,false,1,id,0,0});
        }
    }
    sync_capacity(s);
}
bool hq_computer_available(const SimulationState& s) {return enabled(s) && s.infrastructure->workplace->headquarters && std::ranges::any_of(hq(s).items,[&](const auto& i){return hq_desk_device(offer(s,i).kind) && working(s,i);});}
std::uint32_t furnished_hq_desks(const SimulationState& s) {return static_cast<std::uint32_t>(std::ranges::count_if(hq(s).items,[&](const auto& i){return offer(s,i).kind=="desk" && working(s,i);}));}
std::uint32_t assigned_hq_desks(const SimulationState& s) {return static_cast<std::uint32_t>(std::ranges::count_if(hq(s).items,[&](const auto& i){return offer(s,i).kind=="desk" && !i.person.empty() && working(s,i);}));}
std::uint32_t assigned_hq_desk(const SimulationState& s,std::string_view person) {
    if(person.empty() || !enabled(s) || !s.infrastructure->workplace->headquarters) return 0;
    const auto& items=hq(s).items;
    const auto found=std::ranges::find_if(items,[&](const auto& i){return offer(s,i).kind=="desk" && i.person==person && working(s,i);});
    return found==items.end()?0:found->id;
}
bool hq_workstation_ready(const SimulationState& s,std::string_view person) {
    const auto desk=assigned_hq_desk(s,person);
    return hq_laptop_ready(s,person) || (desk && std::ranges::any_of(hq(s).items,[&](const auto& i) {
        return i.mount==desk && hq_desk_device(offer(s,i).kind) && working(s,i);
    }));
}
bool hq_has_lab_equipment(const SimulationState& s,std::span<const std::string> required) {
    if(required.empty()) return true;
    if(!enabled(s) || !s.infrastructure->workplace->headquarters) return false;
    const auto& location=current_headquarters(s);
    const auto& items=hq(s).items;
    return std::ranges::any_of(location.rooms,[&](const auto& room) {
        return std::ranges::all_of(required,[&](const auto& id) {
            return std::ranges::any_of(items,[&](const auto& i) {
                if(i.offer!=id || !working(s,i)) return false;
                const auto& floor=i.mount?find(items,i.mount):i;
                return floor.x>=room.x && floor.y>=room.y &&
                    floor.x<room.x+room.width && floor.y<room.y+room.depth;
            });
        });
    });
}
bool headquarters_purchase(const HeadquartersAction& a) {return std::holds_alternative<BuyHQEquipment>(a) || std::holds_alternative<LeaseHQ>(a) || std::holds_alternative<BuyHQLand>(a) || std::holds_alternative<BuildHQ>(a) || std::holds_alternative<MoveToOwnedHQ>(a) || std::holds_alternative<ReturnHomeHQ>(a);}
void manage_headquarters(SimulationState& s,const HeadquartersAction& action,Cents protected_cash) {
    const auto& r=rules(s);auto& h=hq(s);
    std::visit([&](const auto& a) {
        using A=std::decay_t<decltype(a)>;
        if constexpr(std::is_same_v<A,BuyHQEquipment>) {
            const auto& e=find(r.equipment,a.offer);if(h.items.size()>=r.maximum_items) invalid("The company's equipment register is full.");affordable(s,e.price,protected_cash);
            const auto delivery=advance_days(s.date,e.delivery);if(delivery>=s.end_date) invalid("Delivery would arrive after the campaign.");
            const auto id=static_cast<std::uint32_t>(h.items.size()+1);pay(s,e.price,Account::expenses,"Equipment purchase from "+e.supplier+": "+e.name,key("hq.item/",id));
            h.items.push_back({id,e.id,{},s.date,delivery,e.price,false,true,false,0,0,0,0});
        } else if constexpr(std::is_same_v<A,PlaceHQEquipment>) {
            const auto blocker=hq_placement_blocker(s,a);if(!blocker.empty()) throw std::invalid_argument(blocker);
            auto& i=item(s,a.item);
            if(a.mount) {
                const auto& desk=find(h.items,a.mount);
                if(!hq_desk_device(offer(s,i).kind) || offer(s,desk).kind!="desk" || desk.premises!=current_headquarters(s).id || desk.delivered_on>s.date) invalid("Place the computer on a delivered desk in this office.");
                if(std::ranges::any_of(h.items,[&](const auto& other){return other.id!=i.id && other.mount==a.mount;})) invalid("That desk already has a computer.");
                i.person.clear();i.mount=a.mount;i.premises=desk.premises;i.x=0;i.y=0;i.rotated=false;
            } else {
                if(hq_desk_device(offer(s,i).kind)) invalid("Place this computer on a desk, or issue a laptop to an employee.");
                i.mount=0;i.premises=current_headquarters(s).id;i.x=a.x;i.y=a.y;i.rotated=a.rotated;
                for(auto& child:h.items) if(child.mount==i.id) child.premises=i.premises;
            }
        } else if constexpr(std::is_same_v<A,StoreHQEquipment>) {
            auto& i=item(s,a.item);i.premises=0;i.mount=0;i.person.clear();
            for(auto& child:h.items) if(child.mount==i.id) {child.premises=0;child.mount=0;}
        } else if constexpr(std::is_same_v<A,AssignHQDesk>) {
            auto& i=item(s,a.desk);if(offer(s,i).kind!="desk" || !working(s,i)) invalid("Choose a delivered desk placed in the current headquarters.");
            if(!a.person.empty() && a.person!="founder") {
                if(!s.management || !s.management->people || std::ranges::none_of(s.management->people->employees,[&](const auto& e){return e.terms.profile.id==a.person && !e.departed_on;})) invalid("Assign a current employee to the desk.");
            }
            auto person=a.person;
            if(s.management && s.management->people) for(const auto& employee:s.management->people->employees)
                if(employee.terms.profile.id==person && employee.terms.profile.role=="Founder / researcher") person="founder";
            for(auto& other:h.items) if(offer(s,other).kind=="desk" && other.person==person) other.person.clear();i.person=std::move(person);
        } else if constexpr(std::is_same_v<A,AssignHQLaptop>) {
            auto& laptop=item(s,a.item);
            if(!hq_portable(offer(s,laptop).kind) || laptop.delivered_on>s.date) invalid("Choose a delivered laptop.");
            if(!a.person.empty() && a.person!="founder" && (!s.management || !s.management->people || std::ranges::none_of(s.management->people->employees,[&](const auto& e){return e.terms.profile.id==a.person && !e.departed_on;}))) invalid("Choose a current employee.");
            auto person=a.person;
            if(s.management && s.management->people) for(const auto& e:s.management->people->employees) if(e.terms.profile.id==person && e.terms.profile.role=="Founder / researcher") person="founder";
            if(!person.empty()) for(auto& other:h.items) if(hq_portable(offer(s,other).kind) && other.person==person) other.person.clear();
            laptop.person=person;laptop.mount=0;laptop.premises=0;
        } else if constexpr(std::is_same_v<A,PowerHQEquipment>) {
            auto& i=item(s,a.item);if(offer(s,i).watts==0) invalid("This furnishing has no power switch.");i.powered=a.powered;
        }
        else if constexpr(std::is_same_v<A,ReturnHomeHQ>) {
            (void)preview_hq_home(s);move(s,home_premises(s),protected_cash);
        } else if constexpr(std::is_same_v<A,LeaseHQ>) {
            if(h.premises.size()>=64) invalid("The headquarters move register is full.");move(s,leased(s,a),protected_cash);
        } else if constexpr(std::is_same_v<A,BuyHQLand>) {
            if(h.land.size()>=r.maximum_properties) invalid("The company has reached its property limit.");const auto q=preview_hq_land(s,a);affordable(s,q.price,protected_cash);
            const auto id=static_cast<std::uint32_t>(h.land.size()+1);pay(s,q.price,Account::assets,"Land purchase through "+find(r.markets,a.market).agent,key("hq.land/",id));
            h.land.push_back({id,a.market,a.plan,s.date,q.price});add_recurring(*s.accounts,{key("hq.land.tax/",id),q.monthly,s.date,std::nullopt,"Property tax on company-owned land"});
        } else if constexpr(std::is_same_v<A,BuildHQ>) {
            const auto q=preview_hq_build(s,a);affordable(s,q.maximum,protected_cash);const auto& c=find(r.contractors,a.contractor);
            SplitMix64 rng{h.stream};const bool delayed=rng.bounded(10000)<c.delay_bp,overrun=rng.bounded(10000)<c.overrun_bp;
            const auto delay=delayed?1+rng.bounded(std::max(1U,c.maximum_delay_days)):0;
            const auto extra=overrun?fraction(q.price,1+rng.bounded(std::max(1U,c.maximum_overrun_bp)),10000):Cents{0};
            const auto finish=advance_days(q.ready_on,Days{static_cast<std::int64_t>(delay)});if(finish>=s.end_date) invalid("The contract could not finish within the campaign.");
            h.stream=rng.state();const auto id=static_cast<std::uint32_t>(h.construction.size()+1);
            pay(s,q.maximum,Account::assets,"Construction and authorized contingency prepaid to "+c.name,key("hq.build/",id));
            h.construction.push_back({id,a.land,a.contractor,a.width,a.depth,a.rooms,s.date,q.ready_on,finish,q.price,q.maximum,checked_unit_add(q.price,extra),false,{}});
        } else if constexpr(std::is_same_v<A,MoveToOwnedHQ>) {
            const auto& b=find(h.construction,a.construction);if(!b.completed_on) invalid("Construction must be completed before moving in.");
            const auto& lot=find(h.land,b.land);const auto& m=find(r.markets,lot.market);
            move(s,{static_cast<std::uint32_t>(h.premises.size()+1),"owned",lot.market,lot.plan,"Company-owned land",m.street+" / company building",b.width,b.depth,b.rooms,Cents{0},Cents{0},s.date,{},b.id},protected_cash);
        }
    },action);
    sync_capacity(s);validate_headquarters(s,false);
}
void prepare_headquarters(SimulationState& s) {
    if(!enabled(s) || !s.infrastructure->workplace->headquarters) return;
    for(auto& b:hq(s).construction) {
        if(b.completed_on) continue;
        if(s.date>=b.promised_on && b.finish_on>b.promised_on && !b.delay_announced) {
            b.delay_announced=true;hq(s).notices.push_back({s.date,"The contractor reports a construction delay. The revised completion date is "+iso_date(b.finish_on)+"."});s.paused=true;
        }
        if(s.date>=b.finish_on) {
            const auto refund=checked_unit_subtract(b.prepaid,b.actual);pay(s,Cents{-refund.value()},Account::assets,"Unused construction contingency returned",key("hq.build.refund/",b.id));b.completed_on=s.date;
            const auto tax=fraction(b.actual,rules(s).building_tax_annual_bp,120000);
            add_recurring(*s.accounts,{key("hq.build.tax/",b.id),tax,s.date,std::nullopt,"Property tax on company-owned headquarters building"});
            hq(s).notices.push_back({s.date,"Construction is complete. Final cost "+std::to_string(b.actual.value()/100)+" dollars; unused prepaid contingency returned. The building is ready for a move."});s.paused=true;
        }
    }
    sync_capacity(s);
}
void meter_headquarters_day(SimulationState& s) {
    if(!enabled(s) || !s.infrastructure->workplace->headquarters) return;
    auto& h=hq(s);const auto expected=h.metered_through?advance_days(*h.metered_through,Days{1}):s.infrastructure->workplace->starts_on;
    if(s.date!=expected) invalid("HQ electricity must be metered once per completed day.");
    std::uint64_t wh=0;for(const auto& i:h.items) if(working(s,i)) wh+=static_cast<std::uint64_t>(offer(s,i).watts)*24;
    const auto& region=find(s.infrastructure->catalog.states,s.infrastructure->executive_base);
    const auto numerator=wh*region.electricity_millicents_per_kwh+h.electricity_remainder;
    const auto cost=Cents{numerator/1000000};h.electricity_remainder=numerator%1000000;h.energy_wh+=wh;h.electricity_cost=checked_unit_add(h.electricity_cost,cost);h.metered_through=s.date;
    accrue_metered_expense(*s.accounts,s.date,"hq_electricity",cost,"Metered headquarters equipment electricity",false);
}
void validate_headquarters(const SimulationState& s,bool deep) {
    if(!enabled(s)) return;
    const auto& r=rules(s);const auto& h=hq(s);
    if(h.premises.empty() || h.premises.size()>64 || h.items.size()>r.maximum_items || h.land.size()>r.maximum_properties || h.construction.size()>r.maximum_properties || h.notices.size()>2*r.maximum_properties) invalid("Headquarters records exceed their bounded history.");
    const auto& p=current_headquarters(s);if(p.left_on) invalid("The current headquarters cannot have a departure date.");
    const auto home=headquarters_home(s);
    const auto& home_city=find(r.markets,h.home_market);
    if(!h.home_family.empty() && h.home_family!="poor" && h.home_family!="middle" && h.home_family!="rich") invalid("The family home background is invalid.");
    if(!h.home_family.empty() && home_city.state!="US-CA") invalid("The founder's family home remains in California.");
    validate_hq_plan(p.width,p.depth,p.rooms,r.maximum_edge);
    std::vector<std::uint32_t> occupancy(static_cast<std::size_t>(p.width*p.depth));std::set<std::string> people,laptop_people;std::set<std::uint32_t> mounts;
    const auto opening_people=opening_staff_count(*s.capacity)+(s.infrastructure->workplace->nonresearch_founder?1U:0U);
    for(std::size_t n=0;n<h.items.size();++n) {
        const auto& i=h.items[n];
        if(i.inherited && (s.infrastructure->workplace->opening_office=="home" || i.ordered_on!=s.infrastructure->workplace->starts_on || i.id>2*opening_people)) invalid("Inherited furnishings must come from the actual opening company.");const auto& e=offer(s,i);
        if(i.id!=n+1 || i.ordered_on<s.infrastructure->workplace->starts_on || i.ordered_on>s.date || i.delivered_on!=advance_days(i.ordered_on,i.inherited?Days{0}:e.delivery) || i.paid!=(i.inherited?Cents{0}:e.price)) invalid("Equipment ownership, price or delivery changed.");
        if(!i.person.empty()) {
            if(hq_portable(e.kind)) {
                if(!laptop_people.insert(i.person).second || i.delivered_on>s.date) invalid("A person can hold one delivered company laptop.");
            } else if(!people.insert(i.person).second || e.kind!="desk" || i.premises!=p.id) invalid("A person can be assigned to one placed desk.");
        }
        if(!i.premises) {if(i.mount || (!i.person.empty() && !hq_portable(e.kind))) invalid("Stored equipment cannot have a desk attachment or employee.");continue;}
        if(i.premises!=p.id || i.delivered_on>s.date) invalid("Only delivered equipment can occupy the current premises.");
        if(i.mount) {const auto& desk=find(h.items,i.mount);if(!hq_desk_device(e.kind) || offer(s,desk).kind!="desk" || desk.premises!=p.id || !mounts.insert(i.mount).second) invalid("A computer needs its own placed desk.");continue;}
        if(hq_desk_device(e.kind)) invalid("Computers require a desk attachment or an issued laptop assignment.");
        const auto width=i.rotated?e.depth:e.width,depth=i.rotated?e.width:e.depth;
        if(i.x<0 || i.y<0 || i.x+width>p.width || i.y+depth>p.depth || std::ranges::none_of(p.rooms,[&](const auto& room){return i.x>=room.x && i.y>=room.y && i.x+width<=room.x+room.width && i.y+depth<=room.y+room.depth;})) invalid("The item must fit inside one room, without crossing a wall.");
        for(int y=i.y;y<i.y+depth;++y) for(int x=i.x;x<i.x+width;++x) {auto& cell=occupancy[static_cast<std::size_t>(y*p.width+x)];if(cell) invalid("Equipment cannot overlap. Move the other item or choose another position.");cell=i.id;}
    }
    for(const auto& i:h.items) if(offer(s,i).compute.value()>0 && i.delivered_on<=s.date) {
        const auto& source=find(s.capacity->sources,key("hq.equipment/",i.id));
        if(source.physical_id!="physical/"+s.capacity->owner+"/"+key("hq.equipment/",i.id) || source.nominal!=offer(s,i).compute || source.effective!=(working(s,i)?offer(s,i).compute:MilliCUPerDay{0}) || source.kind!=CapacitySourceKind::owned || source.memory_mib!=offer(s,i).memory_mib || source.available_from!=i.delivered_on) invalid("HQ compute must match a delivered, placed, powered physical machine.");
    }
    if(!deep) return;
    std::map<std::string,Cents> expenses,assets;
    const auto amount=[](const auto& rows,const std::string& id) {const auto it=rows.find(id);return it==rows.end()?Cents{0}:it->second;};
    const auto add=[](auto& rows,const Posting& p) {auto it=rows.try_emplace(p.key,Cents{0}).first;it->second=checked_unit_add(it->second,p.amount);};
    for(const auto& posting:s.accounts->postings) if(posting.credit==Account::cash) {
        if(posting.debit==Account::expenses) add(expenses,posting);
        if(posting.debit==Account::assets) add(assets,posting);
    }
    for(const auto& i:h.items) if(!i.inherited && amount(expenses,key("hq.item/",i.id))!=i.paid) invalid("Purchased equipment needs its actual paid supplier expense.");
    std::map<std::string,Cents> returned;
    for(const auto& posting:s.accounts->postings) if(posting.debit==Account::cash && posting.credit==Account::assets) add(returned,posting);
    const auto obligation=[&](const std::string& id,Cents monthly,OrdinalDay starts,std::optional<OrdinalDay> ends) {
        const auto it=std::ranges::find(s.accounts->recurring,id,&RecurringObligation::id);
        if(it==s.accounts->recurring.end() || it->monthly_amount!=monthly || it->starts_on!=starts || (ends && it->ends_before!=ends)) invalid("A property contract lost its actual rent or tax obligation.");
    };
    for(std::size_t n=0;n<h.premises.size();++n) {
        const auto& location=h.premises[n];const auto& city=market(s,location);
        validate_hq_plan(location.width,location.depth,location.rooms,r.maximum_edge);
        if(location.id!=n+1 || location.entered_on>s.date || (n==0 && location.entered_on!=s.infrastructure->workplace->starts_on) || (n>0 && h.premises[n-1].left_on!=location.entered_on) || (n+1<h.premises.size() && !location.left_on)) invalid("Headquarters occupancy history must be sequential and dated.");
        if(location.kind=="home") {
            if(location.rent.value()!=0 || location.deposit.value()!=0 || location.width!=home.plan.width || location.depth!=home.plan.depth || location.rooms!=home.plan.rooms || location.market!=h.home_market || location.plan!=home.plan.id || location.construction) invalid("The family home must retain its actual free rooms and location.");
        } else if(location.kind=="lease") {
            const auto& plan=find(r.plans,location.plan);
            if(location.width!=plan.width || location.depth!=plan.depth || location.rooms!=plan.rooms || location.lessor!=city.landlord) invalid("Leased rooms must match their actual listed premises and landlord.");
            if(n==0) obligation("office_and_admin",location.rent,location.entered_on,location.left_on);
            else {
                const auto rent=times(city.rent_per_cell,area(plan.width,plan.depth));
                if(location.rent!=rent || location.deposit!=times(rent,city.deposit_months) || amount(assets,key("hq.deposit/",location.id))!=location.deposit) invalid("Lease rent and the paid security deposit must match the signed listing.");
                obligation(key("hq.rent/",location.id),rent,location.entered_on,location.left_on);
                if(location.left_on && amount(returned,key("hq.deposit.return/",location.id))!=location.deposit) invalid("A vacated lease must return its recorded security deposit.");
            }
        } else if(location.kind=="owned") {
            const auto& building=find(h.construction,location.construction);const auto& lot=find(h.land,building.land);
            if(!building.completed_on || *building.completed_on>location.entered_on || lot.market!=location.market || location.width!=building.width || location.depth!=building.depth || location.rooms!=building.rooms || location.rent.value()!=0 || location.deposit.value()!=0) invalid("Owned headquarters must be the completed building on the company's land, without rent.");
        } else invalid("Unsupported headquarters tenure.");
    }
    for(std::size_t n=0;n<h.land.size();++n) {
        const auto& lot=h.land[n];const auto q=preview_hq_land(s,{lot.market,lot.plan});
        obligation(key("hq.land.tax/",lot.id),q.monthly,lot.purchased_on,{});
        if(lot.id!=n+1 || lot.paid!=q.price || lot.purchased_on>s.date || amount(assets,key("hq.land/",lot.id))!=lot.paid) invalid("Land ownership must reconcile with its actual purchase.");
    }
    for(std::size_t n=0;n<h.construction.size();++n) {
        const auto& b=h.construction[n];const auto& c=find(r.contractors,b.contractor);const auto& lot=find(h.land,b.land);const auto& plot=find(r.plans,lot.plan);
        validate_hq_plan(b.width,b.depth,b.rooms,r.maximum_edge);
        if(b.completed_on) {
            if(amount(returned,key("hq.build.refund/",b.id))!=checked_unit_subtract(b.prepaid,b.actual)) invalid("The contractor must return all unused prepaid contingency.");
            obligation(key("hq.build.tax/",b.id),fraction(b.actual,r.building_tax_annual_bp,120000),*b.completed_on,{});
        }
        if(b.promised_on!=advance_days(b.signed_on,Days{c.base_days+(area(b.width,b.depth)+c.cells_per_day-1)/c.cells_per_day})) invalid("The promised completion must follow the signed contractor bid.");
        if(b.id!=n+1 || b.width>plot.width || b.depth>plot.depth || b.signed_on<lot.purchased_on || b.signed_on>s.date || b.quote!=times(c.cost_per_cell,area(b.width,b.depth)) || b.prepaid!=checked_unit_add(b.quote,fraction(b.quote,c.maximum_overrun_bp,10000)) || b.actual<b.quote || b.actual>b.prepaid || b.finish_on<b.promised_on || b.finish_on>advance_days(b.promised_on,Days{c.maximum_delay_days}) || amount(assets,key("hq.build/",b.id))!=b.prepaid || (b.completed_on && *b.completed_on!=b.finish_on) || (b.finish_on<=s.date)!=b.completed_on.has_value()) invalid("Construction must retain the signed plan, prepaid limit, risk bounds and actual completion.");
    }
}
}
