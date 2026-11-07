#include "management_controller.hpp"
#include "society_controller.hpp"
#include <pgp/session/slice_run.hpp>
#include <pgp/runtime/scenario.hpp>
#include <QCoreApplication>
#include <fstream>
#include <iostream>
namespace {
using namespace pgp;
int checks=0;
void check(bool condition,const char* why) {++checks;if (!condition) throw std::runtime_error(why);}
void run() {
    std::ifstream file{PGP_SLICE_CONTENT_PATH};const std::string bytes{std::istreambuf_iterator<char>{file},{}};
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(bytes,registry),registry);
    auto initial=runtime::begin_scenario(program,"scenario.first_playable","US-CA");
    bridge::ManagementController management;
    auto snapshot=std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,initial,{},true});
    management.update(snapshot,program,true);
    check(management.funding()->rowCount()==4 && management.specialists()->rowCount()==1 && management.policyDefaults().value(QStringLiteral("budget")).toString()==QStringLiteral("100000.00"),"Player funding/team/policy defaults differ from content");
    check(management.available() && management.dataOffers()->rowCount()==4 && management.researchTopics()->rowCount()==6,"Player catalog missing authored options");
    check(management.recipes().front().toMap().value(QStringLiteral("id")).toString()==QStringLiteral("recipe.document_baseline"),"Opening recipe starts on a locked research branch");
    check(management.evaluations().front().toMap().value(QStringLiteral("id")).toString()==QStringLiteral("evaluation.held_out"),"Default evaluation cannot qualify the opening service");
    const auto preview=management.trainingPreview(QStringLiteral("recipe.document_baseline"),QStringLiteral("lease.initial"),{QStringLiteral("data.licensed_documents")},QStringLiteral("60"),QStringLiteral("4"),QStringLiteral("3600"));
    check(!preview.value(QStringLiteral("ready")).toBool() && preview.value(QStringLiteral("text")).toString().contains(QStringLiteral("$12,000.00")),"Unlicensed preview omitted cost or permitted training");
    for (const auto& bad:{QStringLiteral("1e6"),QStringLiteral("-1"),QStringLiteral("0.0001"),QStringLiteral("99999999999999999")}) {
        const auto rejected=management.trainingPreview(QStringLiteral("recipe.document_baseline"),QStringLiteral("lease.initial"),{},bad,QStringLiteral("4"),QStringLiteral("3600"));
        check(!rejected.value(QStringLiteral("ready")).toBool() && !rejected.value(QStringLiteral("text")).toString().isEmpty(),"Malformed player amount silently coerced");
    }
    check(snapshot->checkpoint==initial,"Preview changed a checkpoint or random stream");
    auto advanced=session::run_slice_baseline(program,core::OrdinalDay{80}).checkpoint;
    auto later=std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,advanced,{},true});
    management.update(later,program,false);
    check(management.models()->rowCount()==2 && management.evidence()->rowCount()==1 && management.products()->rowCount()==1,"Actual company work is missing from management models");
    const auto evidence=management.evidence()->selectedRow();
    check(evidence.value(QStringLiteral("detail")).toString().contains(QStringLiteral("scanned tables")) && evidence.value(QStringLiteral("fields")).toMap().value(QStringLiteral("independent")).toBool(),"Concrete subgroup result or independence missing");
    const auto fields=management.models()->selectedRow().value(QStringLiteral("fields")).toMap();
    check(!fields.contains(QStringLiteral("capability")) && !fields.contains(QStringLiteral("random")),"Private model truth escaped through player fields");
    const auto review=management.servicePreview(QStringLiteral("product.documents_tables"),QStringLiteral("baseline"),true);
    check(review.contains(QStringLiteral("$20,000.00")) && review.contains(QStringLiteral("500 tasks/day")) && review.contains(QStringLiteral("35.00%")),"Review preview omitted actual staff/throughput/cost qualification tradeoff");
    const auto selected=management.models()->selectedId();int resets=0;
    QObject::connect(management.models(),&QAbstractItemModel::modelReset,[&]{++resets;});
    management.update(later,program,false);
    check(resets==0 && management.models()->selectedId()==selected,"Daily delivery reset a model or lost selection");
    int commands=0;QObject::connect(&management,&bridge::ManagementController::actionRequested,[&](const core::CommandAction&){++commands;});
    management.train(QStringLiteral("recipe.document_baseline"),QStringLiteral("lease.initial"),{},QStringLiteral("1e6"),QStringLiteral("4"),QStringLiteral("3600"));
    check(commands==0,"Malformed amount reached an authoritative command");
    management.acceptOffer(QStringLiteral("offer.enterprise_service"));
    check(commands==1 && later->checkpoint==advanced,"Action adapter mutated the snapshot instead of queuing a command");
    management.authorizeEvaluation(QStringLiteral("baseline"),QStringLiteral("evaluation.held_out"),QStringLiteral("lease.initial"),QStringLiteral("20"),QStringLiteral("1"),QStringLiteral("1e9"),QStringLiteral("100000"),QStringLiteral("30"),QStringLiteral("2"));
    check(commands==1,"Policy dollar parser accepted exponent notation");
    management.authorizeEvaluation(QStringLiteral("baseline"),QStringLiteral("evaluation.held_out"),QStringLiteral("lease.initial"),QStringLiteral("20"),QStringLiteral("1"),QStringLiteral("4000.00"),QStringLiteral("100000"),QStringLiteral("30"),QStringLiteral("2"));
    check(commands==2 && later->checkpoint==advanced,"Policy form mutated authority or failed to emit its bounded command");
    bridge::ManagementController pricing;pricing.update(later,program,true);
    check(!pricing.pricedProducts().empty(),"Operating paid products are missing from pricing choices");
    std::optional<core::CommandAction> pricing_command;
    QObject::connect(&pricing,&bridge::ManagementController::actionRequested,[&](core::CommandAction action){pricing_command=std::move(action);});
    const auto product=pricing.pricedProducts().front().toMap();const auto product_id=product.value(QStringLiteral("id")).toString();
    pricing.authorizePricing(product_id,QStringLiteral("1e6"),QStringLiteral("100"),QStringLiteral("0.05"),QStringLiteral("25"),QStringLiteral("85"),QStringLiteral("30"),QStringLiteral("1200"),QStringLiteral("0"));
    check(!pricing_command,"Pricing form accepted an exponent amount");
    pricing.authorizePricing(product_id,QStringLiteral("0"),QStringLiteral("100"),QStringLiteral("0.05"),QStringLiteral("25"),QStringLiteral("85"),QStringLiteral("30"),QStringLiteral("1200"),QStringLiteral("0"));
    check(pricing_command && std::get<core::AddDelegationPolicy>(*pricing_command).policy.kind==core::DelegationKind::pricing &&
        std::get<core::AddDelegationPolicy>(*pricing_command).policy.maximum_runs==1200 && later->checkpoint==advanced,"Pricing form lost finite authorization or mutated its snapshot");
    check(pricing.pricingPreview(product_id,QStringLiteral("0"),QStringLiteral("100"),QStringLiteral("0.05"),QStringLiteral("25"),QStringLiteral("85")).contains(QStringLiteral("Proposed quote")),"Price preview omitted its quote and settled evidence");
    auto paged=advanced;
    for (int i=0;i<129;++i) paged.world.simulation.management->decisions.push_back({"presentation.fixture","","Dated decision",core::OrdinalDay{80},core::Cents{0},false});
    pricing.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,paged,{},true}),program,true);
    check(pricing.decisionPageCount()==2 && pricing.decisions()->rowCount()==128 && pricing.decisions()->idAt(0)==QStringLiteral("decision:65408"),"Decision log did not bound the newest page");
    pricing.setDecisionPage(1);
    check(pricing.decisions()->rowCount()==1 && pricing.decisions()->idAt(0)==QStringLiteral("decision:65536"),"Older decisions are inaccessible");
    pricing.setDecisionPage(0);check(pricing.decisions()->rowCount()==128,"Newest decision page could not be restored");
    check(management.scoreText().contains(QStringLiteral("Provisional")) && management.scoreText().contains(QStringLiteral("founder")),"Score presentation hid its provisional status or ownership tradeoff");
    bridge::SocietyController society;
    society.update(later,true);society.regions()->setSelectedId(QStringLiteral("region.us"));
    check(society.regions()->rowCount()==8 && society.global().contains(QStringLiteral("lowest region")) &&
        society.audiences().contains(QStringLiteral("Communities")),"World controls omitted a region, minimum or named audience");
    const auto services=society.services();check(!services.empty(),"A deployed service with real evidence is absent from media choices");
    int society_commands=0;core::CommandAction last=core::SetPaused{true};
    QObject::connect(&society,&bridge::SocietyController::actionRequested,[&](core::CommandAction action){++society_commands;last=std::move(action);});
    society.reviewMedia(services.front().toMap().value("id").toString(),2);
    check(!society.reviewText().isEmpty() && society_commands==0 && later->checkpoint==advanced,"Media review mutated authority or emitted a payment");
    society.confirmAction();
    check(society_commands==1 && std::holds_alternative<core::BuyMediaCampaign>(last) &&
        std::get<core::BuyMediaCampaign>(last).region=="region.us", "Media confirmation lost the reviewed region or command");
    society.reviewMedia(services.front().toMap().value("id").toString(),2);
    auto changed=advanced;++changed.world.simulation.revision;
    auto incoming=std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,changed,{},true});
    society.update(incoming,false);society.confirmAction();
    check(society_commands==1 && society.reviewText().isEmpty() && society.regions()->selectedId()==QStringLiteral("region.us"),
        "A stale media review survived a new revision or changed region selection");
    society.reviewEndorsement(society.evidence().front().toString(),true);society.confirmAction();
    check(society_commands==2 && std::holds_alternative<core::EndorseForumStandard>(last) && later->checkpoint==advanced,
        "Forum review mutated the snapshot or omitted its typed disclosure");
    changed.world.simulation.paused=false;
    society.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,changed,{},true}),false);
    society.reviewMedia(services.front().toMap().value("id").toString(),2);
    check(!society.reviewText().isEmpty(),"A stopped calendar driver disabled World review controls");
}
}
int main(int argc,char** argv) {QCoreApplication application{argc,argv};try {run();std::cout<<checks<<" management checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
