#pragma once
// Developer walkthrough helper for the standard workforce start with Programming.
// Choose remaining options by ID so presentation order cannot change the fixture.
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
            const QString preferred=id=="grad_choice"?"workforce":id=="ms_clubs"?"coding":"";
            QString pick;
            for(const auto& optVal:choices) {
                const auto opt=optVal.toMap();
                if(!opt.value("available",true).toBool()) continue;
                const auto option=opt.value("id").toString();
                if(option==preferred) {pick=option;break;}
                if(pick.isEmpty() || option<pick) pick=option;
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
