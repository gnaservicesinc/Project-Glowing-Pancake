#pragma once

#include <iosfwd>
#include <string_view>

struct EnduranceOptions {
    const char* seed = nullptr;
    const char* resume = nullptr;
    const char* save = nullptr;
};
void run_endurance(const char* path, std::string_view scenario, std::string_view through,
                   EnduranceOptions options, std::ostream& output);
