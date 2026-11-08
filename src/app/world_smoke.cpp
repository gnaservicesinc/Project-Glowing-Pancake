#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/runtime/objectives.hpp>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QtTest/QTest>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <algorithm>
#include <iostream>
#include <memory>
namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->objectName()==name && root->isVisible()) return root;
    for(auto* child:root->childItems()) if(auto* item=find(child,name)) return item;
    return nullptr;
}
bool press(QQuickWindow& window,QQuickItem* item) {
    if(!item || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down{QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier},up{QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&down);QCoreApplication::sendEvent(&window,&up);return true;
}
QQuickItem* buttonText(QQuickItem* root,const QString& text) {
    if(root->isVisible() && root->metaObject()->indexOfProperty("down")>=0 && root->property("text").toString()==text) return root;
    for(auto* child:root->childItems()) if(auto* item=buttonText(child,text)) return item;
    return nullptr;
}
}
void schedule_world_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot,bool portraits,bool workplaces,bool devices,bool contacts,bool managers) {
    struct State {int phase=0,ticks=0,portraitPixels=0,constructionDays=0;QString revision;bool resumed=false,workplaceDone=false;};
    auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(150);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot,portraits,workplaces,devices,contacts,managers] {
        const auto fail=[&](const char* message){std::cerr<<"World check: "<<message<<" / "<<campaign.notice().toStdString()<<"\n";if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-failed.png");timer->stop();QCoreApplication::exit(3);};
        if(++state->ticks>200) {fail("Timeout");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto click=[&](const char* name){return press(window,find(window.contentItem(),QString::fromUtf8(name)));};
        if(state->phase==0) {
            state->resumed=campaign.currentCampaign();
            state->workplaceDone=state->resumed;
            if(devices && state->resumed && (campaign.world()->navigation().value("app")!="lab" || window.property("room").toString()!="lab" || window.property("activeDevice").toString()!="tablet" || !window.property("deviceOpen").toBool())) {fail("Tablet application navigation did not restore automatically");return;}
            if(state->resumed && window.property("menuVisible").toBool()) {fail("Launch did not resume automatically");return;}
            if(!state->resumed) {
                auto* founding=campaign.founding();founding->reset();founding->setField("family","rich");
                if(workplaces) founding->setField("office","home");
                for(const auto& chapter:founding->chapters()) {
                    const auto row=chapter.toMap();const auto choices=row.value("choices").toList();
                    founding->choose(row.value("id").toString(),choices.front().toMap().value("id").toString());
                }
                campaign.startFounderCampaign();
            }
            state->phase=1;return;
        }
        if(state->phase==1) {
            if(!campaign.currentCampaign() || campaign.storageBlocked()) {fail("Continuing company unavailable");return;}
            campaign.clearNotice();
            window.setProperty("menuVisible",false);window.setProperty("sessionEntered",true);
            QMetaObject::invokeMethod(&window,"closeApp");
            window.showNormal();window.resize(portraits?1920:1280,portraits?1080:800);
            if((workplaces || devices || contacts || managers) && campaign.textScalePercent()>=200) window.resize(650,480);
            state->phase=2;return;
        }
        if(state->phase==2) {
            if(managers) {
                if(state->resumed) {
                    const auto& s=campaign.snapshot()->checkpoint.world.simulation;
                    if(!pgp::core::operations_manager_on(s,s.date) || pgp::core::operations_availability(s,s.date)<=pgp::core::BasisPoints{8500} || campaign.world()->draft("people/page","")!="branches") {fail("Manager, delivered guidance and People page did not resume");return;}
                    if(!click("peopleTab")) {fail("Resume People");return;}state->phase=510;return;
                }
                if(!click("peopleTab")) {fail("Open HQ people");return;}state->phase=500;return;
            }
            if(contacts) {
                if(state->resumed) {
                    if(!campaign.world()->nationalAccess() || !campaign.world()->worldAccess() || campaign.world()->overseasOffices().size()!=1) {fail("Overseas offices and contacts did not resume");return;}
                    std::cout<<"Paid overseas office, opening history and world contacts resumed exactly\n";timer->stop();QCoreApplication::exit(0);return;
                }
                if(!click("headquartersTab")) {fail("Headquarters and contacts app");return;}state->phase=400;return;
            }
            if(devices) {
                if(state->resumed) {
                    const auto rows=campaign.world()->deviceInventory();
                    if(rows.size()!=4 || rows[1].toMap().value("powered").toBool() || !rows[3].toMap().value("available").toBool()) {fail("Equipment power and delivered tablet did not resume");return;}
                    std::cout<<"Owned equipment, power and delivery resumed exactly\n";timer->stop();QCoreApplication::exit(0);return;
                }
                if(!click("devicesButton")) {fail("Device launcher");return;}state->phase=300;return;
            }
            if(workplaces && state->resumed) {
                const auto& s=campaign.snapshot()->checkpoint.world.simulation;
                if(!s.infrastructure->workplace || s.infrastructure->workplace->rooms.size()!=1 || s.infrastructure->workplace->blueprints.size()!=1 || s.infrastructure->workplace->builds.size()!=1 || pgp::core::headquarters_seats(s)!=8 || campaign.world()->navigation().value("place")!="research_annex") {fail("Persisted workplace and annex position");return;}
                std::cout<<"Paid headquarters, blueprint orders and annex navigation resumed exactly\n";timer->stop();QCoreApplication::exit(0);return;
            }
            if(workplaces && !state->workplaceDone) {
                if(!click("headquartersTab")) {fail("Headquarters planning app");return;}state->phase=200;return;
            }
            if(!screenshot.isEmpty() && !window.grabWindow().save(screenshot+"-world.png")) {fail("World screenshot");return;}
            if(!click("peopleTab")) {fail("People dock navigation");return;}state->phase=3;return;
        }
        if(state->phase==500) {
            if(!click("peopleBranchesTab")) {fail("Branch departments tab");return;}state->phase=501;return;
        }
        if(state->phase==501) {
            if(!find(window.contentItem(),"managerStatus") || !click("reviewOperationsManager")) {fail("Review an available HQ manager");return;}state->phase=502;return;
        }
        if(state->phase==502) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-manager-quote.png");
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Confirm actual manager resources");return;}state->phase=503;return;
        }
        if(state->phase==503) {
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(!pgp::core::operations_manager_on(s,s.date) || pgp::core::operations_availability(s,s.date)!=pgp::core::BasisPoints{8500}) {fail("A new title cannot grant instant branch output");return;}
            if(!click("managerPortrait")) {fail("Manager portrait interaction");return;}state->phase=504;return;
        }
        if(state->phase==504) {
            auto* dialog=window.findChild<QObject*>("employeeProfileDialog");auto* portrait=find(window.contentItem(),"profilePortrait");
            if(!dialog || !dialog->property("visible").toBool() || !portrait || portrait->property("portraitSource").toString().isEmpty()) {fail("Manager opens the actual employee dossier");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-manager-profile.png");
            QMetaObject::invokeMethod(dialog,"close");campaign.step();state->phase=505;return;
        }
        if(state->phase==505) {
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(pgp::core::operations_availability(s,s.date)<=pgp::core::BasisPoints{8500}) {fail("Actual manager work must improve guidance the following morning");return;}
            if(!click("releaseOperationsManager")) {fail("Review manager release");return;}state->phase=506;return;
        }
        if(state->phase==506) {
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Confirm manager release");return;}state->phase=507;return;
        }
        if(state->phase==507) {
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(pgp::core::operations_manager_on(s,s.date) || pgp::core::operations_availability(s,s.date)!=pgp::core::BasisPoints{8500}) {fail("Release must return the worker and coordination reservation");return;}
            if(!click("reviewOperationsManager")) {fail("Reassign manager");return;}state->phase=508;return;
        }
        if(state->phase==508) {
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Confirm manager reassignment");return;}state->phase=509;return;
        }
        if(state->phase==509) {campaign.step();state->phase=510;return;}
        if(state->phase==510) {
            auto* status=find(window.contentItem(),"managerStatus");if(!status || campaign.storageBlocked()) {fail("Manager status, page persistence and durable commands");return;}
            status->forceActiveFocus(Qt::TabFocusReason);state->phase=511;return;
        }
        if(state->phase==511) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-manager-active.png");
            std::cout<<"Manager quote, real work delivery, portrait dossier, release/reassignment and People page persistence passed; resumed="<<state->resumed<<"\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(state->phase==400) {
            if(!campaign.world()->nationalAccess() || campaign.world()->worldAccess()) {fail("Background and overseas progression are distinct");return;}
            if(!click("overseasOfficePlanning")) {fail("Open overseas planning");return;}state->phase=401;return;
        }
        if(state->phase==401) {campaign.togglePaused();state->phase=402;return;}
        if(state->phase==402) {
            if(campaign.paused()) {fail("Exercise office quote from running time");return;}
            if(!click("reviewOffice_liaison.canada")) {fail("Review overseas commitment");return;}state->phase=403;return;
        }
        if(state->phase==403) {
            if(!campaign.paused()) return;
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-office-quote.png");
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Confirm the current paused office quote");return;}state->phase=404;return;
        }
        if(state->phase==404) {
            if(campaign.world()->overseasOffices().size()!=1 || campaign.world()->worldAccess()) {fail("Paid office must await opening");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-opening.png");state->phase=405;return;
        }
        if(state->phase==405) {
            if(state->constructionDays<14) {campaign.step();++state->constructionDays;return;}
            if(!campaign.world()->worldAccess()) {fail("Actual overseas opening must unlock world contacts");return;}
            auto* office=find(window.contentItem(),"closeOffice_1");if(!office || !office->isEnabled()) {fail("Inspect the operating office");return;}
            office->forceActiveFocus(Qt::TabFocusReason);state->phase=406;return;
        }
        if(state->phase==406) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-contacts.png");
            std::cout<<"Live-time pause before quote, paid liaison office, opening delay and national/world progression passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(state->phase==300) {
            if(!click("equipmentApp")) {fail("Equipment launcher entry");return;}state->phase=301;return;
        }
        if(state->phase==301) {
            if(window.property("room").toString()!="devices" || campaign.world()->deviceInventory().size()!=3) {fail("Owned starter equipment");return;}
            if(!click("devicePower2")) {fail("Laptop power control");return;}state->phase=302;return;
        }
        if(state->phase==302) {
            if(campaign.world()->deviceInventory()[1].toMap().value("powered").toBool()) {fail("Laptop power command");return;}
            if(!click("buyDevicetablet")) {fail("Tablet purchase review");return;}state->phase=303;return;
        }
        if(state->phase==303) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-purchase.png");
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Confirm device purchase");return;}state->phase=304;return;
        }
        if(state->phase==304) {
            if(campaign.world()->deviceInventory().size()!=4 || campaign.world()->deviceInventory()[3].toMap().value("available").toBool()) {fail("Paid tablet needs delivery");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-equipment.png");
            campaign.step();state->phase=305;return;
        }
        if(state->phase==305) {campaign.step();state->phase=306;return;}
        if(state->phase==306) {
            if(!campaign.world()->deviceInventory()[3].toMap().value("available").toBool()) {fail("Delivered tablet");return;}
            if(!click("closeDeviceButton") || !click("devicesButton")) {fail("Return to device launcher");return;}state->phase=307;return;
        }
        if(state->phase==307) {
            if(!click("launcherDevice_tablet")) {fail("Choose available tablet");return;}state->phase=308;return;
        }
        if(state->phase==308) {
            if(!click("labApp")) {fail("Launch tablet research app");return;}state->phase=309;return;
        }
        if(state->phase==309) {
            if(window.property("activeDevice").toString()!="tablet" || window.property("room").toString()!="lab") {fail("App header must match its available compatible device");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-tablet.png");
            if(campaign.storageBlocked()) {fail("Durable device commands");return;}
            std::cout<<"Equipment launcher, power controls, reviewed purchase, delayed delivery and tablet app routing passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(state->phase==200) {
            if(window.property("room").toString()!="facilities" || campaign.world()->roomOffers().size()!=3) {fail("Headquarters room catalog");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-planning.png");
            if(!click("reviewRoom_hq.research")) {fail("Review room construction");return;}state->phase=201;return;
        }
        if(state->phase==201) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-room-review.png");
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Authorize room fit-out");return;}state->phase=202;return;
        }
        if(state->phase==202) {
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(!s.infrastructure->workplace || s.infrastructure->workplace->rooms.size()!=1 || pgp::core::headquarters_seats(s)!=2) {fail("Room order and delayed capacity");return;}
            if(!click("saveFacilityBlueprint")) {fail("Save facility blueprint");return;}state->phase=203;return;
        }
        if(state->phase==203) {
            if(campaign.world()->blueprints().size()!=1 || !click("reviewBlueprintBuild")) {fail("Review saved blueprint");return;}state->phase=204;return;
        }
        if(state->phase==204) {
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-blueprint-review.png");
            if(!press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) {fail("Authorize complete facility");return;}state->phase=205;return;
        }
        if(state->phase==205) {
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(s.infrastructure->sites.size()!=1 || s.infrastructure->hardware.size()!=1 || s.infrastructure->hires.size()!=1) {fail("Physical blueprint orders");return;}
            state->phase=206;return;
        }
        if(state->phase==206) {
            if(state->constructionDays<14) {campaign.step();++state->constructionDays;return;}
            const auto& s=campaign.snapshot()->checkpoint.world.simulation;
            if(pgp::core::headquarters_seats(s)!=8) {fail("Completed room workstations");return;}
            if(!click("closeDeviceButton")) {fail("Return to expanded HQ");return;}
            campaign.world()->navigate("research_annex","hq","","","");state->phase=207;return;
        }
        if(state->phase==207) {
            if(campaign.world()->navigation().value("place")!="research_annex") {fail("Paid annex navigation");return;}
            if(campaign.requiredKind()!=QStringLiteral("content") || campaign.canAdvance()) {fail("Prepared decision must stop further advancement");return;}
            if(campaign.textScalePercent()>=200) {
                auto* picker=find(window.contentItem(),"compactRoomPicker");auto* dock=find(window.contentItem(),"officeTab");
                if(!picker || !dock || picker->mapToScene(QPointF{0,picker->height()}).y()>dock->mapToScene(QPointF{0,0}).y()) {fail("Compact room controls overlap the dock");return;}
            }
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-annex.png");
            if(campaign.textScalePercent()>=200) {
                if(!click("mapTab")) {fail("Compact dock keyboard navigation");return;}state->phase=208;return;
            }
            std::cout<<"Paid room construction, complete blueprint purchase, prepared decision stop and 3D annex navigation passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(state->phase==208) {
            if(!find(window.contentItem(),"compactMapScale")) {fail("Compact map scale selector");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-compact-map.png");
            campaign.world()->navigate("research_annex","hq","","","");state->phase=209;return;
        }
        if(state->phase==209) {
            if(campaign.world()->navigation().value("place")!="research_annex") {fail("Return from compact map");return;}
            std::cout<<"Paid room construction, blueprint purchase, compact HUD/map controls and annex navigation passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(state->phase==3) {
            if(window.property("room").toString()!="people" || !window.property("deviceOpen").toBool()) {fail("People app failed to open");return;}
            if(!click("peopleApplicantsTab")) {fail("Applicant navigation");return;}state->phase=30;return;
        }
        if(state->phase==30) {
            if(campaign.world()->applicants().size()!=4) {fail("Authored applicant cohort");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-applicants.png");
            if(portraits) {
                const auto name="personPortrait_"+campaign.world()->applicants().front().toMap().value("id").toString();
                auto* portrait=find(window.contentItem(),name);auto* image=find(window.contentItem(),name+"Image");
                if(!portrait || !image || image->property("status").toInt()!=1 || !press(window,portrait)) {fail("Portrait rendering and keyboard activation");return;}
                state->phase=31;return;
            }
            if(!click("closeDeviceButton")) {fail("Close device");return;}state->phase=4;return;
        }
        if(state->phase==31) {
            auto* dialog=window.findChild<QObject*>("employeeProfileDialog");auto* image=find(window.contentItem(),"profilePortraitImage");
            if(!dialog || !dialog->property("visible").toBool() || !image || image->property("status").toInt()!=1) {fail("Enlarged employee profile");return;}
            state->portraitPixels=image->property("sourceSize").toSize().width();
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-profile-1080p.png");
            window.resize(7680,4320);state->phase=32;return;
        }
        if(state->phase==32) {
            auto* image=find(window.contentItem(),"profilePortraitImage");
            if(window.width()==7680 && window.height()<4320) {
                std::cout<<"Native window constrained by the attached display to "<<window.width()<<"x"<<window.height()<<"; use the offscreen software run for the 8K viewport check.\n";
                campaign.setTextScalePercent(200);state->phase=33;return;
            }
            if(window.width()!=7680 || window.height()!=4320 || !image || image->property("status").toInt()!=1 || image->property("sourceSize").toSize().width()<=state->portraitPixels) {
                std::cerr<<"Requested 7680x4320; actual "<<window.width()<<"x"<<window.height()<<"; portrait pixels "<<(image?image->property("sourceSize").toSize().width():0)<<" vs "<<state->portraitPixels<<'\n';
                fail("8K viewport and vector resampling");return;
            }
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-profile-8k.png");
            campaign.setTextScalePercent(200);state->phase=33;return;
        }
        if(state->phase==33) {window.showNormal();window.resize(650,480);state->phase=34;return;}
        if(state->phase==34) {
            auto* dialog=window.findChild<QObject*>("employeeProfileDialog");
            if(!dialog || dialog->property("width").toReal()>650 || dialog->property("height").toReal()>480) {
                std::cerr<<"Narrow viewport "<<window.width()<<"x"<<window.height()<<"; dialog "<<(dialog?dialog->property("width").toReal():0)<<"x"<<(dialog?dialog->property("height").toReal():0)<<'\n';
                if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-profile-narrow-failed.png");
                fail("Narrow high-text profile bounds");return;
            }
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-profile-narrow.png");
            auto* scroll=find(window.contentItem(),"employeeProfileScroll");
            if(!scroll) {fail("Profile scroll area");return;}
            const auto position=scroll->mapToScene(QPointF{scroll->width()/2,scroll->height()/2});
            QWheelEvent wheel{position,window.mapToGlobal(position),QPoint{},QPoint{0,-960},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false};
            QCoreApplication::sendEvent(&window,&wheel);state->phase=341;return;
        }
        if(state->phase==341) {
            auto* scroll=find(window.contentItem(),"employeeProfileScroll");
            if(!scroll || scroll->property("scrollOffset").toReal()<=0) {fail("Narrow profile wheel scrolling");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-profile-narrow-scrolled.png");
            auto* dialog=window.findChild<QObject*>("employeeProfileDialog");
            if(!QMetaObject::invokeMethod(dialog,"close")) {fail("Profile close action");return;}
            campaign.setTextScalePercent(100);state->phase=35;return;
        }
        if(state->phase==35) {
            window.resize(1280,800);
            if(!click("closeDeviceButton")) {fail("Return from portrait to world");return;}state->phase=4;return;
        }
        if(state->phase==4) {
            if(!click("mapTab")) {fail("Map navigation");return;}state->phase=5;return;
        }
        if(state->phase==5) {
            if(!click("mapScale_country")) {fail("Country scale");return;}state->phase=6;return;
        }
        if(state->phase==6) {
            if(campaign.world()->navigation().value("scale")!="country") {fail("Scale persistence");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-map.png");
            if(!click("mapScale_state")) {fail("State scale");return;}state->phase=60;return;
        }
        if(state->phase==60) {
            if(campaign.world()->countyRows().size()!=58) {fail("California county projection");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-counties.png");
            if(!click("mapScale_world")) {fail("World scale");return;}state->phase=61;return;
        }
        if(state->phase==61) {
            if(campaign.world()->worldGeometry().value("countries").toList().size()!=176) {fail("World display geometry");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-global.png");
            if(!click("labTab")) {fail("Lab dock navigation");return;}state->phase=7;return;
        }
        if(state->phase==7) {
            if(window.property("room").toString()!="lab") {fail("Lab app");return;}
            auto* field=find(window.contentItem(),"researchRateFieldInput");
            if(!field) {fail("Research draft field");return;}
            if(state->resumed && field->property("text").toString()!="37") {fail("Unsubmitted field did not survive restart");return;}
            field->forceActiveFocus(Qt::TabFocusReason);QMetaObject::invokeMethod(field,"selectAll");
            QKeyEvent edit{QEvent::KeyPress,0,Qt::NoModifier,QStringLiteral("37")};QCoreApplication::sendEvent(&window,&edit);
            if(campaign.world()->draft("field/researchRateFieldInput/","")!="37") {fail("Per-keystroke field preservation");return;}
            if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-lab.png");
            if(campaign.requiredKind()==QStringLiteral("content")) {
                if(!click("inboxTab")) {fail("Prepared decision before advancement");return;}state->phase=71;return;
            }
            if(!click("closeDeviceButton")) {fail("Close lab");return;}
            state->revision=campaign.revision();
            campaign.step();state->phase=8;return;
        }
        if(state->phase==71) {
            const auto choices=campaign.requiredChoices();
            const auto decline=std::ranges::find_if(choices,[](const QVariant& value){const auto row=value.toMap();return row.value("choiceId").toString().contains("decline",Qt::CaseInsensitive) || row.value("label").toString().contains("decline",Qt::CaseInsensitive);});
            if(decline==choices.end() || !press(window,find(window.contentItem(),"flowChoice_"+decline->toMap().value("choiceId").toString()))) {fail("Decline optional cloud offer through the actual decision");return;}
            state->phase=72;return;
        }
        if(state->phase==72) {
            if(campaign.requiredKind()!=QStringLiteral("none")) {fail("Prepared choice resolution");return;}
            if(!click("closeDeviceButton")) {fail("Return after decision");return;}
            state->revision=campaign.revision();campaign.step();state->phase=8;return;
        }
        if(state->phase==8) {
            if(campaign.revision()==state->revision || campaign.storageBlocked() || !std::filesystem::exists(paths.saves_directory/"continuing/current.pgpsave")) {fail("Advance and durable checkpoint");return;}
            window.requestActivate();state->phase=81;return;
        }
        if(state->phase==81) {
            QTest::keyClick(&window,Qt::Key_Escape);
            state->phase=9;return;
        }
        if(state->phase==9) {
            if(!window.property("menuVisible").toBool() || !campaign.paused()) {
                const auto* shortcut=window.findChild<QObject*>("gameEscapeShortcut");
                std::cerr<<"Escape diagnostic: menu="<<window.property("menuVisible").toBool()<<" paused="<<campaign.paused()<<" active="<<window.isActive()<<" shortcut="<<(shortcut?shortcut->property("enabled").toBool():false)<<" focus="<<(window.activeFocusItem()?window.activeFocusItem()->metaObject()->className():"none")<<'\n';
                fail("Escape pause menu");return;
            }
            std::cout<<"World navigation, device apps, map, persistence and pause passed; resumed="<<state->resumed<<"\n";
            timer->stop();QCoreApplication::exit(0);
        }
    });timer->start();
}

void schedule_departure_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,QString screenshot) {
    using namespace pgp;
    struct Check {int phase=0,ticks=0;bool resumed=false;core::OrdinalDay handover{0};core::Cents cash{0};};
    const auto check=std::make_shared<Check>();auto* timer=new QTimer(&window);timer->setInterval(80);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,check,screenshot] {
        const auto fail=[&](const char* why){std::cerr<<"Retirement check "<<check->phase<<": "<<why<<" / "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+"-failed.png");timer->stop();QCoreApplication::exit(3);};
        if(++check->ticks>1500) {fail("Timeout");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto click=[&](const char* name){if(press(window,find(window.contentItem(),QString::fromUtf8(name))))return true;fail(name);return false;};
        const auto capture=[&](const char* suffix){if(!screenshot.isEmpty()) (void)window.grabWindow().save(screenshot+QString::fromUtf8(suffix)+".png");};
        const auto* desk=campaign.leadership();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        if(check->phase==0) {
            check->resumed=campaign.currentCampaign();
            if(!check->resumed) {
                auto* founder=campaign.founding();founder->reset();founder->setField("family","rich");founder->setField("office","leased");
                for(const auto& chapter:founder->chapters()) {const auto row=chapter.toMap();founder->choose(row.value("id").toString(),row.value("choices").toList().front().toMap().value("id").toString());}
                campaign.startFounderCampaign();
            }
            ++check->phase;return;
        }
        if(check->phase==1) {
            if(check->resumed && (!campaign.canStartNew() || !runtime::objective_time_stopped(campaign.snapshot()->checkpoint.world) || campaign.snapshot()->checkpoint.world.objectives->end->cause!=runtime::CampaignEndCause::retirement)) {fail("Retirement ending did not resume");return;}
            window.setProperty("menuVisible",false);window.setProperty("sessionEntered",true);QMetaObject::invokeMethod(&window,"closeApp");
            window.showNormal();window.resize(campaign.textScalePercent()>=200?650:1280,campaign.textScalePercent()>=200?480:800);
            if(check->resumed) {check->phase=30;return;}
            check->phase=100;return;
        }
        if(campaign.requiredKind()=="content") {const auto choices=campaign.requiredChoices();campaign.choose(choices.back().toMap().value("choiceId").toString());return;}
        if(campaign.requiredKind()=="management") {campaign.acknowledgeManagement();return;}
        if(campaign.requiredKind()=="site") {campaign.acknowledgeSiteAlert();return;}
        switch(check->phase) {
        case 100: if(click("devicesButton")) check->phase=101;return;
        case 101: if(click("leadershipApp")) check->phase=2;return;
        case 2:
            if(desk->candidatesAt(desk->recommendedDate()).isEmpty()) {fail("No opening senior candidate");return;}
            check->cash=s.cash;if(click("reviewLeadershipNomination")) ++check->phase;return;
        case 3:
            if(desk->reviewText().isEmpty() || s.cash!=check->cash) {fail("Nomination preview mutated cash");return;}
            capture("-nomination");if(click("confirmLeadershipAction")) ++check->phase;return;
        case 4: if(click("reviewLeadershipSupport")) ++check->phase;return;
        case 5: capture("-vote");if(click("confirmLeadershipAction")) ++check->phase;return;
        case 6:
            if(s.succession->plans.front().status!=core::SuccessionStatus::approved) {fail("Board did not fund handover");return;}
            check->handover=s.succession->plans.front().handover_on;check->cash=s.cash;
            if(click("reviewDepartureAnnouncement")) ++check->phase;return;
        case 7:
            if(s.cash!=check->cash || !s.succession->releases->empty()) {fail("Announcement preview mutated company");return;}
            capture("-announcement");if(click("confirmLeadershipAction")) ++check->phase;return;
        case 8:
            if(s.succession->releases->size()!=1 || s.cash>=check->cash || desk->canFinish()) {fail("Release was not paid or allowed premature retirement");return;}
            campaign.setDaysPerSecond(60);if(click("advanceLeadership")) ++check->phase;return;
        case 9:
            if(!campaign.paused()) return;
            if(s.date<check->handover) {if(campaign.requiredKind()=="leadership") campaign.leadership()->acknowledge();else (void)click("advanceLeadership");return;}
            if(s.succession->plans.front().status!=core::SuccessionStatus::activated || !desk->canFinish() || !campaign.paused()) {fail("Paid work did not produce the announced handover");return;}
            if(campaign.requiredKind()=="leadership") {campaign.leadership()->acknowledge();return;}
            if(click("reviewFounderRunRetirement")) ++check->phase;return;
        case 10: capture("-ending-review");if(click("confirmLeadershipAction")) ++check->phase;return;
        case 11:
            if(!campaign.canStartNew() || !runtime::objective_time_stopped(campaign.snapshot()->checkpoint.world) || campaign.snapshot()->checkpoint.world.objectives->end->cause!=runtime::CampaignEndCause::retirement) {fail("Ending did not enable New Game");return;}
            capture("-retired");QMetaObject::invokeMethod(&window,"closeApp");check->phase=30;return;
        case 30: if(click("mainMenuButton")) ++check->phase;return;
        case 31: capture("-new-game");if(click("menuNewGameButton")) ++check->phase;return;
        case 32:
            if(press(window,buttonText(window.contentItem(),QStringLiteral("OK")))) ++check->phase;else fail("Archive and New Game review");return;
        default:
            if(!campaign.welcome()) {fail("New Game did not open founding");return;}
            std::cout<<"Retirement "<<(check->resumed?"resume":"walkthrough")<<" passed: paid release, funded successor, frozen ending and New Game route\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
    });timer->start();
}
