#include "campaign_controller.hpp"
#include "game_smoke.hpp"

#include <pgp/core/build_identity.hpp>
#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/resources/resource_guards.hpp>
#include <pgp/runtime/scenario.hpp>

#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>

#include <iostream>

namespace {
std::filesystem::path path(const QString& name) { return std::filesystem::path{QFileInfo{name}.absoluteFilePath().toStdU16String()}; }
}
int main(int argc, char* argv[]) {
    using namespace pgp;
    QGuiApplication application(argc,argv);
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QCoreApplication::setApplicationName(QStringLiteral("Project Glowing Pancake"));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(core::build_identity().game_version));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline business and model-development campaign."));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({QStringLiteral("content"),QStringLiteral("Explicit local content collection."),QStringLiteral("file")});
    parser.addOption({QStringLiteral("scenario"),QStringLiteral("Scenario definition ID."),QStringLiteral("id")});
    parser.addOption({QStringLiteral("headquarters"),QStringLiteral("Headquarters choice offered by the scenario."),QStringLiteral("id"),QStringLiteral("US-CA")});
    parser.addOption({QStringLiteral("portable-root"),QStringLiteral("Explicit directory for local saves/settings."),QStringLiteral("directory")});
    parser.addOption({QStringLiteral("text-scale"),QStringLiteral("Initial text percent, 100 through 200; does not change saved preferences."),QStringLiteral("percent")});
    parser.addOption({QStringLiteral("software"),QStringLiteral("Use the explicit software renderer." )});
    parser.addOption({QStringLiteral("build-info"),QStringLiteral("Print configure-time identity, then exit.")});
    parser.addOption({QStringLiteral("smoke-test"),QStringLiteral("Explicit keyboard/campaign/save check, then exit; requires --portable-root.")});
    parser.addOption({QStringLiteral("screenshot"),QStringLiteral("Capture the smoke-test window."),QStringLiteral("file")});
    parser.process(application);
    if (parser.isSet(QStringLiteral("build-info"))) { core::write_build_identity(std::cout); return 0; }
    if (!parser.positionalArguments().isEmpty() ||
        (parser.isSet(QStringLiteral("screenshot")) && !parser.isSet(QStringLiteral("smoke-test"))) ||
        (parser.isSet(QStringLiteral("smoke-test")) && !parser.isSet(QStringLiteral("portable-root")))) parser.showHelp(2);
    try {
        const bool bundled=!parser.isSet(QStringLiteral("content"));
        std::string bytes;
        if (bundled) {
            QFile file{QStringLiteral(":/pgp/content/first-playable.json")};
            if (!file.open(QIODevice::ReadOnly) || file.size()>static_cast<qint64>(content::maximum_document_bytes))
                throw std::runtime_error("Bundled campaign content is unavailable or oversized");
            bytes=file.readAll().toStdString();
        } else bytes=persistence::read_bounded(path(parser.value(QStringLiteral("content"))),content::maximum_document_bytes);
        const auto registry = content::foundation_registry();
        auto program = runtime::compile_collection(content::load_collection(bytes,registry),registry);
        if (bundled && core::build_identity().runtime_content_hash!="unavailable" &&
            persistence::save_identity(program).content_hash!=core::build_identity().runtime_content_hash)
            throw std::runtime_error("Bundled content differs from its finalized build manifest. Rebuild from a consistent snapshot.");
        std::string scenario_id=parser.value(QStringLiteral("scenario")).toStdString();
        if (scenario_id.empty()) for (const auto& [id,definition]:program.collection.definitions)
            if (std::holds_alternative<content::ScenarioDefinition>(definition.data)) { scenario_id=id;break; }
        auto initial = runtime::begin_scenario(program,scenario_id,parser.value(QStringLiteral("headquarters")).toStdString());
        const auto paths = persistence::local_paths(parser.isSet(QStringLiteral("portable-root")) ?
            std::optional{path(parser.value(QStringLiteral("portable-root")))} : std::nullopt);
        auto settings = persistence::load_settings(paths.settings_file);
        if (parser.isSet(QStringLiteral("text-scale"))) {
            bool valid = false;
            const auto percent = parser.value(QStringLiteral("text-scale")).toInt(&valid);
            if (!valid || percent < 100 || percent > 200) throw std::invalid_argument("Text scale must be an integer from 100 to 200");
            settings.text_scale_percent = static_cast<std::uint32_t>(percent);
        }
        const auto selection = graphics::configure(parser.isSet(QStringLiteral("software")));
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        bridge::CampaignController campaign{std::move(program),std::move(initial),paths,settings,nullptr,bundled};
        QQmlApplicationEngine engine;
        const auto qt_imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        engine.setImportPathList({qt_imports,QStringLiteral("qrc:/qt/qml")});
        resources::ResourcePolicy policy;
        if (!policy.allow_local_root(qt_imports) || !policy.allow_resource_prefix(QStringLiteral("/qt-project.org/imports/")) ||
            !policy.allow_resource_prefix(QStringLiteral("/qt/qml/"))) throw std::runtime_error("Trusted local Qt imports are unavailable");
        const auto resource_stats = resources::install_resource_guards(engine,std::move(policy));
        Q_UNUSED(resource_stats);
        engine.rootContext()->setContextProperty(QStringLiteral("campaign"),&campaign);
        engine.load(QUrl(QStringLiteral("qrc:/pgp/game/Main.qml")));
        if (engine.rootObjects().isEmpty()) return 2;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window) return 2;
        auto configuration = window->graphicsConfiguration(); configuration.setAutomaticPipelineCache(false);
        window->setGraphicsConfiguration(configuration);
        QObject::connect(window,&QQuickWindow::sceneGraphError,&application,[](QQuickWindow::SceneGraphError,const QString& message) {
            qCritical().noquote() << "Graphics initialization failed:" << message; QCoreApplication::exit(4);
        });
        if (parser.isSet(QStringLiteral("smoke-test"))) {
            window->resize(1280,720);
            if (bundled) schedule_playable_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
            else schedule_game_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
        }
        window->show();
        return application.exec();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
