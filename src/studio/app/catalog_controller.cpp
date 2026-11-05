#include "world_controller.hpp"
#include <algorithm>

namespace pgp::studio {
namespace {
QString text(std::string_view v) {return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size()));}
std::string path_parent(std::string_view path) {return std::string(path.substr(0,path.rfind('/')));}
std::string record_path(const CatalogEntry& entry) {return entry.family=="text" || entry.family=="owner"?entry.path:path_parent(entry.path);}
}
const CatalogIndex& WorldController::catalog() {
    if(!draft_) throw std::invalid_argument("Open a working collection.");
    if(!catalog_index_ || catalog_revision_!=draft_->edit_revision()) {
        catalog_index_=catalog_index(*draft_);catalog_revision_=draft_->edit_revision();
    }
    return *catalog_index_;
}
void WorldController::project_catalog() {
    const auto previous_identity=identity_path_;
    catalog_entries_.clear();catalog_templates_.clear();reference_uses_.clear();identity_.clear();family_.clear();identity_path_.clear();
    if(!draft_ || !catalog_mode_) {
        rename_plan_.reset();
        rename_changes_.clear();
        return;
    }
    const auto& index=catalog();
    const auto target_family=catalog_list_family(path_);
    for(const auto& entry:index.entries) {
        const auto record=record_path(entry);
        if(!target_family.empty() && entry.family==target_family && entry.path.ends_with("/id"))
            catalog_templates_.push_back(QVariantMap{{"value",text(record)},{"label",text(entry.title+" · "+entry.id+" · "+entry.owner)}});
        const auto* owner=draft_->collection().find(entry.owner);
        const bool global=owner && !std::holds_alternative<content::ScenarioDefinition>(owner->data);
        if((entry.owner!=scenario_ && !global && entry.family!="text") || entry.family=="scenario" ||
            entry.family=="owner" || entry.family=="project") continue;
        const auto label=entry.family+" · "+entry.title+" · "+entry.id+(owner && owner->deprecated?" · Deprecated":"");
        if(search_.empty() || text(label).contains(text(search_),Qt::CaseInsensitive))
            catalog_entries_.push_back(QVariantMap{{"key",text(record)},{"family",text(entry.family)},{"label",text(label)}});
        const auto selected_depth=family_=="text" || family_=="owner"?identity_path_.size():path_parent(identity_path_).size();
        if((path_==record || path_.starts_with(record+"/")) && record.size()>selected_depth) {
            identity_=entry.id;family_=entry.family;identity_path_=entry.path;
        }
    }
    if(!identity_.empty()) for(const auto& use:content_uses(index,family_,identity_,true))
        reference_uses_.push_back(QVariantMap{{"owner",text(use.owner)},{"path",text(use.path)},{"family",text(use.family)}});
    if(previous_identity!=identity_path_) {
        rename_plan_.reset();
        rename_changes_.clear();
    }
}
bool WorldController::setCatalogMode(bool enabled) {
    if(enabled==catalog_mode_) return true;
    return perform([&] {
        if(hasStaged() || hasUnapplied() || adding_) throw std::invalid_argument("Apply or reset the current fields before switching authoring workspaces.");
        catalog_mode_=enabled;path_.clear();search_.clear();refresh();
    });
}
void WorldController::selectCatalogEntry(QString selected) {
    if(!draft_) return;
    perform([&] {
        if(hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply pending fields and staged sections before selecting another catalog entry.");
        const auto& index=catalog();
        const auto found=std::find_if(index.entries.begin(),index.entries.end(),[&](const auto& e){return record_path(e)==selected.toStdString();});
        if(found==index.entries.end()) throw std::invalid_argument("Choose a current catalog entry.");
        (void)WorldForm::open_catalog(*draft_,scenario_,selected.toStdString());
        catalog_mode_=true;path_=selected.toStdString();search_.clear();adding_=false;refresh();
    });
}
void WorldController::openSelectedFlow() {
    if(family_=="flow" && !hasStaged() && !hasUnapplied()) emit flowRequested(text(identity_));
}
bool WorldController::reviewRename(QString target) {
    return perform([&] {
        if(!draft_ || identity_.empty() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose an identity and apply pending fields before reviewing references.");
        auto plan=RefactorPlan::rename(*draft_,family_,identity_,target.toStdString());
        rename_target_=target.toStdString();rename_changes_.clear();
        for(const auto& change:plan.changes()) rename_changes_.push_back(QVariantMap{{"owner",text(change.owner)},{"path",text(change.path)},
            {"before",text(change.before)},{"after",text(change.after)},{"declaration",change.declaration}});
        rename_plan_=std::move(plan);emit changed();
    });
}
bool WorldController::applyRename() {
    return perform([&] {
        if(!draft_ || !rename_plan_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Review a current reference edit first.");
        const auto old=identity_,type=family_,next=rename_target_;
        const auto changed=rename_plan_->apply(*draft_);rename_plan_.reset();rename_changes_.clear();
        if(changed) {
            if(type=="scenario") scenario_=next;
            const auto& index=catalog();
            const auto original_path=identity_path_;
            const auto owning_prefix=original_path.starts_with("/definitions/") ?
                original_path.substr(0,original_path.find('/',13)) : path_parent(original_path);
            const auto matching=std::find_if(index.entries.begin(),index.entries.end(),[&](const auto& entry) {
                return entry.family==type && entry.id==next &&
                    (entry.path.starts_with(owning_prefix+"/") || entry.path=="/definitions/"+next+"/id");
            });
            if(matching!=index.entries.end()) path_=record_path(*matching);
            emit identityRenamed(text(type),text(old),text(next));emit documentEdited();
        }
    });
}
bool WorldController::cloneCatalogEntry(QString id) {
    return perform([&] {
        if(!draft_ || identity_path_.empty() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose a catalog entry and apply pending fields first.");
        const auto record=path_parent(identity_path_);
        if(path_parent(record)=="/definitions") {
            if(duplicate_catalog_definition(*draft_,identity_,id.toStdString())) {path_="/definitions/"+id.toStdString();emit documentEdited();}
        } else {
            const auto list=path_parent(record);
            auto copy=WorldForm::append_catalog_from(*draft_,scenario_,list,record);
            if(copy.apply(*draft_,{{"id",id.toStdString()}})) {path_=list;emit documentEdited();}
        }
    });
}
bool WorldController::beginCatalogTemplate(QString source) {
    return perform([&] {
        if(!draft_ || !catalog_mode_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose a catalog list and apply pending fields first.");
        form_=WorldForm::append_catalog_from(*draft_,scenario_,path_,source.toStdString());adding_=true;project_form();emit changed();
    });
}
bool WorldController::removeIdentity() {
    return perform([&] {
        if(!draft_ || identity_path_.empty() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose a catalog identity and apply pending fields first.");
        if(remove_catalog_entry(*draft_,identity_path_)) {path_.clear();emit documentEdited();}
    });
}
bool WorldController::canDeprecate() const {
    return draft_ && !identity_.empty() && identity_path_=="/definitions/"+identity_+"/id";
}
bool WorldController::selectedDeprecated() const {
    return canDeprecate() && draft_->collection().definitions.at(identity_).deprecated;
}
bool WorldController::deprecateIdentity(bool deprecated) {
    return perform([&] {
        if(!canDeprecate() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Select a definition and apply pending fields first.");
        auto next=draft_->collection();
        next.definitions.at(identity_).deprecated=deprecated;
        if(draft_->replace(next)) emit documentEdited();
    });
}
void WorldController::followReference(QString owner, QString path) {
    perform([&] {
        if(!draft_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply pending fields before following a reference.");
        const auto id=owner.toStdString();
        const auto* definition=draft_->collection().find(id);
        if(!definition) throw std::invalid_argument("The reference owner is no longer available.");
        if(std::holds_alternative<content::FlowDefinition>(definition->data)) {emit flowRequested(owner);return;}
        const auto section=path_parent(path.toStdString());
        auto scenario=scenario_;
        const bool scenario_owner=std::holds_alternative<content::ScenarioDefinition>(definition->data);
        if(scenario_owner) scenario=id;
        try {
            (void)WorldForm::open_catalog(*draft_,scenario,section);
            catalog_mode_=true;
        } catch(const std::invalid_argument&) {
            (void)WorldForm::open(*draft_,scenario,section);
            catalog_mode_=false;
        }
        scenario_=std::move(scenario);
        path_=section;
        search_.clear();
        refresh();
    });
}
QString WorldController::messageValue(QString key,QString locale,bool fallback) const {
    if(!draft_) return {};
    const auto id=key.toStdString(),language=locale.toStdString();
    const auto& collection=draft_->collection();
    if(fallback || language=="en") {
        const auto* found=collection.message(id,language);
        return found?text(*found):QString{};
    }
    const auto table=collection.translations.find(language);
    if(table==collection.translations.end()) return {};
    const auto found=table->second.find(id);
    return found==table->second.end()?QString{}:text(found->second);
}
bool WorldController::saveMessage(QString key,QString locale,QString value) {
    return perform([&] {
        if(!draft_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply pending fields before editing localized text.");
        if(edit_message(*draft_,key.toStdString(),locale.toStdString(),value.toStdString())) {
            catalog_mode_=true;
            path_="/messages/"+key.toStdString();
            emit documentEdited();
        }
    });
}
} // namespace pgp::studio
