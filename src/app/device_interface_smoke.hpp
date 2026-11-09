#pragma once
#include <pgp/persistence/storage.hpp>
#include <QString>
class QQuickWindow;
namespace pgp::bridge {class CampaignController;}
void schedule_device_interface_smoke(QQuickWindow&,pgp::bridge::CampaignController&,pgp::persistence::LocalPaths,QString);
