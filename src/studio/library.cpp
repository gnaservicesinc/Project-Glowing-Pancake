#include <pgp/studio/library.hpp>

#include "checkout.hpp"
#include "references.hpp"
#include <pgp/core/build_identity.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/persistence/storage.hpp>
#include <pgp/runtime/runtime.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>

namespace pgp::studio {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::size_t maximum_entries = 4096;
constexpr std::size_t maximum_documents = 128;
constexpr std::size_t maximum_definitions = 8192;

[[noreturn]] void refuse(const char* detail) { throw std::invalid_argument(detail); }
fs::file_status status_at(const fs::path& path) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error && error != std::errc::no_such_file_or_directory)
        throw fs::filesystem_error("Cannot inspect the selected checkout", path, error);
    return status;
}
} // namespace

namespace detail {
fs::file_status checked_path(const fs::path& root, const fs::path& relative) {
    if (relative.empty() || relative.has_root_path() ||
        std::any_of(relative.begin(), relative.end(), [](const auto& part) { return part.empty() || part == "." || part == ".."; }))
        refuse("Library paths must be relative to the selected checkout.");
    auto current = root;
    auto status = status_at(root);
    for (const auto& component : relative) {
        if (!fs::exists(status)) return status;
        if (!fs::is_directory(status))
            refuse("Library paths must stay inside ordinary checkout directories.");
        current /= component;
        status = status_at(current);
        if (fs::is_symlink(status)) refuse("Symbolic links are blocked in the Content Studio library.");
    }
    return status;
}
std::uint32_t checkout_schema(const fs::path& root) {
    const auto git = status_at(root / ".git");
    if (!fs::is_directory(git) &&
        (!fs::is_regular_file(git) || !persistence::read_bounded(root / ".git", 8192).starts_with("gitdir: ")))
        refuse("Select a development checkout or Git worktree.");
    if (!fs::is_regular_file(checked_path(root, "CMakeLists.txt")) ||
        !fs::is_regular_file(checked_path(root, "src/content/collection.cpp")) ||
        !fs::is_regular_file(checked_path(root, "project-versions.json")))
        refuse("The selected folder lacks the source tree and project marker.");
    const auto manifest = Json::parse(persistence::read_bounded(root / "project-versions.json", 16384));
    if (!manifest.is_object() || !manifest.contains("project_id") || !manifest.at("project_id").is_string() ||
        manifest.at("project_id") != core::build_identity().project_id)
        refuse("The selected checkout belongs to another project.");
    if (!manifest.contains("manifest_format") || !manifest.at("manifest_format").is_number_integer())
        refuse("The checkout project marker has an invalid manifest format.");
    if (manifest.at("manifest_format") != 1) return 0;
    const auto& compatibility = manifest.at("compatibility");
    const auto& schema = compatibility.at("content_schema");
    if (!schema.is_number_unsigned() || schema == 0 || schema.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
        refuse("The checkout project marker has an invalid content schema.");
    return schema.get<std::uint32_t>();
}
runtime::CompiledProgram compile_source(std::string_view bytes, const content::Registry& registry) {
    return runtime::compile_collection(content::load_collection(bytes, registry), registry, runtime::supported_effects());
}
} // namespace detail

namespace {
using detail::checked_path;
struct Candidate {
    fs::path relative;
    Origin origin;
};
void collect(const fs::path& root, const fs::path& folder, Origin origin, std::vector<Candidate>& candidates,
             std::size_t& visited) {
    const auto status = checked_path(root, folder);
    if (!fs::exists(status)) return;
    if (!fs::is_directory(status)) refuse("A configured library location is not a directory.");
    for (const auto& entry : fs::recursive_directory_iterator(root / folder)) {
        if (++visited > maximum_entries) refuse("The selected checkout exceeds the library scan limit.");
        const auto type = entry.symlink_status();
        if ((!fs::is_regular_file(type) && !fs::is_symlink(type)) || entry.path().extension() != ".json") continue;
        if (candidates.size() >= maximum_documents) refuse("The selected checkout exceeds the library document limit.");
        candidates.push_back({entry.path().lexically_relative(root), origin});
    }
}
std::vector<std::string> uses(const content::Definition& definition, const content::Registry& registry) {
    std::set<std::string> targets;
    detail::visit_definition_references(definition, registry, [&](const auto& target) { targets.insert(target); });
    return {targets.begin(), targets.end()};
}
LibraryDocument inspect(const Library& library, const Candidate& candidate, const content::Registry& registry,
                        std::size_t& indexed) {
    LibraryDocument document{};
    document.relative_path = candidate.relative;
    document.origin = candidate.origin;
    try {
        if (!fs::is_regular_file(checked_path(library.checkout, candidate.relative)))
            refuse("The library document is no longer an ordinary local file.");
    } catch (const std::exception& error) {
        document.validation = Validation::blocked;
        document.diagnostic = error.what();
        return document;
    }
    if (library.checkout_schema != content::content_schema) {
        document.validation = Validation::unsupported;
        document.diagnostic = "The checkout schema or project marker format is unsupported. The file is listed without opening.";
        return document;
    }
    runtime::CompiledProgram program;
    try {
        const auto bytes = persistence::read_bounded(library.checkout / candidate.relative, content::maximum_document_bytes);
        document.source_hash = persistence::sha256(bytes);
        program = detail::compile_source(bytes, registry);
    } catch (const content::ValidationError& error) {
        document.validation = error.code() == "unsupported_schema" ? Validation::unsupported : Validation::invalid;
        document.diagnostic = error.what();
        return document;
    } catch (const runtime::CompileError& error) {
        document.validation = Validation::invalid;
        document.diagnostic = error.what();
        return document;
    } catch (const persistence::PersistenceError& error) {
        document.validation = Validation::blocked;
        document.diagnostic = error.what();
        return document;
    }
    indexed += program.collection.definitions.size();
    if (indexed > maximum_definitions) refuse("The selected checkout exceeds the definition index limit.");
    document.validation = Validation::validated;
    document.content_version = program.collection.content_version;
    document.canonical_hash = persistence::sha256(program.canonical_content_bytes);
    std::map<std::string, std::vector<std::string>, std::less<>> reverse;
    for (const auto& [id, definition] : program.collection.definitions) {
        DefinitionEntry entry{id, std::string(content::definition_kind(definition)), definition.revision, {},
                              uses(definition, program.registry), {}};
        if (const auto* flow = std::get_if<content::FlowDefinition>(&definition.data)) entry.tags = flow->tags;
        for (const auto& target : entry.uses) reverse[target].push_back(id);
        document.definitions.push_back(std::move(entry));
    }
    for (auto& entry : document.definitions) {
        const auto found = reverse.find(entry.id);
        if (found != reverse.end()) entry.used_by = std::move(found->second);
    }
    return document;
}
} // namespace

namespace detail {
fs::path checkout_root(const fs::path& selected) {
    if (!selected.is_absolute() || !fs::is_directory(status_at(selected)))
        refuse("Select an absolute development checkout directory.");
    return fs::canonical(selected);
}
} // namespace detail

Library inspect_checkout(const fs::path& selected) {
    const auto root = detail::checkout_root(selected);
    Library library{root, detail::checkout_schema(root), {}};
    std::vector<Candidate> candidates;
    std::size_t visited = 0;
    collect(root, "data/runtime", Origin::reviewed, candidates, visited);
    collect(root, "data/drafts", Origin::draft, candidates, visited);
    collect(root, "tests/content/fixtures", Origin::engineering_fixture, candidates, visited);
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return left.relative.generic_u8string() < right.relative.generic_u8string();
    });
    const auto registry = content::foundation_registry();
    std::size_t indexed = 0;
    library.documents.reserve(candidates.size());
    for (const auto& candidate : candidates) library.documents.push_back(inspect(library, candidate, registry, indexed));
    return library;
}
} // namespace pgp::studio
