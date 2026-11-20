#include "world_controller.hpp"
#include <pgp/runtime/career.hpp>
#include <algorithm>

namespace pgp::studio {
namespace {
QString text(std::string_view v) {return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size()));}
std::string path_parent(std::string_view path) {return std::string(path.substr(0,path.rfind('/')));}
std::string record_path(const CatalogEntry& entry) {return entry.family=="text" || entry.family=="owner"?entry.path:path_parent(entry.path);}
std::string short_name(std::string_view id) {
    const auto dot=id.find('.');
    return readable_name(dot==std::string_view::npos?id:id.substr(dot+1));
}
QString category_name(std::string_view family) {
    if(family=="research") return QStringLiteral("Research topic");
    if(family=="recipe") return QStringLiteral("AI model recipe");
    if(family=="evaluation") return QStringLiteral("Evaluation benchmark");
    if(family=="dataset") return QStringLiteral("Dataset");
    if(family=="hardware") return QStringLiteral("Hardware");
    if(family=="state") return QStringLiteral("US state");
    if(family=="protection") return QStringLiteral("Site protection");
    if(family=="insurance") return QStringLiteral("Insurance plan");
    if(family=="product") return QStringLiteral("Product / service");
    if(family=="segment") return QStringLiteral("Market segment");
    if(family=="region") return QStringLiteral("World region");
    if(family=="contract") return QStringLiteral("Customer contract");
    if(family=="wholesale") return QStringLiteral("Wholesale offer");
    if(family=="company") return QStringLiteral("Rival company");
    if(family=="executive") return QStringLiteral("Executive");
    if(family=="specialist") return QStringLiteral("Specialist role");
    if(family=="board") return QStringLiteral("Board seat");
    if(family=="leadership_track") return QStringLiteral("Succession track");
    if(family=="institution_role") return QStringLiteral("Institutional role");
    if(family=="business_offer") return QStringLiteral("Business offer");
    if(family=="financing" || family=="corporate_financing") return QStringLiteral("Financing round");
    if(family=="funding_support") return QStringLiteral("Funding support");
    if(family=="deal") return QStringLiteral("Supplier deal");
    if(family=="setback") return QStringLiteral("Project setback");
    if(family=="variable") return QStringLiteral("Story variable");
    if(family=="flow") return QStringLiteral("Event flow");
    if(family=="text") return QStringLiteral("Localized text");
    return text(readable_name(family));
}
bool matches_family_filter(std::string_view family,std::string_view filter,bool search_empty) {
    if(filter.empty()) return !search_empty || (family!="text" && family!="flow");
    if(filter=="research") return family=="research";
    if(filter=="models") return family=="recipe" || family=="evaluation";
    if(filter=="datasets") return family=="dataset";
    if(filter=="infrastructure") return family=="hardware" || family=="state" || family=="protection" || family=="insurance";
    if(filter=="markets") return family=="product" || family=="segment" || family=="region" || family=="contract" || family=="wholesale";
    if(filter=="rivals") return family=="company";
    if(filter=="people") return family=="executive" || family=="specialist" || family=="board" || family=="leadership_track" || family=="institution_role";
    if(filter=="offers") return family=="business_offer" || family=="financing" || family=="corporate_financing" || family=="funding_support" || family=="deal" || family=="setback";
    if(filter=="events") return family=="variable" || family=="flow";
    if(filter=="text") return family=="text";
    return true;
}
std::string human_use_location(std::string_view owner,std::string_view path) {
    const std::string prefix="/definitions/"+std::string(owner)+"/";
    if(path.starts_with(prefix)) path.remove_prefix(prefix.size());
    else if(path.starts_with('/')) path.remove_prefix(1);
    std::string out;
    std::size_t pos=0;
    while(pos<path.size()) {
        const auto next=path.find('/',pos);
        const auto part=path.substr(pos,next==std::string_view::npos?std::string_view::npos:next-pos);
        pos=next==std::string_view::npos?path.size():next+1;
        const bool numeric=!part.empty() && std::all_of(part.begin(),part.end(),[](unsigned char c){return std::isdigit(c)!=0;});
        if(part.empty() || numeric) continue;
        if(!out.empty()) out+=" · ";
        out+=readable_name(part);
    }
    return out.empty()?std::string("Reference"):out;
}
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
    const auto filter_str=family_filter_.toStdString();
    for(const auto& entry:index.entries) {
        const auto record=record_path(entry);
        const auto* owner=draft_->collection().find(entry.owner);
        std::string clean_title=entry.title;
        if(entry.family=="flow" && owner) clean_title=event_name(draft_->collection(),*owner);
        else if(entry.family=="text") clean_title=entry.id;
        else if(clean_title.empty() || clean_title==entry.id) clean_title=short_name(entry.id);
        const auto* scenario = owner ? std::get_if<content::ScenarioDefinition>(&owner->data) : nullptr;
        if (scenario && scenario->founding && scenario->founding->generation) {
            if (entry.family == "executive" && scenario->management && scenario->management->governance) {
                for (const auto& role : scenario->management->governance->executives)
                    if (role.id == entry.id) clean_title = role.role;
            }
            if (entry.family == "company") {
                for (const auto& rival : scenario->rivals)
                    if (rival.id == entry.id && rival.planning && !runtime::career_entry_definition(rival))
                        clean_title = readable_name(rival.planning->group) + " · " + short_name(rival.id.substr(rival.id.rfind('.')+1));
            }
        }
        if(!target_family.empty() && entry.family==target_family && entry.path.ends_with("/id")) {
            const auto owner_title=owner?event_name(draft_->collection(),*owner):short_name(entry.owner);
            catalog_templates_.push_back(QVariantMap{{"value",text(record)},{"label",text(clean_title+" · "+owner_title)}});
        }
        const bool global=owner && !std::holds_alternative<content::ScenarioDefinition>(owner->data);
        if((entry.owner!=scenario_ && !global && entry.family!="text") || entry.family=="scenario" ||
            entry.family=="owner" || entry.family=="project") continue;
        const auto category=category_name(entry.family);
        const bool deprecated=owner && owner->deprecated;
        const auto qtitle=text(clean_title);
        const auto label=qtitle+QStringLiteral(" · ")+category+(deprecated?QStringLiteral(" · Deprecated"):QString{});
        const auto search_blob=text(entry.family+" "+entry.title+" "+entry.id+" ")+category+QStringLiteral(" ")+qtitle;
        if(matches_family_filter(entry.family,filter_str,search_.empty()) &&
            (search_.empty() || search_blob.contains(text(search_),Qt::CaseInsensitive))) {
            catalog_entries_.push_back(QVariantMap{{"key",text(record)},{"family",text(entry.family)},{"category",category},
                {"title",qtitle},{"id",text(entry.id)},{"deprecated",deprecated},{"label",label}});
        }
        const auto selected_depth=family_=="text" || family_=="owner"?identity_path_.size():path_parent(identity_path_).size();
        if((path_==record || path_.starts_with(record+"/")) && record.size()>selected_depth) {
            identity_=entry.id;family_=entry.family;identity_path_=entry.path;
            if (path_ == record && (family_ == "company" || family_ == "executive")) section_title_ = label;
        }
    }
    if(!identity_.empty()) for(const auto& use:content_uses(index,family_,identity_,true)) {
        const auto* use_owner=draft_->collection().find(use.owner);
        const auto owner_name=use_owner?event_name(draft_->collection(),*use_owner):readable_name(use.owner);
        reference_uses_.push_back(QVariantMap{{"owner",text(use.owner)},{"path",text(use.path)},{"family",text(use.family)},
            {"summary",text(owner_name+" · "+human_use_location(use.owner,use.path))}});
    }
    if(previous_identity!=identity_path_) {
        rename_plan_.reset();
        rename_changes_.clear();
    }
}
void WorldController::setFamilyFilter(QString filter) {
    if(family_filter_==filter) return;
    family_filter_=std::move(filter);
    if(draft_ && catalog_mode_) {
        project_catalog();
        emit changed();
    } else emit changed();
}
bool WorldController::setCatalogMode(bool enabled) {
    if(enabled==catalog_mode_) return true;
    return perform([&] {
        if(hasStaged() || hasUnapplied() || adding_) throw std::invalid_argument("Apply or reset the current fields before switching authoring workspaces.");
        catalog_mode_=enabled;path_.clear();search_.clear();refresh();
        if(catalog_mode_ && !catalog_entries_.isEmpty() && (identity_.empty() || family_=="flow")) {
            path_=catalog_entries_.front().toMap().value(QStringLiteral("key")).toString().toStdString();
            refresh();
        }
    });
}
void WorldController::selectCatalogEntry(QString selected) {
    if(!draft_) return;
    perform([&] {
        if(hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply pending fields and staged sections before selecting another entry.");
        const auto& index=catalog();
        const auto found=std::find_if(index.entries.begin(),index.entries.end(),[&](const auto& e){return record_path(e)==selected.toStdString();});
        if(found==index.entries.end()) throw std::invalid_argument("Choose an entry in this project.");
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
        if(!draft_ || identity_path_.empty() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose an entry and apply pending fields first.");
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
        if(!draft_ || !catalog_mode_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose an entry list and apply pending fields first.");
        form_=WorldForm::append_catalog_from(*draft_,scenario_,path_,source.toStdString());adding_=true;project_form();emit changed();
    });
}
bool WorldController::removeIdentity() {
    return perform([&] {
        if(!draft_ || identity_path_.empty() || hasStaged() || hasUnapplied()) throw std::invalid_argument("Choose an entry and apply pending fields first.");
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
