#include <pgp/studio/draft.hpp>

#include "checkout.hpp"

#include <pgp/persistence/sha256.hpp>
#include <pgp/persistence/storage.hpp>

#include <nlohmann/json.hpp>

namespace pgp::studio {
namespace {
namespace fs = std::filesystem;
using persistence::PersistenceError;
constexpr std::size_t maximum_undo = 16;
const fs::path drafts{"data/drafts"};
bool below(const fs::path& path, const fs::path& folder) {
    const auto relative = path.lexically_relative(folder);
    return !relative.empty() && relative != "." && *relative.begin() != "..";
}
std::string read_source(const fs::path& root, const fs::path& relative) {
    if (!fs::is_regular_file(detail::checked_path(root, relative)))
        throw PersistenceError("conflict", "The working source was removed or replaced.");
    return persistence::read_bounded(root / relative, content::maximum_document_bytes);
}
std::string marker_hash(const fs::path& root) {
    constexpr auto marker = "project-versions.json";
    if (!fs::is_regular_file(detail::checked_path(root, marker)))
        throw PersistenceError("conflict", "The selected checkout no longer has its project marker.");
    return persistence::sha256(persistence::read_bounded(root / marker, 16384));
}
}

DraftDocument::DraftDocument(const fs::path& checkout, const fs::path& source)
    : checkout_{detail::checkout_root(checkout)}, base_{source} {
    if (detail::checkout_schema(checkout_) != content::content_schema)
        throw PersistenceError("unsupported_schema", "The checkout is read-only for this Studio build.");
    if (source.extension() != ".json" || (!below(source, drafts) && !below(source, "data/runtime") &&
        !below(source, "tests/content/fixtures")))
        throw PersistenceError("path", "Choose a collection from the selected checkout library.");
    marker_hash_ = marker_hash(checkout_);
    const auto bytes = read_source(checkout_, base_);
    base_hash_ = persistence::sha256(bytes);
    program_ = detail::compile_source(bytes, content::foundation_registry());
    saved_semantics_ = program_.canonical_content_bytes;
}

bool DraftDocument::is_draft() const { return below(base_, drafts); }

bool DraftDocument::replace(const content::Collection& candidate) {
    if (candidate.content_version != program_.collection.content_version)
        throw PersistenceError("version", "Draft edits retain the content version.");
    for (const auto& [id, definition] : candidate.definitions) {
        const auto* prior = program_.collection.find(id);
        if (prior && prior->revision != definition.revision)
            throw PersistenceError("version", "Draft edits retain existing definition revisions.");
    }
    auto next = runtime::compile_collection(candidate, content::foundation_registry(), runtime::supported_effects());
    if (next.canonical_content_bytes == program_.canonical_content_bytes) return false;
    undo_.push_back(program_.canonical_content_bytes);
    if (undo_.size() > maximum_undo) undo_.pop_front();
    redo_.clear();
    program_ = std::move(next);
    ++edit_revision_;
    return true;
}

bool DraftDocument::restore(std::deque<std::string>& from, std::deque<std::string>& to) {
    if (from.empty()) return false;
    auto restored = detail::compile_source(from.back(), content::foundation_registry());
    to.push_back(program_.canonical_content_bytes);
    from.pop_back();
    program_ = std::move(restored);
    ++edit_revision_;
    return true;
}
bool DraftDocument::undo() { return restore(undo_, redo_); }
bool DraftDocument::redo() { return restore(redo_, undo_); }

void DraftDocument::save(const fs::path& new_name) {
    if (fs::exists(detail::checked_path(checkout_,".pgp-editor/snapshot-transaction")))
        throw PersistenceError("pending_snapshot","Recover the pending content snapshot before saving drafts.");
    const bool replace_current = new_name.empty();
    if (replace_current && !is_draft())
        throw PersistenceError("path", "Give this copy a new draft filename.");
    if (!replace_current && (new_name != new_name.filename() || new_name.extension() != ".json" ||
        !content::valid_id(new_name.stem().string())))
        throw PersistenceError("path", "Choose a lowercase draft basename ending in .json.");
    auto destination = replace_current ? base_ : drafts / new_name;
    (void)detail::checked_path(checkout_, destination);
    if (marker_hash(checkout_) != marker_hash_ ||
        persistence::sha256(read_source(checkout_, base_)) != base_hash_)
        throw PersistenceError("conflict", "The checkout or source changed externally. Reopen it before saving.");
    if (replace_current && !dirty()) return;
    auto semantics = program_.canonical_content_bytes;
    auto bytes = nlohmann::json::parse(semantics).dump(2) + "\n";
    if (bytes.size() > content::maximum_document_bytes) bytes = semantics;
    auto hash = persistence::sha256(bytes);
    (void)detail::checked_path(checkout_, destination);
    persistence::atomic_write(checkout_ / destination, bytes, [&](std::string_view previous) {
        if (persistence::sha256(previous) != base_hash_)
            throw PersistenceError("conflict", "The draft changed while the replacement was being prepared.");
    }, content::maximum_document_bytes, replace_current);
    base_ = std::move(destination);
    base_hash_ = std::move(hash);
    saved_semantics_ = std::move(semantics);
}
} // namespace pgp::studio
