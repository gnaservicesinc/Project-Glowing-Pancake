#include <pgp/studio/catalog.hpp>
#include <pgp/runtime/runtime.hpp>
#include "structured_document.hpp"
#include <algorithm>
#include <set>
#include <tuple>

namespace pgp::studio {
namespace {
using Json=detail::StructuredJson;
std::string tail(std::string_view path) {return std::string(path.substr(path.rfind('/')+1));}
std::string parent(std::string_view path) {return std::string(path.substr(0,path.rfind('/')));}
std::string family(std::string_view kind) {
    if(kind=="data") return "dataset";
    if(kind=="service") return "product";
    if(kind=="market_segment") return "segment";
    if(kind=="variable") return "variable";
    if(kind=="template") return "deal";
    return std::string(kind);
}
std::string record_family(std::string_view path,std::string_view kind) {
    if(path.empty()) return std::string(kind);
    const auto array=parent(path);
    if(array.ends_with("/catalog/offers") && kind=="institutional_authority") return "institution_role";
    if(array.ends_with("/lab/data")) return "dataset";
    if(array.ends_with("/lab/research")) return "research";
    if(array.ends_with("/lab/recipes")) return "recipe";
    if(array.ends_with("/lab/evaluations")) return "evaluation";
    if(array.ends_with("/infrastructure/hardware")) return "hardware";
    if(array.ends_with("/infrastructure/states")) return "state";
    if(array.ends_with("/business_offers")) return "business_offer";
    if(array.ends_with("/rivals")) return "company";
    if(array.ends_with("/governance/financing")) return "corporate_financing";
    if(array.ends_with("/governance/executives")) return "executive";
    if(array.ends_with("/governance/board")) return "board";
    if(array.ends_with("/management/financing")) return "financing";
    if(array.ends_with("/management/specialists")) return "specialist";
    if(array.ends_with("/management/setbacks")) return "setback";
    if(array.ends_with("/segments")) return "segment";
    if(array.ends_with("/offers")) return "product";
    if(array.ends_with("/contracts")) return "contract";
    if(array.ends_with("/wholesale")) return "wholesale";
    if(array.ends_with("/regions")) return "region";
    if(array.ends_with("/protections")) return "protection";
    if(array.ends_with("/insurance")) return "insurance";
    return {};
}
std::string reference_family(std::string_view key,std::string_view path,std::string_view kind) {
    const auto array=tail(parent(path));
    if(kind=="flow" && array=="bindings") return "variable";
    if(key=="regional_markets") return "regional_markets";
    if(key=="resilience") return "site_resilience";
    if(key=="civic") return "us_civic";
    if(key=="society") return "regional_society";
    if(key=="authority") return "institutional_authority";
    if(key=="frontier") return "frontier_research";
    if(kind=="frontier_research" && (key=="refresh_research" || key=="research")) return "research";
    if(array=="flows") return "flow";
    if(array=="research" || array=="requires_research" || array=="all_of" || array=="any_of" || key=="publication_node" || key=="publication_research") return "research";
    if(array=="recipes" || key=="recipe_id" || key=="required_recipe") return "recipe";
    if(array=="data_ids" || key=="data_id") return "dataset";
    if(key=="plan_id" || key=="evaluation_id") return "evaluation";
    if(array=="services" || key=="product_id" || key=="pilot_product") return "product";
    if(key=="segment_id" || (kind=="regional_society" &&
        (array=="public_services" || array=="science" || array=="industry"))) return "segment";
    if(key=="specialist_id") return "specialist";
    if(key=="lease_offer_id") return "business_offer";
    if(key=="source_id") return "capacity_source";
    if(key=="project_id") return "project";
    if(key=="map_asset_id" || key=="asset_id") return "asset";
    if(key=="state_id" || key=="state" || key=="from" || key=="to" || array=="headquarters_choices") return "state";
    if(key=="region_id" || array=="regions") return "region";
    if(key=="founder" || key=="holder") return "owner";
    if(kind=="flow" && key=="flow_id") return "flow";
    if(kind=="flow" && key=="variable") return "variable";
    if(kind=="flow" && (key=="text_key" || key=="title_key" || key=="label_key")) return "text";
    return {};
}
void index_node(CatalogIndex& index,const content::Registry& registry,const Json& node,const std::string& path,const std::string& owner,
    const std::string& kind,const std::string& relative) {
    if(index.entries.size()>16384 || index.references.size()>65536) throw std::invalid_argument("Catalog/reference index exceeds its bounded size.");
    if(node.is_object()) {
        const auto type=record_family(relative,kind);
        if(node.contains("id") && node.at("id").is_string()) {
            if(!type.empty()) {
                const auto id=node.at("id").get<std::string>();
                index.entries.push_back({type,id,node.value("name",id),owner,path+"/id",type!="state" && type!="region"});
            }
        }
        if(type=="product" && node.contains("capacity_project_id"))
            index.entries.push_back({"project",node.at("capacity_project_id"),node.at("capacity_project_id"),owner,path+"/capacity_project_id",true});
        const auto* effect=kind=="flow" && parent(relative).ends_with("/effects") && node.contains("op") && node.at("op").is_string() ?
            registry.effect(node.at("op").get_ref<const std::string&>()) : nullptr;
        for(const auto& [key,value]:node.items()) {
            const auto child=path+"/"+detail::pointer_escape(key);
            if(value.is_string()) {
                auto reference=reference_family(key,child,kind);
                bool linked_identity=false;
                if (kind=="frontier_research") {
                    if (key=="target" && parent(relative).ends_with("/gates")) {
                        reference=node.value("kind",std::string{});
                        if (reference=="data") reference="dataset";
                    }
                    if (key=="id" && parent(relative).ends_with("/corpora")) { reference="dataset"; linked_identity=true; }
                    if (key=="id" && parent(relative).ends_with("/recipes")) { reference="recipe"; linked_identity=true; }
                }
                if(effect) for(const auto& arg:effect->arguments) if(arg.name==key && arg.type==content::ValueType::reference) reference=family(arg.reference_kind);
                if(kind=="flow" && key=="fact") {
                    const auto* fact=registry.fact(value.get<std::string>());
                    reference=fact && fact->default_value?"variable":"fact";
                }
                if((key!="id" || linked_identity || relative.ends_with("/bindings")) && !reference.empty() && !value.get_ref<const std::string&>().empty())
                    index.references.push_back({reference,value,owner,child});
            } else if(value.is_structured()) index_node(index,registry,value,child,owner,kind,relative+"/"+detail::pointer_escape(key));
        }
    } else if(node.is_array()) for(std::size_t i=0;i<node.size();++i) {
        const auto child=path+"/"+std::to_string(i);
        if(node[i].is_string()) {
            const auto type=reference_family(std::to_string(i),child,kind);
            if(!type.empty() && !node[i].get_ref<const std::string&>().empty()) index.references.push_back({type,node[i],owner,child});
        } else if(node[i].is_structured()) index_node(index,registry,node[i],child,owner,kind,relative+"/"+std::to_string(i));
    }
}
}
CatalogIndex catalog_index(const DraftDocument& draft) {return catalog_index(draft.program());}
std::string catalog_list_family(std::string_view path) {return record_family(std::string(path)+"/0","");}
CatalogIndex catalog_index(const runtime::CompiledProgram& program) {
    const auto document=detail::structured_document(program);CatalogIndex result;
    for(const auto& [id,row]:document.at("definitions").items()) {
        const auto path="/definitions/"+detail::pointer_escape(id);
        index_node(result,program.registry,row,path,id,row.at("kind"),"");
        if(row.contains("ownership_basis_points")) for(const auto& [owner,value]:row.at("ownership_basis_points").items()) {
            (void)value;result.entries.push_back({"owner",owner,owner,id,path+"/ownership_basis_points/"+detail::pointer_escape(owner),true});
        }
    }
    if(document.contains("variables")) for(std::size_t i=0;i<document.at("variables").size();++i) {
        const auto& row=document.at("variables")[i];result.entries.push_back({"variable",row.at("id"),row.at("id"),"variables","/variables/"+std::to_string(i)+"/id",true});
    }
    if(document.contains("messages")) for(const auto& [key,value]:document.at("messages").items())
        result.entries.push_back({"text",key,value,"messages","/messages/"+detail::pointer_escape(key),true});
    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){return std::tie(a.family,a.id,a.path)<std::tie(b.family,b.id,b.path);});
    std::sort(result.references.begin(),result.references.end(),[](const auto& a,const auto& b){return std::tie(a.family,a.id,a.path)<std::tie(b.family,b.id,b.path);});
    return result;
}
std::vector<ContentReference> content_uses(const CatalogIndex& index,std::string_view type,std::string_view id,bool transitive) {
    std::vector<ContentReference> out;std::set<std::string> owners,paths;
    for(const auto& ref:index.references) if(ref.family==type && ref.id==id) {out.push_back(ref);owners.insert(ref.owner);paths.insert(ref.path);}
    if(transitive) {
        std::map<std::string,std::vector<const ContentReference*>,std::less<>> reverse;
        for(const auto& ref:index.references)
            if(ref.family=="flow" || ref.family=="scenario" || ref.family=="research" || ref.family=="regional_markets" ||
                ref.family=="site_resilience" || ref.family=="us_civic" || ref.family=="regional_society" || ref.family=="institutional_authority" ||
                ref.family=="frontier_research")
                reverse[ref.id].push_back(&ref);
        std::vector<std::string> pending{owners.begin(),owners.end()};
        for(std::size_t i=0;i<pending.size();++i) {
            const auto found=reverse.find(pending[i]);if(found==reverse.end()) continue;
            for(const auto* ref:found->second) {
                if(paths.insert(ref->path).second) out.push_back(*ref);
                if(owners.insert(ref->owner).second) pending.push_back(ref->owner);
            }
        }
    }
    return out;
}
RefactorPlan RefactorPlan::rename(const DraftDocument& draft,std::string_view type,std::string_view before,std::string_view after) {
    if(before==after || !content::valid_id(after)) throw std::invalid_argument("Choose a different stable lowercase identifier.");
    const auto index=catalog_index(draft);RefactorPlan result;result.document_=&draft;result.revision_=draft.edit_revision();
    auto document=detail::structured_document(draft);
    const auto found=std::count_if(index.entries.begin(),index.entries.end(),[&](const auto& entry){return entry.family==type && entry.id==before;});
    if(found==0) throw std::invalid_argument("Select an existing catalog identity.");
    if(std::any_of(index.entries.begin(),index.entries.end(),[&](const auto& entry){return entry.family==type && entry.id==after;}))
        throw std::invalid_argument("The proposed identifier already belongs to this catalog family.");
    for(const auto& entry:index.entries) if(entry.family==type && entry.id==before) {
        if(!entry.identity_editable) throw std::invalid_argument("Registered state/region identities retain their simulation contract.");
        if(type=="text" || type=="owner") {
            auto& table=document.at(Json::json_pointer{parent(entry.path)});
            const auto value=table.at(std::string(before));table.erase(std::string(before));table[std::string(after)]=value;
        } else document.at(Json::json_pointer{entry.path})=std::string(after);
        result.changes_.push_back({entry.owner,entry.path,std::string(before),std::string(after),true});
    }
    for(const auto& ref:index.references) if(ref.family==type && ref.id==before) {
        document.at(Json::json_pointer{ref.path})=std::string(after);
        result.changes_.push_back({ref.owner,ref.path,std::string(before),std::string(after),false});
    }
    if (type=="text" && document.contains("translations")) {
        for (auto& [locale,table]:document.at("translations").items()) if (table.contains(std::string(before))) {
            const auto value=table.at(std::string(before));
            table.erase(std::string(before));
            table[std::string(after)]=value;
            result.changes_.push_back({"translations","/translations/"+locale+"/"+detail::pointer_escape(before),
                std::string(before),std::string(after),true});
        }
    }
    if(document.at("definitions").contains(std::string(before)) && document.at("definitions").at(std::string(before)).at("id")==after) {
        if(document.at("definitions").contains(std::string(after))) throw std::invalid_argument("A definition already uses the proposed identifier.");
        auto definition=document.at("definitions").at(std::string(before));
        definition["revision"]=1;
        document["definitions"].erase(std::string(before));
        document["definitions"][std::string(after)]=std::move(definition);
    }
    const auto candidate=content::load_collection(detail::collection_document(document).dump(),content::foundation_registry());
    result.candidate_=runtime::compile_collection(candidate,content::foundation_registry(),runtime::supported_effects()).canonical_content_bytes;
    return result;
}
bool RefactorPlan::apply(DraftDocument& draft) const {
    if(document_!=&draft || revision_!=draft.edit_revision()) throw std::invalid_argument("The document changed. Review this reference edit again.");
    return draft.replace(content::load_collection(candidate_,content::foundation_registry()));
}
bool duplicate_catalog_definition(DraftDocument& draft,std::string_view source,std::string_view id) {
    if(!content::valid_id(id) || draft.collection().find(id)) throw std::invalid_argument("Choose a new unique definition ID.");
    auto next=draft.collection();auto copy=next.definitions.at(std::string(source));copy.id=id;copy.revision=1;
    next.definitions.emplace(copy.id,std::move(copy));return draft.replace(next);
}
bool remove_catalog_entry(DraftDocument& draft,std::string_view path) {
    const auto index=catalog_index(draft);
    const auto selected=std::find_if(index.entries.begin(),index.entries.end(),[&](const auto& row){return row.path==path;});
    if(selected==index.entries.end() || !selected->identity_editable) throw std::invalid_argument("Select a removable catalog record.");
    if(!content_uses(index,selected->family,selected->id).empty()) throw std::invalid_argument("This identity still has typed references. Review its uses before removing it.");
    if(selected->family!="text" && selected->family!="owner" && !path.ends_with("/id"))
        throw std::invalid_argument("This identity belongs to a structural field. Select its owning catalog record.");
    auto next=detail::structured_document(draft);const auto record=parent(path);
    if(selected->family=="text" || selected->family=="owner") {
        next.at(Json::json_pointer{parent(path)}).erase(tail(path));
        if(selected->family=="text" && next.contains("translations")) {
            auto& translations=next.at("translations");
            for(auto i=translations.begin();i!=translations.end();) {
                i.value().erase(selected->id);
                if(i.value().empty()) i=translations.erase(i); else ++i;
            }
        }
    }
    else {
        const auto location=Json::json_pointer{record};const auto container=parent(record);
        if(container=="/definitions") next["definitions"].erase(selected->id);
        else {
            auto& list=next.at(Json::json_pointer{container});
            const auto at=static_cast<std::size_t>(std::stoull(tail(record)));
            if(!list.is_array()) throw std::invalid_argument("This catalog identity belongs to a structural field.");
            list.erase(list.begin()+static_cast<Json::difference_type>(at));
        }
    }
    return detail::replace_structured(draft,std::move(next));
}
bool edit_message(DraftDocument& draft,std::string_view key,std::string_view locale,std::string value) {
    if (!content::valid_id(key)) throw std::invalid_argument("Choose a stable localized message key.");
    auto candidate=draft.collection();
    const std::string id{key}, language{locale};
    if (locale=="en") {
        if(value.empty()) throw std::invalid_argument("Enter an English default for the message.");
        candidate.messages[id]=std::move(value);
    } else {
        if (!candidate.messages.contains(key)) throw std::invalid_argument("Add the English default before its translations.");
        if (value.empty()) {
            const auto found=candidate.translations.find(locale);
            if(found==candidate.translations.end()) return false;
            found->second.erase(id);
            if(found->second.empty()) candidate.translations.erase(found);
        } else candidate.translations[language][id]=std::move(value);
    }
    return draft.replace(candidate);
}
} // namespace pgp::studio
