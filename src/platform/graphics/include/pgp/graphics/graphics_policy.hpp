#pragma once

#include <QSGRendererInterface>
#include <QString>

namespace pgp::graphics {

enum class Platform { Mac, Linux, Windows };
struct Selection {
    QSGRendererInterface::GraphicsApi api;
    QString reason;
};

// Pure selection logic permits testing both Windows preflight outcomes anywhere.
[[nodiscard]] Selection select(Platform platform, bool opengl_available, bool software);
// Requires QGuiApplication, runs before constructing any QQuickWindow.
[[nodiscard]] Selection configure(bool software);
[[nodiscard]] QString name(QSGRendererInterface::GraphicsApi api);

} // namespace pgp::graphics
