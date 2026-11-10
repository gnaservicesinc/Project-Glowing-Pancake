#include <pgp/core/portrait.hpp>
#include <pgp/core/random.hpp>
#include <stdexcept>

namespace pgp::core {
PortraitRecipe make_portrait(std::uint64_t seed,std::string_view identity) {
    // A private visual stream: appearance never consumes hiring or simulation RNG.
    std::uint64_t hash=UINT64_C(14695981039346656037);
    for(const char byte:identity) {hash^=static_cast<unsigned char>(byte);hash*=UINT64_C(1099511628211);}
    SplitMix64 rng{seed^hash^UINT64_C(0x504f525452414954)};
    PortraitRecipe result;
    for(std::size_t i=0;i<result.parts.size();++i) result.parts[i]=static_cast<std::uint8_t>(rng.bounded(portrait_part_counts[i]));
    for(std::size_t i=0;i<result.style.size();++i) result.style[i]=static_cast<std::uint8_t>(rng.bounded(portrait_style_counts[i]));
    if(rng.bounded(3)!=0) result.parts[9]=0;
    if(rng.bounded(3)!=0) result.parts[10]=0;
    return result;
}
void validate_portrait(const PortraitRecipe& recipe) {
    if(recipe.kit!=1) throw std::invalid_argument("Unsupported employee portrait kit. Preserve this save for a compatible game version.");
    for(std::size_t i=0;i<recipe.style.size();++i) if(recipe.style[i]>=portrait_style_counts[i]) throw std::invalid_argument("Unknown portrait styling.");
    for(std::size_t i=0;i<recipe.parts.size();++i)
        if(recipe.parts[i]>=portrait_part_counts[i]) throw std::invalid_argument("Employee portrait contains an unknown layer ID.");
}
}
