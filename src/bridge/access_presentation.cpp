#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
QString money(core::Cents c) {return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(c.value())/100);}
bool present(const CampaignSnapshotPtr& p) {return p && p->checkpoint.world.simulation.infrastructure && p->checkpoint.world.simulation.infrastructure->workplace && p->checkpoint.world.simulation.infrastructure->workplace->access;}
}
bool WorldController::nationalAccess() const {return !snapshot_ || core::national_access(snapshot_->checkpoint.world.simulation);}
bool WorldController::worldAccess() const {return !snapshot_ || core::world_access(snapshot_->checkpoint.world.simulation);}
QString WorldController::accessSummary() const {
    if(!present(snapshot_)) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& a=*s.infrastructure->workplace->access;const auto& r=*s.infrastructure->catalog.workplace->access;
    const std::map<std::string,QString> causes{{"background",tr("your founding background")},{"staff",tr("productive staff")},{"revenue",tr("earned company revenue")},{"publications",tr("published research")}};
    const auto national=a.national?tr("National contacts opened %1 through %2.").arg(q(core::iso_date(a.national->day)),causes.at(a.national->cause)):
        tr("National contacts: staff %1/%2 · earned revenue %3/%4 · publications %5/%6. Reach any one threshold. Rich families and wealthy/political early opportunities begin connected.")
            .arg(s.capacity?s.capacity->available_staff:0).arg(r.productive_staff).arg(money(core::accounts_report(*s.accounts).revenue),money(r.earned_revenue)).arg(s.lab?s.lab->publications.size():0).arg(r.publications);
    const auto world=a.first_overseas_opened?tr("World contacts opened %1 with your first overseas office.").arg(q(core::iso_date(*a.first_overseas_opened))):tr("World contacts open when your first overseas liaison office begins operating.");
    return national+"\n"+world+"\n"+tr("Public information, laws and responses to existing obligations remain available. Contacts grant no automatic contract, vote, authority or market share.");
}
QVariantList WorldController::overseasOfficeOffers() const {
    QVariantList rows;if(!present(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& o:s.infrastructure->catalog.workplace->access->offices) rows.push_back(QVariantMap{{"id",q(o.id)},{"name",q(o.name)},{"revision",QString::number(s.revision)},
        {"detail",tr("%1 setup paid now · %2/month payroll and rent\n%3 local employees · opens %4 (%5 days)\nA liaison office supplies contacts and local representation; it supplies no compute or HQ workstations.")
            .arg(money(o.setup),money(o.monthly)).arg(o.people).arg(q(core::iso_date(core::advance_days(s.date,o.lead)))).arg(o.lead.value())}});
    return rows;
}
QVariantList WorldController::overseasOffices() const {
    QVariantList rows;if(!present(snapshot_)) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    for(const auto& o:s.infrastructure->workplace->access->offices) {
        const auto& offers=s.infrastructure->catalog.workplace->access->offices;const auto& model=*std::ranges::find(offers,o.offer,&core::LiaisonOfficeOffer::id);
        const bool open=core::liaison_office_open(s,o,s.date);
        const auto status=o.closed_on?tr("Closed %1").arg(q(core::iso_date(*o.closed_on))):o.ready_on>s.date?tr("Opening %1").arg(q(core::iso_date(o.ready_on))):open?tr("Operating"):tr("Operations withdrawn");
        rows.push_back(QVariantMap{{"id",QString::number(o.id)},{"name",q(model.name)},{"canClose",!o.closed_on},{"revision",QString::number(s.revision)},
            {"detail",status+tr(" · %1 local employees · %2/month payroll and rent").arg(open?model.people:0).arg(money(model.monthly))}});
    }return rows;
}
void WorldController::openOverseasOffice(QString offer,QString revision) {
    if(!present(snapshot_)) return;
    if(revision!=QString::number(snapshot_->checkpoint.world.simulation.revision)) {emit noticeRaised(tr("The company changed. Review the office setup, payroll and opening date again."));return;}
    emit actionRequested(core::OpenLiaisonOffice{offer.toStdString()});
}
void WorldController::closeOverseasOffice(QString id,QString revision) {
    if(!present(snapshot_)) return;
    if(revision!=QString::number(snapshot_->checkpoint.world.simulation.revision)) {emit noticeRaised(tr("The company changed. Review the office again."));return;}
    bool okay=false;const auto key=id.toULongLong(&okay);if(okay) emit actionRequested(core::CloseLiaisonOffice{key});
}
}
