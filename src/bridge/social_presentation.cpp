#include "world_controller.hpp"
#include <algorithm>
#include <pgp/runtime/romance.hpp>
#include <map>
#include <set>
namespace pgp::bridge {
namespace {QString q(const std::string& value){return QString::fromStdString(value);}}
QVariantMap WorldController::relationships() const {
    if(!snapshot_)return {};const auto& s=snapshot_->checkpoint.world.simulation;const auto* rules=core::social_rules(s);if(!rules)return {};
    const auto& social=*s.management->people->social;QVariantList people,nodes,edges;
    std::map<std::string,QVariantMap> identities;
    auto me=founder();me["id"]="founder";identities.emplace("founder",me);
    for(const auto& employee:s.management->people->employees) {
        const auto& p=employee.terms.profile;if(p.role=="Founder / researcher")continue;
        auto card=person(q(p.id),s.date);identities.emplace(p.id,card);
        const auto* bond=core::social_link(s,"founder",p.id);const int affinity=bond?bond->affinity:0;
        QStringList known;for(const auto& t:core::visible_traits(s,p))known.push_back(q(t));
        const auto status=core::friendship_name(s,affinity,!employee.departed_on);
        int next=0;for(const auto threshold:rules->thresholds)if(threshold>affinity){next=threshold;break;}
        card["relationship"]=q(status);card["coworker"]=!employee.departed_on;card["affinity"]=affinity;card["next"]=next;
        card["traits"]=known.join(" · ");card["memory"]=bond?q(bond->memory):tr("You have not spent time together yet.");
        card["blocker"]=q(core::social_blocker(s,p.id));card["moodBonus"]=core::friendship_mood(s,p.id);
        card["lastContact"]=bond?q(core::iso_date(bond->last_contact)):QString{};
        people.push_back(card);
    }
    for(const auto& [id,peer]:s.management->people->industry) {
        const auto& p=peer.profile;QString portrait=QStringLiteral("image://portraits/%1").arg(p.portrait.kit);
        for(const auto part:p.portrait.parts)portrait+='/'+QString::number(part);for(const auto part:p.portrait.style)portrait+='/'+QString::number(part);
        QStringList skills;if(const auto* talents=core::talent_rules(s))for(const auto& skill:talents->skills)if(core::has_skill(p,skill.id))skills.push_back(q(skill.name));
        auto card=QVariantMap{{"id",q(id)},{"name",q(p.name)},{"portrait",portrait},{"role",q(p.role)},{"detail",q(p.pronouns)+" · "+q(p.education)+"\n"+skills.join(" · ")}};
        identities.emplace(id,card);
        const auto* bond=core::social_link(s,"founder",id);
        if(!bond)continue;
        QStringList known;for(const auto& trait:core::visible_traits(s,p,id))known.push_back(q(trait));
        int next=0;for(const auto threshold:rules->thresholds)if(threshold>bond->affinity){next=threshold;break;}
        card["relationship"]=q(core::friendship_name(s,bond->affinity,false));card["coworker"]=false;card["external"]=true;card["affinity"]=bond->affinity;card["next"]=next;
        card["organization"]=q(peer.organization)+" · "+q(peer.city);card["distance"]=q(core::social_distance(s,"founder",id));
        card["traits"]=known.join(" · ");card["memory"]=q(bond->memory);card["blocker"]=q(core::social_blocker(s,id));card["moodBonus"]=0;people.push_back(card);
    }
    for(auto& value:people) {
        auto card=value.toMap();const auto id=card.value("id").toString().toStdString();
        card["introduction"]=!core::social_introduction(s,id).empty();card["referralBlocker"]=q(core::social_referral_blocker(s,id));value=card;
    }
    std::set<std::string> visible{"founder"};
    for(const auto& bond:social.links)if(bond.known && bond.affinity>=rules->thresholds[3] && identities.contains(bond.first) && identities.contains(bond.second)) {
        visible.insert(bond.first);visible.insert(bond.second);
        edges.push_back(QVariantMap{{"first",q(bond.first)},{"second",q(bond.second)},{"label",q(core::friendship_name(s,bond.affinity,false))},
            {"names",identities.at(bond.first).value("name").toString()+" ↔ "+identities.at(bond.second).value("name").toString()},{"strength",bond.affinity}});
    }
    if(const auto& w=snapshot_->checkpoint.world;w.romance) {
        std::map<std::string,std::string> socialIds;for(const auto& [id,card]:identities){(void)card;socialIds.emplace(runtime::romance_person_id(w,id),id);}
        for(const auto& [key,bond]:w.romance->active) {
            (void)key;if(!bond.known || !socialIds.contains(bond.first) || !socialIds.contains(bond.second))continue;
            const auto first=socialIds.at(bond.first),second=socialIds.at(bond.second);
            const auto label=bond.married_on?tr("Spouses"):bond.engaged_on?tr("Engaged"):tr("Dating");
            auto existing=std::find_if(edges.begin(),edges.end(),[&](const auto& value){const auto edge=value.toMap();return (edge.value("first")==q(first) && edge.value("second")==q(second)) || (edge.value("first")==q(second) && edge.value("second")==q(first));});
            if(existing!=edges.end()){auto row=existing->toMap();row["label"]=row.value("label").toString()+" · "+label;*existing=row;}
            else edges.push_back(QVariantMap{{"first",q(first)},{"second",q(second)},{"label",label},{"names",identities.at(first).value("name").toString()+" ↔ "+identities.at(second).value("name").toString()},{"strength",bond.affection}});
            visible.insert(first);visible.insert(second);
        }
    }
    const auto album=familyAlbum();std::map<std::string,std::string> familyIds;
    for(const auto& [id,card]:identities){(void)card;familyIds.emplace(runtime::romance_person_id(snapshot_->checkpoint.world,id),id);}
    for(const auto& value:album.value("cards").toList()){auto card=value.toMap();const auto id=card.value("id").toString().toStdString();const auto canonical=runtime::romance_person_id(snapshot_->checkpoint.world,id);if(!familyIds.contains(canonical)){familyIds.emplace(canonical,id);identities.emplace(id,card);}}
    for(const auto& value:album.value("edges").toList()) {
        auto edge=value.toMap();const auto a=edge.value("first").toString().toStdString(),b=edge.value("second").toString().toStdString();
        if(!familyIds.contains(a) || !familyIds.contains(b))continue;
        const auto first=familyIds.at(a),second=familyIds.at(b);edge["first"]=q(first);edge["second"]=q(second);edges.push_back(edge);visible.insert(first);visible.insert(second);
    }
    for(const auto& id:visible){auto row=identities.at(id);row["id"]=q(id);nodes.push_back(row);}
    return {{"people",people},{"nodes",nodes},{"edges",edges},{"moodCap",rules->mood_cap},{"interval",rules->interval_days},{"revision",reviewToken()}};
}
void WorldController::socialize(QString person,QString activity,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your plans again."));return;}
    emit actionRequested(core::Socialize{person.toStdString(),activity.toStdString()});
}
}
