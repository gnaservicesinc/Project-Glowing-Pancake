#pragma once
#include <pgp/studio/draft.hpp>
#include <nlohmann/json.hpp>

namespace pgp::studio::detail {
using StructuredJson=nlohmann::json;
inline std::string pointer_escape(std::string_view value) {
    std::string result;
    for(char c:value) {if(c=='~') result+="~0";else if(c=='/') result+="~1";else result+=c;}
    return result;
}
inline StructuredJson structured_document(const runtime::CompiledProgram& program) {
    auto result=StructuredJson::parse(program.canonical_content_bytes);
    StructuredJson definitions=StructuredJson::object();
    for(const auto& row:result.at("definitions")) definitions[row.at("id").get<std::string>()]=row;
    result["definitions"]=std::move(definitions);return result;
}
inline StructuredJson structured_document(const DraftDocument& draft) {return structured_document(draft.program());}
inline StructuredJson collection_document(StructuredJson result) {
    StructuredJson definitions=StructuredJson::array();
    for(const auto& [id,row]:result.at("definitions").items()) {(void)id;definitions.push_back(row);}
    result["definitions"]=std::move(definitions);return result;
}
inline bool replace_structured(DraftDocument& draft,StructuredJson result) {
    const auto bytes=collection_document(std::move(result)).dump();
    return draft.replace(content::load_collection(bytes,content::foundation_registry()));
}
} // namespace pgp::studio::detail
