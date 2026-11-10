#include "world_controller.hpp"
#include "founding_controller.hpp"
#include <pgp/persistence/sha256.hpp>
#include <QFile>
#include <QResource>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <QLocale>
#include <algorithm>

static void initialize_world_baseline() {
    static const bool initialized=[] {Q_INIT_RESOURCE(world_baseline);return true;}();
    (void)initialized;
}
namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
QVariantMap moodRow(const core::EmployeeMood& mood) {
    QVariantList factors;
    const auto add=[&](QString name,int points){factors.push_back(QVariantMap{{"name",std::move(name)},{"points",points}});};
    add(QObject::tr("Salary versus expectations"),mood.pay);
    add(QObject::tr("Company publications"),mood.company_prestige);
    add(QObject::tr("CEO reputation"),mood.ceo_prestige);
    add(QObject::tr("Mission and duties"),mood.role);
    add(QObject::tr("Shared traits with coworkers"),mood.coworkers);
    add(QObject::tr("Office amenities"),mood.amenities);
    add(QObject::tr("Temperament"),mood.temperament_effect);
    add(QObject::tr("Recent hosted gatherings"),mood.gatherings);
    QStringList amenities;
    for(const auto& name:mood.accessible_amenities) amenities.push_back(q(name));
    return {{"score",mood.score},{"label",mood.score>=75?QObject::tr("Thriving"):mood.score>=60?QObject::tr("Content"):
        mood.score>=45?QObject::tr("Steady"):mood.score>=30?QObject::tr("Frustrated"):QObject::tr("Strained")},
        {"temperament",q(mood.temperament)},{"factors",factors},{"amenities",amenities.join(QStringLiteral(", "))}};
}
QString portraitUrl(const core::PortraitRecipe& recipe) {
    QString url=QStringLiteral("image://portraits/%1").arg(recipe.kit);
    for(auto part:recipe.parts) url+='/'+QString::number(part);
    for(auto part:recipe.style) url+='/'+QString::number(part);
    return url;
}
QVariantMap profileRow(const core::PersonProfile& p,core::OrdinalDay now,QString status) {
    return {{"id",q(p.id)},{"portrait",portraitUrl(p.portrait)},{"name",q(p.name)},{"initials",q(p.name).section(' ',0,0).left(1)+q(p.name).section(' ',-1).left(1)}, {"role",q(p.role)},
        {"detail",QObject::tr("%1 · %2 · Age %3 · %4 years of experience\n%5\nResearch %6 · Engineering %7 · Evaluation %8\n%9")
            .arg(q(p.gender),q(p.pronouns)).arg(core::from_ordinal(now).year-p.birth_year)
            .arg(p.experience+static_cast<unsigned>(std::max(0,core::from_ordinal(now).year-core::from_ordinal(p.applied_on).year)))
            .arg(q(p.education)).arg(p.research).arg(p.engineering).arg(p.evaluation).arg(status)}, {"bio",q(p.bio)}};
}
QVariantMap openingOffer(const runtime::CompiledProgram* program,const runtime::WorldState& world,const core::PeopleContact& contact) {
    if(!program || !world.founding || !contact.opening) return {};
    const auto* definition=program->collection.find(world.scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if(!scenario || !scenario->founding) return {};
    return FoundingController::presentOffer(*scenario->founding,*world.founding,
        q(contact.person.substr(core::opening_contact_prefix.size())),q(contact.opening->source),q(contact.opening->outcome));
}
QVariantMap topic(QString id,QString button,QString label,bool asked) {
    return {{"id",std::move(id)},{"button",std::move(button)},{"label",std::move(label)},{"asked",asked}};
}
QString city(std::string_view state) {
    static const QJsonObject cities=[] {
        initialize_world_baseline();QFile file{QStringLiteral(":/pgp/assets/maps/city-context-2010.json")};
        if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object().value("cities").toObject();
    }();
    auto key=q(std::string{state});if(key.startsWith("state.us_")) key="US-"+key.mid(9).toUpper();
    return cities.value(key).toObject().value("name").toString(q(std::string{state}));
}
QVariantMap navDefault() {return {{"place","workspace"},{"scale","hq"},{"device",""},{"app",""},{"section",""}};}
bool valid(const QVariantMap& n) {
    return QStringList{"workspace","lab","meeting","compute","map","research_annex","evaluation_suite","collaboration"}.contains(n.value("place").toString()) &&
        QStringList{"hq","city","state","country","world"}.contains(n.value("scale").toString()) &&
        QStringList{"","desktop","laptop","phone","tablet"}.contains(n.value("device").toString()) &&
        QStringList{"","devices","office","lab","people","facilities","inbox","operations","civic","society","institutions","frontier","leadership","objectives"}.contains(n.value("app").toString()) &&
        n.value("section").toString().size()<=40;
}
}
WorldController::WorldController(std::filesystem::path directory,QObject* parent):QObject(parent),directory_(std::move(directory)) {
    QFile file{QStringLiteral(":/pgp/assets/maps/world-countries.json")};
    if(file.open(QIODevice::ReadOnly) && file.size()<1024*1024) world_geometry_=QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}
void WorldController::update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace) {
    program_=&program;
    snapshot_=std::move(snapshot);
    if (replace || path_.empty()) {
        if(snapshot_->checkpoint.world.simulation.infrastructure) county_state_=q(snapshot_->checkpoint.world.simulation.infrastructure->executive_base);
        const auto& w=snapshot_->checkpoint.world;
        const auto key=w.identity && !w.identity->instance_id.empty()?"instance/"+w.identity->instance_id:
            std::to_string(w.root_seed)+"/"+w.scenario_id+"/"+(w.identity?w.identity->company_name:"")+"/"+(w.identity?w.identity->founder_name:"");
        path_=directory_/(persistence::sha256(key)+".json");
        restore();
    }
    if(navigation_.value("place")!="map" && std::ranges::none_of(rooms(),[&](const QVariant& row){return row.toMap().value("id")==navigation_.value("place");})) {
        navigation_["place"]="workspace";
        emit navigationChanged();
    }
    const auto app=navigation_.value("app").toString();
    if(!app.isEmpty() && !navigate(navigation_.value("place").toString(),navigation_.value("scale").toString(),navigation_.value("device").toString(),app,navigation_.value("section").toString())) {
        navigation_["app"]="";navigation_["device"]="";navigation_["section"]="";emit navigationChanged();
    }
    emit changed();
}
void WorldController::restore() {
    navigation_=navDefault();
    drafts_.clear();
    QFile f{QString::fromStdU16String(path_.u16string())};
    if(f.open(QIODevice::ReadOnly) && f.size()<=8192) {
        const auto n=QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
        if(valid(n)) navigation_=n;
    }
    QFile drafts{QString::fromStdU16String(path_.u16string())+".drafts"};
    if(drafts.open(QIODevice::ReadOnly) && drafts.size()<=256*1024) {
        const auto values=QJsonDocument::fromJson(drafts.readAll()).object().toVariantMap();
        if(values.size()<=512) for(auto it=values.begin();it!=values.end();++it)
            if(it.key().size()<=192 && it.value().metaType().id()==QMetaType::QString && it.value().toString().size()<=8192) drafts_[it.key()]=it.value();
    }
    emit navigationChanged();
    emit draftsReset();
}
QString WorldController::referenceCity(QString state) const {return city(state.toStdString());}
QString WorldController::reviewToken() const {
    return snapshot_?QString::number(snapshot_->generation)+"/"+QString::number(snapshot_->checkpoint.world.simulation.revision):QString{};
}
QString WorldController::draft(QString key,QString fallback) const {return drafts_.value(key,fallback).toString();}
void WorldController::setDraft(QString key,QString value) {
    if(!snapshot_ || !snapshot_->current_campaign || snapshot_->storage_blocked || key.isEmpty() || key.size()>192 || value.size()>8192 ||
        (drafts_.size()>=512 && !drafts_.contains(key)) || drafts_.value(key)==value) return;
    auto next=drafts_;next[key]=value;
    if(!QDir{}.mkpath(QString::fromStdU16String(directory_.u16string()))) {emit noticeRaised(tr("Could not preserve this unfinished field. Check local storage."));return;}
    QSaveFile file{QString::fromStdU16String(path_.u16string())+".drafts"};
    const auto bytes=QJsonDocument{QJsonObject::fromVariantMap(next)}.toJson(QJsonDocument::Compact);
    if(bytes.size()>256*1024) {emit noticeRaised(tr("Unfinished form storage is full. Existing drafts were preserved."));return;}
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) {emit noticeRaised(tr("Could not preserve this unfinished field. Check local storage."));return;}
    drafts_=std::move(next);
}
bool WorldController::navigate(QString place,QString scale,QString device,QString app,QString section) {
    QVariantMap next{{"place",place},{"scale",scale},{"device",device},{"app",app},{"section",section}};
    if(!snapshot_ || !valid(next)) return false;
    if(place!="map" && std::ranges::none_of(rooms(),[&](const QVariant& row){return row.toMap().value("id")==place;})) return false;
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(app=="devices" || app.isEmpty() || companyArchive()) next["device"]="";
    else if(s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->devices) {
        const auto resolved=core::resolve_app_device(s,device.toStdString(),app.toStdString(),atHeadquarters());
        if(resolved.empty()) {
            emit noticeRaised(tr("No working device here supports that application. Turn on a device, arrange service or choose equipment."));
            next["app"]="devices";next["device"]="";next["section"]="";
        } else next["device"]=q(resolved);
    } else if(!device.isEmpty() && std::ranges::none_of(devices(),[&](const QVariant& row){return row.toMap().value("id")==device;})) return false;
    if(next==navigation_) return true;
    if(snapshot_->current_campaign) {
        if(!QDir{}.mkpath(QString::fromStdU16String(directory_.u16string()))) {emit noticeRaised(tr("Could not remember your place. Check local storage."));return false;}
        QSaveFile f{QString::fromStdU16String(path_.u16string())};
        const auto bytes=QJsonDocument{QJsonObject::fromVariantMap(next)}.toJson(QJsonDocument::Compact);
        if(!f.open(QIODevice::WriteOnly) || f.write(bytes)!=bytes.size() || !f.commit()) {emit noticeRaised(tr("Could not remember your place. Check local storage."));return false;}
    }
    navigation_=std::move(next);emit navigationChanged();return true;
}
bool WorldController::atHeadquarters() const {
    if(!snapshot_) return true;
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(!s.infrastructure) return true;
    const auto location=core::executive_location(s);
    if(location.traveling) return false;
    if(location.appointment) for(const auto& appointment:s.infrastructure->appointments)
        if(appointment.id==*location.appointment && appointment.mode==core::AppointmentMode::onsite) return false;
    return true;
}
QString WorldController::location() const {
    if(!snapshot_) return {};
    const auto& w=snapshot_->checkpoint.world;
    const auto& s=w.simulation;
    if(s.infrastructure) {
        const auto loc=core::executive_location(s);
        if(loc.traveling) return tr("In transit · %1").arg(city(loc.state_id));
        if(loc.appointment && !atHeadquarters()) {
            for(const auto& appointment:s.infrastructure->appointments) if(appointment.id==*loc.appointment)
                for(const auto& site:s.infrastructure->sites) if(site.id==appointment.site_id) return q(site.name)+" · "+city(site.state_id);
        }
        if(s.infrastructure->workplace && s.infrastructure->workplace->headquarters) {
            const auto& p=core::current_headquarters(s);const auto& markets=s.infrastructure->catalog.workplace->headquarters->markets;
            const auto it=std::ranges::find(markets,p.market,&core::HQMarket::id);
            if(it!=markets.end()) return q(p.address)+" · "+q(it->city);
        }
        return tr("Headquarters · %1").arg(city(s.infrastructure->executive_base));
    }
    return tr("Headquarters · %1").arg(city(w.state_id));
}
QString WorldController::setting() const {
    if(!snapshot_) return {};
    const auto& w=snapshot_->checkpoint.world;
    if(!atHeadquarters()) return tr("Away from HQ · your portable devices keep you connected.");
    for(const auto& value:headquartersRooms()) {
        const auto room=value.toMap();
        if(room.value("ready").toBool() && room.value("place")==navigation_.value("place"))
            return room.value("name").toString()+QStringLiteral(" · ")+room.value("detail").toString();
    }
    if(w.founding) {
        if(w.founding->office=="home") return tr("Home workspace · a company starts with one good idea.");
        if(w.founding->office=="shared") return tr("Shared workspace · a small team, room to grow.");
    }
    return tr("Company office · people, experiments and decisions in one place.");
}
QVariantMap WorldController::founder() const {
    if(!snapshot_) return {};
    const auto& w=snapshot_->checkpoint.world;
    const auto name=w.identity?q(w.identity->founder_name):tr("Founder");
    QString detail=w.identity?q(w.identity->founder_pronouns):QString{};
    if(w.founding) detail+=tr(" · %1 · Age %2").arg(q(w.founding->gender)).arg(core::from_ordinal(w.simulation.date).year-1997);
    auto portrait=core::make_portrait(w.root_seed,"founder/"+name.toStdString());bool managing=false;
    if(w.founding) portrait=w.founding->appearance;
    const auto* manager=core::operations_manager_on(w.simulation,w.simulation.date);
    if(w.simulation.management && w.simulation.management->people) for(const auto& employee:w.simulation.management->people->employees)
        if(employee.terms.profile.role=="Founder / researcher") {
            const auto& p=employee.terms.profile;portrait=p.portrait;managing=manager && manager->person==p.id;
            detail+=tr("\n%1\nResearch %2 · Engineering %3 · Evaluation %4").arg(q(p.education)).arg(p.research).arg(p.engineering).arg(p.evaluation);
        }
    QString bio;
    if(w.founding) {
        const auto& f=*w.founding;
        const QString advantage=f.leg=="genius"?tr("Actual Genius"):f.leg=="wealthy"?tr("Wealthy Elite"):f.leg=="political"?tr("Child of Political Elite"):tr("Extremely Charismatic");
        bio=tr("%1\nFamily: %2\n%3 · %4 · %5").arg(advantage,q(f.family),q(f.sociability),q(f.activity),q(f.disposition));
    }
    return {{"id","founder"},{"portrait",portraitUrl(portrait)},{"name",name},{"initials",name.section(' ',0,0).left(1)+name.section(' ',-1).left(1)},
        {"detail",detail},{"role",managing?tr("Founder / operations manager"):w.founding && w.founding->route=="hired_ceo"?tr("Chief executive"):tr("Founder")},{"bio",bio}};
}
QVariantList WorldController::people() const {
    QVariantList rows;
    if(!snapshot_) return rows;
    rows.push_back(founder());
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(s.management) {
        const auto moods=core::workplace_moods(s);
        if(s.management->people) for(const auto& employee:s.management->people->employees) {
            if(employee.departed_on || employee.terms.profile.role=="Founder / researcher") continue;
            const auto& p=employee.terms.profile;
            const auto* manager=core::operations_manager_on(s,s.date);const bool managing=manager && manager->person==p.id;
            const auto desk=core::assigned_hq_desk(s,p.id);
            QString workplace;
            if(s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->headquarters)
                workplace=core::hq_laptop_ready(s,p.id)?tr("\nIssued laptop · can work at any desk or from home"):desk?tr("\nAssigned to desk %1").arg(desk):tr("\nNo desk assigned at headquarters");
            if(!employee.opening) workplace+=tr("\nAgreed salary %1/month · signing payment %2")
                .arg(QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(employee.terms.salary.value())/100),
                    QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(employee.terms.signing.value())/100));
            else if(s.management->catalog.people->dismissal) workplace+=tr("\nInherited salary %1/month")
                .arg(QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(core::employee_monthly_salary(s,employee).value())/100));
            const auto years=std::max(0,core::from_ordinal(s.date).year-core::from_ordinal(p.applied_on).year);
            auto row=QVariantMap{{"id",q(p.id)},{"portrait",portraitUrl(p.portrait)},{"name",q(p.name)},{"initials",q(p.name).section(' ',0,0).left(1)+q(p.name).section(' ',-1).left(1)}, {"role",managing?tr("HQ operations manager"):q(p.role)},
                {"detail",tr("%1 · %2 · Age %3 · %4 years of experience\n%5\nResearch %6 · Engineering %7 · Evaluation %8\n%9")
                    .arg(q(p.gender),q(p.pronouns)).arg(core::from_ordinal(s.date).year-p.birth_year).arg(p.experience+static_cast<unsigned>(years))
                    .arg(q(p.education)).arg(p.research).arg(p.engineering).arg(p.evaluation)
                    .arg(s.date<employee.ready_on?tr("Onboarding until %1").arg(q(core::iso_date(employee.ready_on))):managing?tr("Dedicated to branch coordination; one staff allocation is reserved"):tr("Onboarding complete"))+workplace+([&]{QStringList traits;for(const auto& t:p.traits)traits.push_back(q(t));return QStringLiteral("\n")+traits.join(" · ");})()}, {"bio",q(p.bio)}};
            if(const auto mood=moods.find(p.id);mood!=moods.end()) row["mood"]=moodRow(mood->second);
            rows.push_back(std::move(row));
        }
        for(const auto& hire:s.management->specialists) {
            if(hire.withdrawn_on) continue;
            const auto it=std::ranges::find(s.management->catalog.specialists,hire.offer_id,&core::SpecialistOffer::id);
            if(it==s.management->catalog.specialists.end()) continue;
            const auto schedule=core::specialist_schedule(*it,hire.day);
            if(s.management->people) {
                const auto& p=s.management->people->known_people.at(hire.offer_id);
                rows.push_back(profileRow(p,s.date,s.date<schedule.ready_on?tr("Onboarding"):tr("Ready for assignment")));continue;
            }
            rows.push_back(QVariantMap{{"name",q(it->name)},{"initials",q(it->name).left(1)},
                {"role",tr("Evaluation specialist")},{"detail",s.date<schedule.starts_on?tr("Joining soon"):s.date<schedule.ready_on?tr("Onboarding"):tr("Available for assignment")}});
        }
        if(s.management->governance && s.management->catalog.governance)
            for(const auto& hire:s.management->governance->appointments) {
                if(hire.departed_on) continue;
                const auto it=std::ranges::find(s.management->catalog.governance->executives,hire.candidate,&core::ExecutiveCandidate::id);
                if(it!=s.management->catalog.governance->executives.end() && s.management->people) {
                    rows.push_back(profileRow(s.management->people->known_people.at(hire.candidate),s.date,tr("Appointed executive")));continue;
                }
                if(it!=s.management->catalog.governance->executives.end()) rows.push_back(QVariantMap{{"name",q(it->name)},{"initials",q(it->name).left(1)}, {"role",q(it->role)},{"detail",q(it->priorities)}});
            }
    }
    if(s.succession && s.succession->terms.back().person!="founder") rows.push_back(person("leadership.ceo",s.date));
    return rows;
}
bool WorldController::contactsAvailable() const {
    if(!snapshot_) return false;
    const auto& state=snapshot_->checkpoint.world.simulation;
    return state.management && state.management->people && state.infrastructure &&
        state.infrastructure->workplace && state.infrastructure->workplace->headquarters;
}

QVariantList WorldController::contacts() const {
    QVariantList rows;
    if(!contactsAvailable()) return rows;
    const auto& world=snapshot_->checkpoint.world;
    const auto& roster=*world.simulation.management->people;
    for(const auto& contact:roster.contacts) {
        if(contact.opening) {
            const auto offer=openingOffer(program_,world,contact);
            if(offer.isEmpty()) continue;
            const auto name=offer.value("name").toString();
            rows.push_back(QVariantMap{{"id",q(contact.person)},{"name",name},{"role",offer.value("organization")},
                {"portrait",offer.value("portrait")},{"initials",name.section(' ',0,0).left(1)+name.section(' ',1,1).left(1)}});
            continue;
        }
        const auto found=std::ranges::find_if(roster.employees,[&](const auto& row){return row.terms.profile.id==contact.person;});
        if(found==roster.employees.end()) continue;
        const auto& person=found->terms.profile;
        rows.push_back(QVariantMap{{"id",q(person.id)},{"name",q(person.name)},{"portrait",portraitUrl(person.portrait)},
            {"role",found->departed_on?tr("Former employee"):q(person.role)}});
    }
    return rows;
}

QVariantList WorldController::roomContacts(int room) const {
    QVariantList rows;
    if(!contactsAvailable() || room<1) return rows;
    const auto& state=snapshot_->checkpoint.world.simulation;
    for(const auto& employee:state.management->people->employees) {
        const auto& person=employee.terms.profile;
        if(core::employee_contact_room(state,person.id)!=static_cast<std::uint32_t>(room)) continue;
        rows.push_back(QVariantMap{{"id",q(person.id)},{"name",q(person.name)},{"portrait",portraitUrl(person.portrait)},{"role",q(person.role)}});
    }
    return rows;
}

QVariantMap WorldController::contactThread(QString person) const {
    if(!contactsAvailable() || person.isEmpty()) return {};
    const auto& world=snapshot_->checkpoint.world;
    const auto& state=world.simulation;
    const auto& roster=*state.management->people;
    const auto id=person.toStdString();
    const auto contact=std::ranges::find(roster.contacts,id,&core::PeopleContact::person);
    if(contact!=roster.contacts.end() && contact->opening) {
        const auto offer=openingOffer(program_,world,*contact);
        if(offer.isEmpty()) return {};
        const auto& outcome=contact->opening->outcome;
        const bool signed_offer=outcome=="signed";
        const bool followup=std::ranges::any_of(contact->funding,[](const auto& discussion){return discussion.status=="submitted";});
        const bool alder=id==std::string{core::opening_contact_prefix}+"alder";
        const auto original=offer.value("body").toString();
        QVariantList messages{QVariantMap{{"incoming",true},{"prompt",offer.value("subject")},
            {"reply",outcome=="rejected"?original:original+"\n\n"+offer.value("terms").toString()}}};
        const auto opening_label=tr("Confirm the opening agreement");
        const auto board_label=tr("Ask about their current board role");
        bool checked_opening=false,checked_board=false;
        for(const auto& message:contact->messages) {
            const bool opening=message.topic=="opening_terms";
            QString reply;
            if(message.reply=="signed") reply=tr("We signed the agreement in this thread. Those terms applied when your company opened.");
            else if(message.reply=="declined") reply=tr("You declined our opening offer. There is no agreement to carry forward.");
            else if(message.reply=="rejected") reply=tr("The leadership application closed before your company began.");
            else if(message.reply=="unsigned") reply=tr("The opening offer closed when you signed other paperwork.");
            else if(message.reply=="has_seat") reply=offer.value("kind")=="partner"?tr("I vote on the board under the terms we signed."):
                alder?tr("Alder's existing owners have a seat on the current board."):
                tr("Our organization has a seat on the current board under the signed agreement.");
            else reply=tr("Our organization has no seat on the current board.");
            messages.push_back(QVariantMap{{"topic",q(message.topic)},{"date",q(core::iso_date(message.day))},{"prompt",opening?opening_label:board_label},{"reply",reply}});
            if(message.day==state.date) {
                if(opening) checked_opening=true;
                else checked_board=true;
            }
        }
        QVariantList topics{topic("opening_terms","contactOpeningTerms",opening_label,checked_opening)};
        if(signed_offer || followup) topics.push_back(topic("board_role","contactBoardRole",board_label,checked_board));
        const auto status=signed_offer?tr("Signed at opening"):outcome=="declined"?tr("Declined before signing"):
            outcome=="rejected"?tr("Application closed"):tr("Expired at opening");
        return {{"id",person},{"name",offer.value("name")},{"role",signed_offer && offer.value("kind")=="partner"?tr("Cofounder"):offer.value("organization")},
            {"portrait",offer.value("portrait")},{"status",status},
            {"exchanged",true},{"current",false},{"governance",(signed_offer || followup) && state.management->governance.has_value()},
            {"messages",messages},{"topics",topics}};
    }
    const auto employee=std::ranges::find_if(roster.employees,[&](const auto& row){return row.terms.profile.id==id;});
    if(employee==roster.employees.end() || employee->terms.profile.role=="Founder / researcher") return {};
    QVariantList messages;
    bool checked_in=false,checked_workstation=false;
    if(contact!=roster.contacts.end()) for(const auto& message:contact->messages) {
        QString reply;
        if(message.reply=="mood") reply=message.mood<45?tr("It's been a difficult stretch. Can we talk about what would help?"):
            message.mood>=75?tr("I'm happy with how things are going here."):tr("Things are going all right. Thanks for asking.");
        else if(message.reply=="laptop") reply=tr("I have a working company laptop. I can work at a desk or from home.");
        else if(message.reply=="desk") reply=tr("My computer at headquarters is ready.");
        else if(message.reply=="needs_computer") reply=tr("I still need a working desk computer or an issued laptop.");
        else if(message.reply=="former") reply=tr("I've moved on from the company. Thanks for keeping in touch.");
        else reply=tr("I'm getting settled. Thanks for checking in.");
        const bool check_in=message.topic=="check_in";
        messages.push_back(QVariantMap{{"topic",q(message.topic)},{"date",q(core::iso_date(message.day))},
            {"prompt",check_in?tr("How's work going?"):tr("Do you have a working computer?")},{"reply",reply}});
        if(message.day==state.date) {
            if(check_in) checked_in=true;
            else checked_workstation=true;
        }
    }
    const auto& profile=employee->terms.profile;
    const auto employment_status=employee->dismissal_severance?
        tr("Employment ended %1 · Severance paid %2").arg(q(core::iso_date(*employee->departed_on)),
            QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(employee->dismissal_severance->value())/100)):
        employee->departed_on?tr("Former employee"):tr("Current employee");
    const QVariantList topics{topic("check_in","contactCheckIn",tr("How's work going?"),checked_in),
        topic("workstation","contactWorkstation",tr("Ask about their computer"),checked_workstation)};
    return {{"id",person},{"name",q(profile.name)},{"portrait",portraitUrl(profile.portrait)},{"role",q(profile.role)},
        {"exchanged",contact!=roster.contacts.end()},{"current",!employee->departed_on},{"topics",topics},{"messages",messages},
        {"status",employment_status},{"canDismiss",!employee->departed_on && state.management->catalog.people->dismissal.has_value()},
        {"room",core::employee_contact_room(state,id)}};
}

void WorldController::exchangeContact(QString person,int room,QString revision) {
    if(hqReview(revision) && room>0)
        emit actionRequested(core::ManagePeopleContact{core::ExchangePeopleContact{person.toStdString(),static_cast<std::uint32_t>(room)}});
}

void WorldController::messageContact(QString person,QString topic,int room,QString revision) {
    if(!hqReview(revision) || room<0) return;
    const auto device=room==0?navigation_.value("device").toString().toStdString():std::string{};
    emit actionRequested(core::ManagePeopleContact{core::MessagePeopleContact{
        person.toStdString(),topic.toStdString(),device,static_cast<std::uint32_t>(room)}});
}

QVariantMap WorldController::dismissalQuote(QString person,int room) const {
    if(!contactsAvailable() || room<0) return {};
    const auto& state=snapshot_->checkpoint.world.simulation;
    try {
        const auto quote=core::dismissal_quote(state,person.toStdString());
        const auto money=[](core::Cents amount){return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(amount.value())/100);};
        const auto employee=std::ranges::find(state.management->people->employees,person.toStdString(),[](const auto& row){return row.terms.profile.id;});
        QStringList work;
        for(const auto& id:quote.projects) {
            const auto project=std::ranges::find(state.capacity->projects,id,&core::CapacityProject::id);
            work.push_back(q(project->cause.empty()?id:project->cause));
        }
        const auto device=room==0?navigation_.value("device").toString().toStdString():std::string{};
        core::Simulation preview{state};
        const auto result=preview.submit({state.revision,core::ManagePeopleContact{core::DismissPeopleContact{
            person.toStdString(),device,static_cast<std::uint32_t>(room)}}});
        const auto months=state.management->catalog.people->dismissal->severance_months;
        const auto duration=months==1?tr("1 month of salary"):tr("%1 months of salary").arg(months);
        auto detail=tr("End %1's employment today.\n\nCompany payment now: %2 severance (%3).\nPayroll ends today: %4/month. Earned wages and existing bills remain payable.\n\n%5 desk or laptop assignments will be released; the company keeps its equipment.")
            .arg(q(employee->terms.profile.name),money(quote.severance),duration)
            .arg(money(quote.monthly_salary)).arg(quote.returned_items);
        if(!work.empty()) detail+=tr("\n\nThese unfinished projects will be cancelled. Completed work stays in their history; choose a new team to begin again:\n• %1").arg(work.join("\n• "));
        if(quote.operations_manager) detail+=tr("\n\nTheir operations manager appointment will end.");
        detail+=tr("\n\nTheir conversation remains available in Messages.");
        if(!result.accepted()) detail+="\n\n"+q(result.explanation);
        return {{"allowed",result.accepted()},{"detail",detail},{"portrait",portraitUrl(employee->terms.profile.portrait)},
            {"name",q(employee->terms.profile.name)},{"severance",money(quote.severance)},{"payroll",money(quote.monthly_salary)}};
    } catch(const std::exception& error) {return {{"allowed",false},{"detail",q(error.what())}};}
}

void WorldController::dismissContact(QString person,int room,QString revision) {
    if(!hqReview(revision) || room<0) return;
    const auto device=room==0?navigation_.value("device").toString().toStdString():std::string{};
    emit actionRequested(core::ManagePeopleContact{core::DismissPeopleContact{
        person.toStdString(),device,static_cast<std::uint32_t>(room)}});
}

QVariantList WorldController::applicants() const {
    QVariantList rows;
    if(!snapshot_ || !snapshot_->checkpoint.world.simulation.management || !snapshot_->checkpoint.world.simulation.management->people) return rows;
    const auto& s=snapshot_->checkpoint.world.simulation;
    const auto money=[](core::Cents amount){return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(amount.value())/100);};
    for(const auto& applicant:s.management->people->applicants) {
        const auto& p=applicant.profile;
        auto row=QVariantMap{{"id",q(p.id)},{"portrait",portraitUrl(p.portrait)},{"name",q(p.name)},{"role",q(p.role)},{"initials",q(p.name).left(1)},
            {"detail",tr("%1 · %2 · Age %3 · %4 years of experience\n%5\nResearch %6 · Engineering %7 · Evaluation %8 · Mission fit %9/100\n%10/month · %11 signing · %12 days onboarding\nEvaluation work credit: %13%")
                .arg(q(p.gender),q(p.pronouns)).arg(core::from_ordinal(p.applied_on).year-p.birth_year).arg(p.experience).arg(q(p.education))
                .arg(p.research).arg(p.engineering).arg(p.evaluation).arg(p.mission_fit).arg(money(applicant.salary),money(applicant.signing))
                .arg(s.management->catalog.people->onboarding.value()).arg(static_cast<double>(applicant.evaluation_credit.value())/100)}, {"bio",q(p.bio)}};
        std::vector<const core::PersonProfile*> team;
        for(const auto& employee:s.management->people->employees) if(!employee.departed_on) team.push_back(&employee.terms.profile);
        const auto before=core::team_dynamics(team);team.push_back(&p);const auto after=core::team_dynamics(team);
        QStringList traits;for(const auto& trait:p.traits) traits.push_back(q(trait));
        row["detail"]=row["detail"].toString()+tr("\nDistinct perspectives added to the current roster: +%1 · team total %2\nShared-trait compatibility after hiring: %3%\n%4")
            .arg(after.creativity-before.creativity).arg(after.creativity).arg(static_cast<double>(after.compatibility_bp)/100,0,'f',1).arg(traits.join(" · "));
        row["negotiated"]=applicant.recruitment.has_value();
        if(applicant.recruitment) {
            const auto status=core::recruitment_status(applicant,s.date);
            if(status=="waiting") row["discussion"]=tr("Considering your offer");
            else if(status=="counter") row["discussion"]=tr("Counteroffer received");
            else if(status=="agreed") row["discussion"]=tr("Terms agreed · awaiting signature");
            else if(status=="declined") row["discussion"]=tr("Discussion ended");
            else if(status=="expired") row["discussion"]=tr("Terms expired");
        }
        rows.push_back(row);
    }
    return rows;
}
QVariantMap WorldController::recruitingTerms() const {
    if(!snapshot_ || !snapshot_->checkpoint.world.simulation.management || !snapshot_->checkpoint.world.simulation.management->people) return {};
    const auto& p=*snapshot_->checkpoint.world.simulation.management->people;
    return {{"pay",static_cast<int>(p.offered_pay.value()/100)},{"mission",q(p.mission)},{"negotiated",snapshot_->checkpoint.world.simulation.management->catalog.people->recruitment.has_value()}};
}
QString WorldController::recruitingSummary() const {
    if(!snapshot_ || !snapshot_->checkpoint.world.simulation.management || !snapshot_->checkpoint.world.simulation.management->people) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& p=*s.management->people;
    auto summary=tr("%1 individual HQ staff · next applicants %2. New offers reflect pay, published research and mission fit. Existing offers retain their quoted terms. Evaluation expertise supplies the stated work credit.")
        .arg(p.employees.size()).arg(q(core::iso_date(p.next_recruitment)));
    if(s.management->catalog.people->recruitment) summary=tr("%1 HQ people · next applicants %2. Compare individual requests, send an offer and advance time for their response. Agree compensation before hiring; a delivered furnished desk is required. Pay starts when you sign, and research availability starts after onboarding.").arg(p.employees.size()).arg(q(core::iso_date(p.next_recruitment)));
    if(s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->departments) summary+=tr(" Engineering skill supports HQ operations management.");
    if(s.infrastructure && s.infrastructure->workplace) summary+=tr("\nHQ workstations: %1 of %2 reserved. Buy and place desks, or visit the realtor when the office fills.").arg(core::headquarters_seats_used(s)).arg(core::headquarters_seats(s));
    return summary;
}
void WorldController::hireEmployee(QString id) {emit actionRequested(core::HireEmployee{id.toStdString()});}
void WorldController::setRecruitingTerms(int pay,QString mission) {
    if(pay<75 || pay>150) {emit noticeRaised(tr("Recruiting pay must be between 75% and 150%."));return;}
    emit actionRequested(core::SetRecruitingTerms{core::SalaryRate{pay*100},mission.toStdString()});
}
QVariantList WorldController::managerCandidates() const {
    QVariantList rows;
    if(!snapshot_) return rows;const auto& s=snapshot_->checkpoint.world.simulation;
    if(!s.infrastructure || !s.infrastructure->workplace || !s.infrastructure->workplace->departments || !s.management || !s.management->people) return rows;
    const auto* current=core::operations_manager_on(s,s.date);
    for(const auto& person:s.management->people->employees) if(!person.departed_on && person.ready_on<=s.date) {
        const bool managing=current && current->person==person.terms.profile.id;
        auto row=profileRow(person.terms.profile,s.date,managing?tr("Dedicated HQ operations manager; staff and compute are reserved"):tr("Ready for a dedicated HQ role"));
        if(managing) row["role"]=tr("HQ operations manager");
        row["label"]=q(person.terms.profile.name)+tr(" · Engineering %1/100").arg(person.terms.profile.engineering);rows.push_back(std::move(row));
    }
    return rows;
}
QVariantList WorldController::branches() const {
    QVariantList rows;
    if(!snapshot_ || !snapshot_->checkpoint.world.simulation.infrastructure) return rows;
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& i=*s.infrastructure;
    for(const auto& site:i.sites) {
        if(site.closed_on) continue;
        std::uint64_t count=0,pending=0;
        for(const auto& hire:i.hires) if(hire.site_id==site.id) {if(hire.starts_on<=s.date) count+=hire.people;else pending+=hire.people;}
        rows.push_back(QVariantMap{{"name",q(site.name)},{"detail",tr("Operations department · %1 operators · %2 joining\n%3 · Staffing and capacity are managed with this facility.").arg(count).arg(pending).arg(q(site.state_id))}});
    }
    if(i.workplace && i.workplace->departments) for(auto& value:rows) {
        auto row=value.toMap();row["detail"]=row.value("detail").toString()+tr("\nEngineering team skill %1/100 · operations availability %2%\nHQ coordination affects usable output after physical power and operator requirements.")
            .arg(i.catalog.workplace->departments->operator_skill).arg(static_cast<double>(core::operations_availability(s,s.date).value())/100,0,'f',2);value=std::move(row);
    }
    for(const auto& value:overseasOffices()) {const auto office=value.toMap();rows.push_back(QVariantMap{{"name",office.value("name")},{"detail",tr("Local representation department · %1").arg(office.value("detail").toString())}});}
    return rows;
}
void WorldController::setCountyState(QString value) {
    if(value==county_state_ || value.size()!=5) return;
    county_state_=std::move(value);emit changed();
}
QVariantList WorldController::countyRows() const {
    QVariantList rows;
    if(!snapshot_ || !program_ || !snapshot_->checkpoint.world.counties) return rows;
    const auto& w=snapshot_->checkpoint.world;
    const auto& catalog=*std::get<content::ScenarioDefinition>(program_->collection.find(w.scenario_id)->data).counties;
    for(std::size_t i=0;i<catalog.counties.size();++i) {
        const auto& definition=catalog.counties[i];if(q(definition.state)!=county_state_) continue;
        const auto& condition=w.counties->counties[i];
        rows.push_back(QVariantMap{{"id",q(definition.id)},{"name",q(definition.name)}, {"latitude",static_cast<double>(definition.latitude)/1000000},
            {"longitude",static_cast<double>(definition.longitude)/1000000},{"economy",condition.economy},{"development",condition.development},
            {"detail",tr("%1 · %2 people\nEconomy %3/100 · Development %4/100 · Monthly change %5\n%6")
                .arg(q(definition.character),QLocale().toString(condition.population)).arg(static_cast<double>(condition.economy)/100).arg(static_cast<double>(condition.development)/100)
                .arg(static_cast<double>(condition.last_change)/100,0,'f',2).arg(q(definition.connections))}});
    }
    return rows;
}
QString WorldController::countySummary() const {
    if(!snapshot_ || !program_ || !snapshot_->checkpoint.world.counties) return tr("This older scenario uses state summaries. The founder campaign includes county conditions.");
    const auto& w=snapshot_->checkpoint.world;const auto& c=*std::get<content::ScenarioDefinition>(program_->collection.find(w.scenario_id)->data).counties;
    return q(c.assumptions)+tr("\nUpdated %1 · domestic driver %2 · global driver %3").arg(q(core::iso_date(w.counties->updated_on))).arg(w.counties->regional_driver).arg(w.counties->global_driver);
}
QVariantList WorldController::rooms() const {
    if(atHeadquarters() && snapshot_ && snapshot_->checkpoint.world.simulation.infrastructure && snapshot_->checkpoint.world.simulation.infrastructure->workplace && snapshot_->checkpoint.world.simulation.infrastructure->workplace->headquarters) return {QVariantMap{{"id","workspace"},{"name",tr("Headquarters")},{"detail",tr("Your actual premises and equipment")}}};
    if(!atHeadquarters()) return {QVariantMap{{"id","workspace"},{"name",tr("Current location")},{"detail",tr("Portable workspace")}}};
    QVariantList result{QVariantMap{{"id","workspace"},{"name",tr("Workspace")},{"detail",tr("Company and people")}}};
    if(officeKind()!="home") {
        result.push_back(QVariantMap{{"id","lab"},{"name",tr("Research desks")},{"detail",tr("Experiments and evidence")}});
        result.push_back(QVariantMap{{"id","meeting"},{"name",tr("Meeting area")},{"detail",tr("Decisions and leadership")}});
    }
    if(snapshot_ && snapshot_->checkpoint.world.simulation.infrastructure) {
        const auto& infrastructure=*snapshot_->checkpoint.world.simulation.infrastructure;
        if(std::ranges::any_of(infrastructure.sites,[&](const auto& site){return site.state_id==infrastructure.executive_base && site.commissioned_on && !site.closed_on;}))
            result.push_back(QVariantMap{{"id","compute"},{"name",tr("Compute facility")},{"detail",tr("Commissioned headquarters-state infrastructure")}});
    }
    for(const auto& value:headquartersRooms()) {
        const auto room=value.toMap();if(!room.value("ready").toBool()) continue;
        if(std::ranges::none_of(result,[&](const QVariant& row){return row.toMap().value("id")==room.value("place");}))
            result.push_back(QVariantMap{{"id",room.value("place")},{"name",room.value("name")},{"detail",room.value("detail")}});
    }
    return result;
}
QString WorldController::officeKind() const {
    if(snapshot_ && snapshot_->checkpoint.world.simulation.infrastructure && snapshot_->checkpoint.world.simulation.infrastructure->workplace && snapshot_->checkpoint.world.simulation.infrastructure->workplace->headquarters) return core::current_headquarters(snapshot_->checkpoint.world.simulation).kind=="home"?QStringLiteral("home"):QStringLiteral("leased");
    return snapshot_ && snapshot_->checkpoint.world.founding?q(snapshot_->checkpoint.world.founding->office):QStringLiteral("leased");
}
QVariantList WorldController::devices() const {
    if(snapshot_ && snapshot_->checkpoint.world.simulation.infrastructure && snapshot_->checkpoint.world.simulation.infrastructure->workplace && snapshot_->checkpoint.world.simulation.infrastructure->workplace->devices) {
        QVariantList result;QStringList added;const auto& s=snapshot_->checkpoint.world.simulation;const bool headquarters=atHeadquarters();
        for(const auto& d:s.infrastructure->workplace->devices->records) {
            const auto kind=q(d.offer);if(!core::device_available(s,d,headquarters) || added.contains(kind)) continue;
            const auto& offers=s.infrastructure->catalog.workplace->devices->offers;const auto& model=*std::ranges::find(offers,d.offer,&core::DeviceOffer::id);
            added.push_back(kind);result.push_back(QVariantMap{{"id",kind},{"name",q(model.name)}});
        }
        if(headquarters && s.infrastructure->workplace->headquarters && core::hq_computer_available(s)) result.push_back(QVariantMap{{"id","desktop"},{"name",tr("HQ computer")}});
        return result;
    }
    QVariantList result{QVariantMap{{"id","phone"},{"name",tr("Phone")},{"detail",tr("News, messages and calendar")}},
        QVariantMap{{"id","laptop"},{"name",tr("Laptop")},{"detail",tr("Your company applications")}}};
    if(atHeadquarters() && officeKind()!="home") result.push_back(QVariantMap{{"id","desktop"},{"name",tr("Office workstation")},{"detail",tr("All company applications")}});
    return result;
}
QVariantList WorldController::scales() const {
    return {QVariantMap{{"id","hq"},{"name",tr("HQ")}},QVariantMap{{"id","city"},{"name",tr("City")}},
        QVariantMap{{"id","state"},{"name",tr("State")}},QVariantMap{{"id","country"},{"name",tr("Country")}},QVariantMap{{"id","world"},{"name",tr("World")}}};
}
}
