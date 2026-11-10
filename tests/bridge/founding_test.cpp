#include "founding_controller.hpp"
#include "world_controller.hpp"
#include "management_controller.hpp"
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        using namespace pgp;
        const auto check=[](bool okay,const char* why){if(!okay) throw std::runtime_error(why);};
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_F04_SOURCE,content::maximum_document_bytes),registry),registry);
        QTemporaryDir scratch;
        const auto path=std::filesystem::path{scratch.path().toStdString()}/"founding.json";
        bridge::FoundingController setup{program.collection,path};
        setup.setField("gender","woman");setup.setField("pronouns","she/her");
        setup.setField("gender","arbitrary");setup.setField("pronouns","arbitrary");
        check(setup.form().value("gender")=="woman" && setup.form().value("pronouns")=="she/her","Identity must use structured options.");
        check(!setup.answerText("gender","free text").isEmpty(),"Text entry must not bypass identity choices.");
        setup.setField("pronouns","xe/xem");
        check(setup.form().value("pronouns")=="xe/xem","Additional pronouns must persist as a structured selection.");
        setup.setField("pronouns","name only");
        check(setup.form().value("pronouns")=="xe/xem","A name is not a pronoun option.");
        const auto initial=setup.portrait();
        for(int i=0;i<23;++i) setup.cycleAppearance(i,1);
        check(setup.portrait()!=initial,"Every appearance layer must be editable.");
        setup.setBeat("appearance");
        bridge::FoundingController reopened{program.collection,path};
        check(reopened.portrait()==setup.portrait() && reopened.beat()=="appearance","Current-build drafts must retain the edited portrait.");
        setup.setField("family","rich");setup.setField("route","bootstrap");
        check(setup.backers().empty() && setup.form().value("office")=="home","A self-funded founder must not receive unsolicited backers.");
        for(const auto& row:setup.chapters()) {
            const auto chapter=row.toMap();
            setup.choose(chapter.value("id").toString(),chapter.value("choices").toList().front().toMap().value("id").toString());
        }
        setup.setField("investment","100000");
        setup.setField("charter","charter.independent_lab");
        check(setup.signingError().isEmpty(),"A reviewed investment within personal savings must be signable.");
        check(setup.preview().contains("employee program $1,500.00/mo"),"The signing preview must disclose optional bylaws' actual recurring cost.");
        setup.setBeat("home");setup.setField("home_app","formation");
        bridge::FoundingController home_draft{program.collection,path};
        check(home_draft.beat()=="home" && home_draft.form().value("investment")=="100000" && home_draft.form().value("home_app")=="formation","Pre-company home and reviewed investment must survive a restart without founding.");
        runtime::ScenarioOptions options;
        options.founding=setup.selection();
        options.charter=setup.form().value("charter").toString().toStdString();
        options.identity=runtime::CampaignIdentity{"Test lab","Alex Vale","she/her"};
        const auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        const auto& catalog=*std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data).founding;
        const auto terms=content::founding_terms(catalog,*options.founding);
        check(core::active_charter(checkpoint.world.simulation) && core::active_charter(checkpoint.world.simulation)->id==options.charter,"Founding bylaws must become the actual active charter.");
        check(terms.personal_contribution_cents==10000000 && checkpoint.world.simulation.succession->personal_cash==core::Cents{terms.personal_remaining_cents},"Only the chosen investment enters company funds; remaining savings stay personal.");
        check(persistence::decode_save(program,persistence::encode_save(program,checkpoint)).checkpoint==checkpoint,"Personal savings and appearance styling must reload exactly.");
        bridge::WorldController world{std::filesystem::path{scratch.path().toStdString()}/"world"};
        world.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,checkpoint,{},true,false}),program,true);
        check(world.founder().value("portrait")==setup.portrait(),"The world profile must show the customized portrait.");
        check(world.founder().value("bio").toString().contains("Actual Genius"),"The founder's leg up must appear in the profile.");
        bridge::ManagementController management;
        management.update(std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{1,checkpoint,{},true,false}),program,true);
        check(management.recommended().value("trainingRate").toString()=="0" && management.recommended().value("testingRate").toString()=="0","The lab must open without inventing or dereferencing missing compute.");
        setup.setField("route","investment");
        check(!setup.backers().empty() && setup.answerOffer("commercial",true).isEmpty(),"The investment route must solicit and accept a named offer.");
        setup.setField("route","hired_ceo");
        check(setup.form().value("charter").toString().isEmpty(),"A CEO application must clear optional founding bylaws rather than rewriting an existing company.");
        check(setup.backers().empty() && setup.form().value("backers").toStringList().empty() && setup.form().value("company")=="Alder Research","Changing route must discard incompatible funding and identify the existing company.");
        std::cout<<"Structured identity, appearance editing, current drafts, founder profile and route dependencies passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
