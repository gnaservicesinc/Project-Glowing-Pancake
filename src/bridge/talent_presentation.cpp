#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(const std::string& s){return QString::fromStdString(s);}
QString money(std::int64_t cents){return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(cents)/100);}
core::SoftwareDesign design(const QVariantMap& m) {
    core::SoftwareDesign d;d.brief=m.value("brief").toString().toStdString();d.name=m.value("name").toString().trimmed().toStdString();
    d.aesthetic=m.value("aesthetic","calm").toString().toStdString();d.navigation=m.value("navigation","sidebar").toString().toStdString();d.focus=m.value("focus","balanced").toString().toStdString();
    for(const auto& id:m.value("workers").toStringList())d.workers.push_back(id.toStdString());d.previous=m.value("previous",0).toUInt();d.ai_source=m.value("ai_source").toString().toStdString();return d;
}
}
QVariantMap WorldController::talent() const {
    if(!snapshot_)return {};const auto& s=snapshot_->checkpoint.world.simulation;const auto* rules=core::talent_rules(s);if(!rules)return {};
    const auto& t=*s.management->people->talent;QVariantList people,skills,courses,briefs,projects,sources;
    sources.push_back(QVariantMap{{"id",""},{"name",tr("Human team · no AI compute")}});
    for(const auto& source:s.capacity->sources)sources.push_back(QVariantMap{{"id",q(source.id)},{"name",q(source.provider)+" · "+q(source.id)}});
    const auto name=[&](const std::string& id){if(id=="founder")return founder().value("name").toString();for(const auto& e:s.management->people->employees)if(e.terms.profile.id==id)return q(e.terms.profile.name);return q(id);};
    const auto addPerson=[&](const std::string& id,QString label) {
        QStringList known,ids;for(const auto& key:core::person_skills(s,id)){ids.push_back(q(key));const auto skill=std::ranges::find(rules->skills,key,&core::SkillDefinition::id);known.push_back(skill==rules->skills.end()?q(key):q(skill->name));}
        QString learningBudget;double learningCents=0;
        if(const auto fund=t.study_funds.find(id);fund!=t.study_funds.end()) {learningCents=static_cast<double>(core::checked_unit_subtract(core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(fund->second.compensation.value()),static_cast<std::uint64_t>(rules->employee_study_share_bp),10000).quotient},fund->second.spent).value());learningBudget=money(core::checked_unit_subtract(core::Cents{core::mul_div_floor(static_cast<std::uint64_t>(fund->second.compensation.value()),static_cast<std::uint64_t>(rules->employee_study_share_bp),10000).quotient},fund->second.spent).value());}
        people.push_back(QVariantMap{{"id",q(id)},{"name",label},{"skills",ids},{"learningBudget",learningBudget},{"learningCents",learningCents},{"detail",known.isEmpty()?tr("No acquired professional skills yet."):known.join(" · ")},{"blocker",q(core::software_worker_blocker(s,id))}});
    };
    addPerson("founder",founder().value("name").toString());
    for(const auto& e:s.management->people->employees)if(!e.departed_on && e.terms.profile.role!="Founder / researcher")addPerson(e.terms.profile.id,q(e.terms.profile.name));
    for(const auto& skill:rules->skills) {
        QString detail=q(skill.description);if(skill.teachable)detail+=tr("\n%1 · %2 days, part-time. %3").arg(money(skill.tuition_cents)).arg(skill.course_days).arg(skill.prerequisite.empty()?tr("No prerequisite."):tr("Requires %1.").arg(q(std::ranges::find(rules->skills,skill.prerequisite,&core::SkillDefinition::id)->name)));
        if(skill.engineering || skill.research)detail+=tr(" For working HQ staff: +%1 engineering, +%2 research on completion (up to 100).").arg(skill.engineering).arg(skill.research);
        skills.push_back(QVariantMap{{"id",q(skill.id)},{"name",q(skill.name)},{"detail",detail},{"tuitionCents",static_cast<double>(skill.tuition_cents)},{"teachable",skill.teachable},{"prerequisite",q(skill.prerequisite)}});
    }
    for(const auto& c:t.courses)courses.push_back(QVariantMap{{"id",c.id},{"name",name(c.person)+" · "+q(std::ranges::find(rules->skills,c.skill,&core::SkillDefinition::id)->name)},
        {"completed",c.completed},{"progress",static_cast<int>(std::clamp<std::int64_t>((s.date.value()-c.started_on.value())*100/(c.completes_on.value()-c.started_on.value()),0,100))},{"detail",tr("%1 · %2 · %3").arg(c.payer=="company"?tr("Company sponsored"):c.person=="founder"?tr("Your personal tuition"):tr("Employee self-funded"),c.completed?tr("Completed"):tr("Studying until"),q(core::iso_date(c.completes_on)))}});
    for(const auto& b:rules->software){QStringList features;for(const auto& f:b.features)features.push_back(q(f));briefs.push_back(QVariantMap{{"id",q(b.id)},{"name",q(b.name)},{"description",q(b.description)},{"advanced",b.advanced},{"features",features},{"fee",money(b.setup_cents)}});}
    for(const auto& p:t.software) {
        const auto& w=*std::ranges::find(s.capacity->projects,p.project,&core::CapacityProject::id);
        const auto status=w.status==core::ProjectStatus::completed?(p.released_on?tr("Released design"):tr("Ready to release")):w.status==core::ProjectStatus::paused?tr("Paused"):w.status==core::ProjectStatus::cancelled?tr("Cancelled"):tr("In development");
        projects.push_back(QVariantMap{{"id",p.id},{"name",q(p.design.name)},{"brief",q(p.design.brief)},{"aesthetic",q(p.design.aesthetic)},{"navigation",q(p.design.navigation)},{"focus",q(p.design.focus)},
            {"released",p.released_on.has_value()},{"canRelease",!p.released_on && w.status==core::ProjectStatus::completed},{"canPause",w.status==core::ProjectStatus::active},{"canCancel",w.status==core::ProjectStatus::active || w.status==core::ProjectStatus::paused},{"canResume",w.status==core::ProjectStatus::paused},
            {"detail",tr("%1 · %2 / %3 staff-days\nUsability %4 · Reliability %5 · Capability %6\n%7").arg(status).arg(w.completed_staff_days).arg(w.required_staff_days).arg(p.usability).arg(p.reliability).arg(p.capability).arg(q(p.blocker))}});
    }
    return {{"employeeStudyShare",rules->employee_study_share_bp/100},{"employeeStudyInterval",rules->employee_study_interval_days},{"aiLevel",core::software_ai_level(s)},{"aiCompute",rules->ai_compute_milli/1000},{"aiWorkPercent",rules->ai_work_percent},{"aiSources",sources},{"maximumDesignBonus",rules->market_bonus_max_bp/100},{"people",people},{"skills",skills},{"courses",courses},{"briefs",briefs},{"projects",projects},{"companyCash",money(core::uncommitted_cash(s).value())},{"personalCash",s.succession?money(s.succession->personal_cash.value()):QString{}},{"revision",reviewToken()}};
}
QVariantMap WorldController::courseQuote(QString person,QString skill,QString payer) const {
    if(!snapshot_)return {};
    const auto quote=core::course_quote(snapshot_->checkpoint.world.simulation,{person.toStdString(),skill.toStdString(),payer.toStdString()});
    return {{"fee",money(quote.fee.value())},{"budget",money(quote.budget.value())},{"blocker",q(quote.blocker)},{"valid",quote.blocker.empty()}};
}
QVariantMap WorldController::softwareQuote(QVariantMap draft) const {
    if(!snapshot_)return {};const auto& s=snapshot_->checkpoint.world.simulation;
    const auto quote=core::software_quote(s,design(draft));
    return {{"usability",quote.usability},{"reliability",quote.reliability},{"capability",quote.capability},{"work",quote.staff_days},{"fee",money(quote.fee.value())},{"blocker",q(quote.blocker)},{"valid",quote.blocker.empty()},{"revision",reviewToken()}};
}
void WorldController::talentAction(core::TalentAction action,QString revision) {
    if(!snapshot_)return;
    if(companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review this action again."));return;}
    emit actionRequested(core::ManageTalent{std::move(action)});
}
void WorldController::enrollSkill(QString person,QString skill,QString payer,QString revision){talentAction(core::EnrollSkill{person.toStdString(),skill.toStdString(),payer.toStdString()},revision);}
void WorldController::startSoftware(QVariantMap draft,QString revision){talentAction(core::StartSoftware{design(draft)},revision);}
void WorldController::releaseSoftware(int id,QString revision){if(id>0)talentAction(core::ReleaseSoftware{static_cast<std::uint32_t>(id)},revision);}
void WorldController::stopSoftware(int id,bool cancel,QString revision) {
    if(!snapshot_ || id<=0 || companyArchive() || revision!=reviewToken())return;
    const auto& s=snapshot_->checkpoint.world.simulation;if(!core::talent_rules(s))return;
    const auto& rows=s.management->people->talent->software;
    const auto found=std::ranges::find(rows,static_cast<std::uint32_t>(id),&core::SoftwareProject::id);if(found==rows.end())return;
    if(cancel)emit actionRequested(core::CancelProject{found->project});else emit actionRequested(core::PauseProject{found->project});
}
void WorldController::resumeSoftware(int id,QString revision){if(id>0)talentAction(core::ResumeSoftware{static_cast<std::uint32_t>(id)},revision);}
}
