#include "content_validation.hpp"
#include "content_file.hpp"

#include <pgp/content/collection.hpp>

#include <ostream>
#include <string>

int validate_content_file(const char* path, std::ostream& output, std::ostream& errors) {
    try {
        const auto document = read_content_file(path);
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
