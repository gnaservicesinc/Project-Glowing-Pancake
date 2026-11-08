#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->isVisible() && root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* item=find(child,name)) return item;
    return nullptr;
}
bool key(QQuickWindow& window,QQuickItem* item,int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
    if(!item || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    const auto text=code>=Qt::Key_0 && code<=Qt::Key_9?QString(QChar('0'+code-Qt::Key_0)):QString{};
    QKeyEvent down{QEvent::KeyPress,code,modifiers,text},up{QEvent::KeyRelease,code,modifiers,text};
    QCoreApplication::sendEvent(&window,&down);QCoreApplication::sendEvent(&window,&up);return true;
}
}
void schedule_form_drafts_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Check{int phase=0,ticks=0;bool resumed=false;std::optional<runtime::Checkpoint> initial;std::string fingerprint;};
    auto check=std::make_shared<Check>();auto* timer=new QTimer(&window);timer->setInterval(200);
    QObject::connect(timer,&QTimer::timeout,&window,[&,check,timer,paths,screenshot] {
        const auto fail=[&](const char* reason){std::cerr<<"Form drafts: "<<reason<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return find(window.contentItem(),QString::fromUtf8(name));};
        const auto remember=[&](const QString& name,const QString& value){campaign.world()->setDraft("test/forms/"+name,value);};
        const auto expected=[&](const QString& name){return campaign.world()->draft("test/forms/"+name,{});};
        const auto open=[&](const char* app,const char* section){return campaign.world()->navigate("workspace","hq","laptop",app,section);};
        if(++check->ticks>180){fail("timed out");return;}
        if(!campaign.ready() || campaign.busy()) return;
        if(check->initial && campaign.snapshot()->checkpoint!=*check->initial){fail("restoring or editing a draft must not change the simulation");return;}
        if(check->phase==0) {
            if(!campaign.currentCampaign() || campaign.storageBlocked()){fail("needs an isolated continuing company");return;}
            check->initial=campaign.snapshot()->checkpoint;
            check->fingerprint=persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint();
            check->resumed=expected("ready")=="yes";
            if(campaign.management()->researchTopics()->objectName()!="management/researchTopics"){fail("model selections need stable shared names");return;}
            if(!open("lab",check->resumed?"train":"research")){fail("open Lab");return;}
            ++check->phase;return;
        }
        if(check->phase==1) {
            if(!check->resumed) {
                auto* list=item("researchTopicSelectionList");
                if(!key(window,list,Qt::Key_Down) || campaign.management()->researchTopics()->indexOf(campaign.management()->researchTopics()->selectedId())<1){fail("keyboard topic selection");return;}
                remember("topic",campaign.management()->researchTopics()->selectedId());
            } else if(campaign.management()->researchTopics()->selectedId()!=expected("topic")){fail("selected topic did not resume by ID");return;}
            if(!open("lab","train")){fail("open training form");return;}++check->phase;return;
        }
        if(check->phase==2) {
            auto* recipe=item("recipeChoiceInput");auto* continuing=item("continueTrainingChoice");
            auto* rate=item("trainingRateFieldInput");
            const auto data_id=campaign.management()->dataOffers()->idAt(0);
            auto* mixture=find(window.contentItem(),"trainingData_"+data_id);
            if(!recipe || !continuing || !mixture || !rate || recipe->property("count").toInt()<2){fail("training controls and real options");return;}
            if(check->resumed) {
                if(recipe->property("currentValue").toString()!=expected("recipe") || !continuing->property("checked").toBool() || !mixture->property("checked").toBool() || rate->property("text").toString()!="37"){fail("recipe, checkbox, text or checked mixture did not resume");return;}
            } else {
                if(!key(window,recipe,Qt::Key_End) || recipe->property("currentIndex").toInt()!=recipe->property("count").toInt()-1 ||
                    !key(window,continuing,Qt::Key_Space) || !continuing->property("checked").toBool() || !key(window,mixture,Qt::Key_Space) || !mixture->property("checked").toBool()){fail("keyboard draft choices");return;}
                remember("recipe",recipe->property("currentValue").toString());
                if(!key(window,rate,Qt::Key_A,Qt::ControlModifier) || !key(window,rate,Qt::Key_3) || !key(window,rate,Qt::Key_7) || rate->property("text").toString()!="37"){fail("edit numeric draft by keyboard");return;}
                const auto saved=campaign.world()->draft("form/choice/recipeChoiceInput/",{});
                if(saved.isEmpty()){fail("user selection was not preserved");return;}
                campaign.world()->setDraft("form/choice/recipeChoiceInput/","\"missing-recipe\"");
                if(!QMetaObject::invokeMethod(recipe,"restoreDraft") || recipe->property("currentValue").toString()!=expected("recipe")){fail("unavailable draft ID must leave a valid selection");return;}
                campaign.world()->setDraft("form/choice/recipeChoiceInput/",saved);
            }
            if(!open("frontier","")){fail("open research planning");return;}check->phase=20;return;
        }
        if(check->phase==20) {
            auto* section=item("frontierSection");
            if(!section){fail("Frontier section picker");return;}
            if(check->resumed) {
                if(section->property("currentIndex").toInt()!=2){fail("research section did not resume");return;}
            } else if(!key(window,section,Qt::Key_Home) || !key(window,section,Qt::Key_Down) || !key(window,section,Qt::Key_Down)) {fail("choose research planning");return;}
            check->phase=21;return;
        }
        if(check->phase==21) {
            auto* queue=item("frontierQueueDraft");auto* topic=item("frontierQueuedTopic");
            if(!queue || !topic || topic->property("count").toInt()<2){fail("ordered queue and real research topics");return;}
            if(check->resumed) {
                auto value=queue->property("topics");
                if(value.metaType()==QMetaType::fromType<QJSValue>()) value=value.value<QJSValue>().toVariant();
                const auto topics=value.toList();
                if(topics.size()!=2 || topics[0].toString()!=expected("queueFirst") || topics[1].toString()!=expected("queueSecond")){fail("ordered research draft did not resume in the same order");return;}
            } else {
                if(!key(window,topic,Qt::Key_End) || !key(window,item("appendFrontierQueue"),Qt::Key_Space)){fail("append first research topic");return;}
                remember("queueFirst",topic->property("currentValue").toString());
                if(!key(window,topic,Qt::Key_Home) || !key(window,item("appendFrontierQueue"),Qt::Key_Space)){fail("append second research topic");return;}
                remember("queueSecond",topic->property("currentValue").toString());
            }
            if(!open("operations","")){fail("open Operations");return;}check->phase=3;return;
        }
        if(check->phase==3) {
            if(check->resumed) {
                auto* method=item("inspectionMethod");
                if(!method || method->property("currentValue").toString()!=expected("inspection")){fail("Operations page and inspection method did not resume");return;}
            }
            if(!key(window,item("operationsMap"),Qt::Key_Space)){fail("open construction planning");return;}++check->phase;return;
        }
        if(check->phase==4) {
            auto* cooling=item("ownedSiteCooling");
            if(!cooling){fail("cooling picker");return;}
            if(check->resumed) {
                if(cooling->property("currentValue").toString()!=expected("cooling")){fail("cooling draft did not resume");return;}
            } else {
                if(!key(window,cooling,Qt::Key_End) || cooling->property("currentValue").toString()!="evaporative"){fail("select cooling by keyboard");return;}
                remember("cooling",cooling->property("currentValue").toString());
            }
            if(!key(window,item("operationsCalendar"),Qt::Key_Space)){fail("open inspection planning");return;}++check->phase;return;
        }
        if(check->phase==5) {
            auto* method=item("inspectionMethod");
            if(!method){fail("inspection picker");return;}
            if(!check->resumed) {
                if(!key(window,method,Qt::Key_Down) || method->property("currentValue").toString()!="onsite"){fail("select inspection by keyboard");return;}
                remember("inspection",method->property("currentValue").toString());remember("ready","yes");
            }
            method->forceActiveFocus(Qt::TabFocusReason);
            window.resize(650,480);++check->phase;return;
        }
        if(check->phase==6) {
            auto* method=item("inspectionMethod");auto* viewport=item("managementScroll");
            if(!method || !viewport){fail("focused field viewport");return;}
            const auto point=method->mapToItem(viewport,QPointF{});
            if(point.y()<-1 || point.y()+method->height()>viewport->property("availableHeight").toReal()+1){fail("focused selection must remain visible after resizing");return;}
            if(check->fingerprint!=persistence::CurrentCampaign{paths.saves_directory/"continuing"}.fingerprint()){fail("draft edits must not rewrite campaign or day journal");return;}
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot);
            std::cout<<"Stable selections, checked mixture, ordered research queue, pages and focused resize passed without simulation changes; resumed="<<check->resumed<<'\n';
            timer->stop();QCoreApplication::exit(0);
        }
    });timer->start();
}
