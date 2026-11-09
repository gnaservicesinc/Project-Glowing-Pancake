#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
QString money(core::Cents c) {return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(c.value())/100);}
bool present(const CampaignSnapshotPtr& p) {return p && p->checkpoint.world.simulation.infrastructure && p->checkpoint.world.simulation.infrastructure->workplace && p->checkpoint.world.simulation.infrastructure->workplace->devices;}
}
bool WorldController::companyArchive() const {
    if(!snapshot_) return false;const auto& w=snapshot_->checkpoint.world;
    return w.simulation.phase==core::DayPhase::finished || w.simulation.phase==core::DayPhase::closed || runtime::objective_time_stopped(w) || runtime::observing_world(w);
}
QString WorldController::deviceSummary() const {
    if(present(snapshot_) && snapshot_->checkpoint.world.simulation.infrastructure->workplace->headquarters)
        return tr("Your phone and laptop travel with you. HQ desktop computers are actual equipment: buy them from Suppliers, wait for delivery and mount each on a furnished desk. They work only when placed and powered. Inherited company equipment is shown in Arrange headquarters. Portable apps and power controls remain reachable from the world dock.");
    return present(snapshot_)?tr("Your founding package includes a phone and laptop; shared and leased offices also include a workstation. Portable devices travel with you. Workstations stay at HQ. Apps use an available compatible device. Power, delivery and service determine availability. Equipment controls remain reachable from the world dock."):
        tr("This earlier scenario uses a fixed set of company devices.");
}
QVariantList WorldController::deviceInventory() const {
    QVariantList rows;if(!present(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;const bool headquarters=atHeadquarters();
    for(const auto& d:s.infrastructure->workplace->devices->records) {
        const auto& offers=s.infrastructure->catalog.workplace->devices->offers;const auto& model=*std::ranges::find(offers,d.offer,&core::DeviceOffer::id);
        const bool servicing=!d.services.empty() && d.services.back().ready_on>s.date;
        const auto status=d.retired_on?tr("Retired"):d.ready_on>s.date?tr("Delivery %1").arg(q(core::iso_date(d.ready_on))):servicing?tr("In service until %1").arg(q(core::iso_date(d.services.back().ready_on))):!d.powered?tr("Powered off"):core::device_available(s,d,headquarters)?tr("Ready here"):tr("At headquarters");
        const std::map<std::string,QString> names{{"office",tr("Company")},{"people",tr("People")},{"facilities",tr("HQ & blueprints")},{"lab",tr("Research")},{"inbox",tr("News & decisions")},{"operations",tr("Infrastructure & travel")},{"civic",tr("Public affairs")},{"society",tr("World affairs")},{"institutions",tr("Institutions")},{"frontier",tr("Frontier")},{"leadership",tr("Leadership")},{"objectives",tr("Mission")}};
        QStringList apps;for(const auto& app:model.apps) apps.push_back(names.at(app));
        rows.push_back(QVariantMap{{"id",QString::number(d.id)},{"kind",q(d.offer)},{"name",q(model.name)},{"powered",d.powered},
            {"available",core::device_available(s,d,headquarters)},{"canPower",!companyArchive() && !d.retired_on && d.ready_on<=s.date},
            {"canService",!d.retired_on && d.ready_on<=s.date && !servicing},{"canRetire",!d.retired_on && d.ready_on<=s.date && !servicing},
            {"revision",reviewToken()},{"apps",apps.join(", ")},{"serviceCost",money(model.service)},
            {"serviceDetail",tr("%1 service · %2 days offline").arg(money(model.service)).arg(model.service_lead.value())},
            {"detail",status+tr(" · %1\n%2").arg(model.portable?tr("Portable"):tr("HQ workstation"),d.included?tr("Included in founding package"):tr("Company-owned purchase"))}});
    }return rows;
}
QVariantList WorldController::deviceOffers() const {
    QVariantList rows;if(!present(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& d:s.infrastructure->catalog.workplace->devices->offers) {
        if(d.id=="desktop" && s.infrastructure->workplace->headquarters) continue;
        rows.push_back(QVariantMap{{"id",q(d.id)},{"name",q(d.name)},{"revision",reviewToken()},
        {"detail",tr("%1 paid now · %2\nDelivery %3 · %4 days\n%5 service · %6 days offline").arg(money(d.purchase),d.portable?tr("Portable"):tr("At HQ only"),q(core::iso_date(core::advance_days(s.date,d.delivery)))).arg(d.delivery.value()).arg(money(d.service)).arg(d.service_lead.value())}});
    }
    return rows;
}
bool WorldController::appAvailable(QString app,QString preferred) const {
    if(app=="devices" || companyArchive()) return true;
    if(!present(snapshot_)) return true;
    const auto& s=snapshot_->checkpoint.world.simulation;
    return !core::resolve_app_device(s,preferred.toStdString(),app.toStdString(),atHeadquarters()).empty();
}
void WorldController::buyDevice(QString offer,QString revision) {
    if(!present(snapshot_)) return;
    if(revision!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the equipment quote again."));return;}
    emit actionRequested(core::BuyPersonalDevice{offer.toStdString()});
}
void WorldController::serviceDevice(QString id,QString revision) {
    if(!present(snapshot_)) return;
    if(revision!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the service quote again."));return;}
    bool okay=false;const auto key=id.toULongLong(&okay);if(okay) emit actionRequested(core::ServicePersonalDevice{key});
}
void WorldController::powerDevice(QString id,bool powered) {
    if(!present(snapshot_)) return;bool okay=false;const auto key=id.toULongLong(&okay);if(okay) emit actionRequested(core::SetDevicePower{key,powered});
}
void WorldController::retireDevice(QString id,QString revision) {
    if(!present(snapshot_)) return;
    if(revision!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the device again."));return;}
    bool okay=false;const auto key=id.toULongLong(&okay);if(okay) emit actionRequested(core::RetirePersonalDevice{key});
}
}
