#pragma once
#include <pgp/studio/draft.hpp>

namespace pgp::studio {
struct CatalogEntry {
    std::string family,id,title,owner,path;
    bool identity_editable=true;
};
struct ContentReference {
    std::string family,id,owner,path;
};
struct CatalogIndex {
    std::vector<CatalogEntry> entries;
    std::vector<ContentReference> references;
};
[[nodiscard]] CatalogIndex catalog_index(const DraftDocument&);
[[nodiscard]] CatalogIndex catalog_index(const runtime::CompiledProgram&);
[[nodiscard]] std::string catalog_list_family(std::string_view path);
[[nodiscard]] std::vector<ContentReference> content_uses(const CatalogIndex&,std::string_view family,std::string_view id,bool transitive=false);
struct ReferenceChange {
    std::string owner,path,before,after;
    bool declaration=false;
};
// A reviewed, revision-bound family rename. Descriptive strings, provenance
// and unrelated identifier families are never treated as references.
class RefactorPlan {
public:
    [[nodiscard]] static RefactorPlan rename(const DraftDocument&,std::string_view family,std::string_view before,std::string_view after);
    [[nodiscard]] const std::vector<ReferenceChange>& changes() const {return changes_;}
    bool apply(DraftDocument&) const;
private:
    const DraftDocument* document_=nullptr;
    std::uint64_t revision_=0;
    std::string candidate_;
    std::vector<ReferenceChange> changes_;
};
bool duplicate_catalog_definition(DraftDocument&,std::string_view source,std::string_view new_id);
bool remove_catalog_entry(DraftDocument&,std::string_view path);
bool edit_message(DraftDocument&,std::string_view key,std::string_view locale,std::string value);
} // namespace pgp::studio
