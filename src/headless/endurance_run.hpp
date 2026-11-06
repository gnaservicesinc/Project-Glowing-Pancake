#pragma once

#include <iosfwd>
#include <string_view>

struct EnduranceOptions {
    const char* seed = nullptr;
    const char* resume = nullptr;
    const char* save = nullptr;
    const char* strategy = nullptr;
    const char* save_limit_mib = nullptr;
    const char* checkpoint_years = nullptr;
};
void run_endurance(const char* path, std::string_view scenario, std::string_view through,
                   EnduranceOptions options, std::ostream& output);
