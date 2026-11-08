#pragma once
#include <pgp/persistence/save.hpp>
#include <memory>
#include <set>

namespace pgp::persistence {
// Immutable local collections. Never changes saves or downloads content.
class CampaignContent {
public:
    explicit CampaignContent(std::filesystem::path directory):directory_(std::move(directory)) {}
    void preserve(const runtime::CompiledProgram&);
    void restore(const runtime::CompiledProgram&);
    [[nodiscard]] std::shared_ptr<const runtime::CompiledProgram> for_save(
        std::shared_ptr<const runtime::CompiledProgram> installed,const std::filesystem::path&) const;
    [[nodiscard]] std::filesystem::path packet(std::string_view hash) const;
private:
    void write(const runtime::CompiledProgram&,bool recovery);
    std::filesystem::path directory_;
    std::set<std::string> verified_;
};
}
