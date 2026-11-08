#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->isVisible() && root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* item=find(child,name))return item;
    return nullptr;
}
QPoint reveal(QQuickWindow& window,QQuickItem& target,QPointF local) {
    auto* scroll=find(window.contentItem(),QStringLiteral("worldMapScroll"));
    if(!scroll)throw std::runtime_error("Map scroll area is missing");
    auto* viewport=qvariant_cast<QObject*>(scroll->property("contentItem"));
    const auto point=target.mapToItem(scroll,local);
    const auto height=scroll->property("availableHeight").toReal();
    const auto maximum=std::max(0.,viewport->property("contentHeight").toReal()-height);
    viewport->setProperty("contentY",std::clamp(viewport->property("contentY").toReal()+point.y()-height/2,0.,maximum));
    return target.mapToScene(local).toPoint();
}
}
void schedule_map_layout_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Run{int phase=0,size=0,ticks=0;QSize requested;QString country,county;std::optional<runtime::Checkpoint> initial;std::string fingerprint;QPoint pointer;double world_height=0,county_height=0;};
    auto run=std::make_shared<Run>();auto* timer=new QTimer(&window);timer->setInterval(250);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,paths,screenshot] {
        const auto fail=[&](const char* message){std::cerr<<"Map layout: "<<message<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return find(window.contentItem(),QString::fromUtf8(name));};
        const auto capture=[&](const char* scale){if(!screenshot.isEmpty())window.grabWindow().save(screenshot+QStringLiteral("-%1-%2.png").arg(run->size).arg(QString::fromUtf8(scale)));};
        const auto navigate=[&](const char* scale){return campaign.world()->navigate("map",scale,"","","");};
        if(++run->ticks>240){fail("timed out");return;}
        if(!campaign.ready() || campaign.busy())return;
        if(run->initial && campaign.snapshot()->checkpoint!=*run->initial){fail("map inspection must not change simulation state");return;}
        if(run->phase==0) {
            if(!campaign.currentCampaign() || campaign.storageBlocked()){fail("needs an isolated continuing company");return;}
            run->initial=campaign.snapshot()->checkpoint;run->fingerprint=persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint();
            if(!campaign.world()->navigate("workspace","hq","laptop","society","")){fail("open public affairs filter");return;}
            run->phase=1;return;
        }
        if(run->phase==1) {
            auto* filter=item("societyRegionFilter");
            if(!filter || !filter->setProperty("text",QStringLiteral("Map filter isolation")) || !QMetaObject::invokeMethod(filter,"textEdited")){fail("stage a real app filter");return;}
            if(campaign.society()->regions()->rowCount()!=0 || campaign.society()->mapRegions()->rowCount()==0){fail("map needs independent public regions");return;}
            run->phase=2;return;
        }
        if(run->phase==2) {
            const bool offscreen=QGuiApplication::platformName()=="offscreen";
            run->requested=run->size==0?(offscreen?QSize{1920,1080}:QSize{1280,800}):run->size==1?(offscreen?QSize{7680,4320}:QSize{1280,800}):QSize{650,480};
            campaign.setTextScalePercent(run->size==2?200:100);
            if(!navigate("world")){fail("open world map");return;}run->phase=3;return;
        }
        if(run->phase==3) {
            if(window.size()!=run->requested){window.showNormal();window.resize(run->requested);return;}
            campaign.clearNotice();
            auto* canvas=item("worldCountryCanvas");auto* map=item("countryWorldMap");if(!canvas || !map){fail("world canvas");return;}
            const auto text=window.property("textSize").toInt();
            if(map->property("textSize").toInt()!=text || map->height()<text*12){fail("world map must scale with the interface");return;}
            if(run->size==0)run->world_height=map->height();
            if(run->size==1 && QGuiApplication::platformName()=="offscreen" && map->height()<run->world_height*3.8){fail("world map retained a small-screen pixel cap");return;}
            const bool canada=map->property("selectedId").toString()!="CAN";run->country=canada?QStringLiteral("CAN"):QStringLiteral("BRA");
            run->pointer=reveal(window,*canvas,QPointF{canvas->width()*(canada?80:130)/360,canvas->height()*(canada?30:100)/180});
            QTest::mouseMove(&window,run->pointer);run->phase=4;return;
        }
        if(run->phase==4) {
            auto* map=item("countryWorldMap");auto* card=window.findChild<QObject*>("countryHoverCard");
            if(!map || map->property("hoverId").toString()!=run->country || !card || !card->property("visible").toBool() || !map->property("hoverSummary").toString().contains("Regional context")){fail("pointer hover must show a real public briefing");return;}
            // A new public snapshot must refresh a stationary pointer's briefing.
            auto* regions=campaign.society()->mapRegions();std::vector<bridge::CampaignRow> refreshed;
            for(int i=0;i<regions->rowCount();++i) {
                const auto row=regions->row(regions->idAt(i));const auto id=row.value("entityId").toString();
                refreshed.push_back({id,row.value("title").toString(),row.value("subtitle").toString(),row.value("detail").toString(),
                    row.value("status").toString(),row.value("required").toBool(),row.value("fields").toMap()});
            }
            const auto original=refreshed;
            for(auto& row:refreshed)if(row.entity_id==map->property("hoverRegion").toString())row.detail=QStringLiteral("Updated live briefing");
            regions->setRows(std::move(refreshed));
            if(!map->property("hoverSummary").toString().contains("Updated live briefing")){fail("stationary hover must follow updated public conditions");return;}
            regions->setRows(original);
            if(map->property("hoverSummary").toString().contains("Updated live briefing")){fail("restored public snapshot must refresh hover");return;}
            const auto x=card->property("x").toReal(),y=card->property("y").toReal();
            if(x<0 || y<0 || x+card->property("width").toReal()>window.width()+1 || y+card->property("height").toReal()>window.height()+1){fail("hover card must fit the current viewport");return;}
            QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,run->pointer);
            if(map->property("selectedId").toString()!=run->country || campaign.society()->regions()->filterText()!="Map filter isolation"){fail("map selection must keep another app's context");return;}
            capture("world");if(!navigate("country")){fail("open domestic map");return;}run->phase=5;return;
        }
        if(run->phase==5) {
            auto* map=item("domesticWorldMap");if(!map || map->property("textSize").toInt()!=window.property("textSize").toInt()){fail("domestic map scale");return;}
            (void)reveal(window,*map,QPointF{map->width()/2,map->height()/2});capture("country");
            if(!navigate("state")){fail("open county map");return;}run->phase=6;return;
        }
        if(run->phase==6) {
            auto* map=item("stateCountyMap");const auto rows=campaign.world()->countyRows();
            if(!map || rows.size()<2 || map->property("textSize").toInt()!=window.property("textSize").toInt()){fail("county map and marker scale");return;}
            if(run->size==0)run->county_height=map->height();
            if(run->size==1 && QGuiApplication::platformName()=="offscreen" && map->height()<run->county_height*3.8){fail("county map retained a small-screen pixel cap");return;}
            const auto target=map->property("selectedId").toString()==rows.back().toMap().value("id").toString()?rows.front():rows.back();run->county=target.toMap().value("id").toString();
            QVariant point;if(!QMetaObject::invokeMethod(map,"point",Q_RETURN_ARG(QVariant,point),Q_ARG(QVariant,target))){fail("county projection");return;}
            if(point.metaType()==QMetaType::fromType<QJSValue>())point=point.value<QJSValue>().toVariant();
            const auto position=point.toMap();run->pointer=reveal(window,*map,QPointF{position.value("x").toReal(),position.value("y").toReal()});
            QTest::mouseMove(&window,run->pointer);QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,run->pointer);run->phase=7;return;
        }
        if(run->phase==7) {
            auto* map=item("stateCountyMap");
            if(!map || map->property("selectedId").toString()!=run->county){fail("scaled county markers must stay interactive");return;}
            auto* card=window.findChild<QObject*>("countyHoverCard");
            if(!card || !card->property("visible").toBool() || card->property("y").toReal()+card->property("height").toReal()>window.height()+1){fail("county briefing must remain readable in the viewport");return;}
            capture("state");std::cout<<"Map viewport "<<window.width()<<'x'<<window.height()<<" · text "<<window.property("textSize").toInt()<<" passed\n";
            if(++run->size<3){run->phase=2;return;}
            if(run->fingerprint!=persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint()){fail("inspection must preserve campaign and day journal bytes");return;}
            std::cout<<"World/US/county scaling, pointer hover/click, readable briefing bounds and independent app filters passed\n";timer->stop();QCoreApplication::exit(0);
        }
    });timer->start();
}
