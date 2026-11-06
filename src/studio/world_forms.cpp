#include <pgp/studio/world_forms.hpp>
#include <pgp/studio/catalog.hpp>
#include "world_form_metadata.hpp"
#include "structured_document.hpp"
#include <pgp/core/integer_codec.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <sstream>
#include <tuple>

namespace pgp::studio {
namespace {
using namespace world_detail;
using detail::structured_document;
using detail::replace_structured;
std::string definition_path(const Json& doc,std::string_view id) {
    if(doc.at("definitions").contains(std::string(id))) return "/definitions/"+escape(id);
    throw std::invalid_argument("Select an existing scenario or linked world definition.");
}
std::vector<std::string> roots(const DraftDocument& d,const Json& doc,std::string_view scenario) {
    const auto* def=d.collection().find(scenario);
    const auto* s=def?std::get_if<content::ScenarioDefinition>(&def->data):nullptr;
    if(!s) throw std::invalid_argument("Select an existing scenario.");
    std::vector<std::string> result{definition_path(doc,scenario)};
    for(const auto& id:{s->regional_markets,s->resilience,s->civic}) if(!id.empty()) result.push_back(definition_path(doc,id));
    return result;
}
std::vector<std::string> catalog_roots(const Json& doc,std::string_view scenario) {
    std::vector<std::string> result;
    for(const auto& [id,value]:doc.at("definitions").items()) {
        const auto path="/definitions/"+escape(id);
        if(value.at("kind")!="scenario") result.push_back(path);
        else if(id==scenario) for(const auto* section:{"lab","service_catalog","market","management","infrastructure","business_offers","rivals"})
            if(value.contains(section)) result.push_back(path+"/"+section);
    }
    for(const auto* table:{"messages","translations","assets"}) if(doc.contains(table)) result.push_back("/"+std::string(table));
    return result;
}
bool fixed_array(std::string_view path) {
    return path.ends_with("/weights") || path.ends_with("/priorities") || path.find("/monthly_probability_bp")!=path.npos ||
        path.ends_with("/infrastructure/states") || path.ends_with("/catalog/exposures") ||
        path.ends_with("/catalog/constituencies") || path.ends_with("/catalog/blocs") ||
        path.ends_with("/positions_bp") || path.ends_with("/preferences_bp") || path.ends_with("/senate_classes") ||
        (path.find("/definitions/market.")==0 && (path.ends_with("/regions") || path.ends_with("/products/offers") || path.ends_with("/products/segments")));
}
Json entry_template(const DraftDocument& d,const Json& doc,std::string_view scenario,std::string_view path,std::size_t index) {
    const auto& array=doc.at(Json::json_pointer{std::string(path)});
    if(!array.is_array() || fixed_array(path) || (!array.empty() && index>=array.size()) || (array.empty() && index!=0))
        throw std::invalid_argument("Choose an extensible list and an available typed entry template.");
    if(path.ends_with("/market/offers") && array.empty()) {
        const auto& s=std::get<content::ScenarioDefinition>(d.collection().definitions.at(std::string(scenario)).data);
        if(s.lab) throw std::invalid_argument("This Lab schema starts with no stopped model/evidence records. Author deployable products under Service catalog; the campaign Lab qualifies and activates them.");
    }
    if(!array.empty()) return array[index];
    const auto key=last(path);
    for(const auto* name:{"research","flows","headquarters_choices","requires_research","data_ids","all_of","any_of","recipes","services","covered","regions"})
        if(key==name && !path.ends_with("/lab/research") && !path.ends_with("/lab/recipes")) return "";
    if(key=="contracts") {
        const auto& s=std::get<content::ScenarioDefinition>(d.collection().definitions.at(std::string(scenario)).data);
        if(!path.ends_with("/market/contracts") || !s.market || s.market->offers.empty() || s.market->segments.empty())
            throw std::invalid_argument("A starting contract needs a real starting product and customer segment first.");
        const auto& product=s.market->offers.front().product;
        const auto customer=std::find_if(s.market->segments.begin(),s.market->segments.end(),[&](const auto& segment) {
            return core::market_access(product,segment) && std::any_of(segment.preferences.begin(),segment.preferences.end(),[&](const auto& p){return p.product_id==product.id && p.eligible;});
        });
        if(customer==s.market->segments.end()) throw std::invalid_argument("Configure an eligible customer preference for the starting product first.");
        core::MarketContract row;row.id="contract.authored";row.product_id=product.id;
        row.segment_id=customer->id;row.tasks_per_day=1;
        row.accepted_on=s.starts_on;row.start_by=s.starts_on;row.ends_before=s.ends_before;row.cause="Authored opening agreement";
        content::MarketSetup setup=*s.market;setup.contracts.push_back({row,false});
        return Json::parse(content::canonical_market_setup_bytes(setup,s.starts_on)).at("contracts").front();
    }
    std::optional<Json> found;
    const auto search=[&](const auto& self,const Json& value)->void {
        if(found) return;
        if(value.is_object()) for(const auto& [name,child]:value.items()) {
            if(name==key && child.is_array() && !child.empty()) {found=child.front();return;}
            if(child.is_structured()) {self(self,child);if(found) return;}
        } else if(value.is_array()) for(const auto& child:value) if(child.is_structured()) {self(self,child);if(found) return;}
    };
    search(search,doc);
    if(!found) throw std::invalid_argument("This collection has no typed entry template. Add the missing system from a complete scenario template.");
    return *found;
}
void authorized(const DraftDocument& d,const Json& doc,std::string_view scenario,std::string_view path,bool catalog=false) {
    const auto allowed=catalog?catalog_roots(doc,scenario):roots(d,doc,scenario);
    if(std::none_of(allowed.begin(),allowed.end(),[&](const auto& root){
        if(path==root) return true;
        const auto& value=doc.at(Json::json_pointer{root});
        const bool flow_summary=catalog && value.is_object() && value.contains("kind") && value.at("kind")=="flow";
        return !flow_summary && path.starts_with(root+"/");
    }))
        throw std::invalid_argument("Choose a section belonging to the current authoring workspace.");
    if(!doc.contains(Json::json_pointer{std::string(path)})) throw std::invalid_argument("This section changed. Select it again.");
}
std::string index_label(std::string_view,std::size_t,std::size_t);
void outline(std::vector<WorldOutlineRow>& out,const Json& value,const std::string& path,std::string name,std::size_t depth) {
    if(out.size()>=8192) throw std::invalid_argument("World outline exceeds its bounded section count.");
    std::string search=path+" "+name;
    if(value.is_object()) for(const auto& [key,v]:value.items()) if(v.is_primitive()) search+=" "+key+" "+label(key)+" "+scalar(v);
    out.push_back({path,std::move(name),std::move(search),depth});
    if(value.is_object()) for(const auto& [key,child]:value.items()) {
        if(child.is_structured() || path=="/messages" || path.starts_with("/translations/"))
            outline(out,child,path+"/"+escape(key),label(key),depth+1);
    }
    else if(value.is_array()) for(std::size_t i=0;i<value.size();++i) {
        outline(out,value[i],path+"/"+std::to_string(i),row_label(value[i],value[i].is_primitive()?scalar(value[i]):index_label(path,i,value.size())),depth+1);
    }
}
Json parsed(const Json& type,std::string_view text) {
    if(type.is_boolean()) {
        if(text=="true") return true;if(text=="false") return false;
        throw std::invalid_argument("Choose True or False.");
    }
    if(type.is_number_integer()) return text.starts_with('-')?Json(core::parse_json_i64(text)):Json(core::parse_json_u64(text));
    if(type.is_string()) return std::string(text);
    throw std::invalid_argument("Select a scalar field in this section.");
}
void current(const DraftDocument& d,const DraftDocument* expected,std::uint64_t revision) {
    if(&d!=expected || d.edit_revision()!=revision) throw std::invalid_argument("The document changed. Select the section again before applying these fields.");
}
std::string index_label(std::string_view path,std::size_t index,std::size_t count) {
    if(path.ends_with("/monthly_probability_bp") && count==core::site_hazard_count)
        return label(std::string(core::hazard_key(static_cast<core::SiteHazard>(index))));
    if(path.ends_with("/scoring/weights")) {
        constexpr const char* names[]{"Survival","Useful work","Operating profit","Independent evidence","Founder stake"};
        if(index<5) return names[index];
    }
    if(path.ends_with("/priorities")) {
        constexpr const char* names[]{"Data","Research","Training","Evaluation","Deployment","Price","Hiring","Finance","Expansion","Publication"};
        if(index<10) return names[index];
    }
    if(count==12) {
        constexpr const char* names[]{"January","February","March","April","May","June","July","August","September","October","November","December"};
        return names[index];
    }
    if(count==core::lab_domain_count && (path.ends_with("/baseline_bp") || path.ends_with("/gain_bp") || path.ends_with("/ceiling_bp") ||
        path.ends_with("/relevance_bp") || path.ends_with("/coverage_bp") || path.ends_with("/required_bp") ||
        path.ends_with("/fit_bp") || path.ends_with("/data_scale")))
        return label(std::string(core::lab_domain_names[index]));
    return "Entry "+std::to_string(index+1);
}
}
std::vector<WorldOutlineRow> catalog_outline(const DraftDocument& d,std::string_view scenario) {
    const auto doc=structured_document(d);std::vector<WorldOutlineRow> rows;
    for(const auto& path:catalog_roots(doc,scenario)) {
        const auto& value=doc.at(Json::json_pointer{path});
        const auto name=value.is_object() && value.contains("id")?value.at("id").get<std::string>():label(last(path));
        if(value.is_object() && value.contains("kind") && value.at("kind")=="flow")
            rows.push_back({path,name,path+" "+name+" flow",0});
        else outline(rows,value,path,name,0);
    }
    return rows;
}
std::vector<WorldOutlineRow> world_outline(const DraftDocument& d,std::string_view scenario) {
    const auto doc=structured_document(d);std::vector<WorldOutlineRow> rows;
    for(const auto& root:roots(d,doc,scenario)) {
        const auto& value=doc.at(Json::json_pointer{root});
        outline(rows,value,root,value.at("kind")=="scenario"?"Scenario settings · "+value.at("id").get<std::string>():label(value.at("kind"))+" · "+value.at("id").get<std::string>(),0);
    }
    return rows;
}
std::vector<WorldLocation> world_states(const DraftDocument& d,std::string_view scenario) {
    const auto doc=structured_document(d);const auto root=definition_path(doc,scenario)+"/infrastructure/states";
    std::vector<WorldLocation> out;
    if(!doc.contains(Json::json_pointer{root})) return out;
    const auto& states=doc.at(Json::json_pointer{root});
    for(std::size_t i=0;i<states.size();++i) out.push_back({states[i].at("id"),states[i].at("name"),root+"/"+std::to_string(i)});
    return out;
}
WorldForm WorldForm::open(const DraftDocument& d,std::string_view scenario,std::string_view path) {
    const auto doc=structured_document(d);authorized(d,doc,scenario,path);
    WorldForm result{d,std::string(scenario),std::string(path)};result.build(d,false);return result;
}
WorldForm WorldForm::append(const DraftDocument& d,std::string_view scenario,std::string_view path,std::size_t index) {
    const auto doc=structured_document(d);authorized(d,doc,scenario,path);
    const auto& values=doc.at(Json::json_pointer{std::string(path)});
    WorldForm result{d,std::string(scenario),std::string(path)};
    (void)values;
    result.adding_=true;result.prototype_=entry_template(d,doc,scenario,path,index).dump();result.build(d,true);return result;
}
WorldForm WorldForm::open_catalog(const DraftDocument& d,std::string_view scenario,std::string_view path) {
    const auto doc=structured_document(d);authorized(d,doc,scenario,path,true);
    WorldForm result{d,std::string(scenario),std::string(path)};result.catalog_=true;result.build(d,false);return result;
}
WorldForm WorldForm::append_catalog(const DraftDocument& d,std::string_view scenario,std::string_view path,std::size_t index) {
    const auto doc=structured_document(d);authorized(d,doc,scenario,path,true);
    WorldForm result{d,std::string(scenario),std::string(path)};
    result.catalog_=true;result.adding_=true;result.prototype_=entry_template(d,doc,scenario,path,index).dump();
    result.build(d,true);return result;
}
WorldForm WorldForm::append_catalog_from(const DraftDocument& d,std::string_view scenario,std::string_view path,std::string_view source) {
    const auto doc=structured_document(d);authorized(d,doc,scenario,path,true);
    if(!doc.at(Json::json_pointer{std::string(path)}).is_array() || fixed_array(path))
        throw std::invalid_argument("Choose an extensible catalog list.");
    const auto index=catalog_index(d);
    const auto family=catalog_list_family(path);
    if(family.empty() || std::none_of(index.entries.begin(),index.entries.end(),[&](const auto& entry){
        return entry.family==family && entry.path==std::string(source)+"/id";
    }))
        throw std::invalid_argument("Choose a typed catalog entry as the template.");
    auto prototype=doc.at(Json::json_pointer{std::string(source)});
    if(path.ends_with("/lab/research") && prototype.value("kind",std::string{})=="research") {
        prototype={{"id",prototype.at("id")},{"all_of",prototype.at("prerequisites").at("all_of")},{"any_of",prototype.at("prerequisites").at("any_of")},
            {"cash_cents",prototype.at("cash_cents")},{"staff_days",prototype.at("staff_days")},{"experiment_milli_CU",prototype.at("work_milli_CU")}};
    }
    WorldForm result{d,std::string(scenario),std::string(path)};result.catalog_=true;result.adding_=true;result.prototype_=prototype.dump();result.build(d,true);return result;
}
void WorldForm::build(const DraftDocument& d,bool adding) {
    const auto doc=structured_document(d);
    const auto value=adding?Json::parse(prototype_):doc.at(Json::json_pointer{path_});
    if(catalog_ && value.is_object() && value.contains("kind") && value.at("kind")=="flow") {
        fields_.push_back({"","Flow revision",scalar(value.at("revision")),"Use Open flow to edit the graph and its typed settings.",FormControl::note,{}});
        return;
    }
    const auto field=[&](std::string key,std::string name,const Json& scalar_value,std::string field_path) {
        WorldField f;f.key=std::move(key);f.label=std::move(name);f.value=scalar(scalar_value);
        f.options=choices(d,doc,scenario_,field_path,scalar_value);
        f.control=!f.options.empty()?FormControl::choice:scalar_value.is_number_integer()?FormControl::integer:FormControl::text;
        if(!adding && (f.key=="id" || f.key=="revision" || f.key=="deprecated" || (f.key=="kind" && path_.find("/business_offers/")==path_.npos))) {
            f.control=FormControl::note;f.options.clear();
            f.help="Stable identity and revision. Duplicate to create a new definition; the snapshot writer owns revisions.";
        } else {
            f.help=(scalar_value.is_number_integer()?"Unit: "+units(field_path)+". ":"")+help(field_path);
        }
        fields_.push_back(std::move(f));
    };
    if(adding && value.is_structured()) {
        const auto flatten=[&](const auto& self,const Json& v,std::string key,std::string name)->void {
            if(v.is_object()) for(const auto& [child,item]:v.items())
                self(self,item,key.empty()?escape(child):key+"/"+escape(child),name.empty()?label(child):name+" · "+label(child));
            else if(v.is_array()) for(std::size_t i=0;i<v.size();++i)
                self(self,v[i],key+"/"+std::to_string(i),name+" · "+index_label(key,i,v.size()));
            else if(!v.is_null()) field(key,name,v,path_+"/0/"+key);
        };
        flatten(flatten,value,"","");
        return;
    }
    if(value.is_object()) {
        for(const auto& [key,v]:value.items()) if(v.is_primitive() && !v.is_null()) field(key,label(key),v,path_+"/"+escape(key));
        if(dynamic_map(path_)) {
            fields_.push_back({"@new_key","New entry ID","","Optional stable lowercase ID to add with the other changes.",FormControl::text,{}});
            fields_.push_back({"@new_value","New entry value","0","Unit: "+units(path_)+". Added atomically with the other fields.",FormControl::integer,{}});
            std::vector<FormOption> options{{"","Keep every existing entry",{}}};
            for(const auto& [key,v]:value.items()) {(void)v;options.push_back({key,"Remove "+key,{}});}
            fields_.push_back({"@remove","Remove entry","","Removal is checked with the other changes and all dependent references.",FormControl::choice,std::move(options)});
        }
        if(value.contains("kind") && value.at("kind")=="scenario") {
            for(const auto* key:{"market","business_plan","lab","business_offers","rivals","service_catalog","management","competition","infrastructure"}) if(value.contains(key))
                fields_.push_back({"@enabled/"+std::string(key),"Enable "+label(key),"true",
                    "Disable removes this section in the same validated batch. Stage related dependencies together; missing systems can be restored from another scenario template.",
                    FormControl::choice,{{"true","Enabled",{}},{"false","Disabled",{}}}});
        }
    } else if(value.is_array()) {
        for(std::size_t i=0;i<value.size();++i) {
            const auto key=std::to_string(i);
            if(value[i].is_primitive()) field(key,index_label(path_,i,value.size()),value[i],path_+"/"+key);
            templates_.push_back({key,row_label(value[i],key),{}});
        }
        if(!adding && !fixed_array(path_)) {
            try {
                const auto entry=entry_template(d,doc,scenario_,path_,0);
                can_append_=true;
                if(value.empty()) templates_.push_back({"0",row_label(entry,"New typed entry"),{}});
            } catch(const std::invalid_argument& e) {fields_.push_back({"","Adding entries",e.what(),"",FormControl::note,{}});}
        }
    } else field("",label(last(path_)),value,adding?path_+"/0":path_);
    const auto slash=path_.rfind('/');
    if(!adding && slash!=std::string::npos) {
        const auto& parent=doc.at(Json::json_pointer{path_.substr(0,slash)});
        can_remove_=parent.is_array() && !fixed_array(path_.substr(0,slash)) && path_.substr(0,slash)!="/definitions";
    }
}
std::string WorldForm::proposal(const DraftDocument& d,const std::map<std::string,std::string,std::less<>>& changes) const {
    current(d,document_,revision_);auto doc=structured_document(d);authorized(d,doc,scenario_,path_,catalog_);
    auto value=adding_?Json::parse(prototype_):doc.at(Json::json_pointer{path_});
    for(const auto& [key,text]:changes) {
        const auto found=std::find_if(fields_.begin(),fields_.end(),[&](const auto& f){return f.key==key && f.control!=FormControl::note;});
        if(found==fields_.end()) throw std::invalid_argument("This section has no editable field named "+key+".");
        if(found->control==FormControl::choice && std::none_of(found->options.begin(),found->options.end(),[&](const auto& o){return o.value==text;}))
            throw std::invalid_argument("Choose an available typed reference or option.");
        if(key.starts_with('@')) continue;
        auto& target=value.is_structured()?value.at(Json::json_pointer{"/"+key}):value;
        target=parsed(target,text);
    }
    if(dynamic_map(path_)) {
        if(const auto remove=changes.find("@remove");remove!=changes.end() && !remove->second.empty()) value.erase(remove->second);
        if(const auto key=changes.find("@new_key");key!=changes.end() && !key->second.empty()) {
            if(!content::valid_id(key->second) || value.contains(key->second)) throw std::invalid_argument("Give the new entry a unique stable ID.");
            const auto amount=changes.find("@new_value");value[key->second]=core::parse_json_u64(amount==changes.end()?"0":amount->second);
        }
    }
    for(const auto& [key,value_text]:changes) if(key.starts_with("@enabled/") && value_text=="false") value.erase(key.substr(9));
    if(value.is_object() && value.contains("kind") && value.at("kind")=="scenario")
        for(const auto* reference:{"regional_markets","resilience","civic"})
            if(value.contains(reference) && value.at(reference)=="") value.erase(reference);
    return value.dump();
}
WorldSectionChange WorldForm::stage(const DraftDocument& d,const std::map<std::string,std::string,std::less<>>& changes) const {
    if(adding_) throw std::invalid_argument("Finish or cancel the new entry before staging other sections.");
    (void)proposal(d,changes);return {path_,changes};
}
bool WorldForm::apply(DraftDocument& d,const std::map<std::string,std::string,std::less<>>& changes) const {
    const auto value=Json::parse(proposal(d,changes));auto doc=structured_document(d);
    if(adding_) doc.at(Json::json_pointer{path_}).push_back(value);
    else doc.at(Json::json_pointer{path_})=value;
    return replace_structured(d,std::move(doc));
}
bool apply_world_batch(DraftDocument& d,std::string_view scenario,const std::vector<WorldSectionChange>& changes,bool catalog) {
    if(changes.empty() || changes.size()>64) throw std::invalid_argument("Stage from one to 64 sections.");
    auto ordered=changes;
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b) {
        const auto ad=std::count(a.path.begin(),a.path.end(),'/'),bd=std::count(b.path.begin(),b.path.end(),'/');
        return std::tie(ad,a.path)<std::tie(bd,b.path);
    });
    auto doc=structured_document(d);std::set<std::string> seen;
    for(const auto& section:ordered) {
        if(!seen.insert(section.path).second) throw std::invalid_argument("Stage each section once with its combined values.");
        const auto form=catalog?WorldForm::open_catalog(d,scenario,section.path):WorldForm::open(d,scenario,section.path);
        doc.at(Json::json_pointer{section.path})=Json::parse(form.proposal(d,section.values));
    }
    return replace_structured(d,std::move(doc));
}
bool WorldForm::remove(DraftDocument& d) const {
    current(d,document_,revision_);if(!can_remove_) throw std::invalid_argument("Select a removable list entry.");
    auto doc=structured_document(d);authorized(d,doc,scenario_,path_,catalog_);
    const auto slash=path_.rfind('/');auto& parent=doc.at(Json::json_pointer{path_.substr(0,slash)});
    parent.erase(parent.begin()+static_cast<Json::difference_type>(core::parse_json_u64(path_.substr(slash+1))));
    return replace_structured(d,std::move(doc));
}
bool duplicate_scenario(DraftDocument& d,std::string_view source,std::string_view id) {
    if(!content::valid_id(id) || d.collection().find(id)) throw std::invalid_argument("Choose a new unique lowercase scenario ID.");
    auto doc=structured_document(d);auto copy=doc.at(Json::json_pointer{definition_path(doc,source)});
    if(copy.at("kind")!="scenario") throw std::invalid_argument("Choose a scenario template.");
    copy["id"]=id;copy["revision"]=1;doc["definitions"][std::string(id)]=std::move(copy);return replace_structured(d,std::move(doc));
}
bool add_missing_scenario_sections(DraftDocument& d,std::string_view target,std::string_view source) {
    auto doc=structured_document(d);auto& destination=doc.at(Json::json_pointer{definition_path(doc,target)});
    const auto original=doc.at(Json::json_pointer{definition_path(doc,source)});
    if(destination.at("kind")!="scenario" || original.at("kind")!="scenario") throw std::invalid_argument("Choose two existing scenarios.");
    for(const auto& [key,value]:original.items()) if(!destination.contains(key)) destination[key]=value;
    return replace_structured(d,std::move(doc));
}
std::string preview_world(const DraftDocument& d,std::string_view id,std::string_view headquarters) {
    auto checkpoint=runtime::begin_scenario(d.program(),id,headquarters);
    runtime::Runtime engine{d.program(),checkpoint};
    const auto prepared=engine.prepare_opening(checkpoint.world.simulation.revision);
    if(!prepared.accepted()) throw std::invalid_argument(prepared.explanation);
    checkpoint=engine.checkpoint();
    const auto bytes=persistence::encode_save(d.program(),checkpoint);
    if(persistence::decode_save(d.program(),bytes).checkpoint!=checkpoint) throw std::invalid_argument("Opening fixture failed exact checkpoint reload.");
    std::ostringstream out;out<<"Actual isolated opening · "<<id<<" · "<<headquarters<<"\n";
    const auto& w=checkpoint.world;const auto& s=w.simulation;
    out<<core::iso_date(s.date)<<" · cash "<<s.cash.value()<<" constant-2014 cents\n";
    out<<"Exact save reload: "<<bytes.size()<<" bytes. Preview retains the document and its random streams.\n";
    for(const auto& [name,fact]:d.registry().facts()) {
        if(fact.visibility!=content::Visibility::player_known || (fact.scope!=content::Scope::company && fact.scope!=content::Scope::world && fact.scope!=content::Scope::state)) continue;
        const runtime::ScopeBinding binding{fact.scope,fact.scope==content::Scope::company?w.company_id:fact.scope==content::Scope::world?w.world_id:w.state_id};
        out<<name<<" ["<<content::scope_name(fact.scope)<<":"<<binding.id<<"] = ";
        try {
            const auto value=runtime::read_fact(w,fact,binding);
            std::visit([&](const auto& v){out<<v;},value);out<<" "<<content::unit_name(fact.unit)<<"\n";
        } catch(const std::invalid_argument& e) {out<<"Unavailable at this opening: "<<e.what()<<"\n";}
    }
    const auto& scenario=std::get<content::ScenarioDefinition>(d.collection().definitions.at(std::string(id)).data);
    out<<"\nEnabled trigger targets and effects\n";
    for(const auto& flow_id:scenario.flows) {
        const auto& flow=std::get<content::FlowDefinition>(d.collection().definitions.at(flow_id).data);
        out<<flow_id<<" · "<<content::scope_name(flow.scope);
        if(!flow.on_committed.empty()) out<<" · requires committed "<<flow.on_committed;
        out<<"\n";
        if(flow.scope==content::Scope::company || flow.scope==content::Scope::world || flow.scope==content::Scope::state) {
            const auto inspected=runtime::inspect_flow_predicate(d.program(),checkpoint,flow_id,runtime::FlowPredicate::trigger);
            out<<"Opening eligibility: "<<(inspected.trace.result?(*inspected.trace.result?"true":"false"):"unknown")<<"\n";
            for(const auto& clause:inspected.trace.clauses) {
                out<<"  "<<clause.fact<<" → "<<content::scope_name(clause.binding.scope)<<":"<<clause.binding.id;
                if(clause.actual) {out<<" = ";std::visit([&](const auto& v){out<<v;},*clause.actual);}
                out<<" "<<content::unit_name(clause.unit)<<" · "<<(clause.result?(*clause.result?"true":"false"):"unknown")<<"\n";
            }
        } else out<<"Requires a real "<<content::scope_name(flow.scope)<<" member/outcome; no opening member is synthesized.\n";
        for(const auto& [node_id,node]:flow.nodes) for(const auto& effect:node.effects)
            out<<"  "<<node_id<<" · "<<effect.operation<<" → "<<content::scope_name(effect.target_scope)<<":"<<effect.target<<"\n";
    }
    return out.str();
}
} // namespace pgp::studio
