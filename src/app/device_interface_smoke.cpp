#include "founding_fixture.hpp"
#include "device_interface_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>
#include <memory>

namespace {
QQuickItem *visible_item(QQuickItem *root, const QString &name) {
    if (!root->isVisible())
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *result = visible_item(child, name))
            return result;
    return nullptr;
}
} // namespace

void schedule_device_interface_smoke(QQuickWindow &window, pgp::bridge::CampaignController &campaign,
                                     pgp::persistence::LocalPaths paths, QString screenshot) {
    struct Run {
        int phase = 0, ticks = 0, frames = 0;
        std::string fingerprint;
    };
    const auto run = std::make_shared<Run>();
    auto *timer = new QTimer(&window);
    timer->setInterval(120);
    QObject::connect(timer, &QTimer::timeout, &window, [&, run, timer, paths, screenshot] {
        const auto fail = [&](const char *why) {
            std::cerr << "Device interface phase " << run->phase << ": " << why << " · "
                      << campaign.notice().toStdString() << '\n';
            timer->stop();
            QCoreApplication::exit(3);
        };
        const auto item = [&](const QString &name) { return visible_item(window.contentItem(), name); };
        const auto press = [&](const QString &name) {
            auto *control = item(name);
            if (!control || !control->isEnabled())
                return false;
            control->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(&window, Qt::Key_Space);
            return true;
        };
        const auto capture = [&](QString tag) {
            return screenshot.isEmpty() || window.grabWindow().save(screenshot + "." + tag + ".png");
        };
        if (++run->ticks > 480) {
            fail("Timed out");
            return;
        }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())
            return;
        const auto frame = [&](const char *kind) {
            auto *device = item("workspaceDeviceFrame");
            return device && device->property("kind").toString() == QLatin1String{kind};
        };
        auto *world = campaign.world();
        const auto activeApp = [&](const QString& kind) {
            return world->deviceWorkspace(world->resolvePhysicalDevice(kind, "")).value("activeApp").toString();
        };
        switch (run->phase) {
        case 0: {
            if (QCoreApplication::arguments().contains("--playtest-setup")) {
                if (!item("playtestSetupStartButton")) return;
                if (!capture("setup") || !press("playtestSetupStartButton")) { fail("Start custom playtest"); return; }
                break;
            }
            if (!press("menuNewGameButton")) {
                fail("New Game");
                return;
            }
            auto *founding = campaign.founding();
            founding->setField("family", "rich");
            founding->setAdvantage("genius",true);
            founding->setField("route", "bootstrap");
            pgp::bridge::complete_formative_fixture(*founding);
            campaign.startFounderCampaign();
            break;
        }
        case 1:
            if (!campaign.currentCampaign()) {
                fail("Fresh company");
                return;
            }
            run->fingerprint = pgp::persistence::CurrentCampaign{paths.saves_directory / "continuing"}.fingerprint();
            campaign.clearNotice();
            if (!press("devicesButton")) {
                fail("Open the physical device launcher from the dock");
                return;
            }
            break;
        case 2:
            if (++run->frames < 5)
                return;
            if (!item("launcherDevice_laptop") || !capture("laptop-desktop") || !press("labApp")) {
                fail("Laptop application tiles");
                return;
            }
            run->frames = 0;
            break;
        case 3:
            if (activeApp("laptop") != "lab" || !frame("laptop") || !item("technologyGraph") || !capture("laptop-research")) {
                fail("Research must open in the laptop screen");
                return;
            }
            if (!press("surfaceHomeButton")) {
                fail("Switch apps inside the laptop");
                return;
            }
            break;
        case 4:
            if (!press("launcherDevice_phone")) {
                fail("Select the working phone");
                return;
            }
            break;
        case 5: {
            auto *research = item("labApp");
            if (!research || research->isEnabled() || !press("inboxApp")) {
                fail("Only compatible apps open on the phone");
                return;
            }
            break;
        }
        case 6:
            if (activeApp("phone") != "inbox" || !frame("phone") || !capture("phone-inbox") || !press("workspaceCloseButton")) {
                fail("Phone app and return to workspace");
                return;
            }
            break;
        case 7:
            if (item("workspaceDeviceFrame")) {
                fail("Returning restores the world view");
                return;
            }
            campaign.setTextScalePercent(200);
            window.showNormal();
            window.resize(650, 480);
            break;
        case 8:
            if (window.visibility() != QWindow::Windowed || window.width() != 650 || window.height() != 480) {
                window.showNormal();
                window.resize(650, 480);
                run->frames = 0;
                return;
            }
            if (++run->frames < 8)
                return;
            campaign.clearNotice();
            if (!press("devicesButton")) {
                fail("Compact device launcher");
                return;
            }
            run->frames = 0;
            break;
        case 9:
            if (++run->frames < 5)
                return;
            if (!press("launcherDevice_laptop") || !capture("compact-desktop") || !press("peopleApp")) {
                fail("Compact app tiles and keyboard access");
                return;
            }
            break;
        case 10:
            if (activeApp("laptop") != "people" || !frame("laptop") || !item("workspaceTimeButton") || !press("workspaceCloseButton")) {
                fail("Compact app frame and reachable time controls");
                return;
            }
            break;
        case 11:
            if (!press("dockPrevious") || !press("dockNext")) {
                fail("Compact dock scroll buttons must reveal apps in both directions");
                return;
            }
            if (!press("devicesButton") || !press("launcherDevice_laptop") || !press("surfaceHomeButton") || !press("dataApp")) {
                fail("Open the corpus library in the laptop workspace");
                return;
            }
            break;
        case 12:
            if (!item("dataAppRoot") || !press("surfacePopOutButton")) { fail("Detach the Data app"); return; }
            break;
        case 13: {
            QQuickWindow* native = nullptr;
            for (auto* candidate : QGuiApplication::topLevelWindows())
                if (candidate->isVisible() && candidate->objectName().startsWith("detachedDeviceWindow_"))
                    native = qobject_cast<QQuickWindow*>(candidate);
            if (!native || !visible_item(native->contentItem(), "dataAppRoot")) { fail("Native window hosts the live corpus library"); return; }
            const auto before = native;
            auto* tab = visible_item(native->contentItem(), "deviceTab_lab");
            if (!tab) { fail("Open research from detached switcher"); return; }
            tab->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(native, Qt::Key_Space);
            if (!before->close() && activeApp("laptop") != "lab") { fail("Detached application selection"); return; }
            break;
        }
        case 14:
            if (activeApp("laptop") != "lab" || !item("technologyGraph")) { fail("Docked research resumes in the popup"); return; }
            if (!press("researchPopOutTree")) { fail("Expand the selected research tree"); return; }
            break;
        case 15: {
            QQuickWindow* tree = nullptr;
            for (auto* candidate : QGuiApplication::topLevelWindows())
                if (candidate->isVisible() && candidate->objectName() == "detachedResearchTreeWindow")
                    tree = qobject_cast<QQuickWindow*>(candidate);
            if (!tree || !visible_item(tree->contentItem(), "technologyGraph")) { fail("Expanded research workspace"); return; }
            tree->close();
            break;
        }
        case 16:
            if (!press("surfaceHomeButton") || !press("modelsApp")) { fail("Open model checkpoints"); return; }
            break;
        case 17:
            if (!item("modelsAppRoot") || !press("surfaceHomeButton") || !press("accountsApp")) { fail("Inspect model checkpoints and open accounts"); return; }
            break;
        case 18:
            if (!item("accountsAppRoot") || !press("surfaceHomeButton") || !press("productsApp")) { fail("Inspect financial statements and open products"); return; }
            break;
        case 19:
            if (!item("productsAppRoot")) { fail("Inspect product designs"); return; }
            if (run->fingerprint !=
                pgp::persistence::CurrentCampaign{paths.saves_directory / "continuing"}.fingerprint()) {
                fail("Device navigation must not mutate the company");
                return;
            }
            std::cout << "Device interface passed: laptop launcher, in-device switching, compatible phone apps, "
                         "compact keyboard and return to world\n";
            timer->stop();
            QCoreApplication::exit(0);
            return;
        }
        ++run->phase;
    });
    timer->start();
}
