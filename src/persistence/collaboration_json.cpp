#include "collaboration_json.hpp"
namespace pgp::persistence::detail {
Json collaborations_json(const std::vector<runtime::ResearchCollaboration>& rows) {
    Json j=Json::array();
    for(const auto& c:rows) {j.push_back({{"id",c.id},{"first_company",c.first_company},{"second_company",c.second_company},{"connection",c.connection},{"contact",c.contact},
        {"topic",c.topic},{"first_project",c.first_project},{"second_project",c.second_project},{"started_on",c.started_on.value()},{"coordination_fee",c.coordination_fee.value()},
        {"work_credit",c.work_credit.value()},{"friendship",c.friendship},{"published_on",c.published_on?Json(c.published_on->value()):Json(nullptr)}});}
    return j;
}
std::vector<runtime::ResearchCollaboration> parse_collaborations(const Json& j) {
    array(j,"research collaborations",runtime::collaboration_limit);std::vector<runtime::ResearchCollaboration> rows;
    for(const auto& c:j) {
        fields(c,"research collaboration",{"id","first_company","second_company","connection","contact","topic","first_project","second_project","started_on","coordination_fee","work_credit","friendship","published_on"});
        rows.push_back({text(c.at("id"),"agreement",64),text(c.at("first_company"),"company",64),text(c.at("second_company"),"company",64),text(c.at("connection"),"connection",160),text(c.at("contact"),"contact",160),
            text(c.at("topic"),"topic",128),text(c.at("first_project"),"project",256),text(c.at("second_project"),"project",256),core::OrdinalDay{i64(c.at("started_on"),"started")},
            core::Cents{i64(c.at("coordination_fee"),"coordination fee")},core::BasisPoints{i64(c.at("work_credit"),"work credit")},static_cast<int>(u64(c.at("friendship"),"friendship",1000)),
            c.at("published_on").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(c.at("published_on"),"published")}}});
    }return rows;
}
}
