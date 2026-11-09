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
        const auto initial=setup.portrait();
        for(int i=0;i<15;++i) setup.cycleAppearance(i,1);
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
        runtime::ScenarioOptions options;
        options.founding=setup.selection();
        options.identity=runtime::CampaignIdentity{"Test lab","Alex Vale","she/her"};
        const auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
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
        check(setup.backers().empty() && setup.form().value("backers").toStringList().empty() && setup.form().value("company")=="Alder Research","Changing route must discard incompatible funding and identify the existing company.");
        std::cout<<"Structured identity, appearance editing, current drafts, founder profile and route dependencies passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
