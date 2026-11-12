#pragma once

#include <pgp/studio/draft.hpp>

#include <functional>

namespace pgp::studio {

enum class FormControl { note, text, paragraph, integer, choice, button };
struct FormOption {
    std::string value;
    std::string label;
    std::string help{};
};
struct OutlineRow {
    std::string key;
    std::string label;
    std::size_t depth = 0;
};
namespace detail { class FormBuilder; }
class FormField {
public:
    std::string key;
    std::string label;
    std::string value;
    std::string help;
    FormControl control = FormControl::note;
    std::vector<FormOption> options;
private:
    friend class detail::FormBuilder;
    friend class FlowForm;
    std::function<void(content::Collection&, content::FlowDefinition&, std::string_view)> change_;
};

// A revision-bound projection of a single working collection. Only compiled
// edits enter the document, its undo history or the saved draft.
class FlowForm {
public:
    [[nodiscard]] static FlowForm open(const DraftDocument& draft, std::string_view flow, std::string_view row);
    [[nodiscard]] const std::vector<FormField>& fields() const noexcept { return fields_; }
    bool apply(DraftDocument& draft, std::string_view field, std::string_view value) const;
private:
    friend class detail::FormBuilder;
    FlowForm(const DraftDocument& draft, std::string_view flow)
        : document_{&draft}, revision_{draft.edit_revision()}, flow_{flow} {}
    const DraftDocument* document_;
    std::uint64_t revision_;
    std::string flow_;
    std::vector<FormField> fields_;
};
[[nodiscard]] std::vector<OutlineRow> flow_outline(const DraftDocument& draft, std::string_view flow);
[[nodiscard]] std::vector<FormOption> flow_scope_options(const DraftDocument& draft);
[[nodiscard]] std::string readable_name(std::string_view name);
[[nodiscard]] std::string_view event_presentation(const content::FlowDefinition&);
[[nodiscard]] std::string event_name(const content::Collection&, const content::Definition&);
bool create_flow(DraftDocument& draft, std::string_view id, std::string_view scope,
                 std::string_view title = {}, std::string_view presentation = {}, std::string_view body = {});


} // namespace pgp::studio
