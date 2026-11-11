#include "competition_internal.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <stdexcept>

namespace pgp::runtime {
void validate_collaborations(const WorldState& w,const CompiledProgram&,bool deep) {
    using namespace core;
    const auto invalid=[](){throw std::invalid_argument("Joint research differs from its two employers, paid agreement, qualified work or publication.");};
    const auto size=w.competition?w.competition->collaborations.size():0;
    if(size>collaboration_limit || (size && !w.competition->setup))invalid();
    struct Side {const SimulationState* company=nullptr;const ResearchRun* run=nullptr;const CapacityProject* project=nullptr;};
    std::vector<std::array<Side,2>> sides(size);
    std::vector<const SimulationState*> companies{&w.simulation};
    if(w.competition)for(const auto& c:w.competition->companies)companies.push_back(&c.simulation);
    // Index only joint runs, then scan each capacity/ledger history once. No
    // per-agreement copies or nested full-history searches on ordinary days.
    std::map<std::string_view,const Posting*> payments;
    for(const auto* s:companies) {
        if(!s->lab)continue;
        std::map<std::string_view,Side*> projects;
        for(const auto& run:s->lab->research)if(run.cooperation) {
            const auto& id=run.cooperation->agreement;constexpr std::string_view prefix="research-jam/";
            if(!id.starts_with(prefix))invalid();std::size_t ordinal=0;const auto parsed=std::from_chars(id.data()+prefix.size(),id.data()+id.size(),ordinal);
            if(parsed.ec!=std::errc{} || parsed.ptr!=id.data()+id.size() || ordinal==0 || ordinal>size)invalid();
            const auto& c=w.competition->collaborations[ordinal-1];
            if(c.id!=id || (s->lab->owner!=c.first_company && s->lab->owner!=c.second_company))invalid();
            const auto index=s->lab->owner==c.first_company?0U:1U;auto& side=sides[ordinal-1][index];
            if(side.run || run.project_id!=(index==0?c.first_project:c.second_project) || run.research_id!=c.topic ||
               run.cooperation->partner!=(index==0?c.second_company:c.first_company) || run.cooperation->credit!=c.work_credit)invalid();
            side.company=s;side.run=&run;if(!projects.emplace(run.project_id,&side).second)invalid();
        }
        if(!projects.empty())for(const auto& p:s->capacity->projects)if(const auto it=projects.find(p.id);it!=projects.end())it->second->project=&p;
        if(deep && s->accounts)for(const auto& p:s->accounts->postings)if(p.key.starts_with("research-jam/"))
            if(!payments.emplace(p.key,&p).second)invalid();
    }
    std::size_t payment_count=0;
    std::map<std::pair<std::string,std::string>,OrdinalDay> previous;
    for(std::size_t i=0;i<size;++i) {
        const auto& c=w.competition->collaborations[i];const auto& a=sides[i][0];const auto& b=sides[i][1];
        if(c.id!="research-jam/"+std::to_string(i+1) || !a.run || !b.run || !a.project || !b.project || c.first_company==c.second_company ||
           c.started_on!=a.project->starts_on || c.started_on!=b.project->starts_on || c.started_on>campaign_day(w) ||
           a.run->researchers.size()!=1 || b.run->researchers.size()!=1 || c.contact!=c.second_company+"/"+b.run->researchers.front())invalid();
        if(!a.company->management || !b.company->management || !a.company->management->catalog.people->collaboration || !b.company->management->catalog.people->collaboration)invalid();
        const auto& ar=*a.company->management->catalog.people->collaboration;const auto& br=*b.company->management->catalog.people->collaboration;
        if(c.coordination_fee!=std::max(ar.coordination_fee,br.coordination_fee) || c.work_credit!=std::min(ar.work_credit,br.work_credit) ||
           c.friendship<std::max(ar.minimum_friendship,br.minimum_friendship) || c.friendship>1000)invalid();
        const auto local_prefix=c.first_company==w.company_id?std::string{}:c.first_company+"/";
        bool connection=c.connection=="founder" && c.first_company==w.company_id;
        for(const auto& e:a.company->management->people->employees)if(local_prefix+e.terms.profile.id==c.connection && e.joined_on<=c.started_on && e.ready_on<=c.started_on && (!e.departed_on || *e.departed_on>=c.started_on))connection=true;
        if(!connection)invalid();
        const auto pair=std::minmax(c.first_company,c.second_company);const std::pair<std::string,std::string> key{pair.first,pair.second};
        if(const auto last=previous.find(key);last!=previous.end() && c.started_on<advance_days(last->second,Days{std::max(ar.cooldown_days,br.cooldown_days)}))invalid();previous.insert_or_assign(key,c.started_on);
        if(deep && c.coordination_fee.value()) {
            for(std::size_t side=0;side<2;++side) {
                const auto found=payments.find(c.id+(side==0?"/pay":"/receive"));
                if(found==payments.end())invalid();const auto& p=*found->second;
                if(p.day!=c.started_on || p.amount!=c.coordination_fee || p.owner!=(side==0?c.first_company:c.second_company) ||
                   p.debit!=(side==0?Account::expenses:Account::cash) || p.credit!=(side==0?Account::cash:Account::revenue))invalid();
                ++payment_count;
            }
        }
        if(c.published_on) {
            if(*c.published_on<c.started_on || *c.published_on>campaign_day(w) || !a.run->completed || !b.run->completed ||
               !a.project->stopped_on || !b.project->stopped_on || *c.published_on<std::max(*a.project->stopped_on,*b.project->stopped_on))invalid();
            for(const auto* s:{a.company,b.company})if(std::ranges::none_of(s->lab->publications,[&](const auto& p){return p.research_id==c.topic && p.day<=*c.published_on;}))invalid();
        }
    }
    if(deep && payments.size()!=payment_count)invalid();
}
}
