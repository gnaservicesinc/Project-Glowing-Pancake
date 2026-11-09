#include "research_smoke.hpp"
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
QQuickItem *find_visible(QQuickItem *root, const QString &name) {
    if (!root->isVisible())
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *match = find_visible(child, name))
            return match;
    return nullptr;
}
} // namespace

void schedule_research_smoke(QQuickWindow &window, pgp::bridge::CampaignController &campaign,
                             pgp::persistence::LocalPaths paths, QString screenshot) {
    struct Run {
        int phase = 0, ticks = 0, waited = 0, frames = 0;
        QString worker;
    };
    const auto run = std::make_shared<Run>();
    auto *timer = new QTimer(&window);
    timer->setInterval(100);
    QObject::connect(timer, &QTimer::timeout, &window, [&, run, timer, paths, screenshot] {
        const auto fail = [&](const char *reason) {
            std::cerr << "Research UI phase " << run->phase << ": " << reason << " · "
                      << campaign.notice().toStdString() << '\n';
            timer->stop();
            QCoreApplication::exit(3);
        };
        const auto item = [&](const QString &name) { return find_visible(window.contentItem(), name); };
        const auto press = [&](const QString &name) {
            auto *control = item(name);
            if (!control || !control->isEnabled())
                return false;
            control->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(&window, Qt::Key_Space);
            return true;
        };
        const auto capture = [&](QString label) {
            return screenshot.isEmpty() || window.grabWindow().save(screenshot + "." + label + ".png");
        };
        if (++run->ticks > 500) {
            fail("Timed out");
            return;
        }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())
            return;
        const auto &s = campaign.snapshot()->checkpoint.world.simulation;
        const auto *manager = campaign.management();
        auto *world = campaign.world();
        const QString root = "research.reproducible_tooling", child = "research.sequence_modeling";
        switch (run->phase) {
        case 0: {
            if (!press("menuNewGameButton")) {
                fail("New Game");
                return;
            }
            auto *founding = campaign.founding();
            founding->setField("family", "rich");
            founding->setField("route", "bootstrap");
            for (const auto &row : founding->chapters()) {
                const auto chapter = row.toMap();
                founding->choose(chapter.value("id").toString(),
                                 chapter.value("choices").toList().front().toMap().value("id").toString());
            }
            campaign.startFounderCampaign();
            break;
        }
        case 1:
            if (!manager->groundedResearch() || !s.capacity->sources.empty()) {
                fail("Garage must start without research compute");
                return;
            }
            run->worker = QString::fromStdString(s.management->people->employees.front().terms.profile.id);
            world->buyHQEquipment("desk", world->reviewToken());
            break;
        case 2:
            world->buyHQEquipment("desktop", world->reviewToken());
            break;
        case 3:
            if (run->waited++ < 3) {
                campaign.step();
                return;
            }
            world->placeHQEquipment(1, 0, 0, false, 0, world->reviewToken());
            break;
        case 4:
            world->placeHQEquipment(2, 0, 0, false, 1, world->reviewToken());
            break;
        case 5:
            world->assignHQDesk(1, "founder", world->reviewToken());
            break;
        case 6:
            campaign.clearNotice();
            if (!press("labTab")) {
                fail("Open the Lab from the world dock");
                return;
            }
            break;
        case 7:
            if (!press("technology_" + child)) {
                fail("Select a connected technology");
                return;
            }
            break;
        case 8: {
            auto *start = item("startGroundedResearch");
            if (!start || start->isEnabled() ||
                !manager->researchTopic(child).value("blocker").toString().contains("prerequisite")) {
                fail("Connected prerequisite must lock the start action");
                return;
            }
            if (!press("technology_" + root) || !capture("graph")) {
                fail("Select an available graph node");
                return;
            }
            break;
        }
        case 9:
            if (!press("researcher_" + run->worker)) {
                fail("Assign the named researcher");
                return;
            }
            break;
        case 10:
            if (!press("startGroundedResearch")) {
                fail("Start from the graph with the workstation");
                return;
            }
            break;
        case 11:
            if (s.lab->research.size() != 1 ||
                s.lab->research.front().researchers != std::vector{run->worker.toStdString()}) {
                fail("Research uses the selected person");
                return;
            }
            campaign.step();
            break;
        case 12: {
            const auto report = manager->researchTopic(root);
            if (!report.value("progress").toString().contains("1 of 6 staff-days")) {
                fail("Show settled people and compute progress");
                return;
            }
            world->powerHQEquipment(2, false, world->reviewToken());
            break;
        }
        case 13: {
            const auto report = manager->researchTopic(root);
            if (!report.value("paused").toBool() || !report.value("hold").toString().contains("powered computer")) {
                fail("Power loss pauses with a reason");
                return;
            }
            if (!press("rentCompute_offer.supplier_small") || !capture("paused")) {
                fail("Review and sign named rental");
                return;
            }
            break;
        }
        case 14: {
            if (s.capacity->sources.size() != 2) {
                fail("Paid rental registers a new source");
                return;
            }
            pgp::persistence::CurrentCampaign current{paths.saves_directory / "continuing"};
            if (current.resume(*campaign.snapshot()->content) != std::optional{campaign.snapshot()->checkpoint}) {
                fail("Research and lease fresh save");
                return;
            }
            campaign.setTextScalePercent(200);
            window.showNormal();
            window.resize(650, 480);
            break;
        }
        case 15:
            if (window.visibility() != QWindow::Windowed || window.width() != 650 || window.height() != 480) {
                window.showNormal();
                window.resize(650, 480);
                run->frames = 0;
                return;
            }
            if (++run->frames < 8)
                return;
            if (!press("technology_" + root) || !item("technologyPicker") || !capture("compact")) {
                fail("Compact graph and keyboard selector");
                return;
            }
            std::cout << "Research UI passed: graph selection, prerequisite lock, named staff, local compute, "
                         "progress, power loss, paid rental and exact fresh reload\n";
            timer->stop();
            QCoreApplication::exit(0);
            return;
        }
        ++run->phase;
    });
    timer->start();
}
