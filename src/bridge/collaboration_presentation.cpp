#include "world_controller.hpp"
#include "lab_inputs.hpp"
#include "flow_presentation.hpp"
#include <pgp/core/workplace_research.hpp>
#include <QLocale>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(std::string_view s){return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QString money(core::Cents c){return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(c.value())/100);}
runtime::CollaborationRequest request(const QVariantMap& draft) {
    return {draft.value("contact").toString().toStdString(),draft.value("topic").toString().toStdString(),draft.value("worker").toString().toStdString(),draft.value("source").toString().toStdString(),core::MilliCUPerDay{lab_inputs::compute(draft.value("rate","4").toString())}};
}
int progress(const core::CapacityProject& p) {
    const auto labor=p.required_staff_days?static_cast<int>(core::mul_div_floor(p.completed_staff_days,100,p.required_staff_days).quotient):100;
    const auto compute=p.total_work.value()?static_cast<int>(core::mul_div_floor(static_cast<std::uint64_t>(p.completed_work.value()),100,static_cast<std::uint64_t>(p.total_work.value())).quotient):100;
    return std::clamp(std::min(labor,compute),0,100);
}
}
QVariantMap WorldController::researchJams() const {
    if(!snapshot_)return {};const auto& w=snapshot_->checkpoint.world;const auto& s=w.simulation;
    if(!s.management || !s.management->people || !s.management->catalog.people->collaboration || !core::workplace_research_enabled(s) || !s.capacity || !s.accounts)return {};
    QVariantList topics,workers,sources,agreements;
    const auto title=[&](const std::string& id){const auto* text=snapshot_->content->collection.message(id+".title");return text?q(*text):readable(id);};
    for(const auto& topic:s.lab->catalog.research)topics.push_back(QVariantMap{{"id",q(topic.id)},{"name",title(topic.id)}});
    workers.push_back(QVariantMap{{"id",""},{"name",tr("Pick an available teammate")},{"detail",tr("A qualified person with a powered computer is required.")}});
    for(const auto& worker:core::research_workers(s))workers.push_back(QVariantMap{{"id",q(worker.id)},{"name",q(worker.name)},{"detail",worker.unavailable.empty()?tr("Ready for research"):q(worker.unavailable)}});
    sources.push_back(QVariantMap{{"id",""},{"name",tr("Use available company compute")}});
    for(const auto& source:s.capacity->sources)sources.push_back(QVariantMap{{"id",q(source.id)},{"name",tr("%1 · %2 CU/day installed").arg(readable(source.provider)).arg(core::company_capacity(source).value()/1000)}});
    if(w.competition)for(const auto& c:w.competition->collaborations) {
        if(c.first_company!=w.company_id && c.second_company!=w.company_id)continue;
        const auto* a=c.first_company==w.company_id?&s:nullptr;const core::SimulationState* b=nullptr;
        for(const auto& company:w.competition->companies)if(company.definition_id==c.second_company)b=&company.simulation;
        if(!a || !b)continue;
        const auto& first=*std::ranges::find(a->capacity->projects,c.first_project,&core::CapacityProject::id);
        const auto& second=*std::ranges::find(b->capacity->projects,c.second_project,&core::CapacityProject::id);
        const auto state=runtime::collaboration_status(w,c);
        QString status=c.published_on?tr("★ Published together"):state=="completed"?tr("★ Joint result ready"):state=="paused"?tr("Taking a breather · resume the paused work"):state=="working"?tr("Working together"):state=="cancelled"?tr("Stopped early"):state=="failed"?tr("Research setback"):tr("Partner lab unavailable");
        agreements.push_back(QVariantMap{{"id",q(c.id)},{"contact",q(c.contact)},{"name",title(c.topic)},{"status",status},{"progress",std::min(progress(first),progress(second))},
            {"firstProgress",progress(first)},{"secondProgress",progress(second)},{"canPublish",!c.published_on && state=="completed"},{"detail",tr("Your lab %1% · partner lab %2%\nCoordination paid %3 · joint-work credit %4%\nEach lab keeps its completed research. Both must finish before joint publication.").arg(progress(first)).arg(progress(second)).arg(money(c.coordination_fee)).arg(c.work_credit.value()/100)}});
    }
    return {{"topics",topics},{"workers",workers},{"sources",sources},{"agreements",agreements},{"revision",reviewToken()}};
}
QVariantMap WorldController::collaborationQuote(QVariantMap draft) const {
    if(!snapshot_)return {};
    try {
        const auto r=request(draft);const auto& w=snapshot_->checkpoint.world;
        if(!w.simulation.management || !w.simulation.management->people || !w.simulation.management->catalog.people->collaboration)return {{"valid",false},{"blocker",tr("Research jams are unavailable in this company.")}};
        const auto result=runtime::collaboration_quote(w,*snapshot_->content,w.company_id,r);
        QString partner;const auto& peers=w.simulation.management->people->industry;const auto it=peers.find(r.contact);
        if(it!=peers.end())partner=q(it->second.profile.name)+" · "+q(it->second.organization);
        QString teammate=tr("Available teammate");
        if(!result.first.researchers.empty())for(const auto& e:w.simulation.management->people->employees)if(e.terms.profile.id==result.first.researchers.front())teammate=q(e.terms.profile.name);
        return {{"valid",result.blocker.empty()},{"blocker",q(result.blocker)},{"partner",partner},{"fee",money(result.total_fee)},{"coordination",money(result.coordination_fee)},
            {"credit",result.work_credit.value()/100},{"firstDays",result.first_days},{"secondDays",result.second_days},
            {"summary",tr("Your teammate: %1 · %2 staff-days · %3 CU/day.\nPartner: one researcher · %4 staff-days · %5 CU/day.\nPay %6 now, including %7 coordination. Both labs also pay their own payroll and compute.\nJoint work saves up to %8% of labor and experiment budgets. Experiment discounts use the best available credit.").arg(teammate).arg(result.first_days).arg(static_cast<double>(result.first.daily_compute.value())/1000).arg(result.second_days).arg(static_cast<double>(result.second.daily_compute.value())/1000).arg(money(result.total_fee),money(result.coordination_fee)).arg(result.work_credit.value()/100)}};
    }catch(const std::exception& e){return {{"valid",false},{"blocker",q(e.what())}};}
}
void WorldController::collaborateResearch(QVariantMap draft,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review the jam again."));return;}
    try{emit collaborationRequested(request(draft));}catch(const std::exception& e){emit noticeRaised(q(e.what()));}
}
void WorldController::publishCollaboration(QString id,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review publication again."));return;}
    emit jointPublicationRequested(id);
}
}
