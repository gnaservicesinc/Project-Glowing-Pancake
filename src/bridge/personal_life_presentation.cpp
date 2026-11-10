#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
#include <set>

namespace pgp::bridge {
namespace {
QString q(const std::string& value) {return QString::fromStdString(value);}
QString money(core::Cents amount) {
    const QLocale locale{QLocale::English,QLocale::UnitedStates};
    return QStringLiteral("$%1.%2").arg(locale.toString(amount.value()/100)).arg(amount.value()%100,2,10,QLatin1Char('0'));
}
bool available(const CampaignSnapshotPtr& snapshot) {
    return snapshot && snapshot->checkpoint.world.simulation.succession && snapshot->checkpoint.world.simulation.succession->personal_life;
}
QString visit_dates(const core::FamilyVisit& trip) {
    return QObject::tr("Depart %1 · Arrive %2\nLeave %3 · Return to headquarters %4")
        .arg(q(core::iso_date(trip.departs_on)),q(core::iso_date(trip.arrives_on)),q(core::iso_date(trip.leaves_on)),q(core::iso_date(trip.returns_on)));
}
template<class Rows,class Id> const auto& find(const Rows& rows,const Id& id) {
    const auto found=std::ranges::find_if(rows,[&](const auto& row){return row.id==id;});
    if(found==rows.end())throw std::invalid_argument("Choose an available offer.");
    return *found;
}
}
QVariantMap WorldController::personalLife() const {
    if(!available(snapshot_)) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto& owned=*state.succession->personal_life;
    const auto& catalog=core::personal_life_catalog(state);
    const auto valuation=core::personal_valuation(state,core::OrdinalDay{state.date.value()+1});
    QVariantList properties,offers,decor,items;
    for(const auto& offer:catalog.properties) {
        const auto total=core::checked_unit_add(offer.price,offer.monthly_upkeep);
        offers.push_back(QVariantMap{{"id",q(offer.id)},{"name",q(offer.name)},{"agent",q(offer.agent)},{"description",q(offer.description)},
            {"cost",money(total)},{"width",offer.plan.width},{"depth",offer.plan.depth},{"rooms",static_cast<int>(offer.plan.rooms.size())},
            {"detail",tr("Purchase %1 + first month %2. Upkeep %2 each month. Resale %3. The current month is charged in full.")
                .arg(money(offer.price),money(offer.monthly_upkeep),money(core::personal_resale(offer.price,offer.resale)))},
            {"owned",std::ranges::any_of(owned.properties,[&](const auto& property){return property.offer==offer.id && !property.sold_on;})}});
    }
    for(const auto& property:owned.properties) {
        const auto& offer=find(catalog.properties,property.offer);
        properties.push_back(QVariantMap{{"id",property.id},{"name",q(offer.name)},{"sold",property.sold_on.has_value()},
            {"upkeep",money(offer.monthly_upkeep)},{"resale",money(core::personal_resale(offer.price,offer.resale))},
            {"detail",property.sold_on?tr("Sold %1").arg(q(core::iso_date(*property.sold_on))):
                tr("Owned since %1 · %2/month · next upkeep %3").arg(q(core::iso_date(property.purchased_on)),money(offer.monthly_upkeep),q(core::iso_date(property.next_upkeep)))}});
    }
    for(const auto& offer:catalog.decor)
        decor.push_back(QVariantMap{{"id",q(offer.id)},{"name",q(offer.name)},{"supplier",q(offer.supplier)},{"kind",q(offer.kind)},
            {"price",money(offer.price)},{"detail",tr("%1 × %2 m · %3-day delivery · resale value %4")
                .arg(offer.width).arg(offer.depth).arg(offer.delivery.value()).arg(money(core::personal_resale(offer.price,offer.resale)))}});
    for(const auto& item:owned.decor) {
        if(item.sold_on)continue;
        const auto& offer=find(catalog.decor,item.offer);
        items.push_back(QVariantMap{{"id",item.id},{"name",q(offer.name)},{"kind",q(offer.kind)},
            {"delivered",item.delivered_on<=state.date},{"property",item.property},{"rotated",item.rotated},{"resale",money(core::personal_resale(offer.price,offer.resale))},
            {"detail",item.delivered_on>state.date?tr("Arrives %1").arg(q(core::iso_date(item.delivered_on))):
                item.property?tr("Placed in %1").arg(q(find(catalog.properties,owned.properties[item.property-1].offer).name)):tr("Delivered · in personal storage")}});
    }
    return {{"cash",money(state.succession->personal_cash)},{"assets",money(valuation.assets)},{"unpaid",money(owned.unpaid)},
        {"hasUnpaid",owned.unpaid.value()>0},{"properties",properties},{"offers",offers},{"decor",decor},{"items",items}};
}

QVariantMap WorldController::personalFloor(int property) const {
    if(!available(snapshot_) || property<=0) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto& owned=*state.succession->personal_life;
    if(static_cast<std::size_t>(property)>owned.properties.size() || owned.properties[static_cast<std::size_t>(property-1)].sold_on) return {};
    const auto& catalog=core::personal_life_catalog(state);
    const auto& property_row=owned.properties[static_cast<std::size_t>(property-1)];
    const auto& offer=find(catalog.properties,property_row.offer);
    QVariantList rooms,items;
    for(const auto& room:offer.plan.rooms) rooms.push_back(QVariantMap{{"x",room.x},{"y",room.y},{"width",room.width},{"depth",room.depth}});
    for(const auto& item:owned.decor) {
        if(item.sold_on)continue;
        const auto& shape=find(catalog.decor,item.offer);
        items.push_back(QVariantMap{{"id",item.id},{"kind",q(shape.kind)},{"name",q(shape.name)},{"placed",item.property==property_row.id},
            {"mount",0},{"powered",true},{"x",item.x},{"y",item.y},{"width",item.rotated?shape.depth:shape.width},{"depth",item.rotated?shape.width:shape.depth}});
    }
    return {{"name",q(offer.name)},{"width",offer.plan.width},{"depth",offer.plan.depth},{"kind","home"},{"rooms",rooms},{"items",items}};
}

QVariantMap WorldController::personalPlacement(int item,int property,int x,int y,bool rotated) const {
    if(!available(snapshot_) || item<=0 || property<=0) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto& owned=*state.succession->personal_life;
    if(static_cast<std::size_t>(item)>owned.decor.size() || static_cast<std::size_t>(property)>owned.properties.size()) return {};
    const auto& offer=find(core::personal_life_catalog(state).decor,owned.decor[static_cast<std::size_t>(item-1)].offer);
    const auto why=core::personal_placement_blocker(state,{static_cast<std::uint32_t>(item),static_cast<std::uint32_t>(property),x,y,rotated});
    return {{"valid",why.empty()},{"reason",why.empty()?tr("Click to place"):q(why)},{"x",x},{"y",y},
        {"width",rotated?offer.depth:offer.width},{"depth",rotated?offer.width:offer.depth}};
}

void WorldController::personalAction(core::PersonalLifeAction action,QString revision) {
    if(!available(snapshot_)) return;
    if(companyArchive()) {emit noticeRaised(tr("This company is no longer accepting new commitments."));return;}
    if(revision!=reviewToken()) {emit noticeRaised(tr("Your circumstances changed. Review this personal action again."));return;}
    emit actionRequested(core::ManagePersonalLife{std::move(action)});
}
void WorldController::buyPersonalProperty(QString offer,QString revision) {personalAction(core::BuyPersonalProperty{offer.toStdString()},revision);}
void WorldController::sellPersonalProperty(int property,QString revision) {if(property>0)personalAction(core::SellPersonalProperty{static_cast<std::uint32_t>(property)},revision);}
void WorldController::buyPersonalDecor(QString offer,QString revision) {personalAction(core::BuyPersonalDecor{offer.toStdString()},revision);}
void WorldController::placePersonalDecor(int item,int property,int x,int y,bool rotated,QString revision) {
    if(item>0 && property>0)personalAction(core::PlacePersonalDecor{static_cast<std::uint32_t>(item),static_cast<std::uint32_t>(property),x,y,rotated},revision);
}
void WorldController::storePersonalDecor(int item,QString revision) {if(item>0)personalAction(core::StorePersonalDecor{static_cast<std::uint32_t>(item)},revision);}
void WorldController::sellPersonalDecor(int item,QString revision) {if(item>0)personalAction(core::SellPersonalDecor{static_cast<std::uint32_t>(item)},revision);}
void WorldController::payPersonalUpkeep(QString revision) {personalAction(core::PayPersonalUpkeep{},revision);}

QVariantMap WorldController::familyVisit() const {
    if(!available(snapshot_)) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto& rules=core::personal_life_catalog(state).family_visits;
    if(!rules || core::family_home_state(state).empty()) return {};
    QVariantList stays;
    for(auto days=1;days<=rules->maximum_stay.value();++days) stays.push_back(QVariantMap{{"id",days},{"name",tr("%n day(s) at home",nullptr,days)}});
    QVariantMap result{{"stays",stays},{"active",false}};
    const auto& rows=state.succession->personal_life->family_visits;
    if(rows.empty()) return result;
    const auto& trip=rows.back();
    if(!core::pending_family_visit(state)) {
        result["status"]=trip.cancelled_on?tr("Visit canceled. %1 returned to personal savings.").arg(money(trip.fare)):
            tr("Back at headquarters · %1").arg(q(core::iso_date(trip.returns_on)));
        return result;
    }
    const auto day=state.date;
    result["active"]=true;
    result["canCancel"]=day<trip.departs_on;
    result["atHome"]=day>=trip.arrives_on && day<trip.leaves_on;
    result["stage"]=day<trip.departs_on?0:day<trip.arrives_on?1:day<trip.leaves_on?2:3;
    result["status"]=day<trip.departs_on?tr("Visit booked"):
        day<trip.arrives_on?tr("Traveling to the family home"):day<trip.leaves_on?tr("You’re at the family home"):tr("Returning to headquarters");
    result["advance"]=day<trip.departs_on && trip.departs_on<trip.arrives_on?tr("Advance to departure"):
        day<trip.arrives_on?tr("Advance to arrival"):day<trip.leaves_on && trip.leaves_on<trip.returns_on?tr("Advance to the return trip"):tr("Advance to return");
    result["dates"]=visit_dates(trip);
    result["fare"]=money(trip.fare);
    return result;
}
QVariantMap WorldController::familyVisitQuote(int stay) const {
    if(!available(snapshot_)) return {};
    try {
        const auto& state=snapshot_->checkpoint.world.simulation;
        const auto trip=core::family_visit_quote(state,core::Days{stay});
        const auto affordable=state.succession->personal_cash>=trip.fare;
        return {{"valid",affordable},{"dates",visit_dates(trip)},{"fare",money(trip.fare)},
            {"reason",affordable?QString{}:tr("Your personal savings cannot cover this fare.")}};
    } catch(const std::exception& error) {return {{"valid",false},{"reason",QString::fromUtf8(error.what())}};}
}
void WorldController::bookFamilyVisit(int stay,QString revision) {
    if(stay>0 && stay<=14) personalAction(core::BookFamilyVisit{core::Days{stay}},revision);
}
void WorldController::cancelFamilyVisit(QString revision) {personalAction(core::CancelFamilyVisit{},revision);}

QVariantMap WorldController::personalEvents() const {
    if(!available(snapshot_))return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto& catalog=core::personal_life_catalog(state);
    if(!catalog.events)return {};
    const auto& rules=*catalog.events;
    const auto& owned=*state.succession->personal_life;
    QVariantList packages,venues,history;
    for(const auto& row:rules.offers)packages.push_back(QVariantMap{{"id",q(row.id)},{"name",q(row.name)},{"venue",q(row.venue)},
        {"description",q(row.description)},{"date",q(core::iso_date(core::advance_days(state.date,row.notice)))},
        {"terms",tr("%1 booking + %2 per invited guest, up to %3. Allow %4 days' notice. Attending employees receive +%5 mood for %6 days. The strongest recent gathering applies.")
            .arg(money(row.booking),money(row.per_guest)).arg(row.guests).arg(row.notice.value()).arg(row.mood).arg(row.duration.value())}});
    for(const auto& property:owned.properties)if(!property.sold_on)
        venues.push_back(QVariantMap{{"id",property.id},{"name",q(find(catalog.properties,property.offer).name)}});
    const auto known=contacts();
    const auto name=[&](const std::string& id){
        for(const auto& value:known)if(value.toMap().value("id")==q(id))return value.toMap().value("name").toString();
        return q(id);
    };
    std::set<std::uint32_t> displayed(owned.pending_events.begin(),owned.pending_events.end());
    for(auto it=owned.events.rbegin();it!=owned.events.rend() && std::distance(owned.events.rbegin(),it)<32;++it)displayed.insert(it->id);
    for(auto id=displayed.rbegin();id!=displayed.rend();++id) {
        const auto& event=owned.events[*id-1];
        const auto& package=find(rules.offers,event.offer);
        QVariantList guests;
        for(const auto& guest:event.guests) {
            QString portrait;
            for(const auto& value:known)if(value.toMap().value("id")==q(guest.person))portrait=value.toMap().value("portrait").toString();
            guests.push_back(QVariantMap{{"id",q(guest.person)},{"name",name(guest.person)},{"portrait",portrait},
            {"status",guest.attended?tr("Attended"):guest.reply=="declined"?tr("Declined"):guest.reply=="unavailable"?tr("Unavailable"):
                event.outcome=="scheduled"?tr("Accepted"):event.outcome=="hosted"?tr("Could not attend"):tr("Did not attend")}});
        }
        const auto status=event.outcome=="scheduled"?tr("Scheduled"):event.outcome=="hosted"?tr("Hosted"):
            event.outcome=="cancelled"?tr("Cancelled"):event.outcome=="venue_changed"?tr("Office changed · fully refunded"):tr("Host was away");
        const auto attended=std::ranges::count(event.guests,true,&core::PersonalEventGuest::attended);
        const auto story=event.outcome=="hosted"?tr("The evening is over. %1 of your %2 invited contacts joined you. Attending colleagues carry the good mood back to work for %3 days.")
            .arg(attended).arg(event.guests.size()).arg(package.duration.value()):
            event.outcome=="host_absent"?tr("Your travel or on-site appointment kept you away. The booked service was paid, and the gathering ended without its host."):QString{};
        history.push_back(QVariantMap{{"id",event.id},{"name",q(package.name)},{"day",q(core::iso_date(event.day))},{"status",status},
            {"venue",event.property?q(find(catalog.properties,owned.properties[event.property-1].offer).name):tr("Headquarters at booking")},
            {"guests",guests},{"paid",money(event.paid)},{"refund",money(event.refund)},{"pending",!event.resolved_on},{"story",story},
            {"cancellation",money(core::personal_resale(event.paid,package.cancellation_refund))}});
    }
    return {{"packages",packages},{"venues",venues},{"guests",known},{"history",history},{"upcoming",!owned.pending_events.empty()},
        {"summary",tr("Pay from personal savings at booking, including guests who decline. Leave %1 days between gatherings and book within %2 days. Cancel before the date for the package refund. Keep the host free of travel and on-site appointments. A moved office receives a full refund on the event date.")
            .arg(rules.interval.value()).arg(rules.maximum_notice.value())}};
}
QVariantMap WorldController::personalEventQuote(QString offer,int property,QString day,QStringList guests) const {
    if(!available(snapshot_) || property<0)return {};
    try {
        std::vector<std::string> ids;for(const auto& id:guests)ids.push_back(id.toStdString());
        const auto& state=snapshot_->checkpoint.world.simulation;
        const auto& catalog=core::personal_life_catalog(state);
        if(!catalog.events)return {};
        const auto& package=find(catalog.events->offers,offer.toStdString());
        const auto cost=core::personal_event_quote(state,{offer.toStdString(),static_cast<std::uint32_t>(property),core::parse_iso_date(day.toStdString()),std::move(ids)});
        return {{"valid",cost<=state.succession->personal_cash},{"cost",money(cost)},
            {"refund",money(core::personal_resale(cost,package.cancellation_refund))},
            {"reason",cost<=state.succession->personal_cash?tr("Ready to send invitations"):tr("Your personal savings cannot cover this gathering.")}};
    }catch(const std::exception& error){return {{"valid",false},{"reason",QString::fromUtf8(error.what())}};}
}
void WorldController::bookPersonalEvent(QString offer,int property,QString day,QStringList guests,QString revision) {
    if(property<0)return;
    try {
        std::vector<std::string> ids;for(const auto& id:guests)ids.push_back(id.toStdString());
        personalAction(core::BookPersonalEvent{offer.toStdString(),static_cast<std::uint32_t>(property),core::parse_iso_date(day.toStdString()),std::move(ids)},revision);
    }catch(const std::exception& error){emit noticeRaised(QString::fromUtf8(error.what()));}
}
void WorldController::cancelPersonalEvent(int event,QString revision) {if(event>0)personalAction(core::CancelPersonalEvent{static_cast<std::uint32_t>(event)},revision);}
}
