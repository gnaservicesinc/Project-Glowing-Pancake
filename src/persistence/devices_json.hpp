#pragma once
#include "json_support.hpp"
#include <pgp/core/devices.hpp>
namespace pgp::persistence::detail {
Json devices_json(const core::DeviceInventory&);
core::DeviceInventory parse_devices(const Json&);
}
