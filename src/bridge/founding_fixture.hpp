#pragma once
// Developer walkthrough helper: follow the first available option through the
// actual controller, including chapters revealed by a previous choice.
#include "founding_controller.hpp"
#include <stdexcept>
namespace pgp::bridge {
inline void complete_formative_fixture(FoundingController& story) {
    for(int pass=0;pass<64;++pass) {
        bool changed=false;
        for(const auto& value:story.chapters()) {
            const auto chapter=value.toMap();const auto id=chapter.value("id").toString();
            if(story.form().value("choices").toMap().contains(id))continue;
            story.choose(id,chapter.value("choices").toList().front().toMap().value("id").toString());changed=true;
        }
        if(!changed)return;
    }
    throw std::runtime_error("Formative fixture exceeded the authored chapter bound.");
}
}
