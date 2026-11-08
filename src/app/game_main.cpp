#include "campaign_controller.hpp"
#include "game_smoke.hpp"
#include "music_controller.hpp"
#include "menu_media.hpp"
#include "menu_smoke.hpp"
#include "founding_smoke.hpp"
#include "world_smoke.hpp"
#include "portrait_images.hpp"

#include <pgp/core/build_identity.hpp>
#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/resources/resource_guards.hpp>
#include <pgp/runtime/scenario.hpp>

#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <qqml.h>

#include <iostream>

namespace {
std::filesystem::path path(const QString& name) { return std::filesystem::path{QFileInfo{name}.absoluteFilePath().toStdU16String()}; }
}
int main(int argc, char* argv[]) {
    using namespace pgp;
    QGuiApplication application(argc,argv);
    // Exit is owned by the confirmation, never by temporary window visibility.
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QCoreApplication::setApplicationName(QStringLiteral("Project Glowing Pancake"));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(core::build_identity().game_version));
    QGuiApplication::setWindowIcon(QIcon{QStringLiteral(":/pgp/assets/menu/icon.png")});
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
    parser.addOption({QStringLiteral("competition-smoke"),QStringLiteral("Use the standard rival roster, publication/adoption and pricing path in the explicit playable smoke.")});
    parser.addOption({QStringLiteral("infrastructure-smoke"),QStringLiteral("Exercise the map, paid owned infrastructure and calendar in the explicit playable smoke.")});
    parser.addOption({QStringLiteral("civic-smoke"),QStringLiteral("Exercise the civic docket, payment and exact reload in the explicit playable smoke.")});
    parser.addOption({QStringLiteral("society-smoke"),QStringLiteral("Exercise regional outcomes, paid media and standards disclosures in the explicit playable smoke.")});
    parser.addOption({QStringLiteral("institutions-smoke"),QStringLiteral("Exercise public testimony, disputes and a funded institutional agreement.")});
    parser.addOption({QStringLiteral("frontier-smoke"),QStringLiteral("Exercise Frontier controls with an explicit local late-era fixture.")});
    parser.addOption({QStringLiteral("leadership-smoke"),QStringLiteral("Exercise succession, board review and the retained founder trust with an explicit local fixture.")});
    parser.addOption({QStringLiteral("objectives-smoke"),QStringLiteral("Exercise a settled objective breach, postmortem and sandbox with an explicit local fixture.")});
    parser.addOption({QStringLiteral("music-smoke"),QStringLiteral("Exercise the bundled credits, keyboard and native music output.")});
    parser.addOption({QStringLiteral("menu-smoke"),QStringLiteral("Exercise the Mac menu, settings, media, New Game entry and recovery.")});
    parser.addOption({QStringLiteral("screenshot"),QStringLiteral("Capture the smoke-test window."),QStringLiteral("file")});
    parser.addOption({QStringLiteral("founding-smoke"),QStringLiteral("Exercise the actual New Game biography, funding and first morning with an isolated local save.")});
    parser.addOption({QStringLiteral("world-smoke"),QStringLiteral("Exercise world navigation and continuing campaign in an isolated root.")});
    parser.addOption({QStringLiteral("portrait-smoke"),QStringLiteral("Check interactive portraits at 1080p, 8K and a narrow high-text layout.")});
    parser.addOption({QStringLiteral("decision-smoke"),QStringLiteral("Exercise decision participants, profile inspection, required choices and restart.")});
    parser.addOption({QStringLiteral("retirement-smoke"),QStringLiteral("Exercise the founder announcement, funded handover, retirement ending and resume.")});
    parser.addOption({QStringLiteral("manager-smoke"),QStringLiteral("Exercise HQ manager appointments, work delivery, portraits and resume.")});
    parser.addOption({QStringLiteral("contacts-smoke"),QStringLiteral("Exercise paid overseas offices and national/world contact progression.")});
    parser.addOption({QStringLiteral("device-smoke"),QStringLiteral("Exercise device ownership, equipment controls and compatible app routing.")});
    parser.addOption({QStringLiteral("workplace-smoke"),QStringLiteral("Exercise paid headquarters construction and repeatable facility blueprints.")});
    parser.addOption({QStringLiteral("content-recovery-smoke"),QStringLiteral("Review matching original content and resume an isolated older company."),QStringLiteral("source")});
    parser.addOption({QStringLiteral("form-drafts-smoke"),QStringLiteral("Exercise unsubmitted form choices, exact state and UI-only restart.")});
    parser.process(application);
#if !PGP_TEST_HOOKS
    if(parser.isSet(QStringLiteral("smoke-test"))) {
        std::cerr << "GUI checks require an explicit --enable-tests build.\n";return 2;
    }
#endif
    if (parser.isSet(QStringLiteral("build-info"))) { core::write_build_identity(std::cout); return 0; }
    const bool world_smoke=parser.isSet(QStringLiteral("form-drafts-smoke")) || parser.isSet(QStringLiteral("content-recovery-smoke")) || parser.isSet(QStringLiteral("decision-smoke")) || parser.isSet(QStringLiteral("retirement-smoke")) || parser.isSet(QStringLiteral("world-smoke")) || parser.isSet(QStringLiteral("portrait-smoke")) || parser.isSet(QStringLiteral("workplace-smoke")) || parser.isSet(QStringLiteral("device-smoke")) || parser.isSet(QStringLiteral("contacts-smoke")) || parser.isSet(QStringLiteral("manager-smoke"));
    const bool civic_smoke = parser.isSet(QStringLiteral("civic-smoke"));
    const bool society_smoke = parser.isSet(QStringLiteral("society-smoke"));
    const bool institutions_smoke = parser.isSet(QStringLiteral("institutions-smoke"));
    const bool frontier_smoke = parser.isSet(QStringLiteral("frontier-smoke"));
    const bool leadership_smoke = parser.isSet(QStringLiteral("leadership-smoke"));
    const bool objectives_smoke = parser.isSet(QStringLiteral("objectives-smoke"));
    const bool infrastructure_smoke = parser.isSet(QStringLiteral("infrastructure-smoke"));
    const bool competition_smoke = parser.isSet(QStringLiteral("competition-smoke"));
    const bool music_smoke = parser.isSet(QStringLiteral("music-smoke"));
    const bool menu_smoke = parser.isSet(QStringLiteral("menu-smoke"));
    const bool founding_smoke = parser.isSet(QStringLiteral("founding-smoke"));
    const bool prototype_smoke = parser.isSet(QStringLiteral("smoke-test")) && !menu_smoke && !founding_smoke && !world_smoke;
    const auto variants = static_cast<int>(world_smoke) + static_cast<int>(society_smoke) + static_cast<int>(institutions_smoke) +
        static_cast<int>(civic_smoke) + static_cast<int>(infrastructure_smoke) + static_cast<int>(competition_smoke) + static_cast<int>(frontier_smoke) + static_cast<int>(leadership_smoke) + static_cast<int>(objectives_smoke) + static_cast<int>(music_smoke) + static_cast<int>(menu_smoke) + static_cast<int>(founding_smoke);
    if (!parser.positionalArguments().isEmpty() ||
        (parser.isSet(QStringLiteral("screenshot")) && !parser.isSet(QStringLiteral("smoke-test"))) ||
        (variants > 0 && !parser.isSet(QStringLiteral("smoke-test"))) || variants > 1 ||
        ((frontier_smoke || leadership_smoke || objectives_smoke) && !parser.isSet(QStringLiteral("content"))) ||
        (parser.isSet(QStringLiteral("smoke-test")) && !parser.isSet(QStringLiteral("portable-root")))) {
        std::cerr << "Invalid combination of game/check arguments.\n";
        parser.showHelp(2);
    }
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
        if (scenario_id.empty() && program.collection.find("scenario.first_playable")) scenario_id="scenario.first_playable";
        if (scenario_id.empty()) for (const auto& [id,definition]:program.collection.definitions)
            if (std::holds_alternative<content::ScenarioDefinition>(definition.data)) { scenario_id=id;break; }
        auto initial = runtime::begin_scenario(program,scenario_id,parser.value(QStringLiteral("headquarters")).toStdString());
        const auto paths = persistence::local_paths(parser.isSet(QStringLiteral("portable-root")) ?
            std::optional{path(parser.value(QStringLiteral("portable-root")))} : std::nullopt);
        auto settings = persistence::load_settings(paths.settings_file);
        if (!prototype_smoke && !std::filesystem::exists(paths.settings_file)) {
            settings.music_enabled=true;
            settings.volume_percent=40;
        }
        if (parser.isSet(QStringLiteral("text-scale"))) {
            bool valid = false;
            const auto percent = parser.value(QStringLiteral("text-scale")).toInt(&valid);
            if (!valid || percent < 100 || percent > 200) throw std::invalid_argument("Text scale must be an integer from 100 to 200");
            settings.text_scale_percent = static_cast<std::uint32_t>(percent);
        }
        [[maybe_unused]] const auto selection = graphics::configure(parser.isSet(QStringLiteral("software")));
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        bridge::CampaignController campaign{std::move(program),std::move(initial),paths,settings,nullptr,
            bundled || competition_smoke || infrastructure_smoke || institutions_smoke || founding_smoke, !prototype_smoke};
        const auto audio_mode=prototype_smoke || ((menu_smoke || founding_smoke) && parser.isSet(QStringLiteral("software")))
            ? audio::DeviceMode::Silent : audio::DeviceMode::NativeIfAvailable;
        MusicController music{nullptr,music_smoke ? audio::DeviceMode::NativeIfAvailable : audio_mode};
        MenuAudio menu_audio{campaign,audio_mode};
        bool interface_ready=false;
        const auto sync_music = [&] {
            if (!interface_ready) return;
            const auto kind = campaign.requiredKind();
            const auto urgent = kind == QStringLiteral("objective") || kind == QStringLiteral("liquidity") ||
                                kind == QStringLiteral("site") || kind == QStringLiteral("institution");
            music.configure(campaign.musicEnabled(),campaign.musicVolume(),urgent);
        };
        QObject::connect(&campaign,&bridge::CampaignController::settingsChanged,&music,sync_music);
        QObject::connect(&campaign,&bridge::CampaignController::stateChanged,&music,sync_music);
        qmlRegisterType<MenuVideo>("Pgp.Menu",1,0,"MenuVideo");
        qmlRegisterModule("Pgp.Menu",1,0);
        qmlProtectModule("Pgp.Menu",1);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("portraits"),new pgp::app::PortraitImages);
        QObject::connect(&engine,&QQmlEngine::warnings,&application,[](const QList<QQmlError>& warnings) {
            for (const auto& warning:warnings) std::cerr << warning.toString().toStdString() << '\n';
        });
        const auto qt_imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        engine.setImportPathList({qt_imports,QStringLiteral("qrc:/qt/qml")});
        resources::ResourcePolicy policy;
        if(!policy.allow_image_provider(QStringLiteral("portraits"))) throw std::runtime_error("The local portrait renderer is unavailable");
        if (!policy.allow_local_root(qt_imports) || !policy.allow_resource_prefix(QStringLiteral("/qt-project.org/imports/")) ||
            !policy.allow_resource_prefix(QStringLiteral("/qt/qml/"))) throw std::runtime_error("Trusted local Qt imports are unavailable");
        const auto resource_stats = resources::install_resource_guards(engine,std::move(policy));
        Q_UNUSED(resource_stats);
        engine.rootContext()->setContextProperty(QStringLiteral("campaign"),&campaign);
        engine.rootContext()->setContextProperty(QStringLiteral("soundtrack"),&music);
        engine.rootContext()->setContextProperty(QStringLiteral("menuAudio"),&menu_audio);
        engine.rootContext()->setContextProperty(QStringLiteral("prototypeSmoke"),prototype_smoke);
        engine.rootContext()->setContextProperty(QStringLiteral("softwareWorld"),parser.isSet(QStringLiteral("software")));
        QTimer startup_deadline;
        startup_deadline.setSingleShot(true);
        QQuickWindow* window=nullptr;
        QObject::connect(&startup_deadline,&QTimer::timeout,&application,[] {
            std::cerr << "Timed out while creating the game interface.\n"; QCoreApplication::exit(2);
        });
        // Local imports may finish asynchronously under the resource guard.
        // Run startup on the one application loop, including native Mac events.
        QObject::connect(&engine,&QQmlApplicationEngine::objectCreated,&startup_deadline,[&](QObject* object,const QUrl&) {
            startup_deadline.stop();
            window=qobject_cast<QQuickWindow*>(object);
            if (!window) { std::cerr << "Could not create the game window.\n"; QCoreApplication::exit(2); return; }
            auto configuration = window->graphicsConfiguration(); configuration.setAutomaticPipelineCache(false);
            window->setGraphicsConfiguration(configuration);
            QObject::connect(window,&QQuickWindow::sceneGraphError,&application,[](QQuickWindow::SceneGraphError,const QString& message) {
                qCritical().noquote() << "Graphics initialization failed:" << message; QCoreApplication::exit(4);
            });

#if PGP_TEST_HOOKS
            if (parser.isSet(QStringLiteral("smoke-test"))) {
                window->resize(1280,720);
                if(parser.isSet(QStringLiteral("form-drafts-smoke")))
                    schedule_form_drafts_smoke(*window,campaign,paths,parser.value(QStringLiteral("screenshot")));
                else if(parser.isSet(QStringLiteral("content-recovery-smoke")))
                    schedule_content_recovery_smoke(*window,campaign,paths,parser.value(QStringLiteral("content-recovery-smoke")),parser.value(QStringLiteral("screenshot")));
                else if(parser.isSet(QStringLiteral("decision-smoke")))
                    schedule_decision_smoke(*window,campaign,parser.value(QStringLiteral("screenshot")),parser.isSet(QStringLiteral("portrait-smoke")));
                else if(parser.isSet(QStringLiteral("retirement-smoke")))
                    schedule_departure_smoke(*window,campaign,parser.value(QStringLiteral("screenshot")));
                else if (world_smoke)
                    schedule_world_smoke(*window,campaign,paths,parser.value(QStringLiteral("screenshot")),parser.isSet(QStringLiteral("portrait-smoke")),parser.isSet(QStringLiteral("workplace-smoke")),parser.isSet(QStringLiteral("device-smoke")),parser.isSet(QStringLiteral("contacts-smoke")),parser.isSet(QStringLiteral("manager-smoke")));
                else if (founding_smoke)
                    schedule_founding_smoke(*window,campaign,paths,parser.value(QStringLiteral("screenshot")));
                else if (menu_smoke)
                    schedule_menu_smoke(*window,campaign,music,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (music_smoke)
                    schedule_music_smoke(*window,campaign,music,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (objectives_smoke)
                    schedule_objectives_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (leadership_smoke)
                    schedule_succession_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (frontier_smoke)
                    schedule_frontier_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (society_smoke || institutions_smoke)
                    schedule_society_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")),institutions_smoke);
                else if (civic_smoke) schedule_civic_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (infrastructure_smoke) schedule_infrastructure_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
                else if (bundled || competition_smoke) schedule_playable_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")),competition_smoke);
                else schedule_game_smoke(*window,campaign,selection.api,paths,parser.value(QStringLiteral("screenshot")));
            }
#endif
            if (prototype_smoke || founding_smoke || world_smoke) window->show();
            else QMetaObject::invokeMethod(window,"applyDisplayPreferences",Qt::QueuedConnection);
            interface_ready=true;
            sync_music();
        },Qt::QueuedConnection);
        startup_deadline.start(5000);
        engine.load(QUrl(QStringLiteral("qrc:/pgp/game/Main.qml")));
        const auto result=application.exec();
        if (menu_smoke && result==0 && (!window || !window->property("menuSmokePassed").toBool()))
            throw std::runtime_error("The menu check ended before its final confirmed-exit assertion");
        return result;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
