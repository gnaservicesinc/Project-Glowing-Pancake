#include "departments_json.hpp"
namespace pgp::persistence::detail {
Json departments_json(const core::DepartmentsState& d) {
    auto operations=Json::array();for(const auto& a:d.operations) operations.push_back({{"id",a.id},{"person",a.person},{"project",a.project},{"source",a.source},{"starts_on",a.starts_on.value()},{"planned_until",a.planned_until.value()},{"ends_before",a.ends_before?Json(a.ends_before->value()):Json(nullptr)}});
    return {{"operations",std::move(operations)}};
}
core::DepartmentsState parse_departments(const Json& j) {
    fields(j,"departments",{"operations"});array(j.at("operations"),"operations appointments",1024);core::DepartmentsState d;
    for(const auto& a:j.at("operations")) {
        fields(a,"operations appointment",{"id","person","project","source","starts_on","planned_until","ends_before"});
        core::OperationsAppointment row{u64(a.at("id"),"operations appointment",1024),text(a.at("person"),"manager",128),text(a.at("project"),"manager project",100),text(a.at("source"),"manager source",128),core::OrdinalDay{i64(a.at("starts_on"),"manager starts_on")},core::OrdinalDay{i64(a.at("planned_until"),"manager planned_until")},{}};
        if(!a.at("ends_before").is_null()) row.ends_before=core::OrdinalDay{i64(a.at("ends_before"),"manager ends_before")};d.operations.push_back(std::move(row));
    }return d;
}
}
