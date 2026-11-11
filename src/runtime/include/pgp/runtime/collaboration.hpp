#pragma once
#include <pgp/core/lab.hpp>
#include <string>
namespace pgp::runtime {
struct WorldState;
struct CompiledProgram;
struct CollaborationRequest {
    std::string contact,topic,worker,source;
    core::MilliCUPerDay rate{4000};
};
struct ResearchCollaboration {
    std::string id,first_company,second_company,connection,contact,topic,first_project,second_project;
    core::OrdinalDay started_on{0};
    core::Cents coordination_fee{0};
    core::BasisPoints work_credit{0};
    int friendship=0;
    std::optional<core::OrdinalDay> published_on;
    auto operator<=>(const ResearchCollaboration&) const=default;
};
inline constexpr std::size_t collaboration_limit=1024;
struct CollaborationQuote {
    core::LabAllocation first,second;
    std::string partner,connection,contact,blocker;
    core::Cents total_fee{0},coordination_fee{0};
    core::BasisPoints work_credit{0};
    std::uint32_t first_days=0,second_days=0;
    int friendship=0;
};
[[nodiscard]] CollaborationQuote collaboration_quote(const WorldState&,const CompiledProgram&,std::string_view initiator,const CollaborationRequest&);
void start_collaboration(WorldState&,const CompiledProgram&,std::string_view initiator,const CollaborationRequest&);
[[nodiscard]] std::string collaboration_status(const WorldState&,const ResearchCollaboration&);
void publish_collaboration(WorldState&,std::string_view id);
void publish_industry_collaborations(WorldState&,const CompiledProgram&);
[[nodiscard]] std::string plan_research_collaboration(WorldState&,const CompiledProgram&,std::string_view initiator);
void validate_collaborations(const WorldState&,const CompiledProgram&,bool deep);
}
