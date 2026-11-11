#include "world_controller.hpp"
#include <algorithm>
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
    std::set<std::string> visible{"founder"};
    for(const auto& bond:social.links)if(bond.known && bond.affinity>=rules->thresholds[3] && identities.contains(bond.first) && identities.contains(bond.second)) {
        visible.insert(bond.first);visible.insert(bond.second);
        edges.push_back(QVariantMap{{"first",q(bond.first)},{"second",q(bond.second)},{"label",q(core::friendship_name(s,bond.affinity,false))},
            {"names",identities.at(bond.first).value("name").toString()+" ↔ "+identities.at(bond.second).value("name").toString()},{"strength",bond.affinity}});
    }
    for(const auto& id:visible){auto row=identities.at(id);row["id"]=q(id);nodes.push_back(row);}
    return {{"people",people},{"nodes",nodes},{"edges",edges},{"moodCap",rules->mood_cap},{"interval",rules->interval_days},{"revision",reviewToken()}};
}
void WorldController::socialize(QString person,QString activity,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your plans again."));return;}
    emit actionRequested(core::Socialize{person.toStdString(),activity.toStdString()});
}
}
