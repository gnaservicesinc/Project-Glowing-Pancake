#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
QString money(core::Cents amount) {return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(amount.value())/100);}
bool available(const CampaignSnapshotPtr& p) {return p && p->checkpoint.world.simulation.infrastructure && p->checkpoint.world.simulation.infrastructure->workplace.has_value();}
}
QString WorldController::headquartersSummary() const {
    if(!available(snapshot_)) return tr("This scenario has no headquarters construction plan. The founder campaign supports room expansion and repeatable facility blueprints.");
    const auto& s=snapshot_->checkpoint.world.simulation;
    return tr("%1 of %2 paid HQ workstations reserved. People in onboarding reserve their places. Completed rooms add space; recruitment supplies the people. Fit-out costs are paid now, monthly room costs begin when construction completes.")
        .arg(core::headquarters_seats_used(s)).arg(core::headquarters_seats(s));
}
QVariantList WorldController::roomOffers() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& offer:s.infrastructure->catalog.workplace->rooms) rows.push_back(QVariantMap{{"id",q(offer.id)},{"name",q(offer.name)},{"place",q(offer.place)},
        {"detail",q(offer.description)+tr("\n%1 fit-out · %2/month · %3 workstations\nReady %4 (%5 days)").arg(money(offer.construction),money(offer.monthly)).arg(offer.seats).arg(q(core::iso_date(core::advance_days(s.date,offer.lead)))).arg(offer.lead.value())}});
    return rows;
}
QVariantList WorldController::headquartersRooms() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& room:s.infrastructure->workplace->rooms) {
        const auto& offers=s.infrastructure->catalog.workplace->rooms;const auto it=std::ranges::find(offers,room.offer,&core::HeadquartersRoomOffer::id);if(it==offers.end()) continue;
        const auto rent=std::ranges::find(s.accounts->recurring,"hq.room/"+room.id,&core::RecurringObligation::id);
        const bool open=rent!=s.accounts->recurring.end() && (!rent->ends_before || *rent->ends_before>s.date);
        rows.push_back(QVariantMap{{"id",q(room.id)},{"name",q(it->name)},{"place",q(it->place)},{"ready",open && room.ready_on<=s.date},
            {"detail",!open?tr("Operations withdrawn"):room.ready_on>s.date?tr("Under construction · ready %1").arg(q(core::iso_date(room.ready_on))):tr("Open · %1 workstations · %2/month").arg(it->seats).arg(money(it->monthly))}});
    }
    return rows;
}
QVariantList WorldController::blueprintHardware() const {
    QVariantList rows;if(!available(snapshot_)) return rows;
    for(const auto& h:snapshot_->checkpoint.world.simulation.infrastructure->catalog.hardware) rows.push_back(QVariantMap{{"id",q(h.id)},{"name",q(h.name)},
        {"detail",tr("%1 purchase · %2/month · %3 operators · %4 kW IT power · %5 CU/day rated").arg(money(h.purchase_cost),money(h.monthly_maintenance)).arg(h.operators).arg(static_cast<double>(h.active_it_watts)/1000).arg(static_cast<double>(h.capacity.value())/1000)}});
    return rows;
}
QVariantList WorldController::blueprints() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& b:s.infrastructure->workplace->blueprints) {
        const auto count=std::ranges::count(s.infrastructure->workplace->builds,b.id,&core::BlueprintBuild::blueprint);
        rows.push_back(QVariantMap{{"id",q(b.id)},{"name",q(b.name)},{"detail",tr("%1 kW grid reservation · %2 · %3 dedicated operators · %4 cooling\nBuilt %5 times. Site construction, hardware and recruitment are paid for each instance.")
            .arg(b.grid_watts/1000).arg(q(b.hardware_family)).arg(b.operators).arg(b.evaporative?tr("evaporative"):tr("air")).arg(count)}});
    }
    return rows;
}
void WorldController::orderHeadquartersRoom(QString offer,QString revision) {
    if(!available(snapshot_)) return;
    if(revision!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the room cost and completion date again."));return;}
    const auto next=snapshot_->checkpoint.world.simulation.infrastructure->workplace->rooms.size()+1;
    emit actionRequested(core::ExpandHeadquarters{"room_"+std::to_string(next),offer.toStdString()});
}
void WorldController::createBlueprint(QString name,QString hardware,QString gridKw,QString operators,bool evaporative) {
    if(!available(snapshot_)) return;bool grid_ok=false,people_ok=false;
    const auto kw=gridKw.toUInt(&grid_ok),people=operators.toUInt(&people_ok);
    if(!grid_ok || kw>1000 || !people_ok || people>32) {emit noticeRaised(tr("Enter whole kW (up to 1,000) and an operator headcount up to 32."));return;}
    const auto next=snapshot_->checkpoint.world.simulation.infrastructure->workplace->blueprints.size()+1;
    emit actionRequested(core::CreateFacilityBlueprint{{"blueprint_"+std::to_string(next),name.trimmed().toStdString(),hardware.toStdString(),static_cast<std::uint64_t>(kw)*1000,people,evaporative}});
}
QVariantMap WorldController::blueprintQuote(QString blueprint,QString state) const {
    if(!available(snapshot_)) return {{"error",tr("No facility planning office is available.")}};
    try {
        const auto& s=snapshot_->checkpoint.world.simulation;const auto& list=s.infrastructure->workplace->blueprints;
        const auto it=std::ranges::find(list,blueprint.toStdString(),&core::FacilityBlueprint::id);if(it==list.end()) return {{"error",tr("Choose a saved blueprint.")}};
        const auto id="branch_"+std::to_string(s.infrastructure->sites.size()+1);const auto name=it->name+" / "+state.toStdString();
        const auto quote=core::preview_blueprint_build(s,{it->id,id,name,state.toStdString()});
        return {{"blueprint",blueprint},{"site",q(id)},{"name",q(name)},{"state",state},{"revision",reviewToken()},
            {"detail",tr("%1\nConstruction %2 · hardware %3 · recruitment %4\nTotal now %5 · fixed monthly costs %6, plus metered electricity/water\nBuilding %7 · grid %8 · hardware %9 · operators %10\nAn inspection is still required before commissioning. No capacity is granted at purchase.")
                .arg(q(name),money(quote.construction),money(quote.hardware),money(quote.recruitment),money(quote.total),money(quote.monthly),q(core::iso_date(quote.built_on)),q(core::iso_date(quote.grid_on)),q(core::iso_date(quote.delivery_on)),q(core::iso_date(quote.operators_on)))}};
    } catch(const std::exception& error) {return {{"error",QString::fromUtf8(error.what())}};}
}
void WorldController::buildBlueprint(QVariantMap reviewed) {
    if(!available(snapshot_)) return;
    if(reviewed.value("revision").toString()!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the state-specific facility quote again."));return;}
    emit actionRequested(core::BuildFromBlueprint{reviewed.value("blueprint").toString().toStdString(),reviewed.value("site").toString().toStdString(),reviewed.value("name").toString().toStdString(),reviewed.value("state").toString().toStdString()});
}
}
