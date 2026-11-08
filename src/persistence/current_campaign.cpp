#include <pgp/persistence/current_campaign.hpp>
#include <pgp/persistence/sha256.hpp>
#include "json_support.hpp"
#include <limits>
#include <chrono>

namespace pgp::persistence {
namespace {
constexpr std::size_t journal_limit = 8192;
using detail::Json;
struct Journal { std::uint64_t base; std::vector<std::uint64_t> days; };
Journal decode(std::string_view bytes) {
    auto data = detail::parse(bytes, journal_limit);
    detail::fields(data, "current journal", {"format", "base", "days", "sha256"});
    const auto checksum = detail::text(data.at("sha256"), "sha256", 64);
    data.erase("sha256");
    if (checksum != sha256(data.dump()) || detail::u64(data.at("format"), "format") != 1)
        throw PersistenceError("current_journal", "The current campaign journal is damaged or unsupported. Its files were preserved.");
    Journal result{detail::u64(data.at("base"), "base"), {}};
    if (!data.at("days").is_array() || data.at("days").size() > 31)
        throw PersistenceError("current_journal", "The current campaign journal exceeds its day limit.");
    for (const auto& revision : data.at("days")) result.days.push_back(detail::u64(revision, "revision"));
    return result;
}
std::string encode(std::uint64_t base, const std::vector<std::uint64_t>& days) {
    Json data{{"format", 1}, {"base", base}, {"days", days}};
    data["sha256"] = sha256(data.dump());
    return data.dump() + "\n";
}
bool local_exists(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) return false;
    if (error) throw PersistenceError("io", "Cannot inspect the current campaign: " + error.message());
    return std::filesystem::exists(status);
}
}
CurrentCampaign::CurrentCampaign(std::filesystem::path directory)
    : path_(directory / "current.pgpsave"), journal_(directory / "current-days.json") {}
std::optional<runtime::Checkpoint> CurrentCampaign::resume(const runtime::CompiledProgram& program) {
    if (!local_exists(path_)) {
        if (local_exists(journal_) || local_exists(path_.string() + ".bak") || local_exists(path_.string() + ".tmp"))
            throw PersistenceError("current_missing", "Current campaign data is incomplete. Recovery files were preserved; choose a recovery copy before continuing.");
        return std::nullopt;
    }
    auto loaded = load_file(program, path_);
    std::vector<std::uint64_t> replay;
    if (local_exists(journal_)) {
        const auto journal = decode(read_bounded(journal_, journal_limit));
        if (journal.base > loaded.autosave_sequence)
            throw PersistenceError("current_journal", "The current campaign and day journal do not belong to the same checkpoint.");
        // Checkpoint replacement commits first. A journal from an older base is
        // already included in that checkpoint, including after an interrupted write.
        if (journal.base == loaded.autosave_sequence) replay = journal.days;
    }
    runtime::Runtime engine{program, std::move(loaded.checkpoint)};
    for (const auto revision : replay) {
        const auto result = engine.submit({engine.checkpoint().world.simulation.revision, core::AdvanceDay{}});
        if (!result.accepted() || engine.checkpoint().world.simulation.revision != revision)
            throw PersistenceError("current_replay", "A recorded day could not be resumed exactly. Current and recovery files were preserved.");
    }
    generation_ = loaded.autosave_sequence;
    days_ = std::move(replay);
    return engine.checkpoint();
}
void CurrentCampaign::commit(const runtime::CompiledProgram& program, const runtime::Checkpoint& checkpoint, bool advanced_day) {
    if (advanced_day && generation_ != 0 && days_.size() < 31) {
        auto days = days_;
        days.push_back(checkpoint.world.simulation.revision);
        atomic_write(journal_, encode(generation_, days), [](auto prior) { (void)decode(prior); }, journal_limit);
        days_ = std::move(days);
        return;
    }
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw PersistenceError("current_limit", "The current campaign checkpoint generation is exhausted.");
    const auto next = generation_ + 1;
    save_file(program, checkpoint, path_, next);
    // Do not clear the old journal here: its lower base is harmless, and keeping
    // it permits exact replay of the .bak checkpoint for deliberate recovery.
    generation_ = next;
    days_.clear();
}
void CurrentCampaign::preserve_for_recovery() {
    const auto directory = path_.parent_path();
    if (local_exists(directory)) {
        const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto preserved = directory.parent_path() / (directory.filename().string() + "-preserved-" + suffix);
        if (local_exists(preserved)) throw PersistenceError("recovery_conflict", "Recovery folder already exists.");
        std::filesystem::rename(directory, preserved);
    }
    generation_ = 0;
    days_.clear();
}
void CurrentCampaign::archive(const runtime::CompiledProgram& program, const runtime::Checkpoint& checkpoint) {
    const auto directory = path_.parent_path() / "completed";
    const auto name = std::to_string(generation_) + "-" + std::to_string(checkpoint.world.simulation.revision) + ".pgpsave";
    const auto target = directory / name;
    if (local_exists(target)) {
        if (load_file(program, target).checkpoint != checkpoint)
            throw PersistenceError("archive_conflict", "A different completed campaign already occupies this archive name.");
        return;
    }
    save_file(program, checkpoint, target, generation_);
}
}
