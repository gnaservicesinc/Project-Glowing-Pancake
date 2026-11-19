#pragma once
#include <pgp/core/units.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
namespace pgp::core {
struct SimulationState;
struct DeviceOffer {
    std::string id,name;
    Cents purchase{0},service{0};Days delivery{0},service_lead{0};
    bool portable=true;
    std::vector<std::string> included_offices,apps;
    auto operator<=>(const DeviceOffer&) const = default;
};
struct DeviceRules {std::vector<DeviceOffer> offers;auto operator<=>(const DeviceRules&) const = default;};
struct DeviceService {OrdinalDay ordered_on{0},ready_on{0};auto operator<=>(const DeviceService&) const = default;};
struct PersonalDevice {
    std::uint64_t id=0;std::string offer;
    OrdinalDay ordered_on{0},ready_on{0};bool included=false,powered=true;
    std::optional<OrdinalDay> retired_on;
    std::vector<DeviceService> services;
    bool playtest_delivery=false;
    std::string owner; // "protagonist" for personal devices, "company" for company-owned purchases
    auto operator<=>(const PersonalDevice&) const = default;
};
struct PhysicalDeviceInfo {
    std::string device_id;      // "personal/<id>" or "hq/<id>"
    std::string kind;           // "phone", "laptop", "tablet", "desktop"
    std::string name;           // Human-readable name distinguishing multiple physical units
    std::string owner;          // "protagonist" or "company"
    std::string location;       // "with you", "headquarters", "in transit", "in service", "retired", "storage"
    bool portable = true;
    bool powered = true;
    bool available = false;
    bool can_power = false;
    bool can_service = false;
    bool can_retire = false;
    std::uint64_t numeric_id = 0;
    std::string unavailability_reason;
    std::vector<std::string> apps;
    auto operator<=>(const PhysicalDeviceInfo&) const = default;
};
struct DeviceInventory {std::vector<PersonalDevice> records;auto operator<=>(const DeviceInventory&) const = default;};
struct BuyPersonalDevice {std::string offer;};
struct ServicePersonalDevice {std::uint64_t id;};
struct SetDevicePower {std::uint64_t id;bool powered;};
struct RetirePersonalDevice {std::uint64_t id;};
void validate_device_rules(const DeviceRules&);
void initialize_devices(SimulationState&);
bool device_functioning(const SimulationState&,const PersonalDevice&);
bool device_available(const SimulationState&,const PersonalDevice&,bool at_headquarters);
bool device_supports_app(const SimulationState&,std::string_view kind,std::string_view app);
std::string resolve_app_device(const SimulationState&,std::string_view preferred,std::string_view app,bool at_headquarters);
[[nodiscard]] std::vector<PhysicalDeviceInfo> physical_devices(const SimulationState&,bool at_headquarters);
[[nodiscard]] std::optional<PhysicalDeviceInfo> inspect_physical_device(const SimulationState&,std::string_view device_id,bool at_headquarters);
[[nodiscard]] bool physical_device_supports_app(const SimulationState&,std::string_view device_id,std::string_view app,bool at_headquarters);
[[nodiscard]] std::string resolve_physical_app_device(const SimulationState&,std::string_view preferred_device_id,std::string_view app,bool at_headquarters);
void buy_personal_device(SimulationState&,std::string_view,Cents protected_cash);
void service_personal_device(SimulationState&,std::uint64_t,Cents protected_cash);
void set_device_power(SimulationState&,const SetDevicePower&);
void retire_personal_device(SimulationState&,std::uint64_t);
void validate_devices(const SimulationState&,bool deep=true);
}
