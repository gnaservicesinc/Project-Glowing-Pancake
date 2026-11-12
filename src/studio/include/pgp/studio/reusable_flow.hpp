#pragma once

#include <pgp/studio/flow_forms.hpp>

namespace pgp::studio {
bool create_flow_group(DraftDocument&, std::string_view source, std::string_view id, std::string_view title = {});
bool insert_group_call(DraftDocument&, std::string_view caller, std::string_view before_node,
    std::string_view group, std::map<std::string,std::string,std::less<>> bindings);
[[nodiscard]] std::vector<FormOption> group_variable_options(const DraftDocument&, content::Scope,
    const content::GroupPort* contract = nullptr, const content::FlowDefinition* caller = nullptr);
} // namespace pgp::studio
