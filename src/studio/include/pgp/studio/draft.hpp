#pragma once

#include <pgp/runtime/compiler.hpp>

#include <deque>
#include <filesystem>

namespace pgp::studio {
class DraftDocument {
public:
    DraftDocument(const std::filesystem::path& checkout, const std::filesystem::path& source);
    [[nodiscard]] const content::Collection& collection() const noexcept { return program_.collection; }
    [[nodiscard]] const std::filesystem::path& relative_file() const noexcept { return base_; }
    [[nodiscard]] bool is_draft() const;
    [[nodiscard]] bool dirty() const noexcept { return program_.canonical_content_bytes != saved_semantics_; }
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }

    // All authoring surfaces propose typed document changes through this
    // model. Rejected changes leave the working document and history intact.
    bool replace(const content::Collection& candidate);
    bool undo();
    bool redo();

    // With no name, save the currently opened draft in place. Save As
    // accepts a new basename only and never adopts an existing file.
    void save(const std::filesystem::path& new_name = {});
private:
    bool restore(std::deque<std::string>& from, std::deque<std::string>& to);
    std::filesystem::path checkout_;
    std::filesystem::path base_;
    std::string base_hash_;
    std::string marker_hash_;
    std::string saved_semantics_;
    runtime::CompiledProgram program_;
    std::deque<std::string> undo_, redo_;
};
} // namespace pgp::studio
