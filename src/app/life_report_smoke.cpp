#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <pgp/runtime/career.hpp>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->isVisible() && root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* result=find(child,name)) return result;
    return nullptr;
}
void press(QQuickWindow& window,QQuickItem* item) {
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down{QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier},up{QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&down);
    QCoreApplication::sendEvent(&window,&up);
}
}
void schedule_life_report_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
        pgp::persistence::LocalPaths paths,QString screenshot) {
    struct Check {int phase=0,ticks=0,settle=0;std::optional<pgp::runtime::Checkpoint> initial;std::string fingerprint;};
    auto check=std::make_shared<Check>();
    auto* timer=new QTimer(&window);
    timer->setInterval(150);
    QObject::connect(timer,&QTimer::timeout,&window,[&,check,timer,paths,screenshot]{
        const auto item=[&](const char* name){return find(window.contentItem(),QLatin1String{name});};
        const auto fail=[&](const char* reason){
            std::cerr<<"Life report "<<check->phase<<" · "<<reason<<'\n';
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".failure.png");
            timer->stop();QCoreApplication::exit(3);
        };
        if(++check->ticks>200){fail("Timed out");return;}
        if(!campaign.ready() || campaign.busy() || !window.isSceneGraphInitialized()) return;
        if(check->initial && campaign.snapshot()->checkpoint!=*check->initial){fail("Reading changed the simulation");return;}
        if(check->phase>=10) {
            const auto& world=campaign.snapshot()->checkpoint.world;
            const auto click=[&](const char* id) {
                auto* control=item(id);
                if(!control || !control->isEnabled()) {fail(id);return false;}
                press(window,control);return true;
            };
            if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);check->settle=0;return;}
            if(++check->settle<5) return;
            if(check->phase==10) {
                auto* heading=item("careerHeading");
                if(!heading || !item("careerJobPicker") || !heading->property("text").toString().contains("appointment")){fail("Find funded work after closure");return;}
                if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".career-offers.png");
                if(click("reviewCareerAppointment")) {check->settle=0;++check->phase;}
                return;
            }
            if(check->phase==11 || check->phase==13) {
                if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+(check->phase==11?".career-appointment.png":".career-research.png"));
                if(click("confirmCareerAction")) {check->settle=0;++check->phase;}
                return;
            }
            if(check->phase==12) {
                if(!pgp::runtime::career_employer(world) || !campaign.canAdvance()){fail("Paid appointment opens playable time");return;}
                if(!item("reviewCareerResearch")) {campaign.step();return;}
                if(click("reviewCareerResearch")) {check->settle=0;++check->phase;}
                return;
            }
            if(world.objectives->career->back().projects.empty()){fail("Research did not enter the lab's saved work register");return;}
            std::cout<<"Career desk passed at 650×480 and 200% text with keyboard hiring and funded research.\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        if(check->phase==0) {
            if(auto* resume=item("menuResumeButton")){press(window,resume);return;}
            if(!campaign.currentCampaign() || !campaign.objectives()->retrospective()){fail("Needs an isolated retrospective campaign");return;}
            if(pgp::runtime::career_open(campaign.snapshot()->checkpoint.world)) {
                campaign.setTextScalePercent(200);
                window.showNormal();window.resize(650,480);
                if(!campaign.world()->navigate("workspace","hq","laptop","objectives","")){fail("Open the career desk");return;}
                check->phase=pgp::runtime::career_employer(campaign.snapshot()->checkpoint.world)?14:10;
                return;
            }
            campaign.setTextScalePercent(100);
            window.showNormal();window.resize(1280,800);
            check->initial=campaign.snapshot()->checkpoint;
            check->fingerprint=pgp::persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint();
            if(!campaign.world()->navigate("workspace","hq","laptop","objectives","")){fail("Open World & legacy");return;}
            check->phase=1;return;
        }
        if(check->phase==1) {
            auto* headline=item("lifeReportHeadline");
            if(!headline || !headline->property("text").toString().contains("life in progress") ||
                !item("lifeReportSection_world") || !item("lifeReportSection_industry")){fail("Live edition and actual world/industry sections");return;}
            if(++check->settle<5) return;
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".edition.png");
            press(window,item("lifeReportSection_people"));
            check->settle=0;check->phase=2;return;
        }
        if(check->phase==2) {
            auto* button=item("lifeReportSection_people");
            if(!button || !button->property("checked").toBool()){fail("Read people section by keyboard");return;}
            auto* article=item("lifeReportArticle_people");
            auto* viewport=item("managementScroll");
            if(!article || !viewport){fail("People article did not open");return;}
            article->forceActiveFocus(Qt::TabFocusReason);
            QMetaObject::invokeMethod(viewport,"reveal",Q_ARG(QVariant,QVariant::fromValue(article)));
            if(++check->settle<4) return;
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".people.png");
            campaign.setTextScalePercent(200);
            window.showNormal();window.resize(650,480);
            press(window,item("lifeReportSection_all"));
            check->settle=0;check->phase=3;return;
        }
        if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);check->settle=0;return;}
        if(++check->settle<8) return;
        auto* headline=item("lifeReportHeadline");
        auto* viewport=item("managementScroll");
        if(check->phase==3) {
            headline->forceActiveFocus(Qt::TabFocusReason);
            QMetaObject::invokeMethod(viewport,"reveal",Q_ARG(QVariant,QVariant::fromValue(headline)));
            check->settle=0;check->phase=4;return;
        }
        const auto top=headline->mapToItem(viewport,{0,0}).y();
        if(top<-1 || headline->width()>viewport->width() || top+headline->height()>viewport->height()+1){
            fail("Compact headline is clipped");return;
        }
        if(check->fingerprint!=pgp::persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint()){
            fail("Reading changed the saved campaign");return;
        }
        if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".compact.png");
        std::cout<<"Newspaper edition, world/industry records, people navigation and 200% compact headline passed without simulation changes.\n";
        timer->stop();QCoreApplication::exit(0);
    });
    timer->start();
}
