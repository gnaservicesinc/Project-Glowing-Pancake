#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <set>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* why) {throw std::invalid_argument(why);}
template<class Rows,class Id> auto& find(Rows& rows,const Id& id) {
    const auto found=std::ranges::find_if(rows,[&](const auto& row){return row.id==id;});
    if(found==rows.end()) invalid("Choose an existing personal property or furnishing.");
    return *found;
}
OrdinalDay next_month(OrdinalDay day) {
    auto date=from_ordinal(day);
    if(++date.month>12) {date.month=1;++date.year;}
    date.day=1;
    return to_ordinal(date);
}
PersonalLifeState& life(SimulationState& state) {
    if(!state.succession || !state.succession->personal_life) invalid("Personal property is unavailable in this campaign.");
    return *state.succession->personal_life;
}
void spend(SimulationState& state,Cents amount) {
    if(amount>state.succession->personal_cash) invalid("Your personal savings cannot cover this payment.");
    state.succession->personal_cash=checked_unit_subtract(state.succession->personal_cash,amount);
    auto& owned=life(state);
    owned.spent=checked_unit_add(owned.spent,amount);
}
void bill(SimulationState& state,PersonalProperty& property,OrdinalDay day) {
    auto& owned=life(state);
    if(owned.bills.size()>=personal_bill_limit) invalid("The personal upkeep register is full.");
    const auto amount=find(personal_life_catalog(state).properties,property.offer).monthly_upkeep;
    PersonalBill entry{static_cast<std::uint32_t>(owned.bills.size()+1),property.id,day,amount,{}};
    if(owned.unpaid.value()==0 && amount<=state.succession->personal_cash) {
        spend(state,amount);
        entry.paid_on=state.date;
    } else owned.unpaid=checked_unit_add(owned.unpaid,amount);
    owned.bills.push_back(entry);
    property.next_upkeep=next_month(day);
}
bool overlap(int x,int y,int width,int depth,int other_x,int other_y,int other_width,int other_depth) {
    return x<other_x+other_width && x+width>other_x && y<other_y+other_depth && y+depth>other_y;
}
void text(const std::string& value) {
    if(value.empty() || value.size()>512 || std::ranges::any_of(value,[](unsigned char c){return c<32;}))
        invalid("Personal property labels must be bounded single-line text.");
}
void money(Cents value) {
    if(value.value()<0 || value.value()>100000000000000LL) invalid("Personal property money is outside its supported range.");
}
}

const PersonalLifeCatalog& personal_life_catalog(const SimulationState& state) {
    if(!state.management || !state.management->catalog.succession || !state.management->catalog.succession->personal_life)
        invalid("This campaign has no personal property catalog.");
    return *state.management->catalog.succession->personal_life;
}
Cents personal_resale(Cents price,BasisPoints resale) {
    return Cents{static_cast<std::int64_t>(mul_div_floor(static_cast<std::uint64_t>(price.value()),static_cast<std::uint64_t>(resale.value()),10000).quotient)};
}
void validate_personal_life_catalog(const PersonalLifeCatalog& catalog) {
    if(catalog.events)validate_personal_event_rules(*catalog.events);
    if(catalog.properties.empty() || catalog.properties.size()>8 || catalog.decor.empty() || catalog.decor.size()>16)
        invalid("The personal property catalog needs bounded property and furnishing offers.");
    std::set<std::string> ids;
    for(const auto& offer:catalog.properties) {
        text(offer.id);text(offer.name);text(offer.agent);text(offer.description);text(offer.plan.id);text(offer.plan.name);
        if(!ids.insert(offer.id).second || offer.id.size()>64 || offer.resale.value()<0 || offer.resale.value()>10000 || offer.price.value()==0)
            invalid("Personal property offers need unique IDs, positive prices and bounded resale values.");
        money(offer.price);money(offer.monthly_upkeep);
        validate_hq_plan(offer.plan.width,offer.plan.depth,offer.plan.rooms,32);
    }
    ids.clear();
    for(const auto& offer:catalog.decor) {
        text(offer.id);text(offer.name);text(offer.supplier);text(offer.kind);money(offer.price);
        if(!ids.insert(offer.id).second || offer.id.size()>64 || offer.price.value()==0 || offer.delivery.value()<0 || offer.delivery.value()>90 ||
            offer.width<1 || offer.width>8 || offer.depth<1 || offer.depth>8 || offer.resale.value()<0 || offer.resale.value()>10000 ||
            (offer.kind!="plant" && offer.kind!="sofa" && offer.kind!="table" && offer.kind!="painting" && offer.kind!="sculpture"))
            invalid("Personal furnishings need supported shapes, prices and delivery terms.");
    }
}

std::string personal_placement_blocker(const SimulationState& state,const PlacePersonalDecor& request) {
    if(!state.succession || !state.succession->personal_life) return "Personal furnishing is unavailable.";
    const auto& owned=*state.succession->personal_life;
    const auto& item=find(owned.decor,request.item);
    const auto& property=find(owned.properties,request.property);
    const auto& catalog=personal_life_catalog(state);
    if(item.sold_on) return "This furnishing has been sold.";
    if(property.sold_on) return "This property has been sold.";
    if(item.delivered_on>state.date) return "Wait for this furnishing to be delivered.";
    const auto& offer=find(catalog.decor,item.offer);
    const auto& plan=find(catalog.properties,property.offer).plan;
    const int width=request.rotated?offer.depth:offer.width, depth=request.rotated?offer.width:offer.depth;
    if(request.x<0 || request.y<0 || request.x>plan.width-width || request.y>plan.depth-depth)
        return "The furnishing must fit inside the property.";
    if(std::ranges::none_of(plan.rooms,[&](const auto& room){return request.x>=room.x && request.y>=room.y && request.x+width<=room.x+room.width && request.y+depth<=room.y+room.depth;}))
        return "Place the furnishing entirely inside one room.";
    for(const auto& other:owned.decor) if(other.id!=item.id && other.property==property.id) {
        const auto& shape=find(catalog.decor,other.offer);
        if(overlap(request.x,request.y,width,depth,other.x,other.y,other.rotated?shape.depth:shape.width,other.rotated?shape.width:shape.depth))
            return "Another furnishing occupies these blocks.";
    }
    return {};
}

void manage_personal_life(SimulationState& state,const PersonalLifeAction& action) {
    auto& owned=life(state);
    const auto& catalog=personal_life_catalog(state);
    std::visit([&](const auto& request) {
        using Action=std::decay_t<decltype(request)>;
        if constexpr(std::is_same_v<Action,BuyPersonalProperty>) {
            if(owned.unpaid.value()>0) invalid("Pay outstanding personal upkeep before making another purchase.");
            if(owned.properties.size()>=personal_property_limit) invalid("The personal property register is full.");
            const auto& offer=find(catalog.properties,request.offer);
            if(std::ranges::any_of(owned.properties,[&](const auto& property){return property.offer==offer.id && !property.sold_on;}))
                invalid("You already own this listed property.");
            if(checked_unit_add(offer.price,offer.monthly_upkeep)>state.succession->personal_cash)
                invalid("Your savings must cover the property and its first month of upkeep.");
            spend(state,offer.price);
            owned.properties.push_back({static_cast<std::uint32_t>(owned.properties.size()+1),offer.id,state.date,next_month(state.date),{}});
            bill(state,owned.properties.back(),state.date);
        } else if constexpr(std::is_same_v<Action,SellPersonalProperty>) {
            auto& property=find(owned.properties,request.property);
            if(property.sold_on) invalid("This property has already been sold.");
            if(std::ranges::any_of(owned.pending_events,[&](auto id){return owned.events[id-1].property==property.id;}))
                invalid("Cancel the gatherings booked at this home before selling it.");
            const auto& offer=find(catalog.properties,property.offer);
            const auto refund=personal_resale(offer.price,offer.resale);
            state.succession->personal_cash=checked_unit_add(state.succession->personal_cash,refund);
            owned.spent=checked_unit_subtract(owned.spent,refund);
            property.sold_on=state.date;
            for(auto& item:owned.decor) if(item.property==property.id) {item.property=0;item.x=0;item.y=0;item.rotated=false;}
        } else if constexpr(std::is_same_v<Action,BuyPersonalDecor>) {
            if(owned.unpaid.value()>0) invalid("Pay outstanding personal upkeep before making another purchase.");
            if(owned.decor.size()>=personal_item_limit) invalid("The personal furnishing register is full.");
            const auto& offer=find(catalog.decor,request.offer);
            const auto ready=advance_days(state.date,offer.delivery);
            if(ready>=state.end_date) invalid("Delivery would arrive after this campaign ends.");
            spend(state,offer.price);
            owned.decor.push_back({static_cast<std::uint32_t>(owned.decor.size()+1),offer.id,state.date,ready,0,0,0,false});
        } else if constexpr(std::is_same_v<Action,PlacePersonalDecor>) {
            const auto why=personal_placement_blocker(state,request);
            if(!why.empty()) throw std::invalid_argument(why);
            auto& item=find(owned.decor,request.item);
            item.property=request.property;item.x=request.x;item.y=request.y;item.rotated=request.rotated;
        } else if constexpr(std::is_same_v<Action,StorePersonalDecor>) {
            auto& item=find(owned.decor,request.item);
            if(item.sold_on) invalid("This furnishing has been sold.");
            item.property=0;item.x=0;item.y=0;item.rotated=false;
        } else if constexpr(std::is_same_v<Action,SellPersonalDecor>) {
            auto& item=find(owned.decor,request.item);
            if(item.sold_on || item.delivered_on>state.date) invalid("Choose a delivered furnishing you still own.");
            const auto& offer=find(catalog.decor,item.offer);
            const auto refund=personal_resale(offer.price,offer.resale);
            state.succession->personal_cash=checked_unit_add(state.succession->personal_cash,refund);
            owned.spent=checked_unit_subtract(owned.spent,refund);
            item.sold_on=state.date;item.property=0;item.x=0;item.y=0;item.rotated=false;
        } else if constexpr(std::is_same_v<Action,BookPersonalEvent>) {
            book_personal_event(state,request);
        } else if constexpr(std::is_same_v<Action,CancelPersonalEvent>) {
            cancel_personal_event(state,request.event);
        } else {
            if(owned.unpaid.value()==0) invalid("You have no unpaid personal upkeep.");
            spend(state,owned.unpaid);
            for(auto& entry:owned.bills) if(!entry.paid_on) entry.paid_on=state.date;
            owned.unpaid=Cents{0};
        }
    },action);
}

void prepare_personal_life(SimulationState& state) {
    if(!state.succession || !state.succession->personal_life || state.date>=state.end_date) return;
    for(auto& property:state.succession->personal_life->properties) if(!property.sold_on)
        while(property.next_upkeep<=state.date) bill(state,property,property.next_upkeep);
    prepare_personal_events(state);
}

PersonalValuation personal_valuation(const SimulationState& state,OrdinalDay before) {
    PersonalValuation result;
    result.spent=personal_tuition(state,before);
    if(!state.succession || !state.succession->personal_life) return result;
    const auto& owned=*state.succession->personal_life;
    const auto& catalog=personal_life_catalog(state);
    for(const auto& property:owned.properties) {
        if(property.purchased_on>=before) continue;
        const auto& offer=find(catalog.properties,property.offer);
        result.spent=checked_unit_add(result.spent,offer.price);
        const auto resale=personal_resale(offer.price,offer.resale);
        if(property.sold_on && *property.sold_on<before) result.spent=checked_unit_subtract(result.spent,resale);
        else result.assets=checked_unit_add(result.assets,resale);
    }
    for(const auto& item:owned.decor) if(item.purchased_on<before) {
        const auto& offer=find(catalog.decor,item.offer);
        result.spent=checked_unit_add(result.spent,offer.price);
        const auto resale=personal_resale(offer.price,offer.resale);
        if(item.sold_on && *item.sold_on<before) result.spent=checked_unit_subtract(result.spent,resale);
        else result.assets=checked_unit_add(result.assets,resale);
    }
    for(const auto& entry:owned.bills) if(entry.charged_on<before) {
        if(entry.paid_on && *entry.paid_on<before) result.spent=checked_unit_add(result.spent,entry.amount);
        else result.liabilities=checked_unit_add(result.liabilities,entry.amount);
    }
    for(const auto& event:owned.events) if(event.booked_on<before) {
        result.spent=checked_unit_add(result.spent,event.paid);
        if(event.resolved_on && *event.resolved_on<before)result.spent=checked_unit_subtract(result.spent,event.refund);
    }
    return result;
}

void validate_personal_life(const SimulationState& state,bool deep) {
    const auto& rules=succession_catalog(state);
    const auto& actor=*state.succession;
    if(rules.personal_life.has_value()!=actor.personal_life.has_value()) invalid("Personal property rules and ownership must be present together.");
    if(!actor.personal_life) return;
    const auto& owned=*actor.personal_life;
    if(owned.properties.size()>personal_property_limit || owned.decor.size()>personal_item_limit || owned.bills.size()>personal_bill_limit ||
        owned.spent.value()<0 || owned.unpaid.value()<0 || actor.personal_cash.value()<0)
        invalid("Personal finances or ownership exceed their supported bounds.");
    if(!deep) return;
    validate_personal_life_catalog(*rules.personal_life);
    validate_personal_events(state);
    const auto& catalog=*rules.personal_life;
    std::vector<OrdinalDay> expected;
    std::set<std::string> active;
    const auto valid_date=[&](OrdinalDay day){return day>=state.accounts->starts_on && day<=state.date && day<state.end_date;};
    for(const auto& property:owned.properties) {
        (void)find(catalog.properties,property.offer);
        if(property.id!=expected.size()+1 || !valid_date(property.purchased_on) ||
            (property.sold_on && (!valid_date(*property.sold_on) || *property.sold_on<property.purchased_on)) ||
            (!property.sold_on && !active.insert(property.offer).second)) invalid("Invalid personal property history.");
        expected.push_back(property.purchased_on);
    }
    std::size_t bill_id=0;
    for(const auto& entry:owned.bills) {
        if(entry.id!=++bill_id || entry.property==0 || entry.property>owned.properties.size()) invalid("An upkeep bill needs its real property.");
        const auto& property=owned.properties[entry.property-1];
        if(entry.charged_on!=expected[entry.property-1] || !valid_date(entry.charged_on) ||
            (property.sold_on && entry.charged_on>*property.sold_on) || entry.amount!=find(catalog.properties,property.offer).monthly_upkeep ||
            (entry.paid_on && (!valid_date(*entry.paid_on) || *entry.paid_on<entry.charged_on))) invalid("Personal upkeep must follow the disclosed monthly schedule.");
        expected[entry.property-1]=next_month(entry.charged_on);
    }
    for(const auto& property:owned.properties) {
        const auto through=std::min(property.sold_on.value_or(state.date),OrdinalDay{state.end_date.value()-1});
        if(property.next_upkeep!=expected[property.id-1] || property.next_upkeep<=through)
            invalid("Personal upkeep is missing or duplicated.");
    }
    std::size_t item_id=0;
    for(const auto& item:owned.decor) {
        const auto& offer=find(catalog.decor,item.offer);
        if(item.id!=++item_id || !valid_date(item.purchased_on) || item.delivered_on!=advance_days(item.purchased_on,offer.delivery) || item.delivered_on>=state.end_date ||
            (item.sold_on && (!valid_date(*item.sold_on) || *item.sold_on<item.delivered_on || item.property)) ||
            (!item.property && (item.x || item.y || item.rotated))) invalid("Invalid personal furnishing history.");
        if(item.property && !personal_placement_blocker(state,{item.id,item.property,item.x,item.y,item.rotated}).empty())
            invalid("A personal furnishing has an invalid placement.");
    }
    const auto totals=personal_valuation(state,OrdinalDay{state.date.value()+1});
    if(totals.spent!=owned.spent || totals.liabilities!=owned.unpaid) invalid("Personal spending must match purchases, sales and paid upkeep.");
}
}
