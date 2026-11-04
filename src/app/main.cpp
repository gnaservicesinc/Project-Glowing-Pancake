#include "probe_controller.hpp"
#include "probe_smoke.hpp"

#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/resources/resource_guards.hpp>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QDebug>

#include <cmath>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    // Keep keyboard access to buttons/sliders consistent with text controls,
    // including macOS systems whose default tab policy skips buttons.
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QCoreApplication::setApplicationName(QStringLiteral("Project Glowing Pancake - interface probe"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("M0 interface/resource/audio probe; simulation is not connected."));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Render, check keyboard/images, then exit.")});
    parser.addOption({QStringLiteral("software"), QStringLiteral("Explicit software-renderer probe.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Write screenshot during --smoke-test."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("text-scale"), QStringLiteral("Probe text scale (1 to 2)."), QStringLiteral("factor"), QStringLiteral("1")});
    parser.process(application);
    bool valid_scale = false;
    const auto scale = parser.value(QStringLiteral("text-scale")).toDouble(&valid_scale);
    if (!valid_scale || !std::isfinite(scale) || scale < 1.0 || scale > 2.0 ||
        (parser.isSet(QStringLiteral("screenshot")) && !parser.isSet(QStringLiteral("smoke-test")))) {
        parser.showHelp(2);
    }

    const auto graphics = pgp::graphics::configure(parser.isSet(QStringLiteral("software")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    ProbeController controller;
    QQmlApplicationEngine engine;
    // Trust installed Qt imports, never arbitrary working-directory/user paths.
    const auto qt_imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    engine.setImportPathList({qt_imports, QStringLiteral("qrc:/qt/qml")});
    pgp::resources::ResourcePolicy policy;
    if (!policy.allow_local_root(qt_imports)) {
        qCritical() << "Qt import root could not be validated";
        return 2;
    }
    if (!policy.allow_resource_prefix(QStringLiteral("/qt-project.org/imports/")) ||
        !policy.allow_resource_prefix(QStringLiteral("/qt/qml/"))) {
        return 2;
    }
    const auto resource_stats = pgp::resources::install_resource_guards(engine, std::move(policy));
    Q_UNUSED(resource_stats);
    engine.rootContext()->setContextProperty(QStringLiteral("probe"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("probeTextScale"), scale);
    engine.load(QUrl(QStringLiteral("qrc:/pgp/probe/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 2;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        return 2;
    }
    auto configuration = window->graphicsConfiguration();
    // The probe needs no persistent shader cache or other local history.
    configuration.setAutomaticPipelineCache(false);
    window->setGraphicsConfiguration(configuration);
    QObject::connect(window, &QQuickWindow::sceneGraphError, &application,
                     [](QQuickWindow::SceneGraphError, const QString& message) {
                         qCritical().noquote() << "Graphics initialization failed:" << message;
                         QCoreApplication::exit(4);
                     });
    if (parser.isSet(QStringLiteral("smoke-test"))) {
        window->resize(1280, 720);
        qInfo().noquote() << "Selected renderer:" << pgp::graphics::name(graphics.api) << graphics.reason;
        schedule_probe_smoke(*window, controller, graphics.api,
                             parser.value(QStringLiteral("screenshot")));
    }
    window->show();
    return application.exec();
}
