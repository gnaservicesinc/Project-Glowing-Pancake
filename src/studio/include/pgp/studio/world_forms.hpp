#pragma once
#include <pgp/studio/flow_forms.hpp>

namespace pgp::studio {
struct WorldField {
    std::string key,label,value,help;
    FormControl control=FormControl::text;
    std::vector<FormOption> options;
};
struct WorldLocation {
    std::string id,label,path;
};
struct WorldOutlineRow {
    std::string key,label,search;
    std::size_t depth=0;
};
struct WorldSectionChange {
    std::string path;
    std::map<std::string,std::string,std::less<>> values;
};
// Projections of compiler-normalized, typed scenario data. A section is applied
// as one transaction so ownership totals and other coupled fields can change
// together. Paths and new-entry templates are bound to the document revision.
class WorldForm {
public:
    [[nodiscard]] static WorldForm open(const DraftDocument&,std::string_view scenario,std::string_view path);
    [[nodiscard]] static WorldForm append(const DraftDocument&,std::string_view scenario,std::string_view path,std::size_t template_index);
    [[nodiscard]] static WorldForm open_catalog(const DraftDocument&,std::string_view scenario,std::string_view path);
    [[nodiscard]] static WorldForm append_catalog(const DraftDocument&,std::string_view scenario,std::string_view path,std::size_t template_index);
    [[nodiscard]] static WorldForm append_catalog_from(const DraftDocument&,std::string_view scenario,std::string_view path,std::string_view source);
    [[nodiscard]] const std::vector<WorldField>& fields() const {return fields_;}
    [[nodiscard]] bool can_append() const {return can_append_;}
    [[nodiscard]] bool can_remove() const {return can_remove_;}
    [[nodiscard]] const std::vector<FormOption>& templates() const {return templates_;}
    bool apply(DraftDocument&,const std::map<std::string,std::string,std::less<>>&) const;
    [[nodiscard]] WorldSectionChange stage(const DraftDocument&,const std::map<std::string,std::string,std::less<>>&) const;
    bool remove(DraftDocument&) const;
private:
    friend bool apply_world_batch(DraftDocument&,std::string_view,const std::vector<WorldSectionChange>&,bool);
    [[nodiscard]] std::string proposal(const DraftDocument&,const std::map<std::string,std::string,std::less<>>&) const;
    WorldForm(const DraftDocument& d,std::string scenario,std::string path)
        :document_{&d},revision_{d.edit_revision()},scenario_{std::move(scenario)},path_{std::move(path)} {}
    void build(const DraftDocument&,bool adding);
    const DraftDocument* document_;
    std::uint64_t revision_;
    std::string scenario_,path_,prototype_;
    bool adding_=false,can_append_=false,can_remove_=false,catalog_=false;
    std::vector<WorldField> fields_;
    std::vector<FormOption> templates_;
};
bool apply_world_batch(DraftDocument&,std::string_view scenario,const std::vector<WorldSectionChange>&,bool catalog=false);
[[nodiscard]] std::vector<WorldOutlineRow> world_outline(const DraftDocument&,std::string_view scenario);
[[nodiscard]] std::vector<WorldOutlineRow> catalog_outline(const DraftDocument&,std::string_view scenario);
[[nodiscard]] std::vector<WorldLocation> world_states(const DraftDocument&,std::string_view scenario);
bool duplicate_scenario(DraftDocument&,std::string_view source,std::string_view new_id);
bool add_missing_scenario_sections(DraftDocument&,std::string_view target,std::string_view source);
// Creates and checks a real isolated opening, including exact save reload and
// registered fact values/trigger evaluations. It never changes the document.
[[nodiscard]] std::string preview_world(const DraftDocument&,std::string_view scenario,std::string_view headquarters);
} // namespace pgp::studio
