#pragma once
#include <pgp/persistence/storage.hpp>
#include <QString>
class QQuickWindow;
namespace pgp::bridge {class CampaignController;}
void schedule_world_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString,bool portraits=false,bool workplaces=false);
