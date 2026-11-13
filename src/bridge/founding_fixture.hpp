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
            const auto choices=chapter.value("choices").toList();
            if(choices.isEmpty())continue;
            QString pick;
            for(const auto& optVal:choices) {
                const auto opt=optVal.toMap();
                if(opt.value("available",true).toBool()){pick=opt.value("id").toString();break;}
            }
            if(pick.isEmpty()) pick=choices.front().toMap().value("id").toString();
            story.choose(id,pick);changed=true;
        }
        if(!changed)return;
    }
    QStringList unresolved;
    for(const auto& value:story.chapters()) {
        const auto id=value.toMap().value("id").toString();
        if(!story.form().value("choices").toMap().contains(id)) unresolved.push_back(id);
    }
    throw std::runtime_error("Formative fixture exceeded the authored chapter bound: "+unresolved.join(',').toStdString()+" (choices="+QString::number(story.form().value("choices").toMap().size()).toStdString()+")");
}
}
