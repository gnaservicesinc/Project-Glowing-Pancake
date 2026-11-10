#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>

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
template<class Rows,class Id> const auto& find(const Rows& rows,const Id& id) {
    return *std::ranges::find_if(rows,[&](const auto& row){return row.id==id;});
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
}
