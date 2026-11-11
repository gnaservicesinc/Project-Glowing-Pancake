#include "founding_fixture.hpp"
#include "recruitment_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
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

void schedule_recruitment_smoke(QQuickWindow &window, pgp::bridge::CampaignController &campaign,
                                pgp::persistence::LocalPaths paths, QString screenshot) {
    struct Walkthrough {
        int phase = 0, ticks = 0, frames = 0;
        std::uint32_t laptop = 0;
        QString first, second;
    };
    const auto run = std::make_shared<Walkthrough>();
    auto *timer = new QTimer(&window);
    timer->setInterval(100);
    QObject::connect(timer, &QTimer::timeout, &window, [&, run, timer, paths, screenshot] {
        const auto fail = [&](const char *reason) {
            std::cerr << "Recruiting phase " << run->phase << ": " << reason << " · " << campaign.notice().toStdString()
                      << '\n';
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".failure.png");
            timer->stop();
            QCoreApplication::exit(3);
        };
        const auto item = [&](const QString &name) { return find_visible(window.contentItem(), name); };
        const auto key = [&](const QString &name, Qt::Key code = Qt::Key_Space) {
            auto *control = item(name);
            if (!control || !control->isEnabled())
                return false;
            control->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(&window, code);
            return true;
        };
        const auto enter = [&](const QString &name, const QString &value) {
            auto *control = item(name);
            if (!control)
                return false;
            control->forceActiveFocus(Qt::TabFocusReason);
            if (!QMetaObject::invokeMethod(control, "selectAll"))
                return false;
            for (const auto digit : value)
                QTest::keyClick(&window, static_cast<Qt::Key>(digit.unicode()));
            return control->property("text").toString() == value;
        };
        const auto capture = [&](const QString &suffix) {
            return screenshot.isEmpty() || window.grabWindow().save(screenshot + "." + suffix + ".png");
        };
        const auto latest_reply_visible = [&](const QString &viewport_name) {
            const auto *viewport = item(viewport_name);
            const auto *reply = item("contactMessageCard");
            if (!viewport || !reply || !reply->hasActiveFocus()) {
                std::cerr << "Reply focus " << (reply && reply->hasActiveFocus()) << " · current "
                    << (window.activeFocusItem()?window.activeFocusItem()->objectName().toStdString():"none") << '\n';
                return false;
            }
            const auto bottom = reply->mapToItem(viewport, {0, reply->height()}).y();
            if(bottom<=0 || bottom>viewport->height()+1)
                std::cerr << "Reply bottom " << bottom << " · viewport " << viewport->height() << '\n';
            return bottom > 0 && bottom <= viewport->height() + 1;
        };
        if (++run->ticks > 500) {
            fail("Timed out");
            return;
        }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())
            return;
        const auto &state = campaign.snapshot()->checkpoint.world.simulation;
        const auto &roster = *state.management->people;
        auto *world = campaign.world();
        const auto first_status = [&] { return world->recruitmentOffer(run->first).value("status").toString(); };
        const auto second_status = [&] { return world->recruitmentOffer(run->second).value("status").toString(); };
        switch (run->phase) {
        case 0: {
            if (!key("menuNewGameButton")) {
                fail("New Game");
                return;
            }
            auto *founding = campaign.founding();
            founding->setField("family", "rich");
            founding->setField("route", "bootstrap");
            founding->setField("leg", "charisma");
            pgp::bridge::complete_formative_fixture(*founding);
            campaign.startFounderCampaign();
            break;
        }
        case 1:
            if (!roster.employees.empty() || roster.applicants.size() < 2) {
                fail("Solo founder and multiple applicants");
                return;
            }
            run->first = QString::fromStdString(roster.applicants[0].profile.id);
            run->second = QString::fromStdString(roster.applicants[1].profile.id);
            world->buyHQEquipment("desk", world->reviewToken());
            break;
        case 2:
            campaign.step();
            break;
        case 3:
            world->placeHQEquipment(1, 0, 0, false, 0, world->reviewToken());
            break;
        case 4:
            campaign.clearNotice();
            if (!key("peopleTab") || !key("peopleApplicantsTab")) {
                fail("Open applicants through People");
                return;
            }
            break;
        case 5:
            if (!key("hireApplicant_" + run->first)) {
                fail("Select the first applicant");
                return;
            }
            break;
        case 6:
            if (!enter("employmentSalary", "2500") || !enter("employmentSigning", "0") || !key("employmentSend")) {
                fail("Enter and send compensation terms");
                return;
            }
            break;
        case 7:
            if (first_status() != "waiting" || !key("employmentWait")) {
                fail("Advance to applicant reply");
                return;
            }
            break;
        case 8:
            if (first_status() != "counter" || !capture("counteroffer") || !key("employmentDecline")) {
                fail("Review and decline counteroffer");
                return;
            }
            break;
        case 9:
            if (first_status() != "declined" || !roster.employees.empty() || !key("employmentBack") || !key("hireApplicant_" + run->second)) {
                fail("Decline creates no employee. Choose someone else.");
                return;
            }
            break;
        case 10:
            if (!enter("employmentSalary", "2500") || !enter("employmentSigning", "0") || !key("employmentSend")) {
                fail("Send another applicant an offer");
                return;
            }
            break;
        case 11:
            if (second_status() != "waiting" || !key("employmentWait")) {
                fail("Wait for the second reply");
                return;
            }
            break;
        case 12:
            if (second_status() != "counter" || !key("employmentAcceptCounter")) {
                fail("Agree to the second counteroffer");
                return;
            }
            break;
        case 13:
            if (second_status() != "agreed" || !key("employmentDesk", Qt::Key_End)) {
                fail("Choose an actual free desk");
                return;
            }
            break;
        case 14:
            if (!capture("signing") || !key("employmentSign")) {
                fail("Sign and assign");
                return;
            }
            break;
        case 15: {
            const auto id = run->second.toStdString();
            if (roster.employees.size() != 1 || roster.employees.front().terms.profile.id != id ||
                pgp::core::assigned_hq_desk(state, id) != 1 || state.capacity->available_staff != 0) {
                fail("Correct employee, desk and onboarding");
                return;
            }
            const auto &program = *campaign.snapshot()->content;
            pgp::persistence::CurrentCampaign current{paths.saves_directory / "continuing"};
            if (current.resume(program) != std::optional{campaign.snapshot()->checkpoint}) {
                fail("Fresh signed company reload");
                return;
            }
            campaign.setTextScalePercent(200);
            window.showNormal();
            window.resize(650, 480);
            break;
        }
        case 16:
            if (window.visibility() != QWindow::Windowed || window.width() != 650 || window.height() != 480) {
                window.showNormal();
                window.resize(650, 480);
                run->frames = 0;
                return;
            }
            if (++run->frames < 8)
                return;
            if (!key("hireApplicant_" + run->first)) {
                fail("Compact applicant controls");
                return;
            }
            run->frames = 0;
            break;
        case 17:
            if (++run->frames < 6)
                return;
            if (!capture("compact") || !item("employmentStatus")) {
                fail("Compact discussion layout");
                return;
            }
            if(state.management->catalog.people->mood) {
                if(!key("employmentBack") || !key("peopleTeamTab")) {fail("Return to the signed employee's team profile");return;}
                run->frames=0;
                break;
            }
            run->phase=19;
            return;
        case 18:
            if(!key("personPortrait_"+run->second)) {fail("Open the signed employee's mood profile");return;}
            break;
        case 19:
            if(state.management->catalog.people->mood) {
                if(++run->frames<6)return;
                auto* mood=item("employeeMoodCard");
                auto* heading=item("employeeMoodHeading");
                auto* scroll=item("employeeProfileScroll");
                const auto report=mood?mood->property("mood").toMap():QVariantMap{};
                const auto expected=pgp::core::workplace_moods(state).at(run->second.toStdString());
                if(!mood || !heading || !scroll || heading->mapToScene({0,heading->height()}).y()>scroll->mapToScene({0,scroll->height()}).y() ||
                    report.value("score").toInt()!=expected.score || report.value("factors").toList().size()!=8 || !capture("mood-compact")) {
                    fail("The profile must show current workplace mood and all causes at 200% text");return;
                }
                if (world->contactsAvailable()) {
                    QTest::keyClick(&window, Qt::Key_Escape);
                    break;
                }
            }
            run->phase = 34;
            return;
        case 20:
            if (item("employeeProfileScroll") || !key("closeDeviceButton")) {
                fail("Return to the headquarters map");
                return;
            }
            world->buyDevice("tablet", world->reviewToken());
            break;
        case 21:
            run->laptop = static_cast<std::uint32_t>(state.infrastructure->workplace->headquarters->items.size() + 1);
            world->buyHQEquipment("laptop", world->reviewToken());
            break;
        case 22:
            if (roster.employees.front().ready_on > state.date) {
                campaign.step();
                return;
            }
            if (!key("hqTalkButton")) {
                fail("Open the room conversation");
                return;
            }
            break;
        case 23:
            if (!key("contactPerson_" + run->second) || !key("exchangeContactButton")) {
                fail("Meet the new employee and exchange contact details");
                return;
            }
            break;
        case 24:
            if (roster.contacts.size() != 1 || !key("contactCheckIn")) {
                fail("Ask a saved contact about their day");
                return;
            }
            run->frames = 0;
            break;
        case 25:
            if (++run->frames < 3) return;
            if (roster.contacts.front().messages.size() != 1 || !latest_reply_visible("roomConversationScroll") || !capture("room-conversation") ||
                !key("hqCloseTalk")) {
                fail("Read and close the room conversation");
                return;
            }
            break;
        case 26:
            if (item("hqCloseTalk")) return;
            if (!key("messagesTab")) {
                fail("Open Messages from the dock");
                return;
            }
            break;
        case 27:
            if (world->navigation().value("device") != "tablet" || world->navigation().value("section") != "messages" ||
                !key("contactPerson_" + run->second) || !key("contactWorkstation")) {
                fail("Open the same contact through the tablet and ask about their computer");
                return;
            }
            run->frames = 0;
            break;
        case 28: {
            if (++run->frames < 3) return;
            const auto &history = roster.contacts.front().messages;
            if (history.size() != 2 || history.front().topic != "check_in" || history.back().topic != "workstation" ||
                !latest_reply_visible("managementScroll") || !capture("tablet-messages")) {
                fail("The same history must appear on the tablet at 200% text");
                return;
            }
            if (!key("contactIssueLaptop_" + QString::number(run->laptop))) {
                fail("Issue a delivered inventory laptop through the conversation");
                return;
            }
            break;
        }
        case 29: {
            pgp::persistence::CurrentCampaign current{paths.saves_directory / "continuing"};
            if (!pgp::core::hq_laptop_ready(state, run->second.toStdString()) ||
                current.resume(*campaign.snapshot()->content) != std::optional{campaign.snapshot()->checkpoint}) {
                fail("Conversation history and the laptop assignment must reload");
                return;
            }
            std::cout << "Headquarters contact exchange, tablet messages, laptop issuance and fresh reload passed\n";
            if(world->contactThread(run->second).value("canDismiss").toBool()) {
                if(!key("contactReviewDismissal")) {fail("Open the dismissal review");return;}
                run->frames=0;
            } else run->phase=33;
            break;
        }
        case 30: {
            if(++run->frames<3) return;
            const auto quote=world->dismissalQuote(run->second,0);
            const auto* terms=item("contactDismissalTerms");
            const auto* confirm=item("contactDismissalConfirm");
            if(!terms || !confirm || !confirm->isEnabled() || !quote.value("allowed").toBool() ||
                terms->property("text").toString()!=quote.value("detail").toString() ||
                confirm->mapToScene({0,confirm->height()}).y()>window.height() || !capture("dismissal-review-compact")) {
                fail("Show quoted severance and reachable confirmation at 200% text, then cancel");return;
            }
            auto* scroll=item("contactDismissalScroll");
            auto* content=scroll?scroll->property("contentItem").value<QObject*>():nullptr;
            if(!content || !content->setProperty("contentY",std::max(0.,content->property("contentHeight").toDouble()-content->property("height").toDouble()))) {
                fail("Scroll through the complete dismissal terms");return;
            }
            run->frames=0;run->phase=35;return;
        }
        case 31:
            if(roster.employees.front().departed_on || !pgp::core::hq_laptop_ready(state,run->second.toStdString()) ||
                !key("contactReviewDismissal")) {
                fail("Cancelling must preserve employment and the issued laptop");return;
            }
            run->frames=0;
            break;
        case 32:
            if(++run->frames<3) return;
            if(!key("contactDismissalConfirm")) {fail("Confirm the reviewed dismissal");return;}
            run->frames=0;
            break;
        case 33: {
            if(++run->frames<3) return;
            pgp::persistence::CurrentCampaign current{paths.saves_directory / "continuing"};
            const auto& employee=roster.employees.front();
            const auto* status=item("contactEmploymentStatus");
            const auto* viewport=item("managementScroll");
            if(!employee.departed_on || !employee.dismissal_severance || pgp::core::hq_laptop_ready(state,run->second.toStdString()) ||
                world->contactThread(run->second).value("canDismiss").toBool() ||
                !world->contactThread(run->second).value("status").toString().contains("Severance") ||
                !status || !viewport || !status->hasActiveFocus() || status->mapToItem(viewport,{0,status->height()}).y()>viewport->height()+1 ||
                current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint} || !capture("dismissed-compact")) {
                fail("Departure, former contact, returned laptop and paid severance must reload");return;
            }
            std::cout<<"Reviewed dismissal, cancellation, company severance, returned laptop, former contact and compact fresh reload passed\n";
            break;
        }
        case 35: {
            if(++run->frames<3) return;
            const auto* terms=item("contactDismissalTerms");
            const auto* scroll=item("contactDismissalScroll");
            if(!terms || !scroll || terms->mapToItem(scroll,{0,terms->height()}).y()>scroll->height()+1 ||
                !capture("dismissal-terms-compact") || !key("contactDismissalBack")) {
                fail("The final terms and cancellation must be reachable at 200% text");return;
            }
            run->phase=31;return;
        }
        case 34:
            std::cout << "Recruiting UI passed: comparison, two counteroffers, decline, signing, desk assignment, "
                         "onboarding, fresh reload and 200% text\n";
            timer->stop();
            QCoreApplication::exit(0);
            return;
        }
        ++run->phase;
    });
    timer->start();
}
