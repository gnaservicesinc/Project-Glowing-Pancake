#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
namespace {
using namespace pgp;using namespace core;using Json=nlohmann::json;unsigned checks=0;
void check(bool value,const char* why) {++checks;if (!value) throw std::runtime_error(why);}
GovernanceCatalog rules() {
    GovernanceCatalog c;
    c.financing={{"round.small","Measured equity round","round",Cents{20000000},Cents{0},Cents{0},BasisPoints{1000},BasisPoints{0},Days{0},true},
        {"debt.runway","Covenanted runway loan","debt",Cents{10000000},Cents{150000000},Cents{100000},BasisPoints{0},BasisPoints{50},Days{365},false}};
    c.financing.push_back({"round.alternate","Smaller counteroffer","round",Cents{15000000},Cents{0},Cents{0},BasisPoints{500},BasisPoints{0},Days{0},false});
    c.executives={{"person.research","Mara Chen","research","Publishing sound methods; predictable cash compensation","research.held_out_evaluation",Cents{100000},Cents{100000},Cents{70000},BasisPoints{200},Days{10},Days{100},Days{30},85,40,55},
        {"person.finance","Dev Patel","finance","Liquidity and clear agreements","",Cents{100000},Cents{100000},Cents{70000},BasisPoints{100},Days{10},Days{100},Days{0},45,90,35}};
    c.board={{"founder","Founder",2,true,Cents{0},BasisPoints{3000}},{"cofounder","Cofounder",1,false,Cents{0},BasisPoints{1500}},
        {"seed","Seed investor",1,false,Cents{0},BasisPoints{1500}},{"independent","Independent director",1,false,Cents{0},BasisPoints{1000}}};
    return c;
}
runtime::CompiledProgram fixture() {
    std::ifstream f{PGP_GOVERNANCE_SOURCE};const std::string bytes{std::istreambuf_iterator<char>{f},{}};
    const auto registry=content::foundation_registry();auto collection=content::load_collection(bytes,registry);
    std::erase_if(collection.definitions,[](const auto& entry) {
        return std::holds_alternative<content::FlowDefinition>(entry.second.data) ||
            (std::holds_alternative<content::ScenarioDefinition>(entry.second.data) && entry.first!="scenario.world_competition");
    });
    auto& s=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.world_competition").data);
    s.flows.clear();s.management->setbacks.clear();s.management->governance=rules();s.competition->domestic=1;s.competition->international=0;s.competition->heavyweights=0;
    for (auto& rival:s.rivals) if (rival.planning->group=="domestic" && rival.planning->slot==1) rival.planning->services={"product.document_specialist","product.documents"};
    return runtime::compile_collection(collection,registry,runtime::supported_effects());
}
const SimulationState& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
const GovernanceState& corporate(const runtime::Runtime& game) {return *state(game).management->governance;}
void apply(runtime::Runtime& game,CommandAction command) {const auto r=game.submit({state(game).revision,std::move(command)});if (!r.accepted()) throw std::runtime_error(r.explanation);}
void roundtrip(const runtime::CompiledProgram& p,const runtime::Runtime& game) {check(persistence::decode_save(p,persistence::encode_save(p,game.checkpoint())).checkpoint==game.checkpoint(),"Corporate continuation did not roundtrip exactly");}
void step(runtime::Runtime& game,unsigned n) {
    while (n--!=0) {
        const auto disputes=corporate(game).disputes;for (const auto& d:disputes) if (!d.acknowledged) apply(game,RespondCorporateDispute{d.id,false});
        if (const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        apply(game,AdvanceDay{});
    }
}
BasisPoints holding(const SimulationState& s,std::string_view id) {for (const auto& h:s.accounts->holdings) if (h.holder==id) return h.shares;return BasisPoints{0};}
void refuse(runtime::Runtime& game,CommandAction command) {const auto before=game.checkpoint();check(!game.submit({state(game).revision,std::move(command)}).accepted() && game.checkpoint()==before,"Refused corporate command changed state");}
void corrupt(const runtime::CompiledProgram& p,Json j) {j.erase("sha256");j["sha256"]=persistence::sha256(j.dump());bool rejected=false;try {(void)persistence::decode_save(p,j.dump());}catch(const persistence::PersistenceError&){rejected=true;}check(rejected,"Forged corporate continuation was accepted");}
void public_listing_route() {
    const auto registry=content::foundation_registry();
    auto collection=content::load_collection(persistence::read_bounded(PGP_GOVERNANCE_SOURCE,content::maximum_document_bytes),registry);
    const auto& century=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.century_frontier").data);
    const auto& authored=*century.management->governance;
    check(authored.public_listing && authored.public_listing->minimum_age==Days{3650} &&
          authored.public_listing->performance_days==Days{365},"The century campaign lacks its authored IPO");
    auto listing=*authored.public_listing;
    const auto offer=*std::ranges::find(authored.financing,listing.offer_id,&CorporateFinanceTerms::id);
    auto& scenario=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.first_playable").data);
    scenario.flows.clear();
    scenario.rivals.clear();
    scenario.competition.reset();
    scenario.management->setbacks.clear();
    listing.minimum_age=Days{28};
    listing.performance_days=Days{28};
    scenario.management->governance->public_listing=listing;
    scenario.management->governance->financing.push_back(offer);
    const auto program=runtime::compile_collection(collection,registry);
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    check(game.prepare_opening(state(game).revision).accepted(),"IPO fixture could not prepare the morning");
    refuse(game,NegotiateCorporateFinance{offer.id});
    refuse(game,InvestEquity{"investor/"+offer.id,offer.proceeds,offer.acquired,"corporate/"+offer.id});
    step(game,28);
    refuse(game,NegotiateCorporateFinance{offer.id});
    apply(game,IssueInvoice{{"fixture.sale",InvoiceKind::receivable,state(game).date,Cents{110'000'000},Cents{0},"Settled service revenue for IPO fixture"}});
    refuse(game,NegotiateCorporateFinance{offer.id});
    step(game,1);
    const auto before=accounts_report(*state(game).accounts);
    const auto director="seat/"+offer.id;
    const auto founder_id=state(game).management->catalog.scoring.founder;
    const auto founder=holding(state(game),founder_id);
    apply(game,NegotiateCorporateFinance{offer.id});
    const auto motion=corporate(game).motions.back().id;
    check(state(game).cash==before.cash && !public_listing_date(state(game)),"IPO raised cash before a board vote");
    refuse(game,NegotiateCorporateFinance{offer.id});
    roundtrip(program,game);
    runtime::Runtime stressed{program,game.checkpoint()};
    apply(stressed,IssueInvoice{{"fixture.loss",InvoiceKind::payable,state(stressed).date,Cents{120'000'000},Cents{0},"New loss before the listing vote"}});
    step(stressed,1);
    refuse(stressed,VoteBoardMotion{motion,true});

    apply(game,VoteBoardMotion{motion,true});
    const auto after=accounts_report(*state(game).accounts);
    check(corporate(game).motions.back().executed && public_listing_date(state(game))==state(game).date &&
          after.cash==checked_unit_add(before.cash,offer.proceeds) && after.profit==before.profit,
          "Approved listing failed to raise the disclosed contributed capital");
    check(holding(state(game),"investor/"+offer.id)==offer.acquired && holding(state(game),founder_id)<founder,
          "Listing did not conserve dilution of the existing ownership");
    check(std::ranges::none_of(corporate(game).motions.back().ballots,[&](const auto& b){return b.member==director;}) &&
          std::ranges::any_of(current_board(state(game)),[&](const auto& b){return b.id==director;}),
          "Public director voted on its own appointment or failed to join");
    refuse(game,NegotiateCorporateFinance{offer.id});
    const auto vote_on_pool=[&](runtime::Runtime& current,bool expected) {
        apply(current,ProposePoolExpansion{BasisPoints{100}});
        apply(current,VoteBoardMotion{corporate(current).motions.back().id,true});
        const auto& votes=corporate(current).motions.back().ballots;
        const auto public_vote=std::ranges::find(votes,director,&BoardBallot::member);
        check(public_vote!=votes.end() && public_vote->support==expected,"The public director did not follow its filing and dilution rules");
    };
    vote_on_pool(game,true);
    const auto april=to_ordinal({2014,4,1});
    while (state(game).date<OrdinalDay{april.value()-1}) step(game,1);
    check(corporate(game).public_filings.empty(),"An unsettled quarter was published early");
    step(game,1);
    const auto& filing=corporate(game).public_filings.back();
    check(corporate(game).public_filings.size()==1 && filing.starts_on==*public_listing_date(state(game)) &&
          filing.ends_before==april && filing.revenue==Cents{0} && filing.profit().value()<0 &&
          filing.ending_cash==state(game).cash,"First public filing did not reflect its actual partial quarter");
    vote_on_pool(game,false);
    const auto save=persistence::encode_save(program,game.checkpoint());
    auto forged=Json::parse(save);
    forged["checkpoint"]["world"]["simulation"]["management"]["governance"]["public_filings"][0]["revenue"]=1;
    corrupt(program,std::move(forged));
    const auto restored=persistence::decode_save(program,save).checkpoint;
    check(restored==game.checkpoint(),"Listing, public seat, vote and filing did not reopen exactly");
    runtime::Runtime reopened{program,restored};
    apply(reopened,IssueInvoice{{"fixture.next_sale",InvoiceKind::receivable,state(reopened).date,Cents{110'000'000},Cents{0},"Second-quarter commercial revenue"}});
    const auto july=to_ordinal({2014,7,1});
    while (state(reopened).date<july) step(reopened,1);
    check(corporate(reopened).public_filings.size()==2 && corporate(reopened).public_filings.back().profit().value()>0,
          "Quarterly reporting did not continue after reopening");
    vote_on_pool(reopened,true);
    roundtrip(program,reopened);
    std::cout<<checks<<" public listing checks passed\n";
}
}
int main(int argc,char** argv) {
 try {
    if (argc>1 && std::string_view{argv[1]}=="--public-listing") {
        public_listing_route();
        return 0;
    }
    const auto p=fixture();runtime::Runtime game{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(game.prepare_opening(state(game).revision).accepted(),"Opening preparation failed");
    check(unallocated_option_pool(state(game))==BasisPoints{500},"Opening pool was duplicated");
    const auto opening=state(game).cash;const auto opening_staff=state(game).capacity->available_staff;const auto rng=state(game).random;
    apply(game,AppointExecutive{"person.research",true});
    check(state(game).cash==checked_unit_subtract(opening,Cents{100000}) && state(game).capacity->available_staff==opening_staff+1,"Appointment did not pay actual cash and add one staff member");
    check(unallocated_option_pool(state(game))==BasisPoints{300} && holding(state(game),"option_pool")==BasisPoints{500},"Unvested grant did not reserve the existing pool");
    check(holding(state(game),"executive.grant/person.research")==BasisPoints{0},"Grant vested before its cliff");
    refuse(game,AppointExecutive{"person.research",true});roundtrip(p,game);
    step(game,9);check(holding(state(game),"executive.grant/person.research").value()==0,"Vesting bypassed the cliff");
    step(game,1);check(holding(state(game),"executive.grant/person.research")==BasisPoints{20},"Cumulative cliff entitlement is wrong");
    const auto at_cliff=game.checkpoint();runtime::Runtime resumed{p,persistence::decode_save(p,persistence::encode_save(p,at_cliff)).checkpoint};
    apply(game,NegotiateCorporateFinance{"round.small"});check(state(game).cash==state(resumed).cash,"Proposing terms paid money before a vote");roundtrip(p,game);
    refuse(game,NegotiateCorporateFinance{"round.alternate"});
    apply(game,VoteBoardMotion{corporate(game).motions.back().id,true});
    refuse(game,NegotiateCorporateFinance{"round.alternate"});
    check(corporate(game).motions.back().executed && holding(state(game),"investor/round.small")==BasisPoints{1000},"Approved financing lacks real proceeds/dilution");
    check(current_board(state(game)).size()==5,"New financing board seat was not recorded separately");
    check(corporate(game).grants.front().unvested==BasisPoints{162},"Financing diluted the unvested claim twice or not at all");
    refuse(game,NegotiateCorporateFinance{"round.small"});refuse(game,VoteBoardMotion{corporate(game).motions.back().id,true});
    apply(game,ProposePoolExpansion{BasisPoints{500}});const auto before_pool=state(game).cash;
    apply(game,VoteBoardMotion{corporate(game).motions.back().id,true});
    check(state(game).cash==before_pool && unallocated_option_pool(state(game)).value()>300,"Approved pool expansion created cash or failed explicit dilution");roundtrip(p,game);
    step(game,20);check(!corporate(game).disputes.empty() && corporate(game).disputes.back().status==CorporateDisputeStatus::concern,"Missed publication promise lacks a concern");
    const auto pending=game.checkpoint();check(!game.submit({state(game).revision,AdvanceDay{}}).accepted() && game.checkpoint()==pending,"Advancement skipped an unread dispute");
    step(game,7);check(corporate(game).disputes.back().status==CorporateDisputeStatus::formal,"Dispute skipped its formal recommendation");roundtrip(p,game);
    step(game,23);check(corporate(game).appointments.front().departed_on.has_value() && corporate(game).grants.front().unvested.value()==0,"Departure failed to return only unvested claims");
    check(holding(state(game),"executive.grant/person.research").value()>0 && state(game).capacity->available_staff==opening_staff,"Departure erased earned ownership or retained paid staffing");
    refuse(game,AppointExecutive{"person.research",true});roundtrip(p,game);
    apply(game,AppointExecutive{"person.finance",false});check(corporate(game).grants.size()==1,"Cash-only negotiation invented equity");
    apply(game,NegotiateCorporateFinance{"debt.runway"});apply(game,VoteBoardMotion{corporate(game).motions.back().id,true});
    check(corporate(game).disputes.back().kind=="covenant","Real cash-floor breach did not open a covenanted loan response");
    const auto waiver_cash=state(game).cash;apply(game,RespondCorporateDispute{corporate(game).disputes.back().id,true});
    check(state(game).cash==checked_unit_subtract(waiver_cash,Cents{100000}) && corporate(game).disputes.back().status==CorporateDisputeStatus::waived,"Negotiated waiver did not pay its actual fee");
    refuse(game,RespondCorporateDispute{corporate(game).disputes.back().id,true});roundtrip(p,game);
    check(state(game).random[static_cast<std::size_t>(RandomStream::narrative)]==rng[static_cast<std::size_t>(RandomStream::narrative)],"Corporate previews/actions consumed narrative RNG");
    runtime::Runtime promise{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(promise.prepare_opening(state(promise).revision).accepted(),"Promise opening failed");
    apply(promise,AppointExecutive{"person.research",true});
    apply(promise,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{10000},1}});
    step(promise,30);check(corporate(promise).disputes.back().status==CorporateDisputeStatus::concern,"Completed unpublished work incorrectly satisfied the promise");
    apply(promise,PublishResearch{"research.held_out_evaluation"});
    check(corporate(promise).disputes.back().status==CorporateDisputeStatus::resolved && !corporate(promise).appointments.front().departed_on,"Real publication failed to resolve its concrete commitment");roundtrip(p,promise);
    runtime::Runtime loan{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(loan.prepare_opening(state(loan).revision).accepted(),"Covenant opening failed");
    apply(loan,NegotiateCorporateFinance{"debt.runway"});apply(loan,VoteBoardMotion{corporate(loan).motions.back().id,true});
    step(loan,30);check(corporate(loan).disputes.front().status==CorporateDisputeStatus::accelerated && state(loan).accounts->debts.back().due==advance_days(state(loan).date,Days{30}),"Unresolved covenant skipped the warned acceleration");roundtrip(p,loan);
    runtime::Runtime cure{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(cure.prepare_opening(state(cure).revision).accepted(),"Covenant cure opening failed");
    apply(cure,NegotiateCorporateFinance{"debt.runway"});apply(cure,VoteBoardMotion{corporate(cure).motions.back().id,true});
    apply(cure,InvestEquity{"cure.capital",Cents{40000000},BasisPoints{500},"cure.capital"});
    check(corporate(cure).disputes.front().status==CorporateDisputeStatus::resolved,"Actual restored cash failed to cure the covenant");
    step(cure,1);apply(cure,ReserveFunds{"A real optional expense",Cents{40000000},true});apply(cure,ConsumeReservation{state(cure).reservations.back().id,Cents{40000000}});
    check(corporate(cure).disputes.size()==2 && corporate(cure).disputes.back().status==CorporateDisputeStatus::concern,"A later real covenant breach escaped a new response window");roundtrip(p,cure);
    runtime::Runtime ignored{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(ignored.prepare_opening(state(ignored).revision).accepted(),"Ignored board opening failed");
    const auto ignored_holdings=state(ignored).accounts->holdings;apply(ignored,ProposePoolExpansion{BasisPoints{500}});step(ignored,14);
    check(corporate(ignored).motions.front().status==BoardMotionStatus::expired && state(ignored).accounts->holdings==ignored_holdings,"Ignoring a board proposal auto-approved dilution");roundtrip(p,ignored);
    runtime::Runtime buyer{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(buyer.prepare_opening(state(buyer).revision).accepted(),"Merger opening failed");
    apply(buyer,InvestEquity{"engineering.capital",Cents{500000000},BasisPoints{1000},"engineering.capital"});
    const auto target_id=buyer.current_state().world.competition->companies.front().definition_id;
    const auto target_before=buyer.current_state().world.competition->companies.front().simulation;
    const auto quote=runtime::quote_acquisition(buyer.current_state().world,p,target_id,false);
    check(buyer.proposeAcquisition(state(buyer).revision,target_id,false).accepted(),"Cash acquisition proposal failed");roundtrip(p,buyer);
    const auto cash_before=state(buyer).cash;const auto revision=state(buyer).revision;
    apply(buyer,VoteBoardMotion{corporate(buyer).motions.back().id,true});
    check(!buyer.submit({revision,VoteBoardMotion{corporate(buyer).motions.back().id,true}}).accepted(),"Stale merger vote was accepted");
    const auto& acquired=buyer.current_state().world.competition->companies.front();
    check(acquired.parent_company==buyer.current_state().world.company_id && acquired.simulation.cash==target_before.cash &&
        acquired.simulation.accounts->debts==target_before.accounts->debts && acquired.simulation.accounts->invoices==target_before.accounts->invoices &&
        acquired.simulation.lab==target_before.lab && acquired.simulation.capacity==target_before.capacity,"Acquisition erased obligations or cloned operating resources");
    check(state(buyer).cash==checked_unit_subtract(cash_before,quote.cash) && holding(acquired.simulation,"parent/company.player")==BasisPoints{10000},"Cash acquisition did not conserve consideration and control");
    std::int64_t seller_cash=0;for (const auto& seller:buyer.current_state().world.competition->acquisitions.front().settlements) seller_cash=checked_add(seller_cash,seller.cash.value());
    check(Cents{seller_cash}==quote.cash,"Seller consideration lost or duplicated a cent");roundtrip(p,buyer);
    check(!buyer.proposeAcquisition(state(buyer).revision,target_id,false).accepted(),"The same company was acquired twice");
    refuse(buyer,TransferCompanyOwnership{"company.fake"});
    const auto group_before=runtime::owned_group_report(buyer.current_state().world);
    const auto funding_revision=state(buyer).revision;
    check(buyer.transferSubsidiaryFunds(funding_revision,target_id,Cents{1000000},true).accepted(),"Real owner capital transfer was refused");
    check(state(buyer).revision==funding_revision+1,"Group transfer incremented the world revision twice");
    const auto group_after=runtime::owned_group_report(buyer.current_state().world);
    check(group_before.cash==group_after.cash && group_before.net_assets==group_after.net_assets && group_before.operating_profit==group_after.operating_profit,"Owner funding manufactured group cash, assets or operating profit");
    const auto funded=buyer.checkpoint();check(!buyer.transferSubsidiaryFunds(state(buyer).revision,target_id,Cents{10000},false).accepted() && buyer.checkpoint()==funded,"Capital funding could immediately be recycled as earned dividend profit");roundtrip(p,buyer);
    const auto funding_boundary=buyer.checkpoint();check(!buyer.transferSubsidiaryFunds(state(buyer).revision,target_id,Cents{100000000000},true).accepted() && buyer.checkpoint()==funding_boundary,"Unfunded group capital transfer partially committed");
    step(buyer,40);roundtrip(p,buyer);
    for (unsigned day=0;day<400;++day) {
        const auto& owned=buyer.current_state().world.competition->companies.front();const auto& acquisition=buyer.current_state().world.competition->acquisitions.front();
        if (accounts_report(*owned.simulation.accounts).profit>checked_unit_add(acquisition.acquired_profit,Cents{1000000})) break;
        step(buyer,1);
    }
    const auto dividend_group=runtime::owned_group_report(buyer.current_state().world);
    const auto dividend_result=buyer.transferSubsidiaryFunds(state(buyer).revision,target_id,Cents{10000},false);
    if (!dividend_result.accepted()) {const auto& sub=buyer.current_state().world.competition->companies.front().simulation;throw std::runtime_error(dividend_result.explanation+"; subsidiary profit="+std::to_string(accounts_report(*sub.accounts).profit.value())+" cash="+std::to_string(sub.cash.value()));}
    check(dividend_result.accepted(),"An actually earned, cash-covered dividend was refused");
    const auto paid_group=runtime::owned_group_report(buyer.current_state().world);
    check(dividend_group.cash==paid_group.cash && dividend_group.net_assets==paid_group.net_assets && dividend_group.operating_profit==paid_group.operating_profit,"Internal dividends duplicated group cash, book assets or profit");roundtrip(p,buyer);
    refuse(buyer,TransferGroupFunds{"company.player",target_id,3,Cents{100},true,true});
    auto forged_group=Json::parse(persistence::encode_save(p,buyer.checkpoint()));forged_group["checkpoint"]["world"]["competition"]["acquisitions"][0]["transfers"][0]["amount"]=1;corrupt(p,forged_group);

    runtime::Runtime stock_buyer{p,runtime::begin_scenario(p,"scenario.world_competition","US-CA")};
    check(stock_buyer.prepare_opening(state(stock_buyer).revision).accepted(),"Equity acquisition opening failed");
    apply(stock_buyer,InvestEquity{"engineering.capital",Cents{500000000},BasisPoints{1000},"engineering.capital"});
    bool ceiling_refused=false;try {(void)runtime::quote_acquisition(stock_buyer.current_state().world,p,target_id,true);}catch (const std::invalid_argument&) {ceiling_refused=true;}
    check(ceiling_refused,"Seller accepted an equity counteroffer beyond the dilution ceiling");
    apply(stock_buyer,InvestEquity{"engineering.capital.2",Cents{500000000},BasisPoints{500},"engineering.capital.2"});
    const auto stock_quote=runtime::quote_acquisition(stock_buyer.current_state().world,p,target_id,true);
    check(stock_buyer.proposeAcquisition(state(stock_buyer).revision,target_id,true).accepted(),"Equity acquisition proposal failed");
    apply(stock_buyer,VoteBoardMotion{corporate(stock_buyer).motions.back().id,true});
    check(holding(state(stock_buyer),"merger.sellers/"+target_id)==stock_quote.shares && stock_quote.cash<stock_quote.value,"Merger equity did not create exactly its conserving seller claim");roundtrip(p,stock_buyer);
    auto forged_merger=Json::parse(persistence::encode_save(p,stock_buyer.checkpoint()));forged_merger["checkpoint"]["world"]["competition"]["acquisitions"][0]["cash"]=1;corrupt(p,forged_merger);
    auto encoded=Json::parse(persistence::encode_save(p,game.checkpoint()));auto forged=encoded;forged["checkpoint"]["world"]["simulation"]["management"]["governance"]["grants"][0]["unvested"]=1;corrupt(p,forged);
    std::cout<<checks<<" corporate governance checks passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<"governance test: "<<e.what()<<'\n';return 1;}
}
