#include <pgp/studio/library.hpp>

#include <pgp/persistence/storage.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs = std::filesystem;
using namespace pgp;
std::size_t checks = 0;
void require(bool valid, const char* detail) {
    ++checks;
    if (!valid) throw std::runtime_error(detail);
}
struct Scratch {
    fs::path root = fs::path{PGP_STUDIO_SCRATCH_PATH} /
        ("library-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        if (!fs::create_directory(root)) throw std::runtime_error("Studio scratch already exists");
    }
    ~Scratch() { std::error_code error; fs::remove_all(root, error); }
};
void put(const fs::path& file, std::string_view bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary);
    output << bytes;
    output.close();
    if (!output) throw std::runtime_error("Cannot write the fixture");
}
void marker(const fs::path& root, int schema) {
    put(root / "project-versions.json",
        "{\"manifest_format\":1,\"project_id\":\"project-glowing-pancake\",\"compatibility\":{\"content_schema\":" +
        std::to_string(schema) + "}}");
}
const studio::LibraryDocument& document(const studio::Library& library, const fs::path& path) {
    const auto found = std::find_if(library.documents.begin(), library.documents.end(),
        [&](const auto& entry) { return entry.relative_path == path; });
    if (found == library.documents.end()) throw std::runtime_error("Missing expected library document");
    return *found;
}
const studio::DefinitionEntry& definition(const studio::LibraryDocument& data, std::string_view id) {
    const auto found = std::find_if(data.definitions.begin(), data.definitions.end(),
        [&](const auto& entry) { return entry.id == id; });
    if (found == data.definitions.end()) throw std::runtime_error("Missing expected definition");
    return *found;
}
bool includes(const std::vector<std::string>& values, std::string_view target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}
template<class Action> void rejected(Action action, const char* detail) {
    try { action(); }
    catch (const std::invalid_argument&) { require(true, detail); return; }
    throw std::runtime_error(detail);
}
void inspect_source_and_drafts() {
    Scratch scratch;
    const auto root = scratch.root / "Author checkout";
    marker(root, 1);
    rejected([&] { (void)studio::inspect_checkout(root); }, "A marker-only package must be refused");
    put(root / ".git", "gitdir: ../git-metadata/worktrees/fixture\n");
    put(root / "CMakeLists.txt", "# development fixture\n");
    put(root / "src/content/collection.cpp", "// source tree marker\n");
    const auto bytes = persistence::read_bounded(PGP_STUDIO_FIXTURE_PATH);
    put(root / "data/runtime/current.json", bytes);
    put(root / "data/drafts/working draft.json", bytes + "\n");
    put(root / "data/drafts/broken.json", "{");
    put(root / "data/drafts/future.json", "{\"content_schema\":2,\"content_version\":\"0.0.0-dev\","
        "\"currency_basis\":\"constant_2014_cents\",\"definitions\":[]}");
    put(root / "tests/content/fixtures/foundation.json", bytes);
    put(root / "unrelated.json", bytes);
    const auto outside = scratch.root / "outside.json";
    put(outside, bytes);
    std::error_code error;
    fs::create_symlink(outside, root / "data/drafts/escaped.json", error);
    const bool linked = !error;

    const auto opened = studio::inspect_checkout(root);
    require(opened.checkout == fs::canonical(root) && opened.checkout_schema == 1, "The selected project identity must be verified");
    require(opened.documents.size() == (linked ? 6U : 5U), "Only configured collection roots may appear in the library");
    require(std::is_sorted(opened.documents.begin(), opened.documents.end(), [](const auto& a, const auto& b) {
        return a.relative_path.generic_u8string() < b.relative_path.generic_u8string();
    }), "Stable library order must be independent of directory iteration");
    const auto& current = document(opened, "data/runtime/current.json");
    const auto& draft = document(opened, "data/drafts/working draft.json");
    require(current.origin == studio::Origin::reviewed && draft.origin == studio::Origin::draft &&
            current.validation == studio::Validation::validated && draft.validation == studio::Validation::validated,
            "Reviewed and draft copies must be separately labeled and share the compiler");
    require(current.source_hash != draft.source_hash && current.canonical_hash == draft.canonical_hash &&
            current.content_version == "0.0.0-dev", "Source fingerprints must distinguish a nonsemantic external edit");
    const auto& scenario = definition(current, "scenario.foundation");
    const auto& offer = definition(current, "flow.supplier.exclusivity_offer");
    const auto& deal = definition(current, "deal.compute_exclusivity");
    require(scenario.kind == "scenario" && scenario.revision == 1 && includes(scenario.uses, offer.id) &&
            includes(offer.used_by, scenario.id) && includes(offer.uses, deal.id) && includes(deal.used_by, offer.id),
            "Reference edges must follow the validated typed content in both directions");
    require(document(opened, "data/drafts/broken.json").validation == studio::Validation::invalid &&
            document(opened, "data/drafts/future.json").validation == studio::Validation::unsupported &&
            document(opened, "tests/content/fixtures/foundation.json").origin == studio::Origin::engineering_fixture,
            "Broken, future and engineering documents must stay visible and separate");
    if (linked) {
        const auto& blocked = document(opened, "data/drafts/escaped.json");
        require(blocked.validation == studio::Validation::blocked && blocked.source_hash.empty() && blocked.definitions.empty(),
                "The library must not open a symlink escaping the selected checkout");
    } else std::cout << "Skipped symlink fixture: " << error.message() << '\n';
    require(persistence::read_bounded(root / "data/drafts/working draft.json") == bytes + "\n",
            "Opening the library must preserve exact authored bytes");
    marker(root, 2);
    const auto future = studio::inspect_checkout(root);
    require(future.checkout_schema == 2 && document(future, "data/runtime/current.json").validation == studio::Validation::unsupported &&
            document(future, "data/runtime/current.json").source_hash.empty(),
            "Unknown checkout schema must list documents without opening them");
    rejected([&] { (void)studio::inspect_checkout("relative checkout"); }, "A checkout must be selected explicitly by absolute path");
}
} // namespace

int main() {
    try {
        inspect_source_and_drafts();
        std::cout << checks << " Studio library checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
