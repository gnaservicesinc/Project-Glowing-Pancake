#include <pgp/graphics/graphics_policy.hpp>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSurfaceFormat>

namespace pgp::graphics {

Selection select(Platform platform, bool opengl_available, bool software) {
    if (software) {
        return {QSGRendererInterface::Software, QStringLiteral("Explicit software probe")};
    }
    if (platform == Platform::Mac) {
        return {QSGRendererInterface::Metal, QStringLiteral("Native macOS Metal policy")};
    }
    if (platform == Platform::Windows && !opengl_available) {
        return {QSGRendererInterface::Direct3D11,
                QStringLiteral("OpenGL context preflight failed; Direct3D 11 recovery")};
    }
    return {QSGRendererInterface::OpenGL, QStringLiteral("Desktop OpenGL policy")};
}

Selection configure(bool software) {
#if defined(Q_OS_MACOS)
    constexpr auto platform = Platform::Mac;
    constexpr bool available = true;
#elif defined(Q_OS_WIN)
    constexpr auto platform = Platform::Windows;
    bool available = false;
    if (!software) {
        QOpenGLContext context;
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(2, 0);
        context.setFormat(format);
        if (context.create() && context.isValid() && context.format().majorVersion() >= 2) {
            QOffscreenSurface surface;
            surface.setFormat(context.format());
            surface.create();
            available = surface.isValid() && context.makeCurrent(&surface);
            if (available) {
                context.doneCurrent();
            }
        }
    }
#else
    constexpr auto platform = Platform::Linux;
    constexpr bool available = true;
#endif
    const auto selection = select(platform, available, software);
    QQuickWindow::setGraphicsApi(selection.api);
    return selection;
}

QString name(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
    case QSGRendererInterface::Metal: return QStringLiteral("Metal");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::Software: return QStringLiteral("Software");
    default: return QStringLiteral("Other/unknown");
    }
}

} // namespace pgp::graphics
