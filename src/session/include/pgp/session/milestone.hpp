#pragma once
#include <pgp/runtime/runtime.hpp>
namespace pgp::session {
struct Milestone {
    std::string kind;
    std::vector<std::string> projects;
    std::optional<core::OrdinalDay> deadline;
};
[[nodiscard]] Milestone plan_milestone(const runtime::Checkpoint& checkpoint,std::string_view kind);
[[nodiscard]] bool milestone_reached(const runtime::Checkpoint& checkpoint,const Milestone& milestone);
}
