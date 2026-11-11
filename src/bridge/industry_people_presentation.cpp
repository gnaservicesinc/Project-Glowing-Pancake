#include "world_controller.hpp"
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
        QString city,state;for(const auto& market:markets)if(market.id==premises.market){city=text(market.city);state=text(market.state);}
        QVariantList roster;
        for(const auto& employee:s.management->people->employees) {
            if(employee.departed_on)continue;const auto& p=employee.terms.profile;
            QString portrait=QStringLiteral("image://portraits/%1").arg(p.portrait.kit);
            for(const auto part:p.portrait.parts)portrait+='/'+QString::number(part);
            for(const auto part:p.portrait.style)portrait+='/'+QString::number(part);
            QStringList skills;if(const auto* rules=core::talent_rules(s))for(const auto& skill:rules->skills)if(core::has_skill(p,skill.id))skills.push_back(text(skill.name));
            roster.push_back(QVariantMap{{"id",text(rival.definition_id)+"/"+text(p.id)},{"name",text(p.name)},{"portrait",portrait},{"role",text(p.role)},
                {"detail",text(p.pronouns)+" · "+text(p.education)+"\n"+skills.join(" · ")}});
        }
        result.push_back(QVariantMap{{"id",text(rival.definition_id)},{"name",text(definition.name)},{"city",city},{"state",state},{"people",roster}});
    }
    return result;
}
}
