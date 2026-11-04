#pragma once
#include <iosfwd>
int validate_content_file(const char* path, std::ostream& output, std::ostream& errors);
