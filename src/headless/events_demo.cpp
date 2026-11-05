#include "events_demo.hpp"
#include "content_file.hpp"
#include <pgp/session/slice_run.hpp>
#include <pgp/session/campaign.hpp>
#include <pgp/persistence/save.hpp>
#include <ostream>
#include <map>

void run_events_demo(const char* path,std::ostream& output,bool specialist) {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(read_content_file(path),registry),registry,runtime::supported_effects());
    const auto run=specialist?session::run_slice_specialist(program):session::run_slice_baseline(program);
    const auto& state=run.checkpoint.world.simulation;
    std::map<std::string,std::size_t> counts;
    for (const auto& activation:run.checkpoint.runtime.activations) ++counts[activation.definition_id];
    output << (specialist?"Efficient specialist run: ":"Baseline events run: ") << core::iso_date(state.date) << "; "
           << (run.stop==session::SliceStop::finished?"finished":"stopped at a real warning/closure") << '\n';
    for (const auto& [id,count]:counts) output << id << ": " << count << " bounded occurrences\n";
    output << "Cash: " << state.cash.value() << " constant-2014 cents; useful tasks: " << core::market_report(*state.market).delivered_tasks << '\n'
           << "Visible competitive notices: " << run.checkpoint.world.competition->notices.size() << "; weekly news groups: " << session::group_weekly_news(run.checkpoint).size() << '\n';
    const auto score=core::slice_score(state);
    output << "Slice score: " << score.total << "/100; operating profit: " << score.profit.value() << " cents; founder stake: " << score.founder_share.value() << " bp\n";
    if (run.stop!=session::SliceStop::finished) throw std::runtime_error("Baseline strategy did not reach the end of the three-year slice");
}
void run_competition_demo(const char* path,const char* preset,unsigned days,std::ostream& output) {
    using namespace pgp;const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(read_content_file(path),registry),registry,runtime::supported_effects());
    const std::map<std::string,std::string> presets{{"standard","scenario.world_competition"},{"reduced","scenario.world_reduced"},{"expanded","scenario.world_expanded"},{"forgiving","scenario.world_forgiving"}};
    const auto it=presets.find(preset);if (it==presets.end() || days==0 || days>1096) throw std::invalid_argument("Choose standard/reduced/expanded/forgiving and 1–1096 days");
    const auto* definition=program.collection.find(it->second);if (!definition) throw std::invalid_argument("This collection has no selected competition scenario");
    const auto& scenario=std::get<content::ScenarioDefinition>(definition->data);
    const auto boundary=std::min(scenario.ends_before,core::advance_days(scenario.starts_on,core::Days{days}));
    const auto run=session::run_competition_specialist(program,it->second,boundary);
    const auto saved=persistence::encode_save(program,run.checkpoint);
    if (persistence::decode_save(program,saved).checkpoint!=run.checkpoint) throw std::runtime_error("Competition checkpoint did not roundtrip exactly");
    const auto& competition=*run.checkpoint.world.competition;std::size_t entered=0,closed=0,decisions=0,accepted=0;
    for (const auto& rival:competition.companies) {
        if (!rival.planning->entered_on) continue;++entered;
        if (rival.simulation.phase==core::DayPhase::closed) ++closed;
        for (const auto& decision:rival.planning->decisions) {++decisions;if (decision.accepted) ++accepted;}
    }
    output<<"Competition "<<preset<<" · "<<core::iso_date(run.checkpoint.world.simulation.date)<<" · "<<competition.companies.size()<<" scheduled companies · "<<entered<<" entered · "<<closed<<" closed\n"
        <<decisions<<" weekly decisions · "<<accepted<<" accepted shared commands · "<<competition.releases.size()<<" actual public service releases\n"
        <<"One customer pool; conserved delivery/budgets; exact save roundtrip "<<saved.size()<<" bytes\n"
        <<"Player cash "<<run.checkpoint.world.simulation.cash.value()<<" constant-2014 cents; "
        <<(run.stop==session::SliceStop::finished?"finished":run.stop==session::SliceStop::requested_boundary?"requested boundary":"stopped at an actual warning/closure")<<"\n";
}
