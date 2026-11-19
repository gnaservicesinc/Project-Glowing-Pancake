#include "world_controller.hpp"
#include "portrait_url.hpp"
#include <algorithm>
#include <pgp/runtime/romance.hpp>
#include <map>
#include <set>
namespace pgp::bridge {
namespace {QString q(const std::string& value){return QString::fromStdString(value);}}
QVariantMap WorldController::deskColleague() const {
    if(!snapshot_ || !atHeadquarters()) return {};
    const auto& world=snapshot_->checkpoint.world;
    const auto& s=world.simulation;
    if(!core::social_rules(s) || !s.infrastructure || !s.infrastructure->workplace ||
       !s.infrastructure->workplace->headquarters) return {};
    std::vector<const core::PersonProfile*> present;
    for(const auto& employee:s.management->people->employees) {
        const auto& profile=employee.terms.profile;
        if(!employee.departed_on && employee.ready_on<=s.date && profile.role!="Founder / researcher" &&
           core::assigned_hq_desk(s,profile.id)) present.push_back(&profile);
    }
    if(present.empty()) return {};
    const auto days=static_cast<std::size_t>(s.date.value()-world.scope_created.value());
    return deskPerson(q(present[days % present.size()]->id));
}
QVariantMap WorldController::deskPerson(QString id) const {
    if(!snapshot_ || !atHeadquarters()) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(!core::social_rules(s) || !core::assigned_hq_desk(s,id.toStdString())) return {};
    const auto& employees=s.management->people->employees;
    const auto employee=std::ranges::find_if(employees,[&](const auto& row) {
        return !row.departed_on && row.ready_on<=s.date && q(row.terms.profile.id)==id &&
               row.terms.profile.role!="Founder / researcher";
    });
    if(employee==employees.end()) return {};
    const auto& profile=employee->terms.profile;
    auto card=person(id,s.date);
    const auto* bond=core::social_link(s,"founder",profile.id);
    card["spokeToday"]=bond && bond->last_invited==s.date;
    card["memory"]=bond?q(bond->memory):QString{};
    card["blocker"]=q(core::social_blocker(s,profile.id));
    return card;
}
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
        const auto* concern=core::mission_concern(s,p.id);
        const bool rival=core::is_personal_rival(s,"founder",p.id);
        const bool frenemy=concern && concern->frenemy;
        const auto status=rival?tr("Rivals"):frenemy?tr("Frenemies"):q(core::friendship_name(s,affinity,!employee.departed_on));
        card["rival"]=rival;
        if(concern && !concern->settled_on) {
            const auto& policy=*core::mission_conflict_rules(s);
            const auto deadline=concern->rivalry_on?
                core::advance_days(concern->heard_on.value_or(*concern->rivalry_on),core::Days{policy.notice_days}):
                core::advance_days(concern->raised_on,core::Days{policy.concern_days});
            card["concern"]=QVariantMap{{"before",q(concern->previous_mission)},{"after",q(concern->new_mission)},
                {"deadline",q(core::iso_date(deadline))},{"noticeDue",s.date>=deadline},{"rival",rival},{"heard",concern->heard_on.has_value()},
                {"resigned",employee.resigned},{"atWork",!employee.departed_on},{"busyToday",social.last_outing==s.date},
                {"blocker",q(core::mission_response_blocker(s,{p.id,{}}))}};
        }
        int next=0;for(const auto threshold:rules->thresholds)if(threshold>affinity){next=threshold;break;}
        card["relationship"]=status;card["coworker"]=!employee.departed_on;card["affinity"]=affinity;card["next"]=next;
        card["traits"]=known.join(" · ");card["memory"]=bond?q(bond->memory):tr("You have not spent time together yet.");
        card["blocker"]=q(core::social_blocker(s,p.id));card["moodBonus"]=core::friendship_mood(s,p.id);
        card["lastContact"]=bond?q(core::iso_date(bond->last_contact)):QString{};
        people.push_back(card);
    }
    for(const auto& [id,peer]:s.management->people->industry) {
        const auto& p=peer.profile;const QString portrait=portrait_url(p,s.date);
        QStringList skills;if(const auto* talents=core::talent_rules(s))for(const auto& skill:talents->skills)if(core::has_skill(p,skill.id))skills.push_back(q(skill.name));
        auto card=QVariantMap{{"id",q(id)},{"name",q(p.name)},{"portrait",portrait},{"role",q(p.role)},{"detail",q(p.pronouns)+" · "+q(p.education)+"\n"+skills.join(" · ")}};
        identities.emplace(id,card);
        const auto* bond=core::social_link(s,"founder",id);
        const bool hostile=core::is_personal_rival(s,"founder",id);
        if(!bond && !hostile)continue;
        if(!bond) {
            QStringList known;for(const auto& trait:core::visible_traits(s,p,id))known.push_back(q(trait));
            card["rival"]=true;card["relationship"]=tr("Rivals");card["coworker"]=false;card["external"]=true;card["affinity"]=0;card["next"]=0;
            card["organization"]=q(peer.organization)+" · "+q(peer.city);card["distance"]=q(core::social_distance(s,"founder",id));
            card["traits"]=known.join(" · ");card["memory"]=tr("Targeted you in school; kept their distance.");
            card["blocker"]=q(core::social_blocker(s,id));card["moodBonus"]=0;people.push_back(card);
            continue;
        }
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
    QVariantList mediators;
    if(social.mission_conflict) {
        for(const auto& worker:s.management->people->employees) {
            const auto& person=worker.terms.profile;
            if(core::mission_mediator_available(s,person.id))mediators.push_back(QVariantMap{{"id",q(person.id)},{"name",q(person.name)}});
        }
        for(const auto& [id,concern]:social.mission_conflict->concerns) {
            if(!identities.contains(id) || (!concern.frenemy && !core::is_personal_rival(s,"founder",id)))continue;
            const auto label=concern.frenemy?tr("Frenemies"):tr("Rivals");
            auto existing=std::find_if(edges.begin(),edges.end(),[&](const auto& value){
                const auto edge=value.toMap();
                return (edge.value("first")==q(id) && edge.value("second")=="founder") ||
                       (edge.value("second")==q(id) && edge.value("first")=="founder");
            });
            if(existing!=edges.end()) {
                auto edge=existing->toMap();
                edge["label"]=edge.value("label").toString()+" · "+label;
                *existing=edge;
            } else {
                edges.push_back(QVariantMap{{"first","founder"},{"second",q(id)},{"label",label},
                    {"names",me.value("name").toString()+" ↔ "+identities.at(id).value("name").toString()},{"strength",0}});
            }
            visible.insert(id);
        }
    }
    for(const auto& id:social.hostile_peers) {
        if(!identities.contains(id))continue;
        const auto existing=std::find_if(edges.begin(),edges.end(),[&](const auto& value){
            const auto edge=value.toMap();
            return (edge.value("first")==q(id) && edge.value("second")=="founder") ||
                   (edge.value("second")==q(id) && edge.value("first")=="founder");
        });
        if(existing==edges.end()) {
            edges.push_back(QVariantMap{{"first","founder"},{"second",q(id)},{"label",tr("School bully · Rivals")},
                {"names",me.value("name").toString()+" ↔ "+identities.at(id).value("name").toString()},{"strength",0}});
        }
        visible.insert(id);
    }
    for(const auto& id:visible){auto row=identities.at(id);row["id"]=q(id);nodes.push_back(row);}
    return {{"people",people},{"nodes",nodes},{"edges",edges},{"mission",social.mission_conflict?q(social.mission_conflict->mission):QString{}},{"mediators",mediators},{"moodCap",rules->mood_cap},{"interval",rules->interval_days},{"revision",reviewToken()}};
}
QStringList WorldController::missionPreview(QString mission) const {
    if(!snapshot_)return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    QStringList names;
    for(const auto& id:core::mission_pivot_concerns(state,mission.toStdString()))
        names.push_back(person(q(id),state.date).value("name").toString());
    return names;
}

void WorldController::changeCompanyMission(QString mission,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){
        emit noticeRaised(tr("Review the current company mission."));
        return;
    }
    emit actionRequested(core::ChangeCompanyMission{mission.toStdString()});
}

void WorldController::respondToMissionConcern(QString person,QString mediator,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){
        emit noticeRaised(tr("Review the latest conversation."));
        return;
    }
    emit actionRequested(core::RespondToMissionConcern{person.toStdString(),mediator.toStdString()});
}

QVariantMap WorldController::companyAuthority() const {
    if(!snapshot_) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;
    const auto auth=core::company_authority(s);
    const auto money=[](core::Cents v) {
        const auto mag=v.value()<0?-v.value():v.value();
        auto n=QString::number(mag/100);
        for(auto i=n.size()-3;i>0;i-=3) n.insert(i,QLatin1Char(','));
        return (v.value()<0?QStringLiteral("−$"):QStringLiteral("$"))+n+QStringLiteral(".%1").arg(mag%100,2,10,QLatin1Char('0'));
    };
    const auto* scandal=core::mission_scandal(s);
    const bool inherited=core::mission_conflict_rules(s) && s.management->people->social->mission_conflict->inherited_purpose;
    const auto personal_cash=s.succession?s.succession->personal_cash:core::Cents{0};
    QVariantMap out{
        {"canChangeMission",auth.can_change_mission},
        {"hasEconomicControl",auth.has_economic_control},
        {"hasVotingControl",auth.has_voting_control},
        {"inheritedPurpose",inherited},
        {"founderEquityBp",static_cast<int>(auth.player_economic_bp.value())},
        {"founderEquityText",QStringLiteral("%1.%2%").arg(auth.player_economic_bp.value()/100).arg(auth.player_economic_bp.value()%100,2,10,QLatin1Char('0'))},
        {"founderSeats",static_cast<int>(auth.player_voting_seats)},
        {"totalSeats",static_cast<int>(auth.total_voting_seats)},
        {"outsideSeats",static_cast<int>(auth.total_voting_seats - auth.player_voting_seats)},
        {"blocker",q(auth.authority_blocker)},
        {"charterConstraints",q(auth.charter_constraints)},
        {"economicStakePrice",money(auth.economic_buyout_price)},
        {"economicStakeShares",static_cast<int>(auth.economic_buyout_shares.value())},
        {"votingControlPrice",money(auth.voting_control_price)},
        {"legalFee",money(auth.voting_company_fee)},
        {"personalCash",money(personal_cash)},
        {"canBuyEconomicStake",auth.can_buy_economic_stake && personal_cash>=auth.economic_buyout_price},
        {"canAcquireVotingControl",auth.can_acquire_voting_control && personal_cash>=auth.voting_control_price},
        {"revision",reviewToken()},
        {"scandalActive",scandal && !scandal->responded_on.has_value()}
    };
    if(scandal) {
        out["scandalPreviousMission"]=q(scandal->previous_mission);
        out["scandalNewMission"]=q(scandal->new_mission);
        out["scandalDate"]=q(core::iso_date(scandal->triggered_on));
        out["scandalResolved"]=scandal->responded_on.has_value();
        out["scandalResponse"]=q(scandal->response);
        out["scandalMoraleHit"]=scandal->employee_morale_hit;
        out["scandalDemandHit"]=static_cast<int>(-scandal->donor_customer_reaction_bp);
        out["scandalInvestorReaction"]=scandal->investor_reaction_bp;
        out["scandalSummary"]=scandal->responded_on
            ? tr("Public backlash over reversing the inherited %1 mission was addressed (%2). Historical records and remaining staff concerns remain visible.")
                .arg(q(scandal->previous_mission), q(scandal->response))
            : tr("Reversing the company's inherited %1 mission to %2 triggered public scrutiny, temporary demand backlash (%3%), and aligned staff morale loss (%4). Choose a response to stabilize operations.")
                .arg(q(scandal->previous_mission), q(scandal->new_mission))
                .arg(scandal->donor_customer_reaction_bp/100).arg(scandal->employee_morale_hit);
    }
    return out;
}

void WorldController::acquireCompanyControl(QString step,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){
        emit noticeRaised(tr("Review current ownership and board authority first."));
        return;
    }
    emit actionRequested(core::AcquireCompanyControl{step.toStdString()});
}

void WorldController::respondToMissionScandal(QString choice,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){
        emit noticeRaised(tr("Review the current mission-reversal response options."));
        return;
    }
    emit actionRequested(core::RespondToMissionScandal{choice.toStdString()});
}

void WorldController::socialize(QString person,QString activity,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your plans again."));return;}
    emit actionRequested(core::Socialize{person.toStdString(),activity.toStdString()});
}
}
