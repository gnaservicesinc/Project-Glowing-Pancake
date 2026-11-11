#include "world_controller.hpp"
#include <QLocale>
#include <algorithm>
#include <map>

namespace pgp::bridge {
namespace {
QString q(const std::string& s) {return QString::fromStdString(s);}
QString date(core::OrdinalDay d) {return q(core::iso_date(d));}
QString money(core::Cents c) {return QLocale{QLocale::English,QLocale::UnitedStates}.toCurrencyString(static_cast<double>(c.value())/100);}
bool available(const CampaignSnapshotPtr& s) {return s && s->checkpoint.world.simulation.infrastructure && s->checkpoint.world.simulation.infrastructure->workplace && s->checkpoint.world.simulation.infrastructure->workplace->headquarters;}
const core::HeadquartersState& hq(const CampaignSnapshotPtr& s) {return *s->checkpoint.world.simulation.infrastructure->workplace->headquarters;}
const core::HeadquartersRules& rules(const CampaignSnapshotPtr& s) {return *s->checkpoint.world.simulation.infrastructure->catalog.workplace->headquarters;}
template<class Rows,class Id> const auto& find(const Rows& rows,const Id& id) {const auto it=std::ranges::find(rows,id,[](const auto& row){return row.id;});if(it==rows.end()) throw std::invalid_argument("The selected listing is no longer available.");return *it;}
// Presentation finishes follow the existing premises and named home rooms.
// They do not change equipment, costs, construction quality or save state.
QString floorFinish(std::string_view kind, std::string_view plan, std::string_view room = {}) {
    if(kind=="home") {
        if(room=="Library" || room=="Guest house") return QStringLiteral("oak");
        if(room=="Home office") return QStringLiteral("carpet");
        return QStringLiteral("concrete");
    }
    return plan=="floor" ? QStringLiteral("oak") : QStringLiteral("carpet");
}
QString floorLabel(const QString& finish) {
    return finish=="oak" ? QObject::tr("Oak flooring") : finish=="carpet" ? QObject::tr("Carpet tiles") : QObject::tr("Concrete floor");
}
QVariantMap roomGeometry(const core::HQRoom& room) {
    return {{"x",room.x},{"y",room.y},{"width",room.width},{"depth",room.depth},{"passage",room.passage}};
}
QVariantList planRoomsJson(const std::vector<core::HQRoom>& rows) {
    QVariantList list;
    for(const auto& room:rows) list.push_back(roomGeometry(room));
    return list;
}
QVariantMap floorAccess(const core::HQAccess& access) {
    QVariantList doors,walls;
    for(const auto& [a,b]:access.doors) doors.push_back(QVariantList{a,b});
    for(const auto& wall:access.walls) walls.push_back(QVariantMap{{"x",wall.x},{"y",wall.y},{"length",wall.length},{"vertical",wall.vertical}});
    return {{"entry",access.entry},{"entryX",access.entry_x},{"entryY",access.entry_y},{"doors",doors},{"walls",walls}};
}
std::vector<core::HQRoom> readRooms(const QVariantList& rows) {
    if(rows.size()>16) throw std::invalid_argument("Use at most sixteen rooms.");std::vector<core::HQRoom> result;
    for(const auto& row:rows) {const auto m=row.toMap();core::HQRoom r;int* fields[]{&r.x,&r.y,&r.width,&r.depth};const char* names[]{"x","y","width","depth"};
        for(int i=0;i<4;++i) {bool okay=false;const auto value=m.value(names[i]).toInt(&okay);if(!okay || m.value(names[i]).toDouble()!=value) throw std::invalid_argument("Use whole-metre room boundaries.");*fields[i]=value;}
        if(m.contains("passage") && m.value("passage").metaType()!=QMetaType::fromType<bool>()) throw std::invalid_argument("Choose a room or a hallway.");
        r.passage=m.value("passage").toBool();result.push_back(r);}
    return result;
}
std::uint32_t id(const QString& value) {bool okay=false;const auto n=value.toUInt(&okay);if(!okay || !n) throw std::invalid_argument("Choose a current property.");return n;}
QVariantList tiles(const std::vector<int>& route) {
    QVariantList values;
    values.reserve(static_cast<qsizetype>(route.size()));
    for(const int tile:route) values.push_back(tile);
    return values;
}
QVariantMap trafficRow(const core::HQTraffic& walk) {
    if(walk.access.entry<0) return {};
    QVariantList desks;
    int reachable=0;
    for(const auto& desk:walk.desks) {
        QStringList amenities;
        for(const auto& amenity:desk.amenities)
            amenities.push_back(QObject::tr("%1 · %2 m").arg(q(amenity.name)).arg(amenity.metres));
        const bool open=!desk.route.empty();
        if(open) ++reachable;
        const auto status=open?QObject::tr("%1 m from entrance").arg(desk.route.size()-1):QObject::tr("No route from entrance");
        const auto detail=desk.congestion?status+QObject::tr(" · %1 crowded tiles").arg(desk.congestion):status;
        desks.push_back(QVariantMap{{"id",desk.desk},{"route",tiles(desk.route)},{"assigned",!desk.person.empty()},
            {"detail",detail},{"mood",desk.mood},{"amenities",amenities.join(QStringLiteral("\n"))}});
    }
    return {{"desks",desks},{"crowded",tiles(walk.crowded_tiles)},
        {"summary",QObject::tr("%1/%2 desks reachable · %3/%4 m² walkable from the entrance")
            .arg(reachable).arg(walk.desks.size()).arg(walk.reachable_tiles).arg(walk.open_tiles)}};
}
}
QVariantMap WorldController::headquarters() const {
    if(!available(snapshot_)) return {};
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& h=hq(snapshot_);const auto& r=rules(snapshot_);const auto& p=core::current_headquarters(s);const auto& city=find(r.markets,p.market);
    QVariantList items,roomRows,notices;std::map<std::string,QString> names;
    names["founder"]=snapshot_->checkpoint.world.identity?q(snapshot_->checkpoint.world.identity->founder_name):tr("You");
    if(s.management && s.management->people) for(const auto& e:s.management->people->employees) names[e.terms.profile.id]=q(e.terms.profile.name);
    for(const auto& i:h.items) {
        const auto& e=find(r.equipment,i.offer);const bool delivered=i.delivered_on<=s.date,placed=i.premises==p.id;
        items.push_back(QVariantMap{{"id",i.id},{"offer",q(e.id)},{"name",q(e.name)},{"kind",q(e.kind)},{"supplier",q(e.supplier)},{"inherited",i.inherited},{"deskDevice",core::hq_desk_device(e.kind)},{"portable",core::hq_portable(e.kind)},
            {"x",i.x},{"y",i.y},{"width",i.rotated?e.depth:e.width},{"depth",i.rotated?e.width:e.depth},{"rotated",i.rotated},{"mount",i.mount},{"placed",placed},{"delivered",delivered},
            {"powered",i.powered},{"watts",e.watts},{"compute",static_cast<double>(e.compute.value())/1000},{"person",q(i.person)},{"personName",names.contains(i.person)?names.at(i.person):QString{}},
            {"detail",!i.person.empty() && core::hq_portable(e.kind)?tr("Issued to %1 · work at any desk or from home").arg(names[i.person]):!delivered?tr("Delivery %1").arg(date(i.delivered_on)):placed?tr("Placed in headquarters"):tr("Delivered · in storage")},
            {"ownership",i.inherited?tr("Inherited company equipment"):tr("Purchased from %1 for %2").arg(q(e.supplier),money(i.paid))}});
    }
    const auto home=core::headquarters_home(s);
    int index=0;
    for(const auto& room:p.rooms) {
        auto geometry=roomGeometry(room);
        if(room.passage) {
            geometry.insert("flooring",p.kind=="home"?"concrete":"oak");
            roomRows.push_back(std::move(geometry));
            continue;
        }
        int desks=0,computers=0,racks=0,benches=0,tests=0;
        for(const auto& i:h.items) if(i.premises==p.id && !i.mount && i.x>=room.x && i.y>=room.y && i.x<room.x+room.width && i.y<room.y+room.depth) {
            const auto& e=find(r.equipment,i.offer);if(e.kind=="desk") ++desks;if(e.kind=="rack") ++racks;if(e.kind=="bench") ++benches;if(e.kind=="evaluation") ++tests;
            for(const auto& child:h.items) if(child.mount==i.id && child.premises==p.id) ++computers;
        }
        const auto finish=floorFinish(p.kind,p.plan,p.kind=="home"?home.room_names.at(static_cast<std::size_t>(index)):std::string{});
        const auto roomName=p.kind=="home"?q(home.room_names.at(static_cast<std::size_t>(index))):tr("Room %1").arg(index+1);
        QStringList functions;if(desks) functions<<tr("Office");if(benches) functions<<tr("Research");if(tests) functions<<tr("Evaluation");if(racks) functions<<tr("Compute");
        geometry.insert("flooring",finish);
        geometry.insert("flooringLabel",floorLabel(finish));
        geometry.insert("label",roomName);
        geometry.insert("id",++index);
        geometry.insert("name",roomName+" · "+(functions.empty()?tr("Empty"):functions.join(" / ")));
        geometry.insert("detail",floorLabel(finish)+tr(" · %1 desks · %2 computers · %3 racks").arg(desks).arg(computers).arg(racks));
        roomRows.push_back(std::move(geometry));
    }
    for(const auto& n:h.notices) notices.push_back(QVariantMap{{"date",date(n.day)},{"text",q(n.text)}});
    return {{"flooring",floorFinish(p.kind,p.plan)},{"title",p.kind=="home"?q(home.plan.name):q(find(r.plans,p.plan).name)},{"width",p.width},{"depth",p.depth},{"rooms",roomRows},{"items",items},{"kind",q(p.kind)},{"address",q(p.address)},{"city",q(city.city)},{"market",q(p.market)},{"landlord",q(p.lessor)},{"trafficEnabled",r.traffic.has_value()},
        {"access",floorAccess(core::hq_access(p.width,p.depth,p.rooms))},
        {"rent",money(p.rent)},{"deposit",money(p.deposit)},{"notices",notices},{"summary",tr("%1 · %2\n%3 × %4 m · %5 desks, %6 assigned\n%7\nSecurity deposit held: %8")
            .arg(q(p.address),q(city.city)).arg(p.width).arg(p.depth).arg(core::furnished_hq_desks(s)).arg(core::assigned_hq_desks(s))
            .arg(p.kind=="home"?tr("Your parents' home workspaces · $0 rent"):p.kind=="owned"?tr("Company-owned building · no rent; property taxes accrue"):tr("Rent %1/month to %2").arg(money(p.rent),q(p.lessor)),money(p.deposit))}};
}
QVariantMap WorldController::familyHome() const {
    if(!available(snapshot_) || !snapshot_->checkpoint.world.founding) return {};
    const auto& world=snapshot_->checkpoint.world;
    const auto& state=world.simulation;
    const auto& market=find(rules(snapshot_).markets,hq(snapshot_).home_market);
    const auto home=core::headquarters_home(state);
    const auto& premises=core::current_headquarters(state);
    QVariantList rooms;
    std::size_t i=0;
    for(const auto& room:home.plan.rooms) {
        auto row=roomGeometry(room);
        if(!room.passage) {
            const auto finish=floorFinish("home",home.plan.id,home.room_names[i]);
            row.insert("flooring",finish);
            row.insert("flooringLabel",floorLabel(finish));
            row.insert("label",q(home.room_names[i]));
            row.insert("name",q(home.room_names[i]));
            row.insert("id",static_cast<int>(++i));
        }
        rooms.push_back(std::move(row));
    }
    return {{"flooring","concrete"},{"kind","home"},{"name",q(home.plan.name)},{"city",q(market.city)},{"rooms",rooms},
        {"access",floorAccess(core::hq_access(home.plan.width,home.plan.depth,home.plan.rooms))},
        {"width",home.plan.width},{"depth",home.plan.depth},{"kind","home"},{"atHome",premises.kind=="home"},
        {"items",premises.kind=="home"?headquarters().value("items").toList():QVariantList{}},
        {"founder",world.identity?q(world.identity->founder_name):QString{}},
        {"birthSex",q(world.founding->birth_sex)},{"surname",q(world.founding->last_name)}};
}
QVariantList WorldController::hqEquipmentOffers() const {
    QVariantList rows;if(!available(snapshot_)) return rows;
    const auto& state=snapshot_->checkpoint.world.simulation;
    const auto day=state.date;
    const auto* people=state.management && state.management->catalog.people?&*state.management->catalog.people:nullptr;
    for(const auto& e:rules(snapshot_).equipment) {
        QString use;
        if(e.kind=="thin_client") use=tr("Marketing / PR terminal. Requires a powered HQ rack and uses its shared capacity.");
        else if(e.kind=="research_laptop") use=tr("Remote GPU research. Issue to an employee and install a powered HQ rack.");
        else if(e.kind=="workstation") use=tr("Multi-monitor workstation for coding, call centers and management.");
        else if(e.id=="budget_desktop") use=tr("Entry desktop for everyday office work.");
        else if(core::hq_portable(e.kind)) use=tr("Issue to an employee for work at any desk or from home.");
        if(e.compute.value()>0) use+=(use.isEmpty()?QString{}:QStringLiteral("\n"))+tr("%1 CU/day · %2 W when on. Requires delivery, placement or issue, and power.").arg(static_cast<double>(e.compute.value())/1000).arg(e.watts);
        if(people && people->mood) {
            const auto bonus=people->mood->amenities.find(e.kind);
            if(bonus!=people->mood->amenities.end()) {
                const auto& traffic=rules(snapshot_).traffic;
                use=traffic?tr("+%1 mood within a %2 m walk of an assigned desk").arg(bonus->second).arg(traffic->amenity_walk)
                           :tr("+%1 mood for employees with assigned desks").arg(bonus->second);
                if(e.watts) use+=tr(" when powered");
            }
        }
        if(use.isEmpty()) use=tr("Office furnishing");
        rows.push_back(QVariantMap{{"id",q(e.id)},{"name",q(e.name)},{"price",money(e.price)},{"kind",q(e.kind)},{"revision",reviewToken()},{"supplier",q(e.supplier)},
        {"detail",tr("%1 · %2\nDelivery %3 · %4 × %5 m%6\n%7")
            .arg(money(e.price),q(e.supplier),date(core::advance_days(day,e.delivery))).arg(e.width).arg(e.depth)
            .arg(core::hq_desk_device(e.kind)?tr(" · mounts on a desk"):QString{})
            .arg(use)}});
    }
    return rows;
}
QVariantList WorldController::hqMarkets() const {QVariantList rows;if(!available(snapshot_)) return rows;for(const auto& m:rules(snapshot_).markets) rows.push_back(QVariantMap{{"id",q(m.id)},{"name",q(m.city)},{"agent",q(m.agent)},{"state",q(m.state)}});return rows;}
QVariantList WorldController::hqListings(QString market) const {
    QVariantList rows;if(!available(snapshot_)) return rows;
    for(const auto& p:rules(snapshot_).plans) {auto row=hqPropertyQuote(market,q(p.id),false);row["name"]=q(p.name);row["flooring"]=floorFinish("lease",p.id);row["flooringLabel"]=floorLabel(floorFinish("lease",p.id));row["width"]=p.width;row["depth"]=p.depth;row["rooms"]=planRoomsJson(p.rooms);row["access"]=floorAccess(core::hq_access(p.width,p.depth,p.rooms));rows.push_back(row);}return rows;
}
QVariantMap WorldController::hqPropertyQuote(QString market,QString plan,bool land) const {
    if(!available(snapshot_)) return {{"error",tr("No property market is available.")}};
    try {
        const auto& s=snapshot_->checkpoint.world.simulation;const auto& m=find(rules(snapshot_).markets,market.toStdString());const auto& p=find(rules(snapshot_).plans,plan.toStdString());
        const auto quote=land?core::preview_hq_land(s,{m.id,p.id}):core::preview_hq_lease(s,{m.id,p.id});
        return {{"flooring",floorFinish("lease",p.id)},{"flooringLabel",floorLabel(floorFinish("lease",p.id))},{"market",market},{"plan",plan},{"land",land},{"revision",reviewToken()},
            {"detail",land?tr("%1 · %2\n%3 × %4 m plot · %5 purchase\nLand property tax %6/month. Hire a contractor separately; no building or equipment is included.")
                .arg(q(m.city),q(m.agent)).arg(p.width).arg(p.depth).arg(money(quote.price),money(quote.monthly)):
            floorLabel(floorFinish("lease",p.id))+QStringLiteral("\n")+tr("%1 · %2\nLandlord: %3\n%4 × %5 m · %6/month\nRefundable deposit %7 · minimum %8 days\nEarly departure: %9 months' rent\nThis move: old deposit returned %10 · exit fee %11\nEquipment is moved into storage for you to place in the new rooms.")
                .arg(q(m.street+" / "+p.name),q(m.agent),q(m.landlord)).arg(p.width).arg(p.depth).arg(money(quote.monthly),money(quote.price)).arg(m.minimum_lease_days).arg(m.early_exit_months).arg(money(quote.refund),money(quote.exit_fee))}};
    } catch(const std::exception& e) {return {{"error",QString::fromUtf8(e.what())}};}
}
QVariantList WorldController::hqProperties() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto& h=hq(snapshot_);const auto& r=rules(snapshot_);
    for(const auto& land:h.land) {
        const auto& m=find(r.markets,land.market);const auto& p=find(r.plans,land.plan);
        QVariantMap row{{"flooring",floorFinish("owned",p.id)},{"id",QString::number(land.id)},{"name",q(m.city)+" · "+q(p.name)+tr(" plot")},{"width",p.width},{"depth",p.depth},{"revision",reviewToken()},{"canBuild",true}};
        row["detail"]=tr("Owned land · %1 × %2 m · paid %3").arg(p.width).arg(p.depth).arg(money(land.paid));
        for(const auto& b:h.construction) if(b.land==land.id) {
            row["canBuild"]=false;row["construction"]=QString::number(b.id);row["ready"]=b.completed_on.has_value();
            row["detail"]=row.value("detail").toString()+tr("\n%1 · %2 × %3 m\n%4\n%5")
                .arg(q(find(r.contractors,b.contractor).name)).arg(b.width).arg(b.depth)
                .arg(b.completed_on?tr("Completed %1 · final cost %2").arg(date(*b.completed_on),money(b.actual)):b.delay_announced?tr("Delayed · revised completion %1").arg(date(b.finish_on)):tr("Construction · promised %1").arg(date(b.promised_on)))
                .arg(tr("Prepaid ceiling %1; unused contingency returns on completion.").arg(money(b.prepaid)));
        }
        rows.push_back(row);
    }return rows;
}
QVariantMap WorldController::hqMoveQuote(QString construction) const {
    if(!available(snapshot_)) return {{"error",tr("No headquarters record is available.")}};
    try {
        const auto quote=core::preview_hq_move(snapshot_->checkpoint.world.simulation,{id(construction)});
        return {{"construction",construction},{"revision",reviewToken()},
            {"detail",tr("Move into the completed company-owned building.\nOld security deposit returned: %1\nOld lease exit fee: %2\nNo new rent. Land and building property taxes continue. Equipment moves into storage for placement in the new rooms.").arg(money(quote.refund),money(quote.exit_fee))}};
    } catch(const std::exception& e) {return {{"error",QString::fromUtf8(e.what())}};}
}
QVariantMap WorldController::hqHomeQuote() const {
    if(!available(snapshot_)) return {{"error",tr("No family home is available.")}};
    try {
        const auto& s=snapshot_->checkpoint.world.simulation;
        const auto home=core::headquarters_home(s);
        const auto quote=core::preview_hq_home(s);
        const auto homeRooms=familyHome().value("rooms").toList();
        return {{"revision",reviewToken()},{"width",home.plan.width},{"depth",home.plan.depth},{"rooms",homeRooms.empty()?planRoomsJson(home.plan.rooms):homeRooms},
            {"access",floorAccess(core::hq_access(home.plan.width,home.plan.depth,home.plan.rooms))},
            {"detail",tr("Return to %1 in %2.\n%3 free workspaces · $0 rent · no deposit\nOld deposit returned: %4\nOld lease exit fee: %5\nYour equipment is kept in storage for rearranging. Issued laptops stay with their holders. Owned land and building taxes continue.")
                .arg(q(home.plan.name),q(find(rules(snapshot_).markets,hq(snapshot_).home_market).city)).arg(home.room_names.size()).arg(money(quote.refund),money(quote.exit_fee))}};
    } catch(const std::exception& e) {return {{"error",QString::fromUtf8(e.what())}};}
}
QVariantList WorldController::hqBids(QString land,int width,int depth,QVariantList plan) const {
    QVariantList rows;if(!available(snapshot_)) return rows;
    try {
        const auto layout=readRooms(plan);const auto lot=id(land);
        for(const auto& c:rules(snapshot_).contractors) {
            const auto quote=core::preview_hq_build(snapshot_->checkpoint.world.simulation,{lot,c.id,width,depth,layout});
            rows.push_back(QVariantMap{{"id",q(c.id)},{"name",q(c.name)},{"revision",reviewToken()},
                {"detail",tr("Bid %1 · promised %2\nDelay risk %3% (up to %4 days) · overrun risk %5% (up to %6%)\nPrepay %7 including the maximum contingency. Unused contingency is returned. Building property tax up to %8/month; land tax continues. No rent.")
                    .arg(money(quote.price),date(quote.ready_on)).arg(static_cast<double>(c.delay_bp)/100).arg(c.maximum_delay_days).arg(static_cast<double>(c.overrun_bp)/100).arg(static_cast<double>(c.maximum_overrun_bp)/100).arg(money(quote.maximum),money(quote.monthly))}});
        }
    } catch(const std::exception& e) {rows.push_back(QVariantMap{{"error",QString::fromUtf8(e.what())}});}return rows;
}
QVariantList WorldController::hqEmployees() const {
    QVariantList rows{{QVariantMap{{"id",""},{"name",tr("Unassigned")}}}};if(!available(snapshot_)) return rows;
    rows.push_back(QVariantMap{{"id","founder"},{"name",tr("You")}});
    const auto& s=snapshot_->checkpoint.world.simulation;
    if(s.management && s.management->people) for(const auto& e:s.management->people->employees) if(!e.departed_on && e.terms.profile.role!="Founder / researcher") rows.push_back(QVariantMap{{"id",q(e.terms.profile.id)},{"name",q(e.terms.profile.name)}});
    return rows;
}
bool WorldController::hqReview(QString revision) {if(!available(snapshot_)) return false;if(companyArchive()) {emit noticeRaised(tr("This company is no longer accepting new commitments."));return false;}if(revision!=reviewToken()) {emit noticeRaised(tr("The company changed. Review the headquarters action again."));return false;}return true;}
void WorldController::buyHQEquipment(QString offer,QString revision) {if(hqReview(revision)) emit actionRequested(core::ManageHeadquarters{core::BuyHQEquipment{offer.toStdString()}});}
QVariantMap WorldController::hqTraffic() const {
    return available(snapshot_)?trafficRow(core::hq_traffic(snapshot_->checkpoint.world.simulation)):QVariantMap{};
}
QVariantMap WorldController::hqPlanAccess(int width,int depth,QVariantList layout) const {
    if(!available(snapshot_)) return {};
    try {
        const auto rooms=readRooms(layout);
        core::validate_hq_plan(width,depth,rooms,rules(snapshot_).maximum_edge);
        return floorAccess(core::hq_access(width,depth,rooms));
    } catch(const std::invalid_argument& error) {
        return {{"error",QString::fromUtf8(error.what())}};
    }
}
QVariantMap WorldController::hqPlacement(int item,int x,int y,bool rotated,int target) const {
    if(!available(snapshot_) || item<=0) return {{"valid",false},{"reason",tr("Choose an owned object.")}};
    const auto& s=snapshot_->checkpoint.world.simulation;const auto& items=hq(snapshot_).items;
    const auto selected=std::ranges::find(items,static_cast<std::uint32_t>(item),&core::HQEquipment::id);
    if(selected==items.end()) return {{"valid",false},{"reason",tr("This object is no longer available.")}};
    const auto& model=find(rules(snapshot_).equipment,selected->offer);
    const auto hit=std::ranges::find(items,static_cast<std::uint32_t>(std::max(0,target)),&core::HQEquipment::id);
    std::uint32_t mount=0;int width=rotated?model.depth:model.width,depth=rotated?model.width:model.depth;
    if(core::hq_desk_device(model.kind) && hit!=items.end()) {
        mount=find(rules(snapshot_).equipment,hit->offer).kind=="desk"?hit->id:hit->mount;
        if(mount) {const auto& desk=find(items,mount);const auto& shape=find(rules(snapshot_).equipment,desk.offer);x=desk.x;y=desk.y;width=desk.rotated?shape.depth:shape.width;depth=desk.rotated?shape.width:shape.depth;}
    }
    const core::PlaceHQEquipment placement{selected->id,mount,x,y,rotated};
    const auto why=core::hq_placement_blocker(s,placement);
    return {{"valid",why.empty()},{"reason",why.empty()?tr("Click to place · R rotates · right-click cancels"):q(why)},
        {"x",x},{"y",y},{"width",width},{"depth",depth},{"mount",mount},{"rotated",rotated},{"revision",reviewToken()},
        {"traffic",why.empty() && !mount?trafficRow(core::hq_traffic(s,&placement)):QVariantMap{}}};
}
void WorldController::assignHQLaptop(int item,QString person,QString revision) {if(hqReview(revision) && item>0) emit actionRequested(core::ManageHeadquarters{core::AssignHQLaptop{static_cast<std::uint32_t>(item),person.toStdString()}});}
void WorldController::placeHQEquipment(int item,int x,int y,bool rotated,int mount,QString revision) {if(hqReview(revision) && item>0 && mount>=0) emit actionRequested(core::ManageHeadquarters{core::PlaceHQEquipment{static_cast<std::uint32_t>(item),static_cast<std::uint32_t>(mount),x,y,rotated}});}
void WorldController::storeHQEquipment(int item,QString revision) {if(hqReview(revision) && item>0) emit actionRequested(core::ManageHeadquarters{core::StoreHQEquipment{static_cast<std::uint32_t>(item)}});}
void WorldController::assignHQDesk(int item,QString person,QString revision) {if(hqReview(revision) && item>0) emit actionRequested(core::ManageHeadquarters{core::AssignHQDesk{static_cast<std::uint32_t>(item),person.toStdString()}});}
void WorldController::powerHQEquipment(int item,bool powered,QString revision) {if(hqReview(revision) && item>0) emit actionRequested(core::ManageHeadquarters{core::PowerHQEquipment{static_cast<std::uint32_t>(item),powered}});}
void WorldController::leaseHQ(QString market,QString plan,QString revision) {if(hqReview(revision)) emit actionRequested(core::ManageHeadquarters{core::LeaseHQ{market.toStdString(),plan.toStdString()}});}
void WorldController::buyHQLand(QString market,QString plan,QString revision) {if(hqReview(revision)) emit actionRequested(core::ManageHeadquarters{core::BuyHQLand{market.toStdString(),plan.toStdString()}});}
void WorldController::buildHQ(QString land,QString contractor,int width,int depth,QVariantList layout,QString revision) {if(!hqReview(revision)) return;try {emit actionRequested(core::ManageHeadquarters{core::BuildHQ{id(land),contractor.toStdString(),width,depth,readRooms(layout)}});} catch(const std::exception& e) {emit noticeRaised(QString::fromUtf8(e.what()));}}
void WorldController::moveToOwnedHQ(QString construction,QString revision) {if(!hqReview(revision)) return;try {emit actionRequested(core::ManageHeadquarters{core::MoveToOwnedHQ{id(construction)}});} catch(const std::exception& e) {emit noticeRaised(QString::fromUtf8(e.what()));}}
void WorldController::returnHomeHQ(QString revision) {if(hqReview(revision)) emit actionRequested(core::ManageHeadquarters{core::ReturnHomeHQ{}});}
}
