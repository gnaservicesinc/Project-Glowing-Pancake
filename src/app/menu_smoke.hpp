#pragma once
#include <pgp/persistence/storage.hpp>
#include <QSGRendererInterface>
#include <QString>
class QQuickWindow;
class MusicController;
namespace pgp::bridge { class CampaignController; }
void schedule_menu_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,MusicController& music,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot,bool settingsOnly=false,bool developmentOnly=false);
