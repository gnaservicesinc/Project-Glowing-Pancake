#pragma once
#include <QString>
#include <pgp/persistence/storage.hpp>
class QQuickWindow;
namespace pgp::bridge {class CampaignController;}
void schedule_social_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString);
