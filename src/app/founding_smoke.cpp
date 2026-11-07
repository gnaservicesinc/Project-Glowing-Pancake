#include "founding_smoke.hpp"
#include "campaign_controller.hpp"

#include <QAccessible>
#include <QCoreApplication>
#include <QHash>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>
#include <memory>

namespace {
QQuickItem* find(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto* child : root->childItems())
        if (auto* result = find(child, name)) return result;
    return nullptr;
}

bool press(QQuickWindow& window, QQuickItem* item, Qt::Key code = Qt::Key_Space) {
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down{QEvent::KeyPress, code, Qt::NoModifier}, up{QEvent::KeyRelease, code, Qt::NoModifier};
    QCoreApplication::sendEvent(&window, &down);
    QCoreApplication::sendEvent(&window, &up);
    return true;
}

void type(QQuickWindow& window, QQuickItem& input, const QString& value) {
    input.forceActiveFocus(Qt::TabFocusReason);
    QMetaObject::invokeMethod(&input, "selectAll");
    QKeyEvent event{QEvent::KeyPress, 0, Qt::NoModifier, value};
    QCoreApplication::sendEvent(&window, &event);
}

// Use scene coordinates so this catches the real resized hit regions and
// verifies that complete text is drawn, even when there is ample logical space.
QString fitting_error(QQuickItem& item, const QRectF& viewport) {
    if (!item.isVisible()) return {};
    const auto bounds = item.mapRectToScene(QRectF{0, 0, item.width(), item.height()});
    if (!bounds.isEmpty() && !viewport.contains(bounds))
        return item.objectName() + QStringLiteral(" lies outside the window");
    if (item.property("truncated").toBool())
        return QStringLiteral("Clipped copy: ") + item.property("text").toString();
    for (auto* child : item.childItems())
        if (auto error = fitting_error(*child, viewport); !error.isEmpty()) return error;
    return {};
}
}

void schedule_founding_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    pgp::persistence::LocalPaths paths, QString screenshot) {
    enum class Phase { launch, opening, birth, compact, questions, office, step, reload, done };
    struct State {
        Phase phase = Phase::launch;
        int ticks = 0;
        bool saw_opening_animation = false;
        bool retried_name = false;
        bool edited_seed = false;
        bool saw_hillside = false;
        bool saw_company = false;
        QString revision;
    };
    auto state = std::make_shared<State>();
    auto* timer = new QTimer(&window);
    timer->setInterval(60);
    QObject::connect(timer, &QTimer::timeout, &window, [&, timer, state, paths, screenshot] {
        auto* story = campaign.founding();
        const auto id = story->beat();
        const auto fail = [&](const QString& why) {
            std::cerr << "Prologue UI [" << id.toStdString() << "]: " << why.toStdString()
                      << " " << campaign.notice().toStdString() << '\n';
            timer->stop();
            QCoreApplication::exit(3);
        };
        if (++state->ticks > 1050) { fail("Timed out"); return; }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto item = [&](const QString& name) { return find(window.contentItem(), name); };
        const auto click = [&](const QString& name) { return press(window, item(name)); };
        const auto capture = [&](const QString& label) {
            return screenshot.isEmpty() || window.grabWindow().save(screenshot + label + ".png");
        };
        const QRectF viewport{-0.5, -0.5, window.contentItem()->width() + 1, window.contentItem()->height() + 1};
        auto* screen = item("foundingScreen");
        auto* scene = item("foundingScene");
        const auto shows = [&](const QString& artwork, const QString& tone) {
            return scene->property("status").toInt() == 1 &&
                scene->property("source").toUrl().fileName() == artwork + ".svg" &&
                screen->property("sceneTone").toString() == tone;
        };
        if (screen && screen->isVisible() && screen->property("settling").toBool()) {
            // Let the first scene play, then exercise player-controlled reveal.
            if (state->phase == Phase::opening && item("foundingAfterword")->opacity() < 1.0)
                state->saw_opening_animation = true;
            if (state->phase == Phase::questions) press(window, screen, Qt::Key_Return);
            return;
        }
        if (screen && screen->isVisible() && scene->opacity() < 1) return;

        switch (state->phase) {
        case Phase::launch:
            if (!click("menuNewGameButton")) { fail("New Game is unavailable"); return; }
            state->phase = Phase::opening;
            break;
        case Phase::opening:
            if (!screen || !screen->isVisible() || item("statusLine")->isVisible() || id != "opening" ||
                (!campaign.reducedMotion() && !state->saw_opening_animation) ||
                item("foundingQuestion")->isVisible() || !item("foundingAfterword")->property("text").toString().contains("newborn") ||
                !shows("hospital", "slate")) {
                fail("New Game must open on the hospital narration"); return;
            }
            if (!capture(".opening") || !click("foundingNextButton")) { fail("Opening narration"); return; }
            state->phase = Phase::birth;
            break;
        case Phase::birth: {
            auto* input = item("foundingAnswer");
            if (id != "first_name" || !input || !input->isVisible() || !QAccessible::queryAccessibleInterface(input)) {
                fail("The first question must be a single, accessible name entry"); return;
            }
            if (!capture(".name")) { fail("Full size name screenshot"); return; }
            window.resize(650, 480);
            state->phase = Phase::compact;
            break;
        }
        case Phase::compact: {
            if (window.width() != 650 || window.height() != 480 || id != "first_name") {
                fail("Resizing must preserve the current beat"); return;
            }
            const auto error = fitting_error(*screen, viewport);
            if (!error.isEmpty()) { fail(error); return; }
            auto* input = item("foundingAnswer");
            if (!state->retried_name) {
                type(window, *input, QStringLiteral(" "));
                if (!press(window, input, Qt::Key_Return) || id != story->beat() || !item("foundingError")->isVisible()) {
                    fail("Empty name should stay on the same question"); return;
                }
                state->retried_name = true;
                return;
            }
            type(window, *input, QStringLiteral("Mira"));
            if (!capture(".compact") || !press(window, input, Qt::Key_Return)) { fail("Compact name entry"); return; }
            state->phase = Phase::questions;
            break;
        }
        case Phase::questions: {
            const auto error = fitting_error(*screen, viewport);
            if (!error.isEmpty()) { fail(error); return; }
            const auto advance = [&] { return click("foundingNextButton"); };
            if (id == "last_name" && !story->form().value("first_name").toString().startsWith("Mira")) {
                fail("Edited name was lost after resizing"); return;
            }
            if (id.startsWith("chapter-")) {
                if (id == "chapter-curiosity" && !capture(".childhood")) { fail("Childhood screenshot"); return; }
                for (const auto& row : story->chapters()) {
                    const auto chapter = row.toMap();
                    if ("chapter-" + chapter.value("id").toString() != id) continue;
                    const auto choice = chapter.value("choices").toList().front().toMap().value("id").toString();
                    if (!click("foundingChoice_" + choice)) fail("Childhood choice is unavailable");
                    return;
                }
                fail("Unknown childhood beat");
                return;
            }
            if (id == "family" && !state->saw_hillside) {
                if (!click("foundingChoice_rich")) fail("Hillside family choice");
                return;
            }
            if (id == "leg" && !state->saw_hillside) {
                if (!shows("street-hillside", "olive") || !capture(".hillside") || !click("foundingBackButton")) {
                    fail("Family choice must change the art and palette");
                    return;
                }
                state->saw_hillside = true;
                return;
            }
            if (id == "legal_form" && !state->saw_company) {
                if (!click("foundingChoice_corporation")) fail("Corporation choice");
                return;
            }
            if (id == "route" && !state->saw_company) {
                if (!shows("company", "amber") || !capture(".company") || !click("foundingBackButton")) {
                    fail("Corporation choice must reveal the term sheet scene");
                    return;
                }
                state->saw_company = true;
                return;
            }
            static const QHash<QString, QString> answers{
                {"family","poor"},{"leg","charisma"},{"sociability","outgoing"},{"activity","athletic"},
                {"disposition","deliberate"},{"legal_form","nonprofit"},{"route","mission_director"},
                {"mission","survival"},{"offer-safety","accept"},{"offer-open_science","decline"},
                {"headquarters","US-CA"},{"office","home"},{"researcher","first_hire"},{"difficulty","difficulty.standard"}
            };
            if (id == "leg" && (!shows("street-apartments", "slate") || item("foundingChoice_wealthy")->isEnabled() ||
                !capture(".apartments"))) { fail("Apartment scene and wealth eligibility"); return; }
            if (id == "route" && !shows("trust", "jade")) { fail("Nonprofit scene and palette"); return; }
            if (id == "mission" && item("foundingChoice_profit")->isEnabled()) { fail("Nonprofit mission compatibility"); return; }
            if (id == "offer-safety" && !capture(".offer")) { fail("Individual offer screenshot"); return; }
            if (const auto chosen = answers.find(id); chosen != answers.end()) {
                if (!click("foundingChoice_" + *chosen)) fail("Expected response is unavailable");
            } else if (id == "review") {
                if (!state->edited_seed) {
                    state->edited_seed = true;
                    if (!click("foundingSeedButton")) fail("World seed action");
                    return;
                }
                if (!capture(".terms") || !click("foundingStartButton")) { fail("Opening terms or office entry"); return; }
                state->phase = Phase::office;
            } else if (id == "seed") {
                type(window, *item("foundingAnswer"), QStringLiteral("718"));
                if (!advance()) fail("Optional seed entry");
            } else if (!advance()) fail("Could not advance narration or answer");
            break;
        }
        case Phase::office: {
            const auto& world = campaign.snapshot()->checkpoint.world;
            if (campaign.welcome() || story->hasDraft() || !world.founding || !world.identity || world.founding->choices.size() != 8 ||
                world.founding->legal_form != "nonprofit" || world.founding->backers != std::vector<std::string>{"safety"} ||
                world.identity->founder_name != "Mira Vale" || world.root_seed != 718 ||
                pgp::core::iso_date(world.simulation.date) != "2015-01-01" ||
                pgp::core::iso_date(world.simulation.end_date) != "2115-01-01" || !campaign.paused()) {
                fail("The founding choices did not reach the paused campaign"); return;
            }
            state->revision = campaign.revision();
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory / "founder-ui.pgpsave").u16string())));
            state->phase = Phase::step;
            break;
        }
        case Phase::step:
            campaign.step();
            state->phase = Phase::reload;
            break;
        case Phase::reload:
            if (campaign.date() != QStringLiteral("2015-01-02")) { fail("First day did not advance"); return; }
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory / "founder-ui.pgpsave").u16string())));
            state->phase = Phase::done;
            break;
        case Phase::done:
            if (campaign.revision() != state->revision || !campaign.snapshot()->checkpoint.world.founding || campaign.welcome()) {
                fail("Exact save continuation"); return;
            }
            std::cout << "Prologue UI passed: hospital opening, resized 650×480 canvas, editable name, branching art and palette, "
                         "eight chapters, individual offers, world seed, paused office and exact reload\n";
            timer->stop();
            QCoreApplication::exit(0);
            break;
        }
    });
    timer->start();
}
