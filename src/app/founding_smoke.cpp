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
    auto bounds = item.mapRectToScene(QRectF{0, 0, item.width(), item.height()});
    for(auto* parent=item.parentItem();parent;parent=parent->parentItem()) if(parent->clip())
        bounds=bounds.intersected(parent->mapRectToScene({0,0,parent->width(),parent->height()}));
    if(bounds.isEmpty()) return {};
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
        int ticks = 0,home_phase=0;
        bool saw_opening_animation = false;
        bool retried_name = false;
        bool edited_seed = false;
        bool saw_hillside = false;
        bool saw_company = false;
        bool customized = false;
        pgp::runtime::Checkpoint opening;
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
            if (!screen || !screen->isVisible() || !item("workspaceScene") || item("workspaceScene")->isVisible() || id != "opening" ||
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
            if (id.startsWith("chapter-") && !id.endsWith("-after")) {
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
            static const QHash<QString, QString> answers{
                {"family","poor"},{"leg","charisma"},{"sociability","outgoing"},{"activity","athletic"},
                {"disposition","deliberate"},{"legal_form","nonprofit"},{"route","mission_director"},
                {"mission","survival"},{"offer-safety","accept"},{"offer-open_science","decline"},
                {"headquarters","US-CA"},{"gender","woman"},{"pronouns","she/her"}
            };
            if(id=="appearance" && !state->customized) {
                const auto before=story->portrait();
                if(!click("appearanceNext_1") || before==story->portrait() ||
                    item("foundingPortraitPreview")->property("status").toInt()!=1) {fail("Live appearance customization");return;}
                state->customized=true;
                return;
            }
            if(id=="appearance" && !capture(".appearance")) {fail("Appearance screenshot");return;}
            if (id == "offer-safety" && !capture(".offer")) { fail("Individual offer screenshot"); return; }
            if (const auto chosen = answers.find(id); chosen != answers.end()) {
                if (!click("foundingChoice_" + *chosen)) fail("Expected response is unavailable");
            } else if (id == "home") {
                if(item("homeStart")->property("transitioning").toBool())return;
                if(state->home_phase==0) {
                    if(!capture(".home") || !click("homeApp_browser")) {fail("Home desktop before company formation");return;}
                    story->setField("seed","718");++state->home_phase;return;
                }
                if(state->home_phase==1) {if(!click("homeService_legal")){fail("Browser legal service");return;}++state->home_phase;return;}
                if(state->home_phase==2) {if(!click("homeFoundNonprofit")){fail("Nonprofit formation");return;}++state->home_phase;return;}
                if(state->home_phase==3) {if(!click("homeBacker_safety")){fail("Named backer offer");return;}++state->home_phase;return;}
                if(state->home_phase==4) {if(!capture(".terms") || !click("homeSign")){fail("Reviewed signing");return;}++state->home_phase;return;}
                if(state->home_phase==5) {
                    if(!item("homeEnterCompany")->isVisible())return;
                    item("homeEnterCompany")->forceActiveFocus(Qt::TabFocusReason);
                    ++state->home_phase;return;
                }
                if(state->home_phase==6) {if(!capture(".signed") || !click("homeEnterCompany")){fail("Signed company entry");return;}state->phase=Phase::office;}
            } else if (!advance()) fail("Could not advance narration or answer");
            break;
        }
        case Phase::office: {
            const auto& world = campaign.snapshot()->checkpoint.world;
            if (campaign.welcome() || story->hasDraft() || !world.founding || !world.identity || world.founding->choices.size() != 8 ||
                world.founding->legal_form != "nonprofit" || world.founding->backers != std::vector<std::string>{"safety"} ||
                world.identity->founder_name != "Mira Vale" || world.root_seed != 718 ||
                world.founding->gender!="woman" || world.identity->founder_pronouns!="she/her" ||
                !state->customized || world.founding->appearance.parts[1]!=3 ||
                !world.difficulty.empty() || !world.simulation.capacity->sources.empty() ||
                !world.simulation.management->people->employees.empty() ||
                pgp::core::iso_date(world.simulation.date) != "2015-01-01" ||
                pgp::core::iso_date(world.simulation.end_date) != "2115-01-01" || !campaign.paused()) {
                fail("The founding choices did not reach the paused campaign"); return;
            }
            state->opening = campaign.snapshot()->checkpoint;
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory / "founder-ui.pgpsave").u16string())));
            state->phase = Phase::step;
            break;
        }
        case Phase::step:
            campaign.step();
            state->phase = Phase::reload;
            break;
        case Phase::reload: {
            if (campaign.date() != QStringLiteral("2015-01-02")) { fail("First day did not advance"); return; }
            try {
                const auto& program=*campaign.snapshot()->content;
                const auto saved=pgp::persistence::load_file(program,paths.saves_directory/"founder-ui.pgpsave");
                pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
                const auto resumed=current.resume(program);
                if(saved.checkpoint!=state->opening || !resumed || *resumed!=campaign.snapshot()->checkpoint) {
                    fail("Fresh export and automatic day continuation must reload exactly");return;
                }
            } catch(const std::exception& error) {fail(QString::fromUtf8(error.what()));return;}
            state->phase = Phase::done;
            break;
        }
        case Phase::done:
            if (!campaign.snapshot()->checkpoint.world.founding || campaign.welcome()) {
                fail("Exact save continuation"); return;
            }
            std::cout << "Prologue UI passed: hospital opening, resized 650×480 canvas, editable name, branching art and palette, "
                         "eight chapters, individual offers, world seed, appearance, paused office and fresh-save/day continuation\n";
            timer->stop();
            QCoreApplication::exit(0);
            break;
        }
    });
    timer->start();
}
