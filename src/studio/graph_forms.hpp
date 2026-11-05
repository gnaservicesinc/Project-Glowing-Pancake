#pragma once

#include "form_builder.hpp"

namespace pgp::studio::detail {
using LinkAccess = std::function<std::string&(FlowDefinition&)>;
using EffectAccess = std::function<Effect&(FlowDefinition&)>;

void add_link_fields(FormBuilder& builder, std::string key, std::string label, const std::string& target, LinkAccess access);
void append_execution_outline(std::vector<OutlineRow>& rows, const DraftDocument& draft, const FlowDefinition& flow);
void build_execution_form(FormBuilder& builder, std::string_view row);
[[nodiscard]] std::vector<FormOption> effect_choices(const DraftDocument& draft, Scope scope);
[[nodiscard]] Effect new_effect(const DraftDocument& draft, Scope scope, std::string_view operation);
void build_effect_fields(FormBuilder& builder, const Effect& effect, EffectAccess access);
} // namespace pgp::studio::detail
