#pragma once

#include <QSGRendererInterface>
#include <QString>

class QQuickWindow;
class ProbeController;

// Explicit development command, never invoked by a build or install.
void schedule_probe_smoke(QQuickWindow& window, ProbeController& controller,
                          QSGRendererInterface::GraphicsApi expected,
                          const QString& screenshot);
