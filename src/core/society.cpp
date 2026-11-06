#include <pgp/core/society.hpp>

#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
constexpr std::array<std::string_view,society_components> components{
    "Food access","Housing access","Essential services","Care access","Prevention",
    "Institutional checks","Participation","Information integrity","Emissions trend",
    "Resource resilience","Adaptation capacity","Income opportunity","Transition support","Distribution"};
constexpr std::array<std::string_view,5> indices{"Basic needs","Health","Civic agency","Ecological resilience","Shared prosperity"};
constexpr std::array<std::string_view,society_audiences> audiences{"Investors","Researchers","Customers","Staff","Regulators","Communities"};
constexpr std::array<std::string_view,society_regions> regions{
    "region.africa_middle_east","region.asia_pacific","region.canada","region.china",
    "region.europe","region.india","region.latin_america","region.us"};
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
std::uint32_t bounded(std::int64_t value) { return static_cast<std::uint32_t>(std::clamp<std::int64_t>(value,0,10000)); }
std::uint32_t fraction(std::uint64_t part,std::uint64_t all) {
    return all ? static_cast<std::uint32_t>(mul_div_floor(part,10000,all).quotient) : 0;
}
WelfareComponents evolve(const SocietyRegion& profile,const WelfareComponents& before,const RegionOutcomes& outcomes,
    std::uint32_t durable_gain) {
    const auto use = static_cast<std::int64_t>(fraction(outcomes.delivered,outcomes.demand));
    const auto public_use = static_cast<std::int64_t>(fraction(outcomes.public_work,outcomes.public_demand));
    const auto science = static_cast<std::int64_t>(fraction(outcomes.science_work,outcomes.demand));
    const auto productivity = static_cast<std::int64_t>(fraction(outcomes.industrial_work,outcomes.demand));
    const auto automation = static_cast<std::int64_t>(fraction(outcomes.agent_work,outcomes.demand));
    const auto missed = static_cast<std::int64_t>(fraction(outcomes.missed,outcomes.demand));
    const auto energy_intensity = outcomes.delivered ? std::min<std::uint64_t>(10000,outcomes.compute_milli/outcomes.delivered*5) : 0;
    const auto energy = static_cast<std::int64_t>(energy_intensity)*use/10000;
    const auto concentration = outcomes.concentration.value()*use/10000;
    const auto quality = outcomes.delivered ? (outcomes.quality.value()-5000)*use/10000 : 0;
    const auto transition = static_cast<std::int64_t>(public_use)/3 + science/2 - automation/3;
    const std::array<std::int64_t,society_components> change{
        use/5 + public_use/8, productivity/5 + public_use/10, public_use/3 - missed/4,
        public_use/4 + quality/8, science/3 + public_use/8,
        public_use/10 - concentration/10, public_use/6 - automation/12,
        quality/4 - missed/5, static_cast<std::int64_t>(use)/8-static_cast<std::int64_t>(energy)/5,
        public_use/8-static_cast<std::int64_t>(energy)/8, science/3+public_use/8,
        productivity/3+use/8-automation/6,transition,use/6-concentration/8+transition/3};
    WelfareComponents next{};
    for (std::size_t i=0;i<next.size();++i) {
        const auto target=bounded(static_cast<std::int64_t>(profile.initial[i])+durable_gain+change[i]);
        const auto difference=static_cast<std::int64_t>(target)-before[i];
        const auto magnitude=static_cast<std::uint64_t>(difference<0?-difference:difference);
        const auto step=static_cast<std::int64_t>(mul_div_round_half_up(magnitude,
            static_cast<std::uint64_t>(profile.adaptation.value()),10000));
        next[i]=bounded(static_cast<std::int64_t>(before[i])+(difference<0?-step:step));
    }
    return next;
}
std::uint32_t develop(const SocietyDevelopment& terms,std::uint32_t before,const RegionOutcomes& observed) {
    const bool supported=fraction(observed.public_work,observed.public_demand)>=terms.public_coverage.value() &&
        fraction(observed.delivered,observed.demand)>=terms.useful_coverage.value() && observed.quality>=terms.evidence_floor;
    const auto gain=static_cast<std::int64_t>(before)+(supported?terms.monthly_gain.value():-terms.monthly_decay.value());
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(gain,0,terms.maximum_gain.value()));
}
OrdinalDay next_month(OrdinalDay date) {
    const auto civil=from_ordinal(date);
    return to_ordinal({civil.month==12?civil.year+1:civil.year,civil.month==12?1U:civil.month+1,1});
}
std::optional<OrdinalDay> next_forum(const SocietyState& state,OrdinalDay from) {
    const auto& terms=state.catalog.forum;
    const auto initial=from_ordinal(terms.first_agenda);
    if (from<=terms.first_agenda) return terms.first_agenda;
    const auto year=from_ordinal(from).year;
    const auto repeat=static_cast<int>(terms.repeat_years);
    auto candidate=initial.year+(year-initial.year)/repeat*repeat;
    if(to_ordinal({candidate,initial.month,std::min(initial.day,days_in_month(candidate,initial.month))})<from) candidate+=repeat;
    if(candidate>9980) return {};
    return to_ordinal({candidate,initial.month,std::min(initial.day,days_in_month(candidate,initial.month))});
}
AudienceScores next_audiences(const AudienceScores& before,const std::array<RegionOutcomes,society_regions>& outcomes) {
    std::uint64_t work=0,quality=0,public_work=0,science=0,promised=0,missed=0;
    for(const auto& outcome:outcomes) {
        work+=outcome.player_work;quality+=outcome.player_work*static_cast<std::uint64_t>(outcome.player_quality.value());
        public_work+=outcome.player_public;science+=outcome.player_science;promised+=outcome.player_promised;missed+=outcome.player_missed;
    }
    if(work==0 && promised==0) return before;
    const auto evidence=static_cast<std::int64_t>(work ? quality/work : 0);
    const auto reliability=10000-static_cast<std::int64_t>(fraction(missed,promised));
    const auto common=bounded((evidence+reliability)/2);
    const std::array<std::uint32_t,society_audiences> targets{
        common,bounded(evidence+fraction(science,work)/10),common,bounded(reliability),
        bounded(evidence),bounded(static_cast<std::int64_t>(common)+fraction(public_work,work)/10)};
    AudienceScores result{};
    for(std::size_t i=0;i<result.size();++i) result[i]=bounded(static_cast<std::int64_t>(before[i])+
        (static_cast<std::int64_t>(targets[i])-before[i])/8);
    return result;
}
void ballot(const SocietyState& state,ForumRound& round,const SocietyMonth* latest) {
    const auto& terms=state.catalog.forum;
    std::uint32_t count=0;
    for(std::size_t i=0;i<state.catalog.regions.size();++i) {
        const auto& member=state.catalog.regions[i];
        std::int64_t support=member.forum_interest.value();
        if(latest) {
            const auto& observed=latest->outcomes[i];
            support+=static_cast<std::int64_t>(fraction(observed.public_work,observed.public_demand))/10+
                (observed.delivered?observed.quality.value()-5000:0)/8;
        }
        for(const auto& endorsement:round.endorsements) if(endorsement.region==member.id)
            support+=endorsement.support?terms.endorsement_shift.value():-terms.endorsement_shift.value();
        const auto agrees=support>=terms.support_threshold.value();
        if(agrees) ++count;
        round.members.push_back({member.id,BasisPoints{bounded(support)},agrees,{}});
    }
    if(count<terms.quorum) return;
    for(std::size_t i=0;i<round.members.size();++i) if(round.members[i].supports)
        round.members[i].effective_from=advance_days(round.ballot_on,state.catalog.regions[i].implementation);
}
} // namespace
std::string_view welfare_component_name(std::size_t index) {return components.at(index);}
std::string_view welfare_index_name(std::size_t index) {return indices.at(index);}
std::string_view welfare_index_key(std::size_t index) {
    constexpr std::array<std::string_view,5> keys{"basic_needs","health","civic_agency","ecological_resilience","shared_prosperity"};
    return keys.at(index);
}
std::string_view society_audience_name(std::size_t index) {return audiences.at(index);}
WelfareIndices welfare_indices(const WelfareComponents& values) {
    return {(values[0]+values[1]+values[2])/3,(values[3]+values[4])/2,(values[5]+values[6]+values[7])/3,
        (values[8]+values[9]+values[10])/3,(values[11]+values[12]+values[13])/3};
}
void validate_society_catalog(const SocietyCatalog& catalog) {
    if(const auto& d=catalog.development;d &&
        (d->public_coverage.value()==0 || d->useful_coverage.value()==0 || d->evidence_floor.value()<4000 ||
         d->maximum_gain.value()<1 || d->maximum_gain.value()>6000 || d->monthly_gain.value()<1 ||
         d->monthly_gain.value()>100 || d->monthly_gain>d->maximum_gain || d->monthly_decay.value()<1 ||
         d->monthly_decay>d->monthly_gain))
        invalid("Long-term regional gains need bounded growth and decay tied to qualified public delivery.");
    if(catalog.assumptions.empty() || catalog.assumptions.size()>1024 || catalog.regions.size()!=society_regions ||
        catalog.lag_months<1 || catalog.lag_months>12 || catalog.media.fee.value()<1 || catalog.media.fee.value()>100000000 ||
        catalog.media.duration.value()<1 || catalog.media.duration.value()>180 ||
        catalog.media.awareness.value()>3000 || catalog.media.monthly_limit<1 || catalog.media.monthly_limit>6)
        invalid("Regional society needs eight regions, bounded monthly lags and explicit media terms.");
    for(std::size_t i=0;i<regions.size();++i) {
        const auto& region=catalog.regions[i];
        if(region.id!=regions[i] || region.name.empty() || region.name.size()>96 ||
            region.population_weight==0 || region.population_weight>10000 || region.output_weight==0 || region.output_weight>10000 ||
            region.adaptation.value()<1 || region.adaptation.value()>5000 ||
            region.implementation.value()<1 || region.implementation.value()>360 ||
            std::any_of(region.initial.begin(),region.initial.end(),[](auto v){return v>10000;}))
            invalid("Regional profiles need canonical identities, disclosed weights and bounded adaptation.");
    }
    std::set<std::string> sectors;
    for (const auto* group : {&catalog.public_services, &catalog.science, &catalog.industry}) {
        if (group->size()!=society_regions) invalid("Each society sector needs one customer group per region.");
        for (const auto& id : *group)
            if (id.empty() || id.size()>128 || !sectors.insert(id).second)
                invalid("Society sector references must be distinct bounded customer identities.");
    }
    const auto& forum=catalog.forum;
    (void)from_ordinal(forum.first_agenda);(void)channel_key(forum.channel);
    if(forum.id.empty() || forum.id.size()>96 || forum.title.empty() || forum.title.size()>120 ||
        from_ordinal(forum.first_agenda).year<2014 || from_ordinal(forum.first_agenda).year>9980 ||
        forum.repeat_years<4 || forum.repeat_years>16 || forum.quorum<1 || forum.quorum>society_regions ||
        forum.hearing.value()<1 || forum.hearing.value()>180 || forum.term.value()<361 || forum.term.value()>1461 ||
        forum.channel==ProductChannel::legacy || forum.evidence_axis>=2 || forum.evidence_floor.value()==0 ||
        forum.endorsement_fee.value()<1 || forum.endorsement_fee.value()>100000000 || forum.endorsement_shift.value()>1500)
        invalid("The standards forum requires a bounded local implementation and evidence contract.");
}
SocietyState opening_society(SocietyCatalog catalog,OrdinalDay start) {
    validate_society_catalog(catalog);
    if(catalog.forum.first_agenda<start) invalid("A new society scenario cannot skip its first forum agenda.");
    return {std::move(catalog),start,{},{},{},{}};
}
std::size_t society_region_index(const SocietyCatalog& catalog,std::string_view id) {
    const auto found=std::lower_bound(catalog.regions.begin(),catalog.regions.end(),id,
        [](const auto& region,std::string_view key){return region.id<key;});
    if(found==catalog.regions.end() || found->id!=id) invalid("Choose a regional society profile.");
    return static_cast<std::size_t>(found-catalog.regions.begin());
}
void prepare_society(SocietyState& state,OrdinalDay day) {
    if(state.prepared_on==day) return;
    if(state.prepared_on && day.value()!=state.prepared_on->value()+1) invalid("Society preparation cannot skip a played morning.");
    if(next_forum(state,day)==day) {
        if(state.rounds.size()>=society_forum_limit) invalid("The standards forum reached its round bound.");
        const auto ballot_on=advance_days(day,state.catalog.forum.hearing);
        state.rounds.push_back({state.rounds.size()+1,day,ballot_on,advance_days(ballot_on,state.catalog.forum.term),{},{}});
    }
    for(auto& round:state.rounds) if(round.ballot_on==day)
        ballot(state,round,state.months.empty()?nullptr:&state.months.back());
    state.prepared_on=day;
}
void close_society_month(SocietyState& state,OrdinalDay day,const std::array<RegionOutcomes,society_regions>& observed) {
    const auto civil=from_ordinal(day);
    if(civil.day!=days_in_month(civil.year,civil.month) || state.months.size()>=society_month_limit ||
        (state.months.empty() && (from_ordinal(state.starts_on).year!=civil.year || from_ordinal(state.starts_on).month!=civil.month)) ||
        (!state.months.empty() && next_month(state.months.back().month_end)!=to_ordinal({civil.year,civil.month,1})))
        invalid("Regional society closes consecutive complete calendar months.");
    SocietyMonth row;row.month_end=day;row.outcomes=observed;
    const auto size=state.months.size();
    if(state.catalog.development)
        row.durable_gains=size?state.months.back().durable_gains:std::optional{std::array<std::uint32_t,society_regions>{}};
    for(std::size_t i=0;i<society_regions;++i) {
        row.components[i]=size?state.months.back().components[i]:state.catalog.regions[i].initial;
        if(size>=state.catalog.lag_months) {
            const auto& input=state.months[size-state.catalog.lag_months].outcomes[i];
            auto gain=0U;
            if(row.durable_gains) {
                gain=develop(*state.catalog.development,(*row.durable_gains)[i],input);
                (*row.durable_gains)[i]=gain;
            }
            row.components[i]=evolve(state.catalog.regions[i],row.components[i],input,gain);
        }
    }
    const AudienceScores initial{5000,5000,5000,5000,5000,5000};
    row.audiences=next_audiences(size?state.months.back().audiences:initial,observed);
    state.months.push_back(std::move(row));
}
BasisPoints campaign_awareness(const SocietyState& state,std::string_view region,std::string_view service,OrdinalDay day,std::uint32_t audience) {
    std::int64_t awareness=0;
    for(auto it=state.campaigns.rbegin();it!=state.campaigns.rend();++it) {
        if(it->starts_on>day) continue;
        if(day.value()-it->starts_on.value()>state.catalog.media.duration.value()) break;
        if(it->region==region && it->service==service && it->audience==audience && day<it->ends_before) awareness+=it->awareness.value();
    }
    return BasisPoints{std::min<std::int64_t>(awareness,3000)};
}
bool forum_enforced(const SocietyState& state,std::string_view region,OrdinalDay day) {
    for(auto round=state.rounds.rbegin();round!=state.rounds.rend();++round) {
        if(round->ballot_on>day || day>=round->expires_before) continue;
        for(const auto& member:round->members)
            if(member.region==region && member.effective_from && *member.effective_from<=day) return true;
    }
    return false;
}
MarketInfluences society_market_influences(const SocietyState& state,const MarketState& market,OrdinalDay day,
    const std::function<bool(std::string_view)>& qualified) {
    MarketInfluences result;
    for(const auto& segment:market.segments) {
        if(segment.region_id.empty()) continue;
        for(const auto& preference:segment.preferences) {
            const auto product=std::find_if(market.products.begin(),market.products.end(),[&](const auto& row){return row.id==preference.product_id;});
            if(product==market.products.end()) continue;
            const auto awareness=market.owner=="company.player"?campaign_awareness(state,segment.region_id,product->id,day):BasisPoints{0};
            const auto allowed=product->channel!=state.catalog.forum.channel ||
                !forum_enforced(state,segment.region_id,day) || qualified(product->id);
            if(!allowed || awareness.value()>0) result.push_back({product->id,segment.id,awareness,allowed});
        }
    }
    return result;
}
WelfareIndices global_welfare(const SocietyState& state) {
    WelfareIndices result{};std::uint64_t population=0;
    for(std::size_t i=0;i<society_regions;++i) {
        const auto& region=state.catalog.regions[i];
        const auto scores=welfare_indices(state.months.empty()?region.initial:state.months.back().components[i]);
        population+=region.population_weight;
        for(std::size_t j=0;j<scores.size();++j) result[j]+=scores[j]*region.population_weight;
    }
    for(auto& score:result) score=static_cast<std::uint32_t>(score/population);
    return result;
}
WelfareIndices minimum_welfare(const SocietyState& state) {
    WelfareIndices result{10000,10000,10000,10000,10000};
    for(std::size_t i=0;i<society_regions;++i) {
        const auto scores=welfare_indices(state.months.empty()?state.catalog.regions[i].initial:state.months.back().components[i]);
        for(std::size_t j=0;j<scores.size();++j) result[j]=std::min(result[j],scores[j]);
    }
    return result;
}
std::optional<OrdinalDay> next_society_boundary(const SocietyState& state,OrdinalDay day) {
    auto next=next_month(day);
    const auto consider=[&](OrdinalDay date){if(date>day && date<next) next=date;};
    if(const auto agenda=next_forum(state,advance_days(day,Days{1}))) consider(*agenda);
    for(const auto& round:state.rounds) {
        consider(round.ballot_on);consider(round.expires_before);
        for(const auto& member:round.members) if(member.effective_from) consider(*member.effective_from);
    }
    return next;
}
void validate_society(const SocietyState& state,OrdinalDay day) {
    validate_society_catalog(state.catalog);
    if(state.prepared_on!=day || state.months.size()>society_month_limit || state.rounds.size()>society_forum_limit ||
        state.campaigns.size()>society_campaign_limit) invalid("The regional society history differs from its current morning or record bounds.");
    auto replay=opening_society(state.catalog,state.starts_on);
    for(const auto& month:state.months) {
        if(month.month_end>=day) invalid("A regional observation predates actual settlement.");
        for(const auto& row:month.outcomes)
            if(row.demand>1'000'000'000'000ULL || row.delivered>row.demand || row.public_demand>row.demand || row.promised>row.demand || row.public_work>row.public_demand ||
                row.public_work>row.delivered || row.science_work>row.delivered || row.industrial_work>row.delivered ||
                row.agent_work>row.delivered || row.player_work>row.delivered || row.missed>row.promised ||
                row.player_public>row.player_work || row.player_science>row.player_work || row.player_missed>row.player_promised ||
                row.player_promised>row.promised || row.player_missed>row.missed ||
                row.compute_milli>1'000'000'000'000'000ULL) invalid("Regional outcomes exceed their delivered work or observation bounds.");
        close_society_month(replay,month.month_end,month.outcomes);
    }
    if(replay.months!=state.months) invalid("The welfare lag, component weights or audience outcomes differ from the saved observations.");
    const auto civil=from_ordinal(day), start=from_ordinal(state.starts_on);
    const auto expected_months=(civil.year-start.year)*12+static_cast<int>(civil.month)-static_cast<int>(start.month);
    if(day<state.starts_on || expected_months<0 || state.months.size()!=static_cast<std::size_t>(expected_months))
        invalid("A settled regional month is missing from the society history.");
    std::size_t index=0;
    for(auto agenda=next_forum(state,state.starts_on); agenda && *agenda<=day;
        agenda=next_forum(state,advance_days(*agenda,Days{1}))) {
        if(index>=state.rounds.size()) invalid("A standards forum round is missing.");
        const auto& saved=state.rounds[index];
        const auto ballot_on=advance_days(*agenda,state.catalog.forum.hearing);
        ForumRound expected{++index,*agenda,ballot_on,advance_days(ballot_on,state.catalog.forum.term),saved.endorsements,{}};
        if(expected.endorsements.size()>society_regions) invalid("A member can receive only one endorsement per round.");
        std::set<std::string> endorsed;
        for(const auto& row:expected.endorsements) {
            (void)society_region_index(state.catalog,row.region);
            if(!endorsed.insert(row.region).second || row.day<*agenda || row.day>=ballot_on || row.day>day ||
                row.evidence.empty() || row.evidence.size()>128 || row.paid!=state.catalog.forum.endorsement_fee)
                invalid("A saved forum endorsement violates its window, evidence or payment terms.");
        }
        if(ballot_on<=day) {
            const auto latest=std::lower_bound(state.months.begin(),state.months.end(),ballot_on,
                [](const auto& month,OrdinalDay date){return month.month_end<date;});
            ballot(state,expected,latest==state.months.begin()?nullptr:&*std::prev(latest));
        }
        if(expected!=saved) invalid("A standards ballot or member implementation date differs from its observed inputs.");
    }
    if(index!=state.rounds.size()) invalid("The standards forum contains an unauthored round.");
}
} // namespace pgp::core
