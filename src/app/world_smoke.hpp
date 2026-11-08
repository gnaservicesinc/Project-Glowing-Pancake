#pragma once
#include <pgp/persistence/storage.hpp>
#include <QString>
class QQuickWindow;
namespace pgp::bridge {class CampaignController;}
void schedule_world_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString,bool portraits=false,bool workplaces=false,bool devices=false,bool contacts=false,bool managers=false);

void schedule_departure_smoke(QQuickWindow&,pgp::bridge::CampaignController&,QString);

void schedule_decision_smoke(QQuickWindow&,pgp::bridge::CampaignController&,QString,bool largeViewport=false);
void schedule_content_recovery_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString source,QString screenshot);
void schedule_form_drafts_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString screenshot);
