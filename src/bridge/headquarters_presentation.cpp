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
QVariantList planRoomsJson(const std::vector<core::HQRoom>& rows) {QVariantList list;for(const auto& r:rows) list.push_back(QVariantMap{{"x",r.x},{"y",r.y},{"width",r.width},{"depth",r.depth}});return list;}
std::vector<core::HQRoom> readRooms(const QVariantList& rows) {
    if(rows.size()>16) throw std::invalid_argument("Use at most sixteen rooms.");std::vector<core::HQRoom> result;
    for(const auto& row:rows) {const auto m=row.toMap();core::HQRoom r;int* fields[]{&r.x,&r.y,&r.width,&r.depth};const char* names[]{"x","y","width","depth"};
        for(int i=0;i<4;++i) {bool okay=false;const auto value=m.value(names[i]).toInt(&okay);if(!okay || m.value(names[i]).toDouble()!=value) throw std::invalid_argument("Use whole-metre room boundaries.");*fields[i]=value;}result.push_back(r);}
    return result;
}
std::uint32_t id(const QString& value) {bool okay=false;const auto n=value.toUInt(&okay);if(!okay || !n) throw std::invalid_argument("Choose a current property.");return n;}
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
    int index=0;
    for(const auto& room:p.rooms) {
        int desks=0,computers=0,racks=0,benches=0,tests=0;
        for(const auto& i:h.items) if(i.premises==p.id && !i.mount && i.x>=room.x && i.y>=room.y && i.x<room.x+room.width && i.y<room.y+room.depth) {
            const auto& e=find(r.equipment,i.offer);if(e.kind=="desk") ++desks;if(e.kind=="rack") ++racks;if(e.kind=="bench") ++benches;if(e.kind=="evaluation") ++tests;
            for(const auto& child:h.items) if(child.mount==i.id && child.premises==p.id) ++computers;
        }
        QStringList functions;if(desks) functions<<tr("Office");if(benches) functions<<tr("Research");if(tests) functions<<tr("Evaluation");if(racks) functions<<tr("Compute");
        roomRows.push_back(QVariantMap{{"id",++index},{"x",room.x},{"y",room.y},{"width",room.width},{"depth",room.depth},{"name",(p.kind=="home" && p.rooms.size()==3?QStringList{tr("Garage"),tr("Basement"),tr("Shed")}[index-1]:tr("Room %1").arg(index))+" · "+(functions.empty()?tr("Empty"):functions.join(" / "))},
            {"detail",tr("%1 desks · %2 computers · %3 racks").arg(desks).arg(computers).arg(racks)}});
    }
    for(const auto& n:h.notices) notices.push_back(QVariantMap{{"date",date(n.day)},{"text",q(n.text)}});
    return {{"width",p.width},{"depth",p.depth},{"rooms",roomRows},{"items",items},{"kind",q(p.kind)},{"address",q(p.address)},{"city",q(city.city)},{"market",q(p.market)},{"landlord",q(p.lessor)},
        {"rent",money(p.rent)},{"deposit",money(p.deposit)},{"notices",notices},{"summary",tr("%1 · %2\n%3 × %4 m · %5 desks, %6 assigned\n%7\nSecurity deposit held: %8")
            .arg(q(p.address),q(city.city)).arg(p.width).arg(p.depth).arg(core::furnished_hq_desks(s)).arg(core::assigned_hq_desks(s))
            .arg(p.kind=="home"?tr("Your parents' home workspaces · $0 rent"):p.kind=="owned"?tr("Company-owned building · no rent; property taxes accrue"):tr("Rent %1/month to %2").arg(money(p.rent),q(p.lessor)),money(p.deposit))}};
}
QVariantList WorldController::hqEquipmentOffers() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto day=snapshot_->checkpoint.world.simulation.date;
    for(const auto& e:rules(snapshot_).equipment) rows.push_back(QVariantMap{{"id",q(e.id)},{"name",q(e.name)},{"price",money(e.price)},{"kind",q(e.kind)},{"revision",reviewToken()},{"supplier",q(e.supplier)},
        {"detail",tr("%1 · %2\nDelivery %3 · %4 × %5 m%6\n%7")
            .arg(money(e.price),q(e.supplier),date(core::advance_days(day,e.delivery))).arg(e.width).arg(e.depth)
            .arg(core::hq_desk_device(e.kind)?tr(" · mounts on a desk"):QString{})
            .arg(e.compute.value()>0?tr("%1 CU/day · %2 W when on; output requires delivery, placement and power").arg(static_cast<double>(e.compute.value())/1000).arg(e.watts):e.kind=="thin_client"?tr("Marketing / PR terminal · requires a powered HQ rack; uses its shared capacity"):e.kind=="research_laptop"?tr("Remote GPU research · issue to an employee; requires a powered HQ rack"):e.kind=="workstation"?tr("Multi-monitor coding / management workstation"):tr("Office furnishing · no research compute or mood bonus"))}});
    return rows;
}
QVariantList WorldController::hqMarkets() const {QVariantList rows;if(!available(snapshot_)) return rows;for(const auto& m:rules(snapshot_).markets) rows.push_back(QVariantMap{{"id",q(m.id)},{"name",q(m.city)},{"agent",q(m.agent)},{"state",q(m.state)}});return rows;}
QVariantList WorldController::hqListings(QString market) const {
    QVariantList rows;if(!available(snapshot_)) return rows;
    for(const auto& p:rules(snapshot_).plans) {auto row=hqPropertyQuote(market,q(p.id),false);row["name"]=q(p.name);row["width"]=p.width;row["depth"]=p.depth;row["rooms"]=planRoomsJson(p.rooms);rows.push_back(row);}return rows;
}
QVariantMap WorldController::hqPropertyQuote(QString market,QString plan,bool land) const {
    if(!available(snapshot_)) return {{"error",tr("No property market is available.")}};
    try {
        const auto& s=snapshot_->checkpoint.world.simulation;const auto& m=find(rules(snapshot_).markets,market.toStdString());const auto& p=find(rules(snapshot_).plans,plan.toStdString());
        const auto quote=land?core::preview_hq_land(s,{m.id,p.id}):core::preview_hq_lease(s,{m.id,p.id});
        return {{"market",market},{"plan",plan},{"land",land},{"revision",reviewToken()},
            {"detail",land?tr("%1 · %2\n%3 × %4 m plot · %5 purchase\nLand property tax %6/month. Hire a contractor separately; no building or equipment is included.")
                .arg(q(m.city),q(m.agent)).arg(p.width).arg(p.depth).arg(money(quote.price),money(quote.monthly)):
            tr("%1 · %2\nLandlord: %3\n%4 × %5 m · %6/month\nRefundable deposit %7 · minimum %8 days\nEarly departure: %9 months' rent\nThis move: old deposit returned %10 · exit fee %11\nEquipment is moved into storage for you to place in the new rooms.")
                .arg(q(m.street+" / "+p.name),q(m.agent),q(m.landlord)).arg(p.width).arg(p.depth).arg(money(quote.monthly),money(quote.price)).arg(m.minimum_lease_days).arg(m.early_exit_months).arg(money(quote.refund),money(quote.exit_fee))}};
    } catch(const std::exception& e) {return {{"error",QString::fromUtf8(e.what())}};}
}
QVariantList WorldController::hqProperties() const {
    QVariantList rows;if(!available(snapshot_)) return rows;const auto& h=hq(snapshot_);const auto& r=rules(snapshot_);
    for(const auto& land:h.land) {
        const auto& m=find(r.markets,land.market);const auto& p=find(r.plans,land.plan);
        QVariantMap row{{"id",QString::number(land.id)},{"name",q(m.city)+" · "+q(p.name)+tr(" plot")},{"width",p.width},{"depth",p.depth},{"revision",reviewToken()},{"canBuild",true}};
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
    const auto why=core::hq_placement_blocker(s,{selected->id,mount,x,y,rotated});
    return {{"valid",why.empty()},{"reason",why.empty()?tr("Click to place · R rotates · right-click cancels"):q(why)},
        {"x",x},{"y",y},{"width",width},{"depth",depth},{"mount",mount},{"rotated",rotated},{"revision",reviewToken()}};
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
}
