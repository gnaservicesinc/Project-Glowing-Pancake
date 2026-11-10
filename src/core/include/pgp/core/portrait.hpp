#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace pgp::core {
// Immutable kit v1 order: head, hair, nose, ears, mouth, chin, neck, eyes,
// brows, facial hair, glasses, skin, hair color, clothing color, background.
// IDs and their meanings are append-only within a kit version.
inline constexpr std::array<std::uint8_t,15> portrait_part_counts{6,9,5,3,5,3,3,4,4,4,4,8,8,6,6};
inline constexpr std::array<std::uint8_t,8> portrait_style_counts{10,3,4,4,8,6,6,6};
struct PortraitRecipe {
    std::uint16_t kit=1;
    std::array<std::uint8_t,15> parts{};
    std::array<std::uint8_t,8> style{};
    auto operator<=>(const PortraitRecipe&) const = default;
};
[[nodiscard]] PortraitRecipe make_portrait(std::uint64_t seed,std::string_view identity);
void validate_portrait(const PortraitRecipe&);
}
