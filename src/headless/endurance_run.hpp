#pragma once

#include <iosfwd>
#include <string_view>

void run_endurance(const char* path, std::string_view scenario, std::string_view through,
                   const char* seed, std::ostream& output);
