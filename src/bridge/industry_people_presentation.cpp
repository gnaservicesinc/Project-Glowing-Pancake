#include "world_controller.hpp"
#include "portrait_url.hpp"
#include <pgp/runtime/competition.hpp>
namespace pgp::bridge {
QVariantList WorldController::industryPeople() const {
    QVariantList result;if(!snapshot_)return result;
    const auto& w=snapshot_->checkpoint.world;if(!w.competition)return result;
    const auto text=[](const std::string& value){return QString::fromStdString(value);};
    for(const auto& rival:w.competition->companies) {
        const auto& s=rival.simulation;
        if(!s.management || !s.management->people || !s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->headquarters)continue;
        const auto& definition=runtime::rival_definition(*program_,rival.definition_id);
        const auto& premises=core::current_headquarters(s);
        const auto& markets=s.infrastructure->catalog.workplace->headquarters->markets;
        QString city,state,stateName;for(const auto& market:markets)if(market.id==premises.market){city=text(market.city);state=text(market.state);}
        for(const auto& region:s.infrastructure->catalog.states)if(text(region.id)==state)stateName=text(region.name);
        QVariantList roster;
        for(const auto& employee:s.management->people->employees) {
            if(employee.departed_on)continue;const auto& p=employee.terms.profile;
            const QString portrait=portrait_url(p,s.date);
            QStringList skills;if(const auto* rules=core::talent_rules(s))for(const auto& skill:rules->skills)if(core::has_skill(p,skill.id))skills.push_back(text(skill.name));
            roster.push_back(QVariantMap{{"id",text(rival.definition_id)+"/"+text(p.id)},{"name",text(p.name)},{"portrait",portrait},{"role",text(p.role)},{"status",employee.ready_on>s.date?tr("Onboarding until %1").arg(text(core::iso_date(employee.ready_on))):QString{}},
                {"socialBlocker",text(core::social_blocker(w.simulation,rival.definition_id+"/"+p.id))},{"detail",text(p.pronouns)+" · "+text(p.education)+"\n"+skills.join(" · ")}});
        }
        result.push_back(QVariantMap{{"id",text(rival.definition_id)},{"name",text(definition.name)},{"city",city},{"state",state},{"stateName",stateName},{"people",roster}});
    }
    return result;
}
}
