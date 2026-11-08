#include "world_controller.hpp"
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <chrono>
#include <iostream>

using namespace pgp;
namespace {void check(bool value,const char* reason){if(!value) throw std::runtime_error(reason);}}
class DraftCampaign final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool currentCampaign READ currentCampaign CONSTANT)
    Q_PROPERTY(QObject* world READ world CONSTANT)
public:
    explicit DraftCampaign(bridge::WorldController& world):world_(world){}
    bool currentCampaign() const {return true;}
    QObject* world() const {return &world_;}
private:
    bridge::WorldController& world_;
};
int main(int argc,char** argv) {
    QGuiApplication app{argc,argv};QQuickStyle::setStyle(QStringLiteral("Basic"));
    const auto drain=[] {for(int pass=0;pass<4;++pass)QCoreApplication::processEvents();};
    const auto root=std::filesystem::path{PGP_FORM_SCRATCH}/("drafts-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_FORM_CONTENT,content::maximum_document_bytes),registry),registry);
        runtime::ScenarioOptions options;options.seed=42;options.identity=runtime::CampaignIdentity{"Same company","Same founder","they/them","00000000000000000000000000000001"};
        const auto original=runtime::begin_scenario(program,"scenario.first_playable","US-CA",{},options);
        const auto snapshot=[&](std::uint64_t generation,runtime::Checkpoint checkpoint){return std::make_shared<const bridge::CampaignSnapshot>(bridge::CampaignSnapshot{generation,std::move(checkpoint),{},true,true});};
        bridge::WorldController world{root};world.update(snapshot(1,original),program,true);
        DraftCampaign context{world};QQmlEngine engine;engine.rootContext()->setContextProperty(QStringLiteral("campaign"),&context);
        QQmlComponent component{&engine};
        component.setData("import QtQuick\nimport QtQuick.Controls\nDraftField {objectName: \"unitField\";defaultText: \"17\"}",QUrl::fromLocalFile(QStringLiteral(PGP_FORM_QML)+"/FormDraftTest.qml"));
        std::unique_ptr<QObject> field{component.create()};
        if(!field)throw std::runtime_error(component.errorString().toStdString());drain();
        check(field->property("text")=="17","Fresh field uses its declared default");
        field->setProperty("text","37");check(QMetaObject::invokeMethod(field.get(),"textEdited"),"Edit actual DraftField");drain();
        field->setProperty("defaultText","21");drain();
        check(field->property("text")=="37","A changed recommendation must not overwrite an edited draft");
        world.setDraft("form/choice/recipe","\"recipe.small\"");
        QJsonArray topics;for(int index=0;index<32;++index)topics.append(QStringLiteral("research.%1.").arg(index)+QString(100,QLatin1Char('a')));
        const auto ordered=QString::fromUtf8(QJsonDocument{topics}.toJson(QJsonDocument::Compact));
        check(ordered.size()>1024,"Queue fixture exercises a larger bounded draft");
        world.setDraft("form/frontier/researchQueue",ordered);
        world.update(snapshot(2,original),program,true);
        check(world.draft("form/frontier/researchQueue",{})==ordered,"Ordered draft survives reopening without truncation");
        auto other=original;other.world.identity->instance_id="00000000000000000000000000000002";
        world.update(snapshot(3,other),program,true);
        drain();check(field->property("text")=="21","A new company must reset text to its current default");
        check(world.draft("form/choice/recipe","default")=="default","A new company with the same name and seed must not inherit drafts");
        world.setDraft("form/choice/recipe","\"recipe.other\"");
        world.update(snapshot(4,original),program,true);
        drain();check(field->property("text")=="37","Returning to the original company restores its own edited text");
        check(world.draft("form/choice/recipe",{})=="\"recipe.small\"","Reopening the first company retains its own selections");
        int full_notices=0;QObject::connect(&world,&bridge::WorldController::noticeRaised,&world,[&](auto){++full_notices;});
        for(int index=0;index<40;++index)world.setDraft(QStringLiteral("form/large/%1").arg(index),QString(8192,QLatin1Char('b')));
        check(full_notices>0 && world.draft("form/choice/recipe",{})=="\"recipe.small\"","Full storage explains refusal while retaining old drafts");
        for(const auto& entry:std::filesystem::directory_iterator(root)) if(entry.path().extension()==".drafts")
            check(std::filesystem::file_size(entry.path())<=256*1024,"A written draft file must fit its own read bound");
        world.update(snapshot(5,original),program,true);
        check(world.draft("form/frontier/researchQueue",{})==ordered && world.draft("form/choice/recipe",{})=="\"recipe.small\"","Limit failures retain readable original values");
        world.setDraft("form/oversized",QString(8193,QLatin1Char('c')));
        check(world.draft("form/oversized",{}).isEmpty(),"Oversized individual draft is refused");
        std::filesystem::remove_all(root);std::cout<<"Company isolation, ordered draft reopening and bounded failure preservation passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
#include "form_drafts_test.moc"
