#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
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
}
void schedule_world_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot,bool portraits) {
    struct State {int phase=0,ticks=0,portraitPixels=0;QString revision;bool resumed=false;};
    auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(150);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot,portraits] {
        const auto fail=[&](const char* message){std::cerr<<"World check: "<<message<<" / "<<campaign.notice().toStdString()<<"\n";timer->stop();QCoreApplication::exit(3);};
        if(++state->ticks>200) {fail("Timeout");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto click=[&](const char* name){return press(window,find(window.contentItem(),QString::fromUtf8(name)));};
        if(state->phase==0) {
            state->resumed=campaign.currentCampaign();
            if(state->resumed && window.property("menuVisible").toBool()) {fail("Launch did not resume automatically");return;}
            if(!state->resumed) {
                auto* founding=campaign.founding();founding->reset();founding->setField("family","rich");
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
            state->phase=2;return;
        }
        if(state->phase==2) {
            if(!screenshot.isEmpty() && !window.grabWindow().save(screenshot+"-world.png")) {fail("World screenshot");return;}
            if(!click("peopleTab")) {fail("People dock navigation");return;}state->phase=3;return;
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
            if(!click("closeDeviceButton")) {fail("Close lab");return;}
            state->revision=campaign.revision();
            campaign.step();state->phase=8;return;
        }
        if(state->phase==8) {
            if(campaign.revision()==state->revision || campaign.storageBlocked() || !std::filesystem::exists(paths.saves_directory/"continuing/current.pgpsave")) {fail("Advance and durable checkpoint");return;}
            QKeyEvent escape{QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier};QCoreApplication::sendEvent(&window,&escape);
            state->phase=9;return;
        }
        if(state->phase==9) {
            if(!window.property("menuVisible").toBool() || !campaign.paused()) {fail("Escape pause menu");return;}
            std::cout<<"World navigation, device apps, map, persistence and pause passed; resumed="<<state->resumed<<"\n";
            timer->stop();QCoreApplication::exit(0);
        }
    });timer->start();
}
