#include "world_controller.hpp"
#include "flow_presentation.hpp"
#include <pgp/runtime/romance.hpp>
#include <algorithm>
namespace pgp::bridge {
namespace {QString q(std::string_view s){return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}}
QVariantMap WorldController::romance() const {
    if(!snapshot_ || !snapshot_->checkpoint.world.romance)return {};
    const auto& w=snapshot_->checkpoint.world;const auto& preferences=w.romance->founder.preferences;QStringList genders;
    for(const auto& gender:preferences.genders)genders.push_back(q(gender));
    return {{"available",true},{"interested",preferences.interested},{"genders",genders},{"revision",reviewToken()}};
}
QVariantMap WorldController::romanceDetails(QString person) const {
    if(!snapshot_)return {};const auto& w=snapshot_->checkpoint.world;const auto* rules=runtime::romance_rules(w);if(!rules || !w.romance)return {};
    const auto id=runtime::romance_person_id(w,person.toStdString());const auto day=runtime::campaign_day(w);
    QVariantMap result{{"available",true},{"status",tr("Friends, with room for their own story")},{"together",false},{"revision",reviewToken()}};
    if(const auto* b=runtime::latest_romance(w,"founder",id)) {
        const bool together=!b->ended_on;result["together"]=together;
        result["status"]=b->ended_on?(b->married_on?tr("Ex-spouse"):tr("Former partners")):b->married_on?tr("Married"):b->engaged_on?tr("Engaged"):tr("Dating");
        result["affection"]=b->affection;result["dates"]=b->dates;result["memory"]=q(b->memory);result["strained"]=b->strained_on.has_value();result["stage"]=q(core::romance_status(*b));
        result["since"]=q(core::iso_date(b->started_on));
        const auto milestone=b->engaged_on.value_or(b->started_on);const int duration=b->engaged_on?rules->engagement_days:rules->dating_days;
        result["timeProgress"]=std::clamp(static_cast<int>((day.value()-milestone.value())*100/duration),0,100);
        result["milestone"]=b->married_on?tr("You shared your vows on %1.").arg(q(core::iso_date(*b->married_on))):b->engaged_on?tr("Wedding conversations open after %1; closeness %2 / %3.").arg(q(core::iso_date(core::advance_days(*b->engaged_on,core::Days{rules->engagement_days})))).arg(b->affection).arg(rules->marriage_affection):tr("Engagement conversations open after %1; closeness %2 / %3; dates %4 / %5.").arg(q(core::iso_date(core::advance_days(b->started_on,core::Days{rules->dating_days})))).arg(b->affection).arg(rules->engagement_affection).arg(b->dates).arg(rules->minimum_dates);
    }
    for(auto i=w.romance->replies.size();i>0;--i)if(w.romance->replies[i-1].person==id){const auto& r=w.romance->replies[i-1];result["reply"]=q(core::iso_date(r.day))+" · "+q(r.response);break;}
    for(const auto action:{"ask","walk","listen","hobby","engage","marry","separate"}) {
        const auto preview=runtime::romance_quote(w,{id,action});result[q(action)] = QVariantMap{{"blocker",q(preview.blocker)},{"explanation",q(preview.explanation)}};
    }
    if(runtime::romantic_identity_known(w,id)) {
        const core::PersonProfile* profile=nullptr;
        {
            const auto& p=*w.simulation.management->people;
            for(const auto& e:p.employees)if(runtime::romance_person_id(w,e.terms.profile.id)==id)profile=&e.terms.profile;
            if(const auto found=p.industry.find(id);found!=p.industry.end())profile=&found->second.profile;
        }
        if(profile && profile->romantic){QStringList genders;for(const auto& gender:profile->romantic->preferences.genders)genders.push_back(readable(gender));result["identity"]=tr("Shared orientation: %1. %2").arg(readable(profile->orientation),profile->romantic->preferences.interested?tr("Romantically interested in: %1.").arg(genders.join(", ")):tr("Not seeking romance."));}
    } else result["identity"]=tr("Their romantic preferences are private until they choose to share through trust.");
    return result;
}
void WorldController::pursueRomance(QString person,QString action,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your personal plans again."));return;}
    emit romanceRequested({person.toStdString(),action.toStdString()});
}
void WorldController::setRomanticPreferences(bool interested,QStringList genders,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your preferences again."));return;}
    emit romanticPreferencesRequested(interested,genders);
}
}
