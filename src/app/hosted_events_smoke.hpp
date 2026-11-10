#pragma once
#include <pgp/persistence/storage.hpp>
#include <QString>
class QQuickWindow;
namespace pgp::bridge {class CampaignController;}
void schedule_hosted_events_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString);
