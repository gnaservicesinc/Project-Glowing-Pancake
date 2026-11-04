#include "content_validation.hpp"

#include <pgp/content/collection.hpp>

#include <fstream>
#include <ostream>
#include <string>

int validate_content_file(const char* path, std::ostream& output, std::ostream& errors) {
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) { errors << "Cannot open local content file.\n"; return 3; }
        std::string document(pgp::content::maximum_document_bytes + 1, '\0');
        input.read(document.data(), static_cast<std::streamsize>(document.size()));
        if (input.bad()) { errors << "Cannot read local content file.\n"; return 3; }
        document.resize(static_cast<std::size_t>(input.gcount()));
        const auto collection = pgp::content::load_collection(document, pgp::content::foundation_registry());
        output << "Valid content schema " << collection.schema << "; version " << collection.content_version
               << "; definitions " << collection.definitions.size() << '\n';
        return 0;
    } catch (const pgp::content::ValidationError& error) {
        errors << "Invalid content [" << error.code() << "] at " << error.path() << ": " << error.what() << '\n';
        return 3;
    } catch (const std::exception& error) {
        errors << "Content validation failed: " << error.what() << '\n';
        return 3;
    }
}
