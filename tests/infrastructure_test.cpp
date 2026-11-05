#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <iostream>
#include <sstream>
namespace {
using namespace pgp::core;unsigned checks=0;
void check(bool value,const char* why) {++checks;if (!value) throw std::runtime_error(why);}
InfrastructureCatalog rules() {
    InfrastructureCatalog c;c.map_asset_id="map.us_states_2024";c.map_sha256=std::string(64,'a');c.assumptions="Hand-auditable fictional constant-2014 test profiles; boundaries do not supply economics.";
    c.remote_booking_lead=Days{1};c.delegated_booking_lead=Days{1};c.onsite_booking_lead=Days{1};c.remote_inspection=Cents{100};c.onsite_inspection=Cents{100};c.delegated_inspection=Cents{200};c.building_life=Days{1000};c.initial_lease_notice=Days{3};
    std::istringstream ids{"AL AK AZ AR CA CO CT DE DC FL GA HI ID IL IN IA KS KY LA ME MD MA MI MN MS MO MT NE NV NH NJ NM NY NC ND OH OK OR PA RI SC SD TN TX UT VT VA WA WV WI WY"};
    std::string id;while (ids>>id) {
        StateSiteProfile state;state.id="US-"+id;state.name=id;state.travel_zone=id=="CA"?"west":"east";state.construction_base=Cents{10000};state.construction_per_kw=Cents{100};state.monthly_property=Cents{310};state.operator_signing=Cents{100};state.operator_monthly=Cents{310};
        state.build_lead=Days{1};state.grid_lead=Days{id=="TX"?4:2};state.operator_hire_lead=Days{1};state.grid_watts=100000;state.water_limit_ml_per_day=2500000;state.water_ml_per_kwh=2000;state.air_pue_milli=1250;state.water_pue_milli=1100;state.grid_grams_per_kwh=500;state.electricity_millicents_per_kwh=5000;state.water_cents_per_1000_liters=100;state.connectivity_class=2;c.states.push_back(state);
    }
    HardwareFamily hardware;hardware.id="rack.general";hardware.name="Reference bundle";hardware.interconnect="general";hardware.purchase_cost=Cents{60000};hardware.monthly_maintenance=Cents{310};hardware.capacity=MilliCUPerDay{100000};hardware.memory_mib=32768;hardware.active_it_watts=10000;hardware.idle_it_watts=2000;hardware.operators=1;hardware.delivery_lead=Days{2};hardware.useful_life=Days{100};hardware.resale_fraction=BasisPoints{5000};c.hardware.push_back(hardware);
    for (const auto& from:{"west","east"}) for (const auto& to:{"west","east"}) c.travel.push_back({from,to,Days{std::string_view(from)==to?0:1},Cents{1000}});
    validate_infrastructure_catalog(c);return c;
}
Simulation opening(InfrastructureCatalog c=rules()) {
    SimulationState s;s.date=parse_iso_date("2014-01-01");s.end_date=parse_iso_date("2015-01-01");s.cash=Cents{100000000};s.accounts=opening_accounts(s.cash,s.date);add_recurring(*s.accounts,{"compute_lease",Cents{620},s.date,std::nullopt,"Opening leased compute"});
    s.capacity=opening_capacity({{"lease.initial","cloud.initial","company.player","supplier.initial",CapacitySourceKind::leased,MilliCUPerDay{100000},MilliCUPerDay{100000},0,"general",true}},2,s.date);
    InfrastructureState owned;owned.catalog=std::move(c);owned.executive_base="US-CA";s.infrastructure=std::move(owned);return Simulation{std::move(s)};
}
void apply(Simulation& game,CommandAction action) {const auto result=game.submit({game.checkpoint().revision,std::move(action)});if (!result.accepted()) throw std::runtime_error(result.explanation);}
void refuse(Simulation& game,CommandAction action) {const auto before=game.checkpoint();const auto result=game.submit({before.revision,std::move(action)});check(!result.accepted() && game.checkpoint()==before,"Refused infrastructure action changed state");}
void step(Simulation& game,unsigned days=1) {for (unsigned d=0;d<days;++d) apply(game,AdvanceDay{});}
void restore(const Simulation& game) {check(Simulation{game.checkpoint()}.checkpoint()==game.checkpoint(),"Owned checkpoint did not restore exactly");}
const CapacitySource& source(const Simulation& game) {for (const auto& row:game.checkpoint().capacity->sources) if (row.id=="owned/rack.a") return row;throw std::runtime_error("No commissioned source");}
void setup(Simulation& game,CoolingMode cooling=CoolingMode::air) {
    apply(game,BuildOwnedSite{"site.a","First site","US-WA",20000,cooling});apply(game,OrderOwnedHardware{"rack.a","site.a","rack.general"});apply(game,HireSiteOperators{"crew.a","site.a",1});
    apply(game,ScheduleSiteAppointment{"inspection.a","site.a",advance_days(game.checkpoint().date,Days{2}),AppointmentMode::remote});
}
void run() {
    auto game=opening();const auto random=game.checkpoint().random;setup(game);restore(game);
    check(game.checkpoint().capacity->sources.size()==1,"A purchase granted immediate physical capacity");
    check(game.checkpoint().capacity->available_staff==2,"Dedicated operators created free lab staffing");
    const auto before_preview=game.checkpoint();const auto other=preview_site_build(game.checkpoint(),{"site.tx","Alternative","US-TX",20000,CoolingMode::air});
    check(other.grid_on==advance_days(game.checkpoint().date,Days{4}) && game.checkpoint()==before_preview,"State choice or pure grid preview failed");
    step(game,2);check(source(game).effective==MilliCUPerDay{100000} && !source(game).energy_included,"Delivered, inspected and staffed owned capacity failed to commission");restore(game);
    auto work=CapacityProject{"work.owned","owned/rack.a",ProjectKind::training,ProjectStatus::active,game.checkpoint().date,std::nullopt,MilliCUPerDay{50000},1,32768,"general",MilliCU{1000000},MilliCU{0},0,MilliCU{0},0,Cents{0},"Physical work"};
    apply(game,ReserveProject{work});step(game);
    auto& meter=game.checkpoint().infrastructure->sites.front();
    check(meter.last_energy_wh==180000 && meter.last_water_ml==0 && meter.last_emissions_grams==90000 && meter.last_utility_cost==Cents{900},"Half-load IT energy/PUE/electricity/carbon arithmetic is wrong");
    check(meter.meters.back().operating==Cents{50},"Actual calendar-day property, operator and maintenance accrual is wrong");
    apply(game,SetSitePowerLimit{"site.a",5000});check(source(game).effective==MilliCUPerDay{25000},"Power curtailment did not reduce feasible CU");step(game);
    check(game.checkpoint().infrastructure->sites.front().last_energy_wh==120000 && game.checkpoint().capacity->days.back().projects.front().work==MilliCU{25000},"Curtailment billed imaginary full work or applied cooling twice");
    apply(game,SetSitePowerLimit{"site.a",0});step(game);check(game.checkpoint().infrastructure->sites.front().last_energy_wh==0,"A powered-off bundle still consumed energy");restore(game);
    refuse(game,SetEffectiveCapacity{"owned/rack.a",MilliCUPerDay{100000}});refuse(game,WithdrawOperations{});refuse(game,CloseOwnedSite{"site.a"});
    auto forged=game.checkpoint();forged.infrastructure->sites.front().meters.back().energy_wh+=1;bool rejected=false;try {(void)Simulation{forged};}catch (const std::invalid_argument&) {rejected=true;}check(rejected,"Forged owned meter survived deep restore");
    forged=game.checkpoint();forged.infrastructure->hardware.front().energy_remainder=1;rejected=false;try {(void)Simulation{forged};}catch (const std::invalid_argument&) {rejected=true;}check(rejected,"Forged energy remainder survived deep restore");
    const auto forecast_before=game.checkpoint();const auto forecast=project_company_cash(game.checkpoint(),advance_days(game.checkpoint().date,Days{30}));check(forecast.cash<game.checkpoint().cash && game.checkpoint()==forecast_before,"Owned operating commitments were omitted from a pure cash preview");
    apply(game,SetSitePowerLimit{"site.a",20000});auto incompatible=work;incompatible.id="work.memory";incompatible.starts_on=game.checkpoint().date;incompatible.minimum_memory_mib=65536;refuse(game,ReserveProject{incompatible});incompatible.minimum_memory_mib=0;incompatible.interconnect="fabric.v1";refuse(game,ReserveProject{incompatible});
    apply(game,NoticeInitialLeaseTermination{});step(game,3);
    check(game.checkpoint().capacity->sources.front().effective.value()==0 && game.checkpoint().accounts->recurring.front().ends_before==game.checkpoint().date,"Initial lease notice did not end actual capacity and future bills");restore(game);
    apply(game,CancelProject{"work.owned"});const auto cash=game.checkpoint().cash;apply(game,RetireOwnedHardware{"rack.a"});check(game.checkpoint().cash>cash && source(game).effective.value()==0,"Hardware disposal did not conserve earned resale or stop compute");refuse(game,RetireOwnedHardware{"rack.a"});apply(game,CloseOwnedSite{"site.a"});restore(game);step(game,30);restore(game);
    check(game.checkpoint().random==random,"Owned infrastructure or its previews consumed RNG");
    auto water=opening();setup(water,CoolingMode::evaporative);step(water,2);work.starts_on=water.checkpoint().date;work.id="work.water";apply(water,ReserveProject{work});step(water);
    const auto& water_meter=water.checkpoint().infrastructure->sites.front();check(water_meter.last_energy_wh==158400 && water_meter.last_water_ml==316800 && water_meter.last_utility_cost==Cents{823},"Water cooling PUE, water consumption or carried cent billing is wrong");restore(water);
    auto scarce_rules=rules();for (auto& p:scarce_rules.states) if (p.id=="US-WA") p.water_limit_ml_per_day=240000;
    auto scarce=opening(std::move(scarce_rules));setup(scarce,CoolingMode::evaporative);step(scarce,2);
    check(preview_site_power(scarce.checkpoint(),"site.a",scarce.checkpoint().date).facility_watts==5000 && source(scarce).effective==MilliCUPerDay{31810},"Local water supply did not impose its actual cooling power ceiling");
    auto staffing=opening();apply(staffing,BuildOwnedSite{"site.a","Unstaffed site","US-WA",20000,CoolingMode::air});apply(staffing,OrderOwnedHardware{"rack.a","site.a","rack.general"});
    apply(staffing,ScheduleSiteAppointment{"inspection.a","site.a",advance_days(staffing.checkpoint().date,Days{2}),AppointmentMode::remote});step(staffing,2);
    check(staffing.checkpoint().capacity->sources.size()==1,"A completed inspection bypassed the paid operator requirement");
    apply(staffing,HireSiteOperators{"crew.a","site.a",1});step(staffing);check(source(staffing).effective==MilliCUPerDay{100000},"Paid operators did not activate the ready hardware on their actual start date");restore(staffing);
    auto travel=opening();apply(travel,BuildOwnedSite{"site.a","Travel site","US-WA",20000,CoolingMode::air});
    const auto visit=advance_days(travel.checkpoint().date,Days{4});apply(travel,ScheduleSiteAppointment{"trip.a","site.a",visit,AppointmentMode::onsite});
    refuse(travel,ScheduleSiteAppointment{"conflict","site.a",visit,AppointmentMode::remote});apply(travel,ScheduleSiteAppointment{"delegate","site.a",visit,AppointmentMode::delegated});
    step(travel,3);check(executive_location(travel.checkpoint()).traveling,"Outbound trip did not save the executive's physical location");apply(travel,BuildOwnedSite{"site.b","Managed while away","US-TX",20000,CoolingMode::air});refuse(travel,CancelSiteAppointment{"trip.a"});restore(travel);step(travel,3);check(executive_location(travel.checkpoint()).state_id=="US-CA","The executive did not return to the saved base");restore(travel);
    auto queue=opening();apply(queue,BuildOwnedSite{"site.one","First queue slot","US-WA",60000,CoolingMode::air});refuse(queue,BuildOwnedSite{"site.two","Oversubscribed grid","US-WA",60000,CoolingMode::air});
    apply(queue,BuildOwnedSite{"site.two","Second queue slot","US-WA",40000,CoolingMode::air});check(queue.checkpoint().infrastructure->sites.back().grid_on==advance_days(queue.checkpoint().date,Days{4}),"A second site skipped the finite state grid crew queue");restore(queue);
    std::cout<<checks<<" owned infrastructure checks passed\n";
}
}
int main() {try {run();return 0;}catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
