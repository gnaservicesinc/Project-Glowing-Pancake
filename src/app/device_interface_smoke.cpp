#include "founding_fixture.hpp"
#include "device_interface_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
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
            auto *device = item("activeDeviceFrame");
            return device && device->property("kind").toString() == QLatin1String{kind};
        };
        auto *world = campaign.world();
        const auto app = world->navigation().value("app").toString();
        switch (run->phase) {
        case 0: {
            if (!press("menuNewGameButton")) {
                fail("New Game");
                return;
            }
            auto *founding = campaign.founding();
            founding->setField("family", "rich");
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
            if (app != "lab" || !frame("laptop") || !item("technologyGraph") || !capture("laptop-research")) {
                fail("Research must open in the laptop screen");
                return;
            }
            if (!press("deviceHomeButton")) {
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
            if (app != "inbox" || !frame("phone") || !capture("phone-inbox") || !press("closeDeviceButton")) {
                fail("Phone app and return to workspace");
                return;
            }
            break;
        case 7:
            if (!app.isEmpty() || item("activeDeviceFrame")) {
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
            if (app != "people" || !frame("laptop") || !item("deviceTimeButton") || !press("closeDeviceButton")) {
                fail("Compact app frame and reachable time controls");
                return;
            }
            break;
        case 11:
            if (!press("dockPrevious") || !press("dockNext")) {
                fail("Compact dock scroll buttons must reveal apps in both directions");
                return;
            }
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
