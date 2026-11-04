#pragma once
#include <string>

// Bounded local development input. The content adapter itself accepts bytes
// and has no filesystem access.
[[nodiscard]] std::string read_content_file(const char* path);
