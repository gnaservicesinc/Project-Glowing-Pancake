#pragma once

#include <pgp/persistence/storage.hpp>
#include <QSGRendererInterface>
#include <QString>

class QQuickWindow;
namespace pgp::bridge { class CampaignController; }
// Runs only on an explicit command with an explicit portable scratch root.
void schedule_game_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot);

void schedule_playable_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot, bool competition = false);

void schedule_infrastructure_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot);
void schedule_civic_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot);
void schedule_society_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot);
