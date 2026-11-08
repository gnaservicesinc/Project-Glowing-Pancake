#include "world_controller.hpp"
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
bool present(const CampaignSnapshotPtr& p) {return p && p->checkpoint.world.simulation.infrastructure && p->checkpoint.world.simulation.infrastructure->workplace && p->checkpoint.world.simulation.infrastructure->workplace->departments;}
QString percent(core::BasisPoints x) {return QString::number(static_cast<double>(x.value())/100,'f',2)+"%";}
}
QVariantList WorldController::managementSources() const {
    QVariantList rows;if(!present(snapshot_)) return rows;
    for(const auto& s:snapshot_->checkpoint.world.simulation.capacity->sources) {
        const auto label=s.id=="lease.initial" || s.id=="lease.founder"?tr("Founding compute lease"):q(s.id);
        rows.push_back(QVariantMap{{"id",q(s.id)},{"name",label+tr(" · %1 CU/day effective").arg(static_cast<double>(s.effective.value())/1000,0,'f',2)}});
    }return rows;
}
QVariantMap WorldController::operationsManagement() const {
    if(!present(snapshot_)) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& r=*s.infrastructure->catalog.workplace->departments;
    const auto* appointment=core::operations_manager_on(s,s.date);QString name=tr("No dedicated HQ manager"),person,source;
    if(appointment) {person=q(appointment->person);source=q(appointment->source);for(const auto& e:s.management->people->employees) if(e.terms.profile.id==appointment->person) name=q(e.terms.profile.name);}
    return {{"person",person},{"source",source},{"name",name},{"availability",percent(core::operations_availability(s,s.date))},
        {"detail",tr("Local engineering skill %1/100. A dedicated HQ manager reserves one person and %2 CU/day from an existing compute source, while their agreed salary continues. Engineering skill and actual management work from the previous day improve branch availability. Hardware, power, operators and inspection still limit output. Only company-owned compute facilities receive this coordination benefit; liaison offices keep separate representation teams.")
            .arg(r.operator_skill).arg(static_cast<double>(r.management_compute.value())/1000,0,'f',2)}};
}
QVariantMap WorldController::managerQuote(QString person,QString source) const {
    if(!present(snapshot_)) return {{"error",tr("This scenario has no HQ operations management plan.")}};
    const auto& s=snapshot_->checkpoint.world.simulation;
    try {
        core::Simulation preview{s};const auto result=preview.submit({s.revision,core::AssignOperationsManager{person.toStdString(),source.toStdString()}});
        if(!result.accepted()) return {{"error",q(result.explanation)}};
        const auto& r=*s.infrastructure->catalog.workplace->departments;
        QString detail;
        if(person.isEmpty()) detail=tr("Release the HQ manager and their daily staff/compute reservation. The employee stays on the payroll and returns to other work. Owned facilities return to their local team's availability; physical operator and power requirements continue.");
        else {
            const auto& candidates=s.management->people->employees;const auto it=std::ranges::find(candidates,person.toStdString(),[](const auto& e){return e.terms.profile.id;});
            const auto baseline=static_cast<std::uint64_t>(r.minimum_availability.value())+core::mul_div_floor(static_cast<std::uint64_t>(r.operator_skill_credit.value()),r.operator_skill,100).quotient;
            const auto ceiling=std::min<std::uint64_t>(10000,baseline+core::mul_div_floor(static_cast<std::uint64_t>(r.manager_credit.value()),it->terms.profile.engineering,100).quotient);
            detail=tr("Assign %1 to HQ operations. Engineering %2/100.\nReserve one existing HQ worker and %3 CU/day using %4. Their current salary continues; assignment has no signing fee.\nLocal-team availability: %5. With a fully delivered management day: up to %6, beginning the next morning. Partial delivery yields a smaller benefit. Hardware, power and operator requirements still apply.")
                .arg(q(it->terms.profile.name)).arg(it->terms.profile.engineering).arg(static_cast<double>(r.management_compute.value())/1000,0,'f',2).arg(source=="lease.initial" || source=="lease.founder"?tr("the founding compute lease"):source,percent(core::BasisPoints{baseline}),percent(core::BasisPoints{ceiling}));
        }
        return {{"person",person},{"source",source},{"revision",reviewToken()},{"detail",detail}};
    }catch(const std::exception& error) {return {{"error",QString::fromUtf8(error.what())}};}
}
void WorldController::assignOperationsManager(QVariantMap reviewed) {
    if(!present(snapshot_)) return;
    if(reviewed.value("revision").toString()!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the manager's staff, compute and availability tradeoff again."));return;}
    emit actionRequested(core::AssignOperationsManager{reviewed.value("person").toString().toStdString(),reviewed.value("source").toString().toStdString()});
}
}
