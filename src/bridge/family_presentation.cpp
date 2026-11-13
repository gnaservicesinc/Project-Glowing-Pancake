#include "world_controller.hpp"
#include "flow_presentation.hpp"
#include "portrait_url.hpp"
#include <pgp/runtime/family.hpp>
#include <algorithm>
namespace pgp::bridge {
namespace {
QString q(std::string_view s){return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
runtime::FamilyRequest request(const QVariantMap& row) {return {row.value("action").toString().toStdString(),row.value("band","baby").toString().toStdString(),row.value("name").toString().toStdString(),row.value("child").toString().toStdString(),row.value("track").toString().toStdString(),row.value("withPartner",false).toBool()};}
QString kind(const std::string& id){return id=="biological"?QObject::tr("Birth family"):id=="adoptive"?QObject::tr("Adoptive family"):id=="foster"?QObject::tr("Foster family"):QObject::tr("Stepfamily");}
}
QVariantMap WorldController::familyAlbum() const {
    if(!snapshot_ || !snapshot_->checkpoint.world.family)return {};const auto& w=snapshot_->checkpoint.world;const auto& state=*w.family;const auto day=runtime::campaign_day(w);const auto& rules=*runtime::family_rules(w);
    QVariantList members,plans,tracks,edges;std::map<std::string,QVariantMap> identities;
    auto me=founder();me["id"]="founder";identities.emplace("founder",me);
    for(const auto& e:w.simulation.management->people->employees){const auto id=runtime::romance_person_id(w,e.terms.profile.id);if(id!="founder")identities.emplace(id,person(q(e.terms.profile.id),w.simulation.date));}
    for(const auto& [id,p]:w.simulation.management->people->industry)identities.emplace(runtime::romance_person_id(w,id),QVariantMap{{"id",q(id)},{"name",q(p.profile.name)},{"portrait",portrait_url(p.profile,day)},{"role",q(p.profile.role)},{"detail",q(p.profile.pronouns)}});
    for(const auto& track:rules.tracks)tracks.push_back(QVariantMap{{"id",q(track.skill)},{"name",q(track.name)}});
    for(const auto& member:state.members)if(member.known) {
        const auto& p=member.profile;const auto age=runtime::family_age(p.romantic->born_on,day);const auto& track=*std::ranges::find(rules.tracks,member.track,&core::FamilyLearningTrack::skill);
        bool care=false,foster=false;QStringList parents;
        for(const auto& edge:state.edges)if(edge.child==p.id && edge.known) {
            if(identities.contains(edge.parent))parents.push_back(identities.at(edge.parent).value("name").toString()+" · "+kind(edge.kind)+(edge.ended_on?tr(" (past care)"):QString{}));
            if(edge.parent=="founder" && !edge.ended_on && edge.kind!="step"){care=true;if(edge.kind=="foster")foster=true;}
        }
        QString application;
        if(member.application) {
            const auto& a=*member.application;application=tr("Application sent %1 · %2").arg(q(core::iso_date(a.day)),q(a.company));
            const core::SimulationState* employer=a.company==w.company_id?&w.simulation:nullptr;if(!employer && w.competition)for(const auto& c:w.competition->companies)if(c.definition_id==a.company)employer=&c.simulation;
            if(employer && employer->management && employer->management->people) {
                const auto& people=*employer->management->people;
                if(std::ranges::any_of(people.queued_applicants,[&](const auto& row){return row.profile.id==a.person;}))application+=tr(" · waiting for the next recruitment review");
                else if(std::ranges::any_of(people.employees,[&](const auto& e){return e.terms.profile.id==a.person && !e.departed_on;}))application+=tr(" · hired");
            }
        }
        auto row=QVariantMap{{"id",q(p.id)},{"name",q(p.name)},{"portrait",portrait_url(p.portrait,age)},{"role",age<18?tr("Growing up"):member.qualified_on?q(p.role):tr("Studying")},{"age",age},{"detail",q(p.pronouns)+" · "+q(core::iso_date(p.romantic->born_on))},{"parents",parents.join("\n")},{"care",care},{"foster",foster},{"track",q(member.track)},{"trackName",q(track.name)},{"qualified",member.qualified_on.has_value()},{"application",application},{"skills",member.qualified_on?q(p.education):tr("Little experiments, big discoveries. Curiosity grows with practice.")}};
        identities.insert_or_assign(p.id,row);members.push_back(row);
    }
    for(const auto& e:state.edges)if(e.known && identities.contains(e.parent) && identities.contains(e.child))edges.push_back(QVariantMap{{"first",q(e.parent)},{"second",q(e.child)},{"label",tr("Parent/Child · %1%2").arg(kind(e.kind),e.ended_on?tr(" · past care"):QString{})},{"names",identities.at(e.parent).value("name").toString()+" → "+identities.at(e.child).value("name").toString()},{"strength",500}});
    std::map<std::string,std::vector<const core::FamilyEdge*>> siblings;
    for(const auto& edge:state.edges)if(edge.known && identities.contains(edge.child))siblings[edge.parent].push_back(&edge);
    std::set<std::pair<std::string,std::string>> linked;
    for(const auto& [parent,children]:siblings){(void)parent;for(std::size_t i=0;i<children.size();++i)for(std::size_t j=i+1;j<children.size();++j){const auto& a=*children[i];const auto& b=*children[j];if(a.child==b.child || a.started_on>b.ended_on.value_or(day) || b.started_on>a.ended_on.value_or(day))continue;auto pair=std::minmax(a.child,b.child);if(!linked.emplace(pair.first,pair.second).second)continue;const auto label=(a.kind=="biological" || a.kind=="adoptive") && (b.kind=="biological" || b.kind=="adoptive")?tr("Siblings"):tr("Step/Foster siblings");edges.push_back(QVariantMap{{"first",q(pair.first)},{"second",q(pair.second)},{"label",label},{"names",identities.at(pair.first).value("name").toString()+" ↔ "+identities.at(pair.second).value("name").toString()},{"strength",500}});}}
    const auto show_plan=[&](const core::FamilyPlan& p){if(!p.known)return;const auto title=p.kind=="birth"?tr("Welcoming a baby"):p.kind=="foster"?tr("Foster placement"):tr("Adoption review");plans.push_back(QVariantMap{{"id",p.id},{"title",title},{"status",p.status=="pending"?tr("On the calendar"):p.status=="completed"?tr("A new family chapter"):tr("Review did not proceed")},{"due",q(core::iso_date(p.due_on))},{"detail",p.status=="pending"?tr("Agreed %1 · review/arrival %2").arg(q(core::iso_date(p.requested_on)),q(core::iso_date(p.due_on))):p.status=="completed" && identities.contains(p.child)?identities.at(p.child).value("name").toString():tr("This review did not proceed.")},{"progress",std::clamp(static_cast<int>((day.value()-p.requested_on.value())*100/(p.due_on.value()-p.requested_on.value())),0,100)}});};
    for(const auto& [id,p]:state.pending){(void)id;show_plan(p);}for(auto i=state.history.size();i>0 && i+8>state.history.size();--i)show_plan(state.history[i-1]);
    QVariantList cards;for(const auto& [id,card]:identities){(void)id;cards.push_back(card);}
    const auto* spouse=runtime::romantic_partner(w,"founder");
    return {{"available",true},{"members",members},{"cards",cards},{"edges",edges},{"tracks",tracks},{"plans",plans},{"married",spouse && spouse->married_on},{"reply",state.replies.empty()?QString{}:q(state.replies.back().message)},{"queued",static_cast<int>(w.simulation.management->people->queued_applicants.size())},{"revision",reviewToken()}};
}
QVariantMap WorldController::familyQuote(QVariantMap draft) const {
    if(!snapshot_)return {};try{const auto quote=runtime::family_quote(snapshot_->checkpoint.world,request(draft));return {{"valid",quote.blocker.empty()},{"blocker",q(quote.blocker)},{"explanation",q(quote.explanation)},{"due",quote.due_on?q(core::iso_date(*quote.due_on)):QString{}}};}catch(const std::exception& e){return {{"valid",false},{"blocker",q(e.what())}};}
}
void WorldController::manageFamily(QVariantMap draft,QString revision) {
    if(!snapshot_ || companyArchive() || revision!=reviewToken()){emit noticeRaised(tr("Circumstances changed. Review your family plans again."));return;}emit familyRequested(request(draft));
}
}
