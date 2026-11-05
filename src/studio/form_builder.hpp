#pragma once

#include <pgp/studio/flow_forms.hpp>

namespace pgp::studio::detail {
using namespace pgp::content;

[[nodiscard]] const FlowDefinition& find_flow(const DraftDocument& draft, std::string_view id);
[[nodiscard]] std::string scalar_text(const Scalar& value);
[[nodiscard]] std::string unit_label(Unit unit);
[[nodiscard]] Scalar parse_scalar(ValueType type, std::string_view value);
[[nodiscard]] std::uint32_t parse_count(std::string_view value, std::uint32_t minimum, std::uint32_t maximum);
[[nodiscard]] std::vector<FormOption> boolean_choices();
[[nodiscard]] std::vector<FormOption> fact_choices(const Registry& registry, Scope scope);
[[nodiscard]] Scalar default_scalar(const FactMetadata& fact);
[[nodiscard]] Condition default_condition(const Registry& registry, Scope scope);
[[nodiscard]] std::string_view node_label(NodeKind kind);
[[nodiscard]] std::string_view lifecycle_label(LifecycleMode mode);
[[nodiscard]] std::vector<FormOption> node_choices(const FlowDefinition& flow);
[[nodiscard]] std::string available_node_id(const FlowDefinition& flow, std::string_view base);
[[nodiscard]] bool seeded_root(const FlowDefinition& flow);

class FormBuilder {
public:
    using Change = std::function<void(FlowDefinition&, std::string_view)>;
    using DocumentChange = std::function<void(Collection&, FlowDefinition&, std::string_view)>;
    FormBuilder(const DraftDocument& document, std::string_view flow_id)
        : draft{document}, id{flow_id}, flow{find_flow(document, flow_id)}, form_{document, flow_id} {}
    void add(std::string key, std::string label, std::string value, std::string help,
             FormControl control, Change change = {}, std::vector<FormOption> options = {});
    void add_document(std::string key, std::string label, std::string value, std::string help,
                      FormControl control, DocumentChange change, std::vector<FormOption> options = {});
    void note(std::string label, std::string value, std::string help = {});
    void action(std::string key, std::string label, std::string help, Change change);
    [[nodiscard]] FlowForm finish() && { return std::move(form_); }

    const DraftDocument& draft;
    const std::string id;
    const FlowDefinition& flow;
private:
    FlowForm form_;
};
void append_trigger_outline(std::vector<OutlineRow>& rows, const DraftDocument& draft, const FlowDefinition& flow);
bool build_trigger_form(FormBuilder& builder, std::string_view row);
void build_flow_settings(FormBuilder& builder);
void build_lifecycle_form(FormBuilder& builder);

// Used by both comparison operands and scoped variable effects.
using ExpressionAccess = std::function<Expression&(FlowDefinition&)>;
void append_expression_outline(std::vector<OutlineRow>& rows, const Expression& expression,
                               std::string key, std::size_t depth);
bool build_expression_form(FormBuilder& builder, std::string_view row, const Expression& expression,
                           std::string_view key, const FactMetadata& type, ExpressionAccess access);
} // namespace pgp::studio::detail
