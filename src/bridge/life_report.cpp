#include "life_report.hpp"
#include "departure_presentation.hpp"
#include <pgp/runtime/family.hpp>
#include <pgp/runtime/romance.hpp>
#include <pgp/runtime/competition.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <QStringList>

namespace pgp::bridge {
namespace {
QString text(std::string_view value) {return QString::fromUtf8(value.data(),static_cast<qsizetype>(value.size()));}
QString date(core::OrdinalDay value) {return text(core::iso_date(value));}
QString count(std::uint64_t value) {return QString::number(static_cast<qulonglong>(value));}
QString money(core::Cents value) {
    const auto amount=value.value();
    const auto magnitude=amount<0?static_cast<std::uint64_t>(-(amount+1))+1:static_cast<std::uint64_t>(amount);
    auto whole=count(magnitude/100);
    for(auto position=whole.size()-3;position>0;position-=3) whole.insert(position,',');
    return QStringLiteral("%1$%2.%3").arg(amount<0?QStringLiteral("−"):QString{},whole)
        .arg(magnitude%100,2,10,QLatin1Char('0'));
}
QString index(std::uint32_t value) {
    return QStringLiteral("%1.%2").arg(value/100).arg(value%100,2,10,QLatin1Char('0'));
}
}

QVariantMap lifeReport(const runtime::Checkpoint& checkpoint,const runtime::CompiledProgram& program) {
    const auto& world=checkpoint.world;
    const auto& state=world.simulation;
    const auto day=world.objectives && world.objectives->end?world.objectives->end->day:runtime::campaign_day(world);
    const auto name=world.identity?text(world.identity->founder_name):QStringLiteral("The founder");
    const auto company=world.identity?text(world.identity->company_name):QStringLiteral("The company");
    const bool completed=world.objectives && world.objectives->end &&
        world.objectives->end->cause==runtime::CampaignEndCause::deadline;
    const int age=world.romance?runtime::family_age(world.romance->founder.born_on,day):
        core::from_ordinal(day).year-1997;
    const bool obituary=world.founding && completed && age==100;
    const bool voluntary=world.objectives && world.objectives->end &&
        world.objectives->end->cause==runtime::CampaignEndCause::retirement;
    QVariantList articles;
    const auto article=[&](const QString& id,const QString& title,const QStringList& paragraphs) {
        if(!paragraphs.empty()) articles.push_back(QVariantMap{{"id",id},{"title",title},{"paragraphs",paragraphs}});
    };
    QStringList company_record;
    if(obituary) company_record.push_back(QStringLiteral("%1, whose career included %2, has died at the age of 100. "
        "This account draws on the decisions, relationships and public record of that career.").arg(name,company));
    else company_record.push_back(QStringLiteral("%1's record with %2, as of %3.").arg(name,company,date(day)));
    if(voluntary) company_record.push_back(QStringLiteral("At age %1, %2 stepped down as chief executive following the announced handover.").arg(age).arg(name));
    if(world.founding) company_record.push_back(QStringLiteral("%1 opened as a %2 in %3.")
        .arg(company,world.founding->legal_form=="nonprofit"?QStringLiteral("nonprofit"):QStringLiteral("corporation"),
            date(world.campaign_start)));
    if(state.management && state.management->people) {
        const auto& people=*state.management->people;
        const auto employed=std::ranges::count_if(people.employees,[](const auto& person){return !person.departed_on;});
        const auto departures=people.employees.size()-static_cast<std::size_t>(employed);
        company_record.push_back(QStringLiteral("The company record lists %1 current staff and %2 departures. Its recorded cash balance is %3.")
            .arg(employed).arg(departures).arg(money(state.cash)));
        company_record.push_back(QStringLiteral("Its stated mission was %1.")
            .arg(text(core::committed_company_mission(state))));
    }
    if(const auto* charter=core::active_charter(state))
        company_record.push_back(QStringLiteral("Under the %1 bylaws, its commitments included %2.").arg(text(charter->name),text(charter->priorities)));
    if(state.lab) company_record.push_back(QStringLiteral("The lab's record contains %1 publications and %2 trained models.")
        .arg(state.lab->publications.size()).arg(state.lab->models.size()));
    if(world.objectives && world.objectives->end && !completed && !voluntary)
        company_record.push_back(world.objectives->end->cause==runtime::CampaignEndCause::insolvency?
            QStringLiteral("The company's operations closed following an unresolved financial workout."):
            QStringLiteral("The company's operating chapter ended before the planned retrospective."));
    article("company",QStringLiteral("A company and its commitments"),company_record);

    QStringList world_record;
    if(world.society && !world.society->months.empty()) {
        const auto& month=world.society->months.back();
        world_record.push_back(QStringLiteral("The latest settled world figures cover the month ending %1.").arg(date(month.month_end)));
        for(std::size_t region=0;region<core::society_regions;++region) {
            const auto& outcomes=month.outcomes[region];
            const auto welfare=core::welfare_indices(month.components[region]);
            world_record.push_back(QStringLiteral("%1 recorded %2 delivered tasks. %3 were supplied by %4, including %5 public-service tasks. "
                "Basic needs stood at %6/100, health at %7, civic agency at %8, ecological resilience at %9 and shared prosperity at %10.")
                .arg(text(world.society->catalog.regions[region].name),count(outcomes.delivered),count(outcomes.player_work),
                    company,count(outcomes.player_public),index(welfare[0]),index(welfare[1]),index(welfare[2]),index(welfare[3]),index(welfare[4])));
            std::uint64_t delivered=0,public_work=0,scientific_work=0;
            for(const auto& settled:world.society->months) {
                delivered+=settled.outcomes[region].player_work;
                public_work+=settled.outcomes[region].player_public;
                scientific_work+=settled.outcomes[region].player_science;
            }
            world_record.push_back(QStringLiteral("Across the recorded months in %1, %2 supplied %3 tasks, including %4 public-service "
                "and %5 scientific tasks.").arg(text(world.society->catalog.regions[region].name),company,
                    count(delivered),count(public_work),count(scientific_work)));
        }
        QStringList audiences;
        for(std::size_t i=0;i<core::society_audiences;++i)
            audiences.push_back(QStringLiteral("%1 %2/100").arg(text(core::society_audience_name(i)),index(month.audiences[i])));
        world_record.push_back(QStringLiteral("Recorded public attitudes toward the company were %1.").arg(audiences.join(QStringLiteral(", "))));
    }
    article("world",QStringLiteral("The world at the close of the record"),world_record);

    QStringList competitors;
    if(world.competition) for(const auto& rival:world.competition->companies) {
        const auto& label=runtime::rival_definition(program,rival.definition_id);
        const auto& actor=rival.simulation;
        if(!actor.accounts) continue;
        const auto staff=actor.management && actor.management->people?
            std::ranges::count_if(actor.management->people->employees,[](const auto& person){return !person.departed_on;}):0;
        competitors.push_back(QStringLiteral("%1 had %2 in cash, %3 current staff and %4 publications in its last recorded accounts, dated %5.")
            .arg(text(label.name),money(actor.cash)).arg(staff)
            .arg(actor.lab?actor.lab->publications.size():0).arg(date(actor.date)));
    }
    if(world.objectives && world.objectives->race) {
        QStringList qualifiers;
        for(const auto& id:world.objectives->race->winners) {
            qualifiers.push_back(id==world.company_id?company:text(runtime::rival_definition(program,id).name));
        }
        competitors.push_back(QStringLiteral("The first independently recorded ASI qualification was %1, credited to %2.")
            .arg(date(world.objectives->race->day),qualifiers.join(QStringLiteral(" and "))));
    }
    article("industry",QStringLiteral("Other laboratories, other paths"),competitors);

    std::map<std::string,QString,std::less<>> names;
    if(state.management && state.management->people) {
        for(const auto& employee:state.management->people->employees) {
            names[employee.terms.profile.id]=text(employee.terms.profile.name);
            names[world.company_id+"/"+employee.terms.profile.id]=text(employee.terms.profile.name);
        }
        for(const auto& [id,person]:state.management->people->industry) names[id]=text(person.profile.name);
    }
    if(world.family) for(const auto& person:world.family->members) names[person.profile.id]=text(person.profile.name);
    QStringList people_record;
    if(state.management && state.management->people && state.management->people->social) {
        const auto& social=*state.management->people->social;
        for(const auto& link:social.links) {
            if(link.first!="founder" && link.second!="founder") continue;
            const auto& other=link.first=="founder"?link.second:link.first;
            const auto person=names.find(other);
            if(person==names.end()) continue;
            if(core::is_personal_rival(state,"founder",other)) continue;
            people_record.push_back(QStringLiteral("%1's recorded relationship with %2 was %3 after %4 encounters.")
                .arg(name,person->second,text(core::friendship_name(state,link.affinity,false))).arg(link.encounters));
        }
        if(social.mission_conflict) for(const auto& [id,concern]:social.mission_conflict->concerns) {
            const auto person=names.find(id);
            if(person==names.end()) continue;
            if(concern.frenemy) people_record.push_back(QStringLiteral("%1 and %2 rebuilt a friendship after a rivalry over the company's mission.").arg(name,person->second));
            else if(concern.rivalry_on && !concern.settled_on) people_record.push_back(QStringLiteral("%1 remained estranged from %2 after the change of mission.").arg(person->second,name));
        }
    }
    if(const auto* partner=runtime::romantic_partner(world,"founder")) {
        const auto id=partner->first=="founder"?partner->second:partner->first;
        if(const auto person=names.find(id);person!=names.end())
            people_record.push_back(QStringLiteral("The family record lists %1 as %2's %3.")
                .arg(person->second,name,partner->married_on?QStringLiteral("spouse"):
                    partner->engaged_on?QStringLiteral("fiancé"):QStringLiteral("partner")));
    }
    if(world.family) {
        std::set<std::string> children;
        for(const auto& edge:world.family->edges)
            if(edge.parent=="founder" && !edge.ended_on) children.insert(edge.child);
        QStringList children_names;
        for(const auto& id:children) if(const auto person=names.find(id);person!=names.end()) children_names.push_back(person->second);
        if(!children_names.empty()) people_record.push_back(QStringLiteral("The family register records %1 as children in %2's care.")
            .arg(children_names.join(QStringLiteral(", ")),name));
    }
    article("people",QStringLiteral("The people who knew the founder"),people_record);

    QStringList departures;
    if(state.succession && state.succession->releases) for(const auto& release:*state.succession->releases) {
        const auto coverage=departureArticle(state,release);
        departures.push_back(QStringLiteral("%1 · %2").arg(coverage.value("date").toString(),coverage.value("headline").toString()));
        departures.push_back(coverage.value("statement").toString());
        departures.push_back(coverage.value("response").toString());
        if(!coverage.value("facts").toString().isEmpty()) departures.push_back(coverage.value("facts").toString());
    }
    article("departures",QStringLiteral("The departure and its coverage"),departures);

    QStringList choices;
    const auto& history=checkpoint.runtime.presentations;
    for(auto i=history.rbegin();i!=history.rend() && choices.size()<8;++i) {
        if(i->day>day || !i->resolved || i->choices.size()<2) continue;
        const auto chosen=std::ranges::find(i->choices,i->selected_choice,&runtime::PresentedChoice::id);
        if(chosen==i->choices.end()) continue;
        const auto* label=program.collection.message(chosen->label_key);
        if(!label) continue;
        auto account=QStringLiteral("On %1, %2 chose “%3”.").arg(date(i->day),name,text(*label));
        for(auto outcome=history.rbegin();outcome!=history.rend();++outcome) {
            if(outcome->day>day || outcome->activation_id!=i->activation_id || outcome->kind!=runtime::PresentationKind::result) continue;
            if(const auto* result=program.collection.message(outcome->text_key))
                account+=QStringLiteral(" The recorded outcome on %1 was %2").arg(date(outcome->day),text(*result));
            break;
        }
        choices.push_front(std::move(account));
    }
    article("choices",QStringLiteral("Decisions in the public record"),choices);
    return {{"obituary",obituary},{"date",date(day)},{"masthead",QStringLiteral("THE WORLD REGISTER")},
        {"headline",obituary?QStringLiteral("%1 dies at 100").arg(name):
            voluntary?QStringLiteral("%1 looks back at %2").arg(name).arg(age):QStringLiteral("%1 — a life in progress").arg(name)},
        {"deck",world.founding?QStringLiteral("1997–%1 · %2").arg(core::from_ordinal(day).year).arg(company):company},
        {"articles",articles}};
}
}
