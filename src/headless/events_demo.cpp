#include "events_demo.hpp"
#include "content_file.hpp"
#include <pgp/session/slice_run.hpp>
#include <pgp/session/campaign.hpp>
#include <ostream>
#include <map>

void run_events_demo(const char* path,std::ostream& output) {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(read_content_file(path),registry),registry,runtime::supported_effects());
    const auto run=session::run_slice_baseline(program);
    const auto& state=run.checkpoint.world.simulation;
    std::map<std::string,std::size_t> counts;
    for (const auto& activation:run.checkpoint.runtime.activations) ++counts[activation.definition_id];
    output << "Baseline events run: " << core::iso_date(state.date) << "; "
           << (run.stop==session::SliceStop::finished?"finished":"stopped at a real warning/closure") << '\n';
    for (const auto& [id,count]:counts) output << id << ": " << count << " bounded occurrences\n";
    output << "Cash: " << state.cash.value() << " constant-2014 cents; useful tasks: " << core::market_report(*state.market).delivered_tasks << '\n'
           << "Visible competitive notices: " << run.checkpoint.world.competition->notices.size() << "; weekly news groups: " << session::group_weekly_news(run.checkpoint).size() << '\n';
    if (run.stop!=session::SliceStop::finished) throw std::runtime_error("Baseline strategy did not reach the end of the three-year slice");
}
