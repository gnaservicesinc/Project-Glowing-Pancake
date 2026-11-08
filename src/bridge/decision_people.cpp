#include "world_controller.hpp"
#include <QDate>
#include <QLocale>
#include <algorithm>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString portrait(const core::PortraitRecipe& recipe) {
    QString result=QStringLiteral("image://portraits/%1").arg(recipe.kit);
    for(auto part:recipe.parts) result+='/'+QString::number(part);
    return result;
}
QVariantMap profile(const core::PersonProfile& p,core::OrdinalDay day,QString status) {
    return {{"id",text(p.id)},{"name",text(p.name)},{"role",text(p.role)},{"portrait",portrait(p.portrait)},{"bio",text(p.bio)},
        {"detail",QObject::tr("%1 · %2 · Age %3\n%4\nResearch %5 · Engineering %6 · Evaluation %7\n%8")
            .arg(text(p.gender),text(p.pronouns)).arg(core::from_ordinal(day).year-p.birth_year).arg(text(p.education))
            .arg(p.research).arg(p.engineering).arg(p.evaluation).arg(status)}};
}
QString message(const content::Collection& collection,const std::string& key) {
    auto locale=QLocale().name();locale.replace('_','-');
    const auto* value=collection.message(key,locale.toStdString());return value?text(*value):QString{};
}
}
QVariantMap WorldController::person(QString id,core::OrdinalDay day) const {
    if(!snapshot_) return {};
    if(id=="founder") return founder();
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(id=="leadership.ceo" && s.succession) {
        const auto& terms=s.succession->terms;
        const auto found=std::ranges::find_if(terms,[&](const auto& t){return t.starts_on<=day && (!t.ended_on || day<*t.ended_on);});
        if(found==terms.end()) return {};
        if(found->person=="founder") return founder();
        return {{"id",QStringLiteral("leadership/%1").arg(found->id)},{"name",text(found->name)},{"role",tr("Chief executive")},
            {"portrait",found->portrait?portrait(*found->portrait):QString{}},
            {"detail",tr("Born %1 · appointed %2\nExecutive term through %3\n%4")
                .arg(found->birth_year).arg(text(core::iso_date(found->starts_on)),text(core::iso_date(found->ended_on.value_or(found->due_on))),
                    text(core::leadership_candidate(core::succession_catalog(s),found->person).priorities))}};
    }
    if(!s.management) return {};
    if(s.management->people) {
        const auto& people=*s.management->people;
        if(const auto known=people.known_people.find(id.toStdString());known!=people.known_people.end()) {
            auto row=profile(known->second,day,tr("Company hiring record"));row["id"]=id;return row;
        }
        for(const auto& employee:people.employees) if(text(employee.terms.profile.id)==id)
            return profile(employee.terms.profile,day,employee.departed_on && *employee.departed_on<=day?tr("Former employee"):tr("HQ employee"));
        for(const auto& applicant:people.applicants) if(text(applicant.profile.id)==id) return profile(applicant.profile,day,tr("Applicant"));
    }
    if(s.management->catalog.governance) for(const auto& p:s.management->catalog.governance->executives)
        if(text(p.id)==id) return {{"id",id},{"name",text(p.name)},{"role",text(p.role)},{"detail",text(p.priorities)}};
    for(const auto& p:s.management->catalog.specialists) if(text(p.id)==id)
        return {{"id",id},{"name",text(p.name)},{"role",tr("Evaluation specialist")}};
    return {};
}
QVariantList WorldController::decisionParticipants(QString kind,std::uint64_t presentation) const {
    QVariantList result;if(!snapshot_ || !program_) return result;
    const auto& checkpoint=snapshot_->checkpoint;const auto& s=checkpoint.world.simulation;
    const auto add=[&](QVariantMap row) {
        if(row.isEmpty() || std::ranges::any_of(result,[&](const auto& existing){return existing.toMap().value("id")==row.value("id");})) return;
        result.push_back(std::move(row));
    };
    if(presentation) {
        const auto found=std::ranges::find(checkpoint.runtime.presentations,presentation,&runtime::Presentation::id);
        if(found==checkpoint.runtime.presentations.end()) return result;
        const auto* definition=program_->collection.find(found->flow_id);
        if(!definition) return result;
        const auto* flow=std::get_if<content::FlowDefinition>(&definition->data);
        if(!flow || !flow->nodes.contains(found->node_id)) return result;
        const auto& refs=flow->nodes.at(found->node_id).participants;
        for(const auto& ref:refs) {
            QVariantMap row;
            if(ref.kind==content::ParticipantKind::founder) row=founder();
            else if(ref.kind==content::ParticipantKind::employee) row=person(text(ref.id),found->day);
            else {
                row={{"id",text(ref.id)},{"name",message(program_->collection,ref.name_key)},
                    {"kind",ref.kind==content::ParticipantKind::organization?"organization":"contact"}};
                if(ref.portrait) row["portrait"]=portrait(*ref.portrait);
            }
            if(!row.isEmpty() && !ref.role_key.empty()) row["role"]=message(program_->collection,ref.role_key);
            add(std::move(row));
        }
        if(!refs.empty()) return result;
    }
    add(founder());
    if(kind=="leadership") add(person("leadership.ceo",s.date));
    if((kind=="governance" || kind=="executive") && s.management && s.management->governance)
        for(const auto& dispute:s.management->governance->disputes) if(!dispute.acknowledged) {add(person(text(dispute.subject),s.date));break;}
    return result;
}
}
