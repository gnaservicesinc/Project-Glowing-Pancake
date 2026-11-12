#include "world_controller.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace pgp::studio {
namespace {
QString text(std::string_view s) {return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QVariantMap option(const FormOption& o) {return {{"value",text(o.value)},{"label",text(o.label)},{"help",text(o.help)}};}
QString control(FormControl c) {
    switch(c) {
    case FormControl::note:return QStringLiteral("note");
    case FormControl::paragraph:return QStringLiteral("text");
    case FormControl::choice:return QStringLiteral("choice");
    case FormControl::integer:return QStringLiteral("integer");
    default:return QStringLiteral("text");
    }
}
}
WorldController::WorldController() {
    QFile file{QStringLiteral(":/pgp/studio/us-states-2024.json")};
    if(file.open(QIODevice::ReadOnly)) {
        const auto bytes=file.readAll();map_=QJsonDocument::fromJson(bytes).object().toVariantMap();
        map_hash_=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        map_credit_=map_.value(QStringLiteral("credit")).toString();
    }
}
QVariantMap WorldController::mapGeometry() const {
    if(!draft_ || scenario_.empty()) return {};
    const auto& s=std::get<content::ScenarioDefinition>(draft_->collection().definitions.at(scenario_).data);
    return s.infrastructure && text(s.infrastructure->map_sha256)==map_hash_ &&
        text(s.infrastructure->map_asset_id)==map_.value(QStringLiteral("asset_id")).toString()?map_:QVariantMap{};
}
QString WorldController::mapCredit() const {
    return mapGeometry().isEmpty()?QStringLiteral("The prepared map asset is unavailable or differs from this scenario. Use the state list."):map_credit_;
}
QString WorldController::scenario() const {return text(scenario_);}
QString WorldController::path() const {return text(path_);}
QString WorldController::stagedSummary() const {
    QStringList out;
    for(const auto& [path,values]:staged_) for(const auto& [key,value]:values)
        out.push_back(text(path+"/"+key)+" → "+text(value));
    return out.join(QLatin1Char('\n'));
}
QStringList WorldController::headquarters() const {
    QStringList out;if(!draft_ || scenario_.empty()) return out;
    const auto& s=std::get<content::ScenarioDefinition>(draft_->collection().definitions.at(scenario_).data);
    for(const auto& id:s.headquarters_choices) out.push_back(text(id));return out;
}
void WorldController::set_document(DraftDocument* d,std::string_view preferred) {
    const bool replaced=draft_!=d;
    const bool edited=replaced || (d && revision_!=d->edit_revision());
    if(!edited && (preferred.empty() || preferred==scenario_)) return;
    const auto prior_revision=revision_;
    if(!replaced && edited && hasUnapplied()) {
        auto& kept=staged_[path_];
        for(auto i=unapplied_.cbegin();i!=unapplied_.cend();++i) kept[i.key().toStdString()]=i.value().toString().toStdString();
        staged_revision_=prior_revision;setUnapplied({});
    }
    draft_=d;revision_=d?d->edit_revision():0;
    if(replaced) {staged_.clear();catalog_index_.reset();catalog_mode_=false;}
    if(edited) {rename_plan_.reset();rename_changes_.clear();}
    else if(edited && hasStaged()) message_=QStringLiteral("The document changed while these sections were staged. Their values are preserved; discard the stale batch and review the current document.");
    if(replaced) {setUnapplied({});path_.clear();search_.clear();scenario_.clear();selected_state_.clear();}
    if(!preferred.empty()) {scenario_=preferred;path_.clear();}
    if(edited) {report_.clear();adding_=false;}
    refresh();
}
void WorldController::refresh() {
    scenarios_.clear();sections_.clear();states_.clear();fields_.clear();templates_.clear();form_.reset();++ticket_;
    if(!draft_) {scenario_.clear();path_.clear();emit changed();return;}
    for(const auto& [id,d]:draft_->collection().definitions) if(std::holds_alternative<content::ScenarioDefinition>(d.data)) scenarios_.push_back(option({id,event_name(draft_->collection(),d),{}}));
    const auto* selected=draft_->collection().find(scenario_);
    if(!selected || !std::holds_alternative<content::ScenarioDefinition>(selected->data)) {
        scenario_=scenarios_.isEmpty()?std::string{}:scenarios_.front().toMap().value(QStringLiteral("value")).toString().toStdString();path_.clear();
    }
    if(scenario_.empty() && !catalog_mode_) {emit changed();return;}
    const auto rows=catalog_mode_?catalog_outline(*draft_,scenario_):world_outline(*draft_,scenario_);
    if(rows.empty()) {emit changed();return;}
    while(std::none_of(rows.begin(),rows.end(),[&](const auto& r){return r.key==path_;})) {
        const auto slash=path_.rfind('/');
        path_=slash==std::string::npos || slash<12?rows.front().key:path_.substr(0,slash);
    }
    for(const auto& row:rows) {
        if(!search_.empty() && !text(row.search).contains(text(search_),Qt::CaseInsensitive)) continue;
        sections_.push_back(QVariantMap{{"key",text(row.key)},{"label",QString(static_cast<qsizetype>(row.depth*2),QChar{0x00a0})+text(row.label)}});
    }
    if(!scenario_.empty()) for(const auto& row:world_states(*draft_,scenario_)) states_.push_back(QVariantMap{{"id",text(row.id)},{"label",text(row.label)},{"path",text(row.path)}});
    form_=catalog_mode_?WorldForm::open_catalog(*draft_,scenario_,path_):WorldForm::open(*draft_,scenario_,path_);
    project_form();project_catalog();emit changed();
}
void WorldController::project_form() {
    fields_.clear();templates_.clear();++ticket_;
    if(!form_) return;
    for(const auto& f:form_->fields()) {
        QVariantList options;for(const auto& o:f.options) options.push_back(option(o));
        auto value=f.value;
        if(const auto section=staged_.find(path_);section!=staged_.end())
            if(const auto proposed=section->second.find(f.key);proposed!=section->second.end()) value=proposed->second;
        fields_.push_back(QVariantMap{{"key",text(f.key)},{"label",text(f.label)},{"value",text(value)},{"before",text(f.value)},
            {"help",text(f.help)},{"control",control(f.control)},{"options",std::move(options)}});
    }
    for(const auto& o:form_->templates()) templates_.push_back(option(o));
}
void WorldController::selectScenario(QString id) {
    if(!draft_ || id==scenario()) return;
    perform([&] {
        if(hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply or reset the pending fields and staged sections before selecting another scenario.");
        const auto* d=draft_->collection().find(id.toStdString());
        if(!d || !std::holds_alternative<content::ScenarioDefinition>(d->data)) throw std::invalid_argument("Choose an available scenario.");
        scenario_=id.toStdString();path_.clear();adding_=false;report_.clear();refresh();
    });
}
void WorldController::selectSection(QString path) {
    if(!draft_) return;
    perform([&] {
        if(hasUnapplied()) throw std::invalid_argument("Apply, stage or reset the current fields before selecting another section.");
        if(catalog_mode_) (void)WorldForm::open_catalog(*draft_,scenario_,path.toStdString());
        else (void)WorldForm::open(*draft_,scenario_,path.toStdString());
        path_=path.toStdString();adding_=false;refresh();
    });
}
void WorldController::filter(QString query) {if(hasUnapplied()) return;search_=query.trimmed().toStdString();if(draft_) {adding_=false;refresh();}}
void WorldController::setUnapplied(QVariantMap values) {
    if(unapplied_==values) return;unapplied_=std::move(values);emit pendingChanged();
}
void WorldController::selectState(QString id) {
    if(!draft_) return;
    for(const auto& row:states_) if(row.toMap().value(QStringLiteral("id"))==id) {
        selected_state_=id;selectSection(row.toMap().value(QStringLiteral("path")).toString());return;
    }
}
bool WorldController::apply(QString token,QVariantMap values) {
    return perform([&] {
        if(!draft_ || !form_ || token!=ticket()) throw std::invalid_argument("The form changed. Review the current section before applying.");
        if(hasStaged() && staged_revision_!=draft_->edit_revision()) throw std::invalid_argument("The staged batch is stale. Discard it and review the current document.");
        std::map<std::string,std::string,std::less<>> changes;
        for(auto i=values.cbegin();i!=values.cend();++i) changes.emplace(i.key().toStdString(),i.value().toString().toStdString());
        bool changed=false;
        if(staged_.empty()) changed=form_->apply(*draft_,changes);
        else {
            auto batch=staged_;for(const auto& [key,value]:changes) batch[path_][key]=value;
            std::vector<WorldSectionChange> sections;for(const auto& [path,entries]:batch) sections.push_back({path,entries});
            changed=apply_world_batch(*draft_,scenario_,sections,catalog_mode_);
        }
        staged_.clear();setUnapplied({});
        adding_=false;
        if(changed) {report_.clear();emit documentEdited();} else refresh();
    });
}
bool WorldController::stage(QString token,QVariantMap values) {
    return perform([&] {
        if(!draft_ || !form_ || token!=ticket()) throw std::invalid_argument("Review the current form before staging it.");
        if(hasStaged() && staged_revision_!=draft_->edit_revision()) throw std::invalid_argument("Discard the stale staged batch before making another.");
        std::map<std::string,std::string,std::less<>> entries;
        if(const auto prior=staged_.find(path_);prior!=staged_.end()) entries=prior->second;
        for(auto i=values.cbegin();i!=values.cend();++i) entries[i.key().toStdString()]=i.value().toString().toStdString();
        (void)form_->stage(*draft_,entries);
        if(staged_.size()>=64 && !staged_.contains(path_)) throw std::invalid_argument("Apply the existing staged batch before adding more sections.");
        staged_revision_=draft_->edit_revision();staged_[path_]=std::move(entries);setUnapplied({});project_form();emit changed();
    });
}
bool WorldController::applyStaged() {
    return perform([&] {
        if(!draft_) throw std::invalid_argument("Open a working collection.");
        if(hasUnapplied()) throw std::invalid_argument("Stage or reset the current fields before applying the staged batch.");
        if(hasStaged() && staged_revision_!=draft_->edit_revision()) throw std::invalid_argument("The staged batch is stale. Discard it and review the current document.");
        std::vector<WorldSectionChange> sections;for(const auto& [path,entries]:staged_) sections.push_back({path,entries});
        const auto changed=apply_world_batch(*draft_,scenario_,sections,catalog_mode_);staged_.clear();
        if(changed) emit documentEdited();else refresh();
    });
}
void WorldController::discardStaged() {
    if(hasUnapplied()) {message_=QStringLiteral("Reset or stage the current fields first.");emit changed();return;}
    staged_.clear();if(draft_) refresh();
}
bool WorldController::beginAppend(QString index) {
    return perform([&] {
        bool valid=false;const auto n=index.toULongLong(&valid);
        if(!draft_ || !form_ || !form_->can_append() || !valid || hasStaged()) throw std::invalid_argument("Apply staged sections, then choose a list and an available entry template.");
        form_=catalog_mode_?WorldForm::append_catalog(*draft_,scenario_,path_,static_cast<std::size_t>(n)):
            WorldForm::append(*draft_,scenario_,path_,static_cast<std::size_t>(n));
        adding_=true;project_form();emit changed();
    });
}
void WorldController::cancelAppend() {setUnapplied({});adding_=false;if(draft_) refresh();}
bool WorldController::removeEntry() {
    return perform([&] {
        if(!draft_ || !form_ || hasStaged()) throw std::invalid_argument("Apply staged sections, then choose a list entry.");
        if(form_->remove(*draft_)) emit documentEdited();
    });
}
bool WorldController::duplicate(QString id) {
    return perform([&] {
        if(!draft_ || hasStaged()) throw std::invalid_argument("Apply staged sections before duplicating the scenario.");
        if(duplicate_scenario(*draft_,scenario_,id.toStdString())) {scenario_=id.toStdString();path_.clear();report_.clear();emit documentEdited();}
    });
}
bool WorldController::addMissing(QString source) {
    return perform([&] {
        if(!draft_ || hasStaged()) throw std::invalid_argument("Apply staged sections before adding missing systems.");
        if(add_missing_scenario_sections(*draft_,scenario_,source.toStdString())) emit documentEdited();
    });
}
bool WorldController::preview(QString headquarters) {
    return perform([&] {
        if(!draft_ || hasStaged() || hasUnapplied()) throw std::invalid_argument("Apply pending fields and staged sections before running the opening.");
        report_=text(preview_world(*draft_,scenario_,headquarters.toStdString()));emit changed();
    });
}
} // namespace pgp::studio
