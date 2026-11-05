#include "infrastructure_controller.hpp"
#include "flow_presentation.hpp"
#include "resilience_presentation.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>
namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view v) {return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size()));}
QString date(OrdinalDay d) {return text(iso_date(d));}
QString number(std::uint64_t v) {return QString::number(static_cast<qulonglong>(v));}
QString decimal(std::uint64_t v,std::uint64_t divisor,int digits) {return number(v/divisor)+QStringLiteral(".%1").arg(v%divisor,digits,10,QLatin1Char('0'));}
QString money(Cents v) {
    const auto raw=v.value();const auto amount=raw<0?static_cast<std::uint64_t>(-(raw+1))+1:static_cast<std::uint64_t>(raw);
    auto result=decimal(amount,100,2);for (auto at=result.size()-6;at>0;at-=3) result.insert(at,QLatin1Char(','));
    return (raw<0?QStringLiteral("−$"):QStringLiteral("$"))+result;
}
QString kw(std::uint64_t watts) {return watts%1000==0?number(watts/1000)+QStringLiteral(" kW"):decimal(watts,1000,3)+QStringLiteral(" kW");}
QString kwh(std::uint64_t wh) {return decimal(wh,1000,3)+QStringLiteral(" kWh");}
QString liters(std::uint64_t ml) {return decimal(ml,1000,3)+QStringLiteral(" L");}
QVariantMap option(std::string_view id,QString label) {return {{QStringLiteral("id"),text(id)},{QStringLiteral("label"),std::move(label)}};}
QString mode_name(AppointmentMode mode) {return mode==AppointmentMode::onsite?QStringLiteral("On site"):mode==AppointmentMode::remote?QStringLiteral("Remote"):QStringLiteral("Delegated");}
QString cooling_name(CoolingMode cooling) {return cooling==CoolingMode::air?QStringLiteral("Air cooling"):QStringLiteral("Evaporative cooling");}
AppointmentMode appointment_mode(const QString& mode) {
    if (mode==QStringLiteral("onsite")) return AppointmentMode::onsite;if (mode==QStringLiteral("remote")) return AppointmentMode::remote;if (mode==QStringLiteral("delegated")) return AppointmentMode::delegated;
    throw std::invalid_argument("Choose a site inspection method.");
}
std::uint64_t whole(const QString& value,std::uint64_t minimum,std::uint64_t maximum) {
    static const QRegularExpression digits{QStringLiteral("^[0-9]{1,7}$")};bool ok=false;const auto n=value.toULongLong(&ok);
    if (!ok || !digits.match(value).hasMatch() || n<minimum || n>maximum) throw std::invalid_argument("Enter a whole number within the stated range.");return n;
}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});if (at==rows.end()) throw std::invalid_argument("Select an available record first.");return *at;
}
template<class Rows> std::string next_id(const Rows& rows,std::string_view prefix) {
    auto n=rows.size()+1;for (;;) {auto id=std::string(prefix)+std::to_string(n++);if (std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;})) return id;}
}
QString rack_terms(const HardwareFamily& f) {
    auto result=QStringLiteral("%1 purchase · %2/month maintenance · %3 days to delivery\n%4 CU/day · %5 MiB · %6 interconnect\n%7 active / %8 idle IT · %9 operators · network class %10\n%11 days of depreciation; resale %12% of remaining book value.")
        .arg(money(f.purchase_cost),money(f.monthly_maintenance),number(static_cast<std::uint64_t>(f.delivery_lead.value())),cu(f.capacity.value()),number(f.memory_mib),text(f.interconnect),kw(f.active_it_watts),kw(f.idle_it_watts),number(f.operators),number(f.connectivity_class),number(static_cast<std::uint64_t>(f.useful_life.value())),decimal(static_cast<std::uint64_t>(f.resale_fraction.value()),100,2));
    if (!f.requires_research.empty()) {QStringList required;for (const auto& id:f.requires_research) required.push_back(readable(id));result+=QStringLiteral("\nIntegration research: %1.").arg(required.join(QStringLiteral(", ")));}
    return result;
}
QString site_location(const InfrastructureState& i,const OwnedSite& s) {return text(find(i.catalog.states,s.state_id).name);}
MilliCUPerDay effective(const SimulationState& s,const OwnedSite& owned) {
    MilliCUPerDay value{0};for (const auto& output:preview_site_power(s,owned.id,s.date).hardware) value=checked_unit_add(value,output.effective);return value;
}
QString notice_heading(std::string_view kind) {
    if (kind=="construction_ordered") return QStringLiteral("Site construction ordered");
    if (kind=="construction_complete") return QStringLiteral("Site construction complete");
    if (kind=="grid_connected") return QStringLiteral("Site connected to grid");
    if (kind=="hardware_ordered") return QStringLiteral("Hardware order paid");
    if (kind=="hardware_delivered") return QStringLiteral("Hardware delivered");
    if (kind=="hardware_commissioned") return QStringLiteral("Hardware ready for work");
    if (kind=="hardware_retired") return QStringLiteral("Hardware retired");
    if (kind=="operators_hired") return QStringLiteral("Operators hired");
    if (kind=="operators_started") return QStringLiteral("Site operators started");
    if (kind=="inspection_booked") return QStringLiteral("Inspection booked");
    if (kind=="inspection_cancelled") return QStringLiteral("Inspection canceled");
    if (kind=="inspection_complete") return QStringLiteral("Inspection complete");
    if (kind=="site_closed") return QStringLiteral("Site closed");
    if (kind=="initial_lease_notice") return QStringLiteral("Opening lease notice given");
    if (kind=="initial_lease_ended") return QStringLiteral("Opening compute lease ended");
    return QStringLiteral("Infrastructure update");
}
QString terms(const SimulationState& before,const SimulationState& after,const CommandAction& action) {
    const auto resilience=resilience_review(after,action);
    if (!resilience.isEmpty()) return resilience;
    const auto& i=*after.infrastructure;const auto cash=checked_unit_subtract(before.cash,after.cash);
    if (const auto* a=std::get_if<BuildOwnedSite>(&action)) {
        const auto& s=find(i.sites,a->id);const auto price=preview_site_build(before,*a);const auto& p=find(i.catalog.states,s.state_id);
        return QStringLiteral("Build %1 in %2\n%3 due now after a %4 construction incentive.\n%5 grid reserved · %6. At most %7 facility power and %8 for IT after cooling.\nBuilding %9 · grid connection %10. Property and site overhead %11/month after construction. Hardware, operators and an inspection are separate commitments.")
            .arg(text(s.name),site_location(i,s),money(price.price),money(price.incentive),kw(s.grid_watts),cooling_name(s.cooling),kw(price.facility_watts),kw(price.it_watts),date(s.built_on),date(s.grid_on),money(checked_unit_add(p.monthly_property,p.monthly_overhead)));
    }
    if (const auto* a=std::get_if<OrderOwnedHardware>(&action)) {
        const auto& h=find(i.hardware,a->id);const auto& f=find(i.catalog.hardware,h.family_id);
        return QStringLiteral("Order %1 for %2\n%3\nDelivery %4. Usable compute requires a connected and inspected site, available power and paid operators.")
            .arg(text(f.name),text(find(i.sites,h.site_id).name),rack_terms(f),date(h.delivered_on));
    }
    if (const auto* a=std::get_if<HireSiteOperators>(&action)) {
        const auto& h=find(i.hires,a->id);const auto& s=find(i.sites,h.site_id);const auto& p=find(i.catalog.states,s.state_id);
        return QStringLiteral("Hire %1 dedicated operators for %2\n%3 recruitment now · %4/month per operator from %5. Site operators run hardware; Lab staffing is separate.")
            .arg(number(h.people),text(s.name),money(cash),money(p.operator_monthly),date(h.starts_on));
    }
    if (const auto* a=std::get_if<SetSitePowerLimit>(&action)) {
        const auto& s=find(i.sites,a->site_id);
        return QStringLiteral("Set %1 to %2 facility power\nCurrent usable compute changes from %3 to %4 CU/day. Existing commitments continue to compete for actual capacity; insufficient power can slow work or reduce deliveries.")
            .arg(text(s.name),kw(a->watts),cu(effective(before,find(before.infrastructure->sites,a->site_id)).value()),cu(effective(after,s).value()));
    }
    if (const auto* a=std::get_if<ScheduleSiteAppointment>(&action)) {
        const auto& v=find(i.appointments,a->id);
        return QStringLiteral("%1 inspection at %2\n%3 paid now · inspection %4. %5\nA successful inspection clears the site for staffed, delivered hardware. Confirmed fees are nonrefundable.")
            .arg(mode_name(v.mode),text(find(i.sites,v.site_id).name),money(v.paid),date(v.visit_on),v.mode==AppointmentMode::delegated?QStringLiteral("The executive calendar stays free."):QStringLiteral("Executive calendar occupied %1 to %2 (exclusive). Management remains available.").arg(date(v.starts_on),date(v.ends_before)));
    }
    if (const auto* a=std::get_if<RetireOwnedHardware>(&action)) {
        const auto& h=find(i.hardware,a->id);const auto book=checked_unit_subtract(h.paid,h.depreciated);
        return QStringLiteral("Retire %1 at %2\n%3 resale on %4 book value; %5 disposal loss. The rack stops supplying compute and its maintenance ends today. Work assigned to it stops. Past costs and work remain recorded.")
            .arg(text(find(i.catalog.hardware,h.family_id).name),text(find(i.sites,h.site_id).name),money(h.resale),money(book),money(checked_unit_subtract(book,h.resale)));
    }
    if (const auto* a=std::get_if<CloseOwnedSite>(&action)) {
        const auto& s=find(i.sites,a->id);const auto book=checked_unit_subtract(s.paid,s.depreciated);
        return QStringLiteral("Close %1\n%2 resale on %3 book value; %4 disposal loss. Releases the grid reservation and ends future site operating costs. Accrued bills remain due.")
            .arg(text(s.name),money(s.resale),money(book),money(checked_unit_subtract(book,s.resale)));
    }
    if (const auto* a=std::get_if<CancelSiteAppointment>(&action)) {
        const auto& v=find(i.appointments,a->id);return QStringLiteral("Cancel the %1 inspection on %2\nThe calendar is released. The confirmed %3 fee stays spent.").arg(mode_name(v.mode).toLower(),date(v.visit_on),money(v.paid));
    }
    return QStringLiteral("Give notice on the opening compute lease\nCapacity and future charges end %1. Accrued bills remain due. Move any research, training or customer service that still depends on the opening lease before this date.").arg(date(*i.initial_lease_ends_before));
}
}
InfrastructureController::InfrastructureController(QObject* parent):QObject(parent),states_(new StableListModel(this)),sites_(new StableListModel(this)),hardware_(new StableListModel(this)),calendar_(new StableListModel(this)),notices_(new StableListModel(this)),wholesale_contracts_(new StableListModel(this)),
    protections_(new StableListModel(this)),policies_(new StableListModel(this)),incidents_(new StableListModel(this)),replicas_(new StableListModel(this)) {
    states_->setSortByTitle(true);
    connect(states_,&StableListModel::selectedIdChanged,this,&InfrastructureController::changed);
}
void InfrastructureController::loadMap() {
    map_.clear();map_terms_.clear();rack_options_.clear();if (!available()) return;
    const auto& catalog=state().infrastructure->catalog;
    for (const auto& rack:catalog.hardware) rack_options_.push_back(QVariantMap{{QStringLiteral("id"),text(rack.id)},{QStringLiteral("label"),text(rack.name)},{QStringLiteral("detail"),rack_terms(rack)}});
    QFile file{QStringLiteral(":/pgp/assets/maps/us-states-2024.json")};
    if (file.open(QIODevice::ReadOnly) && file.size()<=200000) {
        const auto bytes=file.readAll();
        if (QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().toStdString()==catalog.map_sha256) {
            const auto parsed=QJsonDocument::fromJson(bytes).object().toVariantMap();
            if (parsed.value(QStringLiteral("asset_id")).toString()==text(catalog.map_asset_id) && parsed.value(QStringLiteral("features")).toList().size()==51) map_=parsed;
        }
    }
    QFile terms_file{QStringLiteral(":/pgp/assets/maps/CENSUS_TERMS.txt")};
    if (terms_file.open(QIODevice::ReadOnly) && terms_file.size()<=8192) map_terms_=QString::fromUtf8(terms_file.readAll());
}
QString InfrastructureController::mapCredit() const {return map_.value(QStringLiteral("credit")).toString();}
QUrl InfrastructureController::mapSource() const {return QUrl{map_.value(QStringLiteral("source_url")).toString()};}
QString InfrastructureController::assumptions() const {return available()?text(state().infrastructure->catalog.assumptions):QString{};}
QString InfrastructureController::executiveLocation() const {
    if (!available()) return {};
    const auto& s=state();const auto& i=*s.infrastructure;const auto location=executive_location(s);const auto base=text(find(i.catalog.states,location.base).name);
    if (!location.appointment) return QStringLiteral("Executive at base: %1").arg(base);
    const auto& appointment=find(i.appointments,*location.appointment);const auto destination=site_location(i,find(i.sites,appointment.site_id));
    if (location.traveling) return QStringLiteral("Executive %1 %2 · return to %3 %4. Management remains available.").arg(s.date<appointment.visit_on?QStringLiteral("traveling to"):QStringLiteral("returning from"),destination,base,date(appointment.ends_before));
    return appointment.mode==core::AppointmentMode::onsite?QStringLiteral("Executive on site in %1 · base %2").arg(destination,base):QStringLiteral("Remote inspection from %1 · site in %2").arg(base,destination);
}
bool InfrastructureController::canNoticeLease() const {
    return available() && !state().infrastructure->initial_lease_noticed_on && std::any_of(state().accounts->recurring.begin(),state().accounts->recurring.end(),[](const auto& row){return row.id=="compute_lease" && !row.ends_before;});
}
QString InfrastructureController::leaseStatus() const {
    if (!available()) return {};
    const auto& i=*state().infrastructure;
    if (i.initial_lease_ends_before) return state().date>=*i.initial_lease_ends_before?QStringLiteral("Opening compute lease ended %1. Accrued bills remain on the ledger.").arg(date(*i.initial_lease_ends_before)):QStringLiteral("Opening compute lease ends %1. Move dependent service before then.").arg(date(*i.initial_lease_ends_before));
    for (const auto& row:state().accounts->recurring) if (row.id=="compute_lease" && !row.ends_before) return QStringLiteral("Opening compute lease: %1/month · %2 days' notice. Owned power is metered separately.").arg(money(row.monthly_amount),number(static_cast<std::uint64_t>(i.catalog.initial_lease_notice.value())));
    return QStringLiteral("No continuing opening compute lease.");
}
void InfrastructureController::update(CampaignSnapshotPtr snapshot,const runtime::CompiledProgram& program,bool replace) {
    const auto* old=available()?&*state().infrastructure:nullptr;
    const auto* incoming=snapshot->checkpoint.world.simulation.infrastructure?&*snapshot->checkpoint.world.simulation.infrastructure:nullptr;
    const bool refresh_states=replace || !old || !incoming || old->sites.size()!=incoming->sites.size() ||
        !std::equal(old->sites.begin(),old->sites.end(),incoming->sites.begin(),[](const auto& a,const auto& b){return a.id==b.id && a.closed_on==b.closed_on;});
    const auto old_revision=snapshot_?state().revision:0;snapshot_=std::move(snapshot);
    program_=&program;
    if (replace) {loadMap();dismissReview();emit catalogChanged();}
    else if (review_ && old_revision!=state().revision) {review_.reset();review_text_=QStringLiteral("The campaign changed. Review the updated terms before confirming.");emit reviewChanged();}
    std::vector<CampaignRow> states,sites,hardware,appointments,notices,contracts;
    wholesale_plans_.clear();wholesale_sources_.clear();wholesale_buyers_.clear();
    const auto& world=snapshot_->checkpoint.world;
    const auto plans=runtime::wholesale_plans(world,program);
    if (!plans.empty()) {
        for (const auto& plan:plans) wholesale_plans_.push_back(option(plan.id,text(plan.name)));
        for (const auto& rival:world.competition->companies) {
            if (!rival.planning->entered_on || rival.response || !rival.parent_company.empty() ||
                rival.simulation.phase==DayPhase::closed || rival.simulation.phase==DayPhase::finished) continue;
            wholesale_buyers_.push_back(option(rival.definition_id,text(runtime::rival_definition(program,rival.definition_id).name)));
        }
        for (const auto& contract:world.competition->wholesale) {
            const auto& plan=find(plans,contract.plan_id);
            Cents paid{0},credits{0};
            for (const auto& row:contract.days) {
                credits=checked_unit_add(credits,row.service_credit);
                paid=checked_unit_add(paid,checked_unit_add(checked_unit_subtract(plan.daily_fee,row.service_credit),row.electricity));
            }
            const auto active=!contract.terminated_on && state().date<contract.ends_before;
            auto detail=QStringLiteral("%1 CU/day from %2 · %3/day before credits and electricity\n%4 through %5 (exclusive)\nPaid %6 · credits %7")
                .arg(cu(plan.capacity.value()),text(contract.source_id),money(plan.daily_fee),date(contract.starts_on),date(contract.ends_before),money(paid),money(credits));
            if (!contract.days.empty()) {
                const auto& last=contract.days.back();
                detail+=QStringLiteral("\nLast settled day · %1 CU available · %2 CU used · %3 credit · %4 metered electricity.")
                    .arg(cu(last.available.value()),cu(last.used.value()),money(last.service_credit),money(last.electricity));
            }
            if (contract.terminated_on) detail+=QStringLiteral("\nOperations ceased on %1. The buyer's unused funds were released.").arg(date(*contract.terminated_on));
            contracts.push_back({text(contract.id),text(runtime::rival_definition(program,contract.buyer_id).name),text(plan.name),std::move(detail),
                active?QStringLiteral("Active"):contract.terminated_on?QStringLiteral("Ended early"):QStringLiteral("Term complete")});
        }
    }
    if (available()) {
        const auto& s=state();const auto& i=*s.infrastructure;
        if (refresh_states) for (const auto& p:i.catalog.states) {
            std::uint64_t committed=0;for (const auto& row:i.sites) if (row.state_id==p.id && !row.closed_on) committed+=row.grid_watts;
            const auto tariff=decimal(p.electricity_millicents_per_kwh,1000,3)+QStringLiteral(" ¢/kWh");
            states.push_back({text(p.id),text(p.name),text(p.id)+QStringLiteral(" · ")+tariff,
                QStringLiteral("%1 free of %2 state grid capacity · %3-day grid slot · %4-day construction\nConstruction %5 base + %6/kW. Incentive %7% up to %8. Property and overhead %9/month.\nElectricity %10 · %11 g CO₂e/kWh. PUE %12 air / %13 evaporative.\nEvaporative water: %14 L/kWh, up to %15 L/day. Water %16/1,000 L.\nOperators: %17/month each, %18 days to start. Network class %19.")
                    .arg(kw(p.grid_watts-committed),kw(p.grid_watts),number(static_cast<std::uint64_t>(p.grid_lead.value())),number(static_cast<std::uint64_t>(p.build_lead.value())),money(p.construction_base),money(p.construction_per_kw),decimal(static_cast<std::uint64_t>(p.construction_incentive.value()),100,2),money(p.incentive_cap),money(checked_unit_add(p.monthly_property,p.monthly_overhead)),tariff,number(p.grid_grams_per_kwh),decimal(p.air_pue_milli,1000,3),decimal(p.water_pue_milli,1000,3),decimal(p.water_ml_per_kwh,1000,3),number(p.water_limit_ml_per_day/1000),money(Cents{p.water_cents_per_1000_liters}),money(p.operator_monthly),number(static_cast<std::uint64_t>(p.operator_hire_lead.value())),number(p.connectivity_class)),
                text(p.id),false,{{QStringLiteral("freeKw"),number((p.grid_watts-committed)/1000)}}});
        }
        for (const auto& owned:i.sites) {
            auto status=owned.closed_on?QStringLiteral("Closed"):s.date<owned.built_on?QStringLiteral("Building"):s.date<owned.grid_on?QStringLiteral("Waiting for grid"):owned.commissioned_on?QStringLiteral("Inspected"):QStringLiteral("Inspection needed");
            std::uint32_t operators=0;QStringList pending;
            for (const auto& hire:i.hires) if (hire.site_id==owned.id && !owned.closed_on) {
                if (hire.starts_on<=s.date) operators+=hire.people;else pending.push_back(QStringLiteral("%1 start %2").arg(number(hire.people),date(hire.starts_on)));
            }
            const auto power=preview_site_power(s,owned.id,s.date);
            QString detail=QStringLiteral("%1 · building %2 · grid %3 · %4 reserved\n%5 current power budget · %6 usable CU/day · %7 operators\nYesterday: %8 · %9 water · %10 kg CO₂e · %11 utilities.")
                .arg(cooling_name(owned.cooling),date(owned.built_on),date(owned.grid_on),kw(owned.grid_watts),kw(owned.closed_on?0:owned.power.back().watts),cu(effective(s,owned).value()),number(operators),kwh(owned.last_energy_wh),liters(owned.last_water_ml),decimal(owned.last_emissions_grams,1000,3),money(owned.last_utility_cost));
            if (!pending.isEmpty()) detail+=QStringLiteral("\nOperators on order: %1.").arg(pending.join(QStringLiteral("; ")));
            if (!owned.meters.empty()) {const auto& m=owned.meters.back();detail+=QStringLiteral("\nMeter month %1: %2 · %3 water · %4 electricity · %5 water charges · %6 property, staffing and maintenance.")
                .arg(date(m.month).left(7),kwh(m.energy_wh),liters(m.water_ml),money(m.electricity),money(m.water),money(m.operating));}
            if (!owned.closed_on && owned.commissioned_on) detail+=QStringLiteral("\nCooling/grid limit %1 facility · %2 IT.").arg(kw(power.facility_watts),kw(power.it_watts));
            sites.push_back({text(owned.id),text(owned.name),site_location(i,owned),std::move(detail),std::move(status),false,{{QStringLiteral("closed"),owned.closed_on.has_value()},{QStringLiteral("gridKw"),number(owned.grid_watts/1000)},{QStringLiteral("powerKw"),number(owned.power.back().watts/1000)}}});
        }
        for (const auto& h:i.hardware) {
            const auto& f=find(i.catalog.hardware,h.family_id);const auto& owned=find(i.sites,h.site_id);
            const auto source=std::find_if(s.capacity->sources.begin(),s.capacity->sources.end(),[&](const auto& p){return p.id==h.source_id;});
            auto status=h.retired_on?QStringLiteral("Retired"):s.date<h.delivered_on?QStringLiteral("Delivery pending"):!h.commissioned_on?QStringLiteral("Awaiting site, staff or power"):
                source->effective.value()==0?QStringLiteral("No usable power") : QStringLiteral("%1 CU/day usable").arg(cu(source->effective.value()));
            auto detail=rack_terms(f)+QStringLiteral("\nPaid %1 · depreciation %2 · resale %3.").arg(money(h.paid),money(h.depreciated),money(h.resale));
            if (source!=s.capacity->sources.end() && source->wholesale_reserved.value()>0)
                detail+=QStringLiteral("\n%1 CU/day committed to wholesale. %2 CU/day usable by your company.")
                    .arg(cu(source->wholesale_reserved.value()),cu(company_capacity(*source).value()));
            if (!plans.empty() && !h.retired_on && source!=s.capacity->sources.end())
                wholesale_sources_.push_back(option(h.source_id,text(owned.name)+QStringLiteral(" · ")+text(h.id)));
            hardware.push_back({text(h.id),text(f.name),text(owned.name)+QStringLiteral(" · delivery ")+date(h.delivered_on),std::move(detail),std::move(status),false,{{QStringLiteral("retired"),h.retired_on.has_value()}}});
        }
        for (const auto& a:i.appointments) {
            const auto& owned=find(i.sites,a.site_id);
            QString detail=QStringLiteral("%1 paid · scheduled %2 to %3 (exclusive). %4").arg(money(a.paid),date(a.starts_on),date(a.ends_before),a.mode==AppointmentMode::delegated?QStringLiteral("Delegated; no executive time used."):QStringLiteral("Management stays available during the appointment."));
            if (a.inspection) {const auto& r=*a.inspection;detail+=QStringLiteral("\nInspection: %1 · %2 racks delivered · %3 operators · %4 usable CU/day · prior meter %5.")
                .arg(r.built && r.connected?QStringLiteral("built and connected"):QStringLiteral("closed or unavailable"),number(r.delivered_bundles),number(r.operators),cu(r.effective.value()),kwh(r.observed_energy_wh));}
            appointments.push_back({text(a.id),mode_name(a.mode)+QStringLiteral(" · ")+text(owned.name),date(a.visit_on),std::move(detail),a.cancelled_on?QStringLiteral("Canceled"):a.inspection?QStringLiteral("Inspected"):s.date>=a.starts_on?QStringLiteral("In progress"):QStringLiteral("Booked"),false,{{QStringLiteral("cancelable"),!a.cancelled_on && !a.inspection && s.date<a.starts_on}}});
        }
        constexpr std::size_t maximum_visible=24;const auto first=i.notices.size()>maximum_visible?i.notices.size()-maximum_visible:0;
        for (auto at=i.notices.size();at>first;--at) {
            const auto& n=i.notices[at-1];QString subject=text(n.subject);
            for (const auto& owned:i.sites) if (owned.id==n.subject) subject=text(owned.name);
            for (const auto& h:i.hardware) if (h.id==n.subject) subject=text(find(i.catalog.hardware,h.family_id).name)+QStringLiteral(" · ")+text(find(i.sites,h.site_id).name);
            notices.push_back({text(n.id),notice_heading(n.kind),date(n.day),subject,QStringLiteral("Recorded")});
        }
    }
    const auto sync=[&](StableListModel* model,std::vector<CampaignRow> rows) {if (replace) model->beginCampaign(std::move(rows));else model->setRows(std::move(rows));};
    if (refresh_states) {sync(states_,std::move(states));if (replace && available()) states_->setSelectedId(text(state().infrastructure->executive_base));}
    sync(sites_,std::move(sites));sync(hardware_,std::move(hardware));sync(calendar_,std::move(appointments));sync(notices_,std::move(notices));
    sync(wholesale_contracts_,std::move(contracts));
    auto risk=resilience_models(state());
    resilience_sites_=std::move(risk.sites);protection_offers_=std::move(risk.protections);insurance_offers_=std::move(risk.insurance);
    replicated_services_=std::move(risk.services);replica_sources_=std::move(risk.sources);
    sync(protections_,std::move(risk.installed));sync(policies_,std::move(risk.policies));
    sync(incidents_,std::move(risk.incidents));sync(replicas_,std::move(risk.replicas));emit changed();
}
QString InfrastructureController::riskSummary() const {return available()?site_exposure(state(),states_->selectedId().toStdString()):QString{};}
QString InfrastructureController::suggestedVisit(QString id,QString mode) const {
    if (!available()) return {};
    try {
        const auto& s=state();const auto& i=*s.infrastructure;const auto& owned=find(i.sites,id.toStdString());auto visit=std::max(owned.built_on,owned.grid_on);const auto m=appointment_mode(mode);
        auto lead=i.catalog.remote_booking_lead;
        if (m==AppointmentMode::delegated) lead=i.catalog.delegated_booking_lead;
        if (m==AppointmentMode::onsite) {
            lead=i.catalog.onsite_booking_lead;const auto& from=find(i.catalog.states,i.executive_base);const auto& to=find(i.catalog.states,owned.state_id);
            for (const auto& route:i.catalog.travel) if (route.from==from.travel_zone && route.to==to.travel_zone) lead=checked_unit_add(lead,route.one_way);
        }
        return date(std::max(visit,advance_days(s.date,lead)));
    } catch (const std::exception&) {return {};}
}
template<class Make> void InfrastructureController::review(Make&& make) {
    review_.reset();review_text_.clear();
    try {
        if (!available() || (!state().paused && !snapshot_->driver_halted)) throw std::invalid_argument("Pause the campaign to review infrastructure commitments.");
        auto action=make(*state().infrastructure);Simulation candidate{state(),TrustedSimulationState{}};
        const auto accepted=candidate.submit_in_transaction({state().revision,action});
        if (!accepted.accepted()) review_text_=text(accepted.explanation);
        else {review_text_=terms(state(),candidate.checkpoint(),action);review_=Command{state().revision,std::move(action)};}
    } catch (const std::exception& e) {review_text_=text(e.what());}
    emit reviewChanged();
}
void InfrastructureController::reviewBuild(QString id,QString name,QString power,QString cooling) {
    review([&](const auto& i)->CommandAction {
        if (cooling!=QStringLiteral("air") && cooling!=QStringLiteral("evaporative")) throw std::invalid_argument("Choose a cooling system first.");
        return BuildOwnedSite{next_id(i.sites,"site."),name.trimmed().toStdString(),id.toStdString(),whole(power,i.catalog.minimum_site_watts/1000,i.catalog.maximum_site_watts/1000)*1000,cooling==QStringLiteral("air")?CoolingMode::air:CoolingMode::evaporative};
    });
}
void InfrastructureController::reviewHardware(QString site,QString family) {review([&](const auto& i)->CommandAction {return OrderOwnedHardware{next_id(i.hardware,"rack."),site.toStdString(),family.toStdString()};});}
void InfrastructureController::reviewHire(QString site,QString people) {review([&](const auto& i)->CommandAction {return HireSiteOperators{next_id(i.hires,"crew."),site.toStdString(),static_cast<std::uint32_t>(whole(people,1,i.catalog.maximum_operators_per_site))};});}
void InfrastructureController::reviewPower(QString site,QString power) {review([&](const auto& i)->CommandAction {const auto id=site.toStdString();return SetSitePowerLimit{id,whole(power,0,find(i.sites,id).grid_watts/1000)*1000};});}
void InfrastructureController::reviewAppointment(QString site,QString when,QString mode) {review([&](const auto& i)->CommandAction {return ScheduleSiteAppointment{next_id(i.appointments,"visit."),site.toStdString(),parse_iso_date(when.toStdString()),appointment_mode(mode)};});}
void InfrastructureController::reviewRetire(QString id) {review([&](const auto&)->CommandAction {return RetireOwnedHardware{id.toStdString()};});}
void InfrastructureController::reviewClose(QString id) {review([&](const auto&)->CommandAction {return CloseOwnedSite{id.toStdString()};});}
void InfrastructureController::reviewCancel(QString id) {review([&](const auto&)->CommandAction {return CancelSiteAppointment{id.toStdString()};});}
void InfrastructureController::reviewLease() {review([](const auto&)->CommandAction {return NoticeInitialLeaseTermination{};});}
void InfrastructureController::reviewProtection(QString site,QString offer) {
    review([&](const auto&)->CommandAction {return BuySiteProtection{site.toStdString(),offer.toStdString()};});
}
void InfrastructureController::reviewInsurance(QString site,QString offer) {
    review([&](const auto&)->CommandAction {return InsureSite{site.toStdString(),offer.toStdString()};});
}
void InfrastructureController::reviewReplica(QString service,QString source) {
    review([&](const auto&)->CommandAction {return ReplicateService{service.toStdString(),source.toStdString()};});
}
void InfrastructureController::reviewStopReplica(QString id) {
    review([&](const auto&)->CommandAction {return StopServiceReplica{id.toStdString()};});
}
void InfrastructureController::reviewWholesale(QString plan_id,QString source_id,QString buyer_id) {
    review_.reset();review_text_.clear();
    try {
        if (!available() || (!state().paused && !snapshot_->driver_halted))
            throw std::invalid_argument("Pause the campaign to review wholesale terms.");
        const auto& world=snapshot_->checkpoint.world;
        const auto quote=runtime::wholesale_quote(world,*program_,plan_id.toStdString(),source_id.toStdString(),buyer_id.toStdString());
        const auto& plan=find(runtime::wholesale_plans(world,*program_),plan_id.toStdString());
        const auto& buyer=runtime::rival_definition(*program_,buyer_id.toStdString());
        review_text_=QStringLiteral("%1 will reserve %2 CU/day on %3 through %4 (exclusive).\n"
            "%5/day · %6 MiB minimum · %7 interconnect. %8\n"
            "The buyer protects up to %9 in cash. Availability below %10% earns a proportional fee credit. "
            "A full outage credits the entire daily fee. The reserved capacity is committed for the term.")
            .arg(text(buyer.name),cu(plan.capacity.value()),source_id,date(quote.ends_before),money(plan.daily_fee),number(plan.minimum_memory_mib),
                text(plan.interconnect),plan.energy_included?QStringLiteral("Electricity included."):QStringLiteral("Marginal electricity billed for actual buyer work."),
                money(quote.funded_ceiling),decimal(static_cast<std::uint64_t>(plan.availability.value()),100,2));
        review_=WholesaleReview{state().revision,std::move(plan_id),std::move(source_id),std::move(buyer_id)};
    } catch (const std::exception& e) {review_text_=text(e.what());}
    emit reviewChanged();
}
void InfrastructureController::confirm() {
    if (!review_ || !available() || (!state().paused && !snapshot_->driver_halted) ||
        state().revision!=std::visit([](const auto& row){return row.expected_revision;},*review_)) {
        dismissReview();emit noticeRaised(QStringLiteral("The campaign changed. Review the new terms before confirming."));return;
    }
    auto action=std::move(*review_);dismissReview();
    if (auto* local=std::get_if<Command>(&action)) emit actionRequested(std::move(local->action));
    else {
        auto& offer=std::get<WholesaleReview>(action);
        emit wholesaleRequested(std::move(offer.plan),std::move(offer.source),std::move(offer.buyer));
    }
}
void InfrastructureController::dismissReview() {review_.reset();review_text_.clear();emit reviewChanged();}
}
