#pragma once

#include <pgp/runtime/runtime.hpp>
#include <QString>

namespace pgp::bridge {
[[nodiscard]] QString cu(std::int64_t amount);
[[nodiscard]] QString readable(std::string_view source);
[[nodiscard]] QString choice_label(const runtime::PresentedChoice& choice);
[[nodiscard]] QString presentation_title(runtime::PresentationKind kind);
[[nodiscard]] QString presentation_context(const runtime::Checkpoint& checkpoint, const runtime::Presentation& row);
[[nodiscard]] QString presentation_detail(const runtime::Checkpoint& checkpoint, const runtime::Presentation& row);
} // namespace pgp::bridge
