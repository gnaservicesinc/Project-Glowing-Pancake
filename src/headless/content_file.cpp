#include "content_file.hpp"

#include <pgp/content/collection.hpp>

#include <fstream>
#include <stdexcept>

std::string read_content_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open local content file.");
    std::string document(pgp::content::maximum_document_bytes + 1, '\0');
    input.read(document.data(), static_cast<std::streamsize>(document.size()));
    if (input.bad()) throw std::runtime_error("Cannot read local content file.");
    document.resize(static_cast<std::size_t>(input.gcount()));
    return document;
}
