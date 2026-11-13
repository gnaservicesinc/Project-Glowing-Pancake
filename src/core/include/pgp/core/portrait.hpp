#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace pgp::core {
// Immutable kit v1 order: head, hair, nose, ears, mouth, chin, neck, eyes,
// brows, facial hair, glasses, skin, hair color, clothing color, background.
// IDs and their meanings are append-only within a kit version.
inline constexpr std::array<std::uint8_t,15> portrait_part_counts{6,9,5,3,5,3,3,4,4,4,4,8,8,6,6};
inline constexpr std::array<std::uint8_t,12> portrait_style_counts{10,3,4,4,8,6,6,6,8,8,8,8};
enum class LifeStage : std::uint8_t { baby, toddler, kid, teen, adult, mature, senior, elder };
[[nodiscard]] constexpr LifeStage life_stage_for_age(int age) noexcept {
    if(age<3) return LifeStage::baby;
    if(age<6) return LifeStage::toddler;
    if(age<13) return LifeStage::kid;
    if(age<20) return LifeStage::teen;
    if(age<40) return LifeStage::adult;
    if(age<55) return LifeStage::mature;
    if(age<75) return LifeStage::senior;
    return LifeStage::elder;
}
[[nodiscard]] constexpr std::string_view life_stage_key(LifeStage stage) noexcept {
    switch(stage) {
        case LifeStage::baby: return "baby";
        case LifeStage::toddler: return "toddler";
        case LifeStage::kid: return "kid";
        case LifeStage::teen: return "teen";
        case LifeStage::adult: return "adult";
        case LifeStage::mature: return "mature";
        case LifeStage::senior: return "senior";
        case LifeStage::elder: return "elder";
    }
    return "adult";
}
[[nodiscard]] constexpr bool try_parse_life_stage(std::string_view key,LifeStage& out) noexcept {
    struct Entry { std::string_view key; LifeStage stage; };
    constexpr std::array<Entry,9> table{{
        {"baby",LifeStage::baby},{"toddler",LifeStage::toddler},{"kid",LifeStage::kid},
        {"teen",LifeStage::teen},{"adult",LifeStage::adult},{"mature",LifeStage::mature},
        {"senior",LifeStage::senior},{"old",LifeStage::senior},{"elder",LifeStage::elder}
    }};
    for(const auto& e:table) if(e.key==key) { out=e.stage; return true; }
    return false;
}
struct PortraitRecipe {
    std::uint16_t kit=1;
    std::array<std::uint8_t,15> parts{};
    std::array<std::uint8_t,12> style{};
    auto operator<=>(const PortraitRecipe&) const = default;
};
[[nodiscard]] PortraitRecipe make_portrait(std::uint64_t seed,std::string_view identity);
void validate_portrait(const PortraitRecipe&);
}
