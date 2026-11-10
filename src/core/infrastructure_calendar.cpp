#include "infrastructure_internal.hpp"
namespace pgp::core {
using namespace infrastructure_detail;
SiteAppointment preview_site_appointment(const SimulationState& s,const ScheduleSiteAppointment& request) {
    const auto& i=infrastructure(s);const auto& owned=site(s,request.site_id);const auto& c=i.catalog;
    if (!identifier(request.id) || owned.closed_on || i.appointments.size()>=maximum_appointments ||
        std::any_of(i.appointments.begin(),i.appointments.end(),[&](const auto& a){return a.id==request.id;})) invalid("Choose a new appointment for an open site");
    SiteAppointment appointment;appointment.id=request.id;appointment.site_id=request.site_id;appointment.mode=request.mode;appointment.booked_on=s.date;appointment.visit_on=request.visit_on;
    Days lead{0},travel{0};
    if (request.mode==AppointmentMode::onsite) {
        const auto& route=travel_route(c,i.executive_base,owned.state_id);
        travel=route.one_way;lead=c.onsite_booking_lead;appointment.paid=checked_unit_add(c.onsite_inspection,checked_unit_add(route.fare,route.fare));
    } else if (request.mode==AppointmentMode::remote) {lead=c.remote_booking_lead;appointment.paid=c.remote_inspection;}
    else if (request.mode==AppointmentMode::delegated) {lead=c.delegated_booking_lead;appointment.paid=c.delegated_inspection;}
    else invalid("Unsupported site appointment mode");
    appointment.starts_on=OrdinalDay{checked_subtract(request.visit_on.value(),travel.value())};
    appointment.ends_before=advance_days(request.visit_on,checked_unit_add(travel,Days{1}));
    if (appointment.starts_on<advance_days(s.date,lead) || request.visit_on<std::max(owned.built_on,owned.grid_on) || appointment.ends_before>s.end_date) invalid("The appointment needs its booking/travel lead, completed building and grid connection, and a return before the ending");
    if (request.mode!=AppointmentMode::delegated) for (const auto& prior:i.appointments)
        if (!prior.cancelled_on && prior.mode!=AppointmentMode::delegated && appointment.starts_on<prior.ends_before && prior.starts_on<appointment.ends_before) invalid("The executive already has a conflicting appointment or trip; choose another date or delegate");
    if(request.mode!=AppointmentMode::delegated) if(const auto* trip=pending_family_visit(s);
        trip && appointment.starts_on<trip->returns_on && trip->departs_on<appointment.ends_before)
        invalid("This appointment overlaps your family visit. Choose another date or delegate.");
    return appointment;
}
void schedule_site_appointment(SimulationState& s,const ScheduleSiteAppointment& request,Cents protected_cash) {
    auto appointment=preview_site_appointment(s,request);
    pay_expense(s,appointment.paid,protected_cash,"owned.appointment/"+appointment.id,"Site inspection and disclosed travel");
    infrastructure(s).appointments.push_back(std::move(appointment));add_notice(s,"booked/"+request.id,"inspection_booked",request.site_id,s.date);
}
void cancel_site_appointment(SimulationState& s,std::string_view id) {
    auto& records=infrastructure(s).appointments;auto at=std::find_if(records.begin(),records.end(),[&](const auto& a){return a.id==id;});
    if (at==records.end() || at->cancelled_on || at->inspection || s.date>=at->starts_on) invalid("Only an appointment whose travel/work has not started can be canceled; confirmed fees are nonrefundable");
    at->cancelled_on=s.date;add_notice(s,"cancelled/"+at->id,"inspection_cancelled",at->site_id,s.date);
}
ExecutiveLocation executive_location(const SimulationState& s) {
    if (!s.infrastructure) return {};
    const auto& i=*s.infrastructure;ExecutiveLocation result{i.executive_base,i.executive_base,false,std::nullopt};
    if(const auto* trip=family_visit_on(s,s.date)) {
        result.family_home=s.date>=trip->arrives_on && s.date<trip->leaves_on;
        result.traveling=!result.family_home;
        result.state_id=family_home_state(s);
        return result;
    }
    for (const auto& a:i.appointments) if (!a.cancelled_on && a.mode!=AppointmentMode::delegated && s.date>=a.starts_on && s.date<a.ends_before) {
        result.appointment=a.id;
        if (a.mode==AppointmentMode::onsite) {
            result.traveling=s.date!=a.visit_on;result.state_id=result.traveling?"":site(s,a.site_id).state_id;
        }
        break;
    }
    return result;
}
bool executive_at_headquarters(const SimulationState& s) {
    if(!s.infrastructure) return false;
    const auto location=executive_location(s);
    if(location.traveling || location.family_home) return false;
    return !location.appointment || std::ranges::none_of(s.infrastructure->appointments,[&](const auto& row) {
        return row.id==*location.appointment && row.mode==AppointmentMode::onsite;
    });
}
namespace infrastructure_detail {
void prepare_site_appointments(SimulationState& s) {
    for (const auto& a:infrastructure(s).appointments) if (!a.cancelled_on && !a.inspection && a.visit_on==s.date) {
        auto& owned=site(s,a.site_id);
        if (!owned.closed_on && owned.built_on<=s.date && owned.grid_on<=s.date && !owned.commissioned_on) owned.commissioned_on=s.date;
    }
}
void finish_site_inspections(SimulationState& s) {
    for (auto& a:infrastructure(s).appointments) if (!a.cancelled_on && !a.inspection && a.visit_on==s.date) {
        const auto& owned=site(s,a.site_id);SiteInspection report;report.day=s.date;report.built=owned.built_on<=s.date && !owned.closed_on;report.connected=owned.grid_on<=s.date && !owned.closed_on;
        for (const auto& h:infrastructure(s).hardware) if (h.site_id==owned.id && h.delivered_on<=s.date && before_end(s.date,h.retired_on)) ++report.delivered_bundles;
        report.operators=operators_on(s,owned,s.date);const auto power=power_plan(s,owned,s.date);
        for (const auto& h:power.hardware) report.effective=checked_unit_add(report.effective,h.effective);
        report.observed_energy_wh=owned.last_energy_wh;a.inspection=report;
        add_notice(s,"inspection/"+a.id,"inspection_complete",owned.id,s.date);
    }
}
}
}
