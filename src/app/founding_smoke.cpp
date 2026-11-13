#include "founding_smoke.hpp"
#include "campaign_controller.hpp"

#include <QAccessible>
#include <QCoreApplication>
#include <QHash>
#include <QJSValue>
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

QQuickItem* find_visible(QQuickItem* root, const QString& name) {
    if(!root->isVisible()) return nullptr;
    if(root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* result=find_visible(child,name)) return result;
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
        return QStringLiteral("%1 %2 at %3,%4 %5×%6 lies outside %7×%8").arg(item.metaObject()->className(),item.objectName()).arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height()).arg(viewport.width()).arg(viewport.height());
    if (item.property("truncated").toBool())
        return QStringLiteral("Clipped copy: ") + item.property("text").toString();
    for (auto* child : item.childItems())
        if (auto error = fitting_error(*child, viewport); !error.isEmpty()) return error;
    return {};
}
}

void schedule_founding_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    pgp::persistence::LocalPaths paths, QString screenshot, bool partner) {
    enum class Phase { launch, opening, birth, compact, questions, office, step, reload,
        company_messages, select_contact, question_contact, verify_contact, done };
    struct State {
        Phase phase = Phase::launch;
        int ticks = 0,home_phase=0,frames=0,keepsake_frames=0;
        QString layout_beat;
        int layout_phase=0, fit_retries=0;
        bool saw_opening_animation = false;
        bool retried_name = false;
        bool edited_seed = false;
        bool saw_hillside = false;
        bool saw_company = false;
        bool customized = false;
        bool saw_founder_audience = false;
        bool saw_signature = false;
        bool opened_keepsake = false;
        pgp::runtime::Checkpoint opening;
    };
    auto state = std::make_shared<State>();
    auto* timer = new QTimer(&window);
    timer->setInterval(60);
    QObject::connect(timer, &QTimer::timeout, &window, [&, timer, state, paths, screenshot, partner] {
        auto* story = campaign.founding();
        const auto id = story->beat();
        const auto fail = [&](const QString& why) {
            std::cerr << "Prologue UI [" << id.toStdString() << "]: " << why.toStdString()
                      << " " << campaign.notice().toStdString() << '\n';
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".failure.png");
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
            if (state->phase == Phase::opening && screen->property("settling").toBool())
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
            if(window.visibility()!=QWindow::Windowed) {window.showNormal();return;}
            if(window.width()!=1440) {window.resize(1440,900);return;}
            auto* input = item("foundingAnswer");
            auto* paper=item("foundingBirthCertificate");
            if (id != "first_name" || !input || !input->isVisible() || !QAccessible::queryAccessibleInterface(input) ||
                !paper || !item("foundingSurname") || !item("foundingSurname")->isVisible()) {
                fail("The accessible name field must be printed on the actual certificate"); return;
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
            if (!error.isEmpty()) { if(++state->fit_retries<8) return; fail(error); return; }
            state->fit_retries=0;
            auto* input = item("foundingAnswer");
            if (!state->retried_name) {
                type(window, *input, QStringLiteral(" "));
                if (!press(window, input, Qt::Key_Return) || id != story->beat() || !item("foundingError")->isVisible()) {
                    fail("Empty name should stay on the same question"); return;
                }
                state->retried_name = true;
                return;
            }
            if(!click("birthSex_male")) {fail("Edit birth sex beside both name fields");return;}
            type(window, *input, QStringLiteral("Mira"));
            type(window, *item("foundingSurname"), QStringLiteral("Vale"));
            if (!capture(".compact") || !press(window, input, Qt::Key_Return)) { fail("Compact name entry"); return; }
            state->phase = Phase::questions;
            break;
        }
        case Phase::questions: {
            const auto error = fitting_error(*screen, viewport);
            if (!error.isEmpty()) { if(++state->fit_retries<8) return; fail(error); return; }
            state->fit_retries=0;
            auto current_beat=screen->property("beat");
            if(current_beat.canConvert<QJSValue>()) current_beat=current_beat.value<QJSValue>().toVariant();
            const auto choices=current_beat.toMap().value("choices").toList();
            if(!choices.empty() && !id.endsWith("-after")) {
                if(state->layout_beat!=id) {state->layout_beat=id;state->layout_phase=0;}
                if(state->layout_phase==0) {
                    campaign.setTextScalePercent(100);window.resize(1440,900);state->layout_phase=1;return;
                }
                if(state->layout_phase==1) {
                    if(!capture(".choices-"+id+"-normal")) {fail("Normal choice layout");return;}
                    campaign.setTextScalePercent(200);window.resize(650,480);state->layout_phase=2;return;
                }
                if(state->layout_phase==2) {
                    auto* choices_scroll=item("foundingChoiceScroll");
                    if (!choices_scroll) { fail("The compact decision view needs a scrollable scene."); return; }
                    const auto scroll_bounds=choices_scroll->mapRectToScene({0,0,choices_scroll->width(),choices_scroll->height()});
                    for(const auto& row:choices) {
                        const auto option=row.toMap();
                        auto* control=find_visible(window.contentItem(),"foundingChoice_"+option.value("id").toString());
                        if(!control || control->height()<22 || control->height()>scroll_bounds.height() ||
                           control->height()>control->implicitHeight()+1) {
                            fail("Every choice must fit its content and be reachable in the same scene");return;
                        }
                        if(!control->isEnabled()) {
                            if(control->property("detail").toString().isEmpty()) {
                                fail("An unavailable choice must explain why it is locked");return;
                            }
                            continue;
                        }
                        control->forceActiveFocus(Qt::TabFocusReason);
                        const auto bounds=control->mapRectToScene({0,0,control->width(),control->height()});
                        if (!scroll_bounds.adjusted(-1,-1,1,1).contains(bounds)) {
                            fail("Keyboard focus must reveal the complete choice: "+option.value("id").toString());return;
                        }
                    }
                    if(!capture(".choices-"+id+"-compact")) {fail("Enlarged choice layout");return;}
                    campaign.setTextScalePercent(100);state->layout_phase=3;return;
                }
            }
            const auto advance = [&] { return click("foundingNextButton"); };
            if (id == "last_name" && !story->form().value("first_name").toString().startsWith("Mira")) {
                fail("Edited name was lost after resizing"); return;
            }
            if (id.startsWith("chapter-") && !id.endsWith("-after")) {
                if (id == "chapter-elem_classroom" && !capture(".childhood")) { fail("Childhood screenshot"); return; }
                for (const auto& row : story->chapters()) {
                    const auto chapter = row.toMap();
                    const auto chapterId = chapter.value("id").toString();
                    if ("chapter-" + chapterId != id) continue;
                    const auto maxSel = chapter.value("maxSelections").toInt();
                    const auto selected = story->form().value("choices").toMap().value(chapterId).toString();
                    if (maxSel > 1 && !selected.isEmpty()) {
                        if (!advance()) fail("Multi-select continue is unavailable");
                        return;
                    }
                    QString choice;
                    for (const auto& optRow : chapter.value("choices").toList()) {
                        const auto opt = optRow.toMap();
                        if (opt.value("available", true).toBool()) { choice = opt.value("id").toString(); break; }
                    }
                    if (choice.isEmpty() || !click("foundingChoice_" + choice)) fail("Childhood choice is unavailable");
                    return;
                }
                fail("Unknown childhood beat");
                return;
            }
            static const QHash<QString, QString> answers{
                {"family","poor"},{"leg","charisma"},{"legal_form","nonprofit"},{"route","mission_director"},
                {"mission","survival"},{"offer-safety","accept"},{"offer-open_science","decline"},
                {"headquarters","US-CA"},{"birth_sex","male"},{"gender","woman"},{"pronouns","she/her"}
            };
            if(id=="certificate") {
                const auto progress=screen->property("autograph").toReal();
                if(progress<1) {state->saw_signature=state->saw_signature || progress>0;return;}
                if((!campaign.reducedMotion() && !state->saw_signature) || !capture(".signed-birth-record") || !advance()) {
                    fail("Reveal the signature and seal before continuing");return;
                }
                return;
            }
            if(id=="gender-after" && !item("foundingAfterword")->property("text").toString().contains("public founding buzz")) {
                fail("Disclose actual support for the transgender founder");return;
            }
            if(id=="pronouns" && partner) {
                if(!click("foundingChoice_other") && !click("foundingMoreChoices"))fail("Open more pronoun choices at age eighteen");
                return;
            }
            if(id=="other_pronouns") {
                if(!click("foundingChoice_fae/faer"))fail("Choose a pronoun set and continue to orientation");
                return;
            }
            if(id=="orientation") {
                if(!click("foundingChoice_bisexual") && !click("foundingMoreChoices"))fail("Choose a separate orientation");
                return;
            }
            if(id=="appearance" && !state->customized) {
                const auto before=story->portrait();
                if(!click("appearanceNext_1") || before==story->portrait() ||
                    item("foundingPortraitPreview")->property("status").toInt()!=1) {fail("Live appearance customization");return;}
                state->customized=true;
                return;
            }
            if(id=="appearance" && state->frames++==0) {window.resize(1440,900);return;}
            if(id=="appearance" && !capture(".appearance")) {fail("Appearance screenshot");return;}
            if (id == "offer-safety" && !capture(".offer")) { fail("Individual offer screenshot"); return; }
            if (id=="gender" && partner && !state->saw_founder_audience) {
                if(campaign.textScalePercent()!=200) {campaign.setTextScalePercent(200);return;}
                campaign.clearNotice();
                const auto* woman=find_visible(window.contentItem(),"foundingChoice_woman");
                const auto* man=find_visible(window.contentItem(),"foundingChoice_man");
                const auto* nonbinary=find_visible(window.contentItem(),"foundingChoice_nonbinary");
                if(!woman || !man || !nonbinary || find_visible(window.contentItem(),"foundingMoreChoices") ||
                    woman->property("detail").toString().contains("salary needed") || !capture(".founder-audience")) {
                    fail("Show all gender identities together without gender salary modifiers");return;
                }
                state->saw_founder_audience=true;
                campaign.setTextScalePercent(100);
                return;
            }
            if (id=="family" && partner) {
                if(!click("foundingChoice_rich") && !click("foundingMoreChoices"))fail("Choose a background with investor connections");
                return;
            }
            if (const auto chosen = answers.find(id); chosen != answers.end()) {
                if (!click("foundingChoice_" + *chosen)) fail("Expected response is unavailable");
            } else if (id == "home") {
                if(item("homeStart")->property("transitioning").toBool())return;
                if(state->home_phase==0) {
                    if(!state->opened_keepsake) {
                        if(!press(window,find_visible(window.contentItem(),"familyBirthRecord"))) {fail("Open the framed certificate from the bedroom");return;}
                        state->opened_keepsake=true;return;
                    }
                    if(find_visible(window.contentItem(),"familyCertificate")) {
                        if(++state->keepsake_frames<4)return;
                        if(!capture(".framed-birth-record")) {fail("Render the framed birth record");return;}
                        QKeyEvent escape{QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier};
                        QCoreApplication::sendEvent(&window,&escape);
                        return;
                    }
                    if(!capture(".home") || !click("homeApp_browser")) {fail("Home desktop before company formation");return;}
                    story->setField("seed","718");campaign.setTextScalePercent(200);++state->home_phase;return;
                }
                if(state->home_phase<=12 && (window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480})) {
                    window.showNormal();window.resize(650,480);return;
                }
                if(state->home_phase<=12 && campaign.textScalePercent()!=200) {fail("Home conversations must be tested at 200% text");return;}
                if(item("homeStart")->property("desktopOpen").toBool()) {
                    if(!capture(".home-desktop") || !click("browserApp")) {fail("Launch the browser from the actual home desktop");return;}
                    return;
                }
                if(partner) {
                    switch(state->home_phase) {
                    case 1:
                        if(!click("homeService_investors")) {fail("Open the investor and cofounder directory");return;}
                        break;
                    case 2:
                        if(find_visible(window.contentItem(),"homeContact_partner_chen") || !click("homeContact_partner_okafor")) {
                            fail("The directory must offer eligible cofounders");return;
                        }
                        break;
                    case 3: {
                        const auto* terms_item=find_visible(window.contentItem(),"homeOfferTerms");
                        const auto terms=terms_item?terms_item->property("text").toString():QString{};
                        if(!terms.contains("24% equity") || !terms.contains("2 board vote") || !terms.contains("Opening mission") ||
                            !capture(".cofounder") || !click("homeDeferOffer")) {fail("Review and defer the named offer");return;}
                        break;
                    }
                    case 4:
                        if(item("homeOfferStatus")->property("text")!="Saved for later" || !click("homeAcceptOffer")) {
                            fail("Accept the deferred agreement");return;
                        }
                        break;
                    case 5:
                        if(story->form().value("partner")!="partner_okafor" || story->form().value("mission")!="utopia" ||
                            item("homeMission")->isEnabled() || !story->signingError().isEmpty() || !click("homeSign")) {
                            fail("The chosen person must set the mission in the paperwork");return;
                        }
                        break;
                    case 6:
                        if(!item("homeEnterCompany")->isVisible()) return;
                        item("homeEnterCompany")->forceActiveFocus(Qt::TabFocusReason);
                        break;
                    case 7:
                        if(!capture(".cofounder-signed") || !click("homeEnterCompany")) {fail("Sign the partnership and enter the company");return;}
                        state->phase=Phase::office;
                        break;
                    }
                    ++state->home_phase;return;
                }
                if(state->home_phase==1) {if(!click("homeService_labs")){fail("Browser careers service");return;}++state->home_phase;return;}
                if(state->home_phase==2) {if(!click("homeApplyCEO")){fail("Submit a leadership application");return;}++state->home_phase;return;}
                if(state->home_phase==3) {
                    auto* reply=item("homeOfferMessage");
                    if(!reply || !reply->isVisible() || !reply->property("text").toString().contains("cannot offer") || item("homeAcceptOffer")->isVisible()) {
                        fail("Ineligible application should have a saved rejection and no acceptance");return;
                    }
                    reply->forceActiveFocus(Qt::TabFocusReason);++state->home_phase;return;
                }
                if(state->home_phase==4) {if(!capture(".application") || !click("homeApp_browser")){fail("Read the rejection");return;}++state->home_phase;return;}
                if(state->home_phase==5) {if(!click("homeService_investors")){fail("Browser investors service");return;}++state->home_phase;return;}
                if(state->home_phase==6) {if(!click("homeContact_commercial")){fail("Pitch a named investor");return;}++state->home_phase;return;}
                if(state->home_phase==7) {if(!click("homeDeferOffer")){fail("Set the investor's offer aside");return;}++state->home_phase;return;}
                if(state->home_phase==8) {
                    auto* status=item("homeOfferStatus");
                    if(!status || !status->isVisible() || status->property("text")!="Saved for later") {fail("Deferred offer status");return;}
                    status->forceActiveFocus(Qt::TabFocusReason);++state->home_phase;return;
                }
                if(state->home_phase==9) {if(!capture(".investor") || !click("homeAcceptOffer")){fail("Accept the investor for review");return;}++state->home_phase;return;}
                if(state->home_phase==10) {
                    if(story->form().value("backers").toStringList()!=QStringList{"commercial"} || !item("homeSign")->isVisible() || !campaign.welcome() ||
                        !click("homeApp_messages")) {fail("Accepted terms must reach unsigned company paperwork");return;}
                    ++state->home_phase;return;
                }
                if(state->home_phase==11) {if(!click("homeDeclineOffer")){fail("Withdraw the selected offer before signing");return;}++state->home_phase;return;}
                if(state->home_phase==12) {
                    if(!story->form().value("backers").toStringList().empty() || item("homeOfferStatus")->property("text")!="Declined" || !click("homeApp_browser")) {
                        fail("Declined equity terms must be removed from paperwork");return;
                    }
                    campaign.setTextScalePercent(100);++state->home_phase;return;
                }
                if(state->home_phase==13) {if(!click("homeService_bank")){fail("Browser banking service");return;}++state->home_phase;return;}
                if(state->home_phase==14) {if(!click("homeReviewStartupLoan")){fail("Startup loan application");return;}++state->home_phase;return;}
                if(state->home_phase==15) {
                    if(!click("homeSign") || story->signingError().isEmpty() || !campaign.welcome()) {fail("Unfunded bank application must refuse");return;}
                    auto* investment=item("homeInvestment");
                    if(!investment) {fail("Loan investment field");return;}
                    type(window,*investment,QStringLiteral("5000"));
                    if(!press(window,investment,Qt::Key_Return) || story->form().value("investment")!="5000") {fail("Loan investment entry");return;}
                    if(!press(window,item("homeBylaws"),Qt::Key_End) || story->form().value("charter")!="charter.shared_research") {fail("Optional bylaws");return;}
                    ++state->home_phase;return;
                }
                if(state->home_phase==16) {if(!capture(".terms") || !click("homeSign")){fail("Reviewed signing");return;}++state->home_phase;return;}
                if(state->home_phase==17) {
                    if(!item("homeEnterCompany")->isVisible())return;
                    item("homeEnterCompany")->forceActiveFocus(Qt::TabFocusReason);
                    ++state->home_phase;return;
                }
                if(state->home_phase==18) {if(!capture(".signed") || !click("homeEnterCompany")){fail("Signed company entry");return;}state->phase=Phase::office;}
            } else if (!advance()) fail("Could not advance narration or answer");
            break;
        }
        case Phase::office: {
            const auto& world = campaign.snapshot()->checkpoint.world;
            const auto& employees=world.simulation.management->people->employees;
            const bool expected_entry=world.founding && (partner ? world.founding->route=="partnership" &&
                world.founding->partner=="partner_okafor" && world.simulation.accounts->debts.empty() && employees.size()==1 &&
                employees.front().terms.profile.name=="Amara Okafor" && world.simulation.capacity->sources.size()==2 : world.founding->route=="bank_loan" &&
                world.simulation.accounts->debts.size()==1 && world.simulation.accounts->debts.front().principal==pgp::core::Cents{5000000} &&
                world.simulation.succession->personal_cash==pgp::core::Cents{1500000} && employees.empty() &&
                world.simulation.capacity->sources.empty() &&
                pgp::core::active_charter(world.simulation) && pgp::core::active_charter(world.simulation)->id=="charter.shared_research");
            if (campaign.welcome() || story->hasDraft() || !world.founding || !world.identity || world.founding->choices.size() < 12 ||
                world.founding->legal_form != "corporation" || !expected_entry || !world.founding->backers.empty() ||
                world.identity->founder_name != "Mira Vale" || world.root_seed != 718 ||
                world.founding->gender!="woman" || world.founding->birth_sex!="male" || world.founding->orientation!="bisexual" ||
                !pgp::content::transgender_founder(*world.founding) || world.identity->founder_pronouns!=(partner?"fae/faer":"she/her") ||
                !state->customized || world.founding->appearance.parts[1]!=3 ||
                !world.difficulty.empty() || !world.simulation.lab->research.empty() ||
                pgp::core::iso_date(world.simulation.date) != "2015-01-01" ||
                pgp::core::iso_date(world.simulation.end_date) != "2097-01-01" || !campaign.paused()) {
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
            campaign.setTextScalePercent(200);
            state->phase = Phase::company_messages;
            break;
        }
        case Phase::company_messages:
            if(window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480}) {
                window.showNormal();window.resize(650,480);return;
            }
            if(!click("messagesTab")) {fail("Open the company Messages app");return;}
            state->phase=Phase::select_contact;
            break;
        case Phase::select_contact: {
            const auto& contacts=campaign.snapshot()->checkpoint.world.simulation.management->people->contacts;
            const auto contact=partner?"contactPerson_opening/partner_okafor":"contactPerson_opening/commercial";
            if(contacts.size()!=(partner?1:2) || !press(window,find_visible(window.contentItem(),contact))) {
                fail("Opening conversations must carry into the company contact list");return;
            }
            state->phase=Phase::question_contact;
            break;
        }
        case Phase::question_contact:
            if(partner ? !press(window,find_visible(window.contentItem(),"contactBoardRole")) :
                !press(window,find_visible(window.contentItem(),"contactOpeningTerms")) ||
                find_visible(window.contentItem(),"contactCheckIn") || find_visible(window.contentItem(),"contactBoardRole")) {
                fail("The declined investor must expose a relevant follow-up");return;
            }
            state->phase=Phase::verify_contact;
            state->frames=0;
            break;
        case Phase::verify_contact: {
            if(++state->frames<3)return;
            const auto& checkpoint=campaign.snapshot()->checkpoint;
            const auto& contacts=checkpoint.world.simulation.management->people->contacts;
            const auto& history=contacts.back().messages;
            auto* reply=find_visible(window.contentItem(),"contactMessageCard");
            auto* viewport=find_visible(window.contentItem(),"managementScroll");
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(history.size()!=1 || history.back().reply!=(partner?"has_seat":"declined") || !reply || !viewport || !reply->hasActiveFocus() ||
                reply->mapToItem(viewport,{0,reply->height()}).y()>viewport->height()+1 ||
                current.resume(*campaign.snapshot()->content)!=std::optional{checkpoint} || !capture(".continued-inbox")) {
                fail("The visible company reply must match the signed or declined terms and reload exactly");return;
            }
            state->phase=Phase::done;
            break;
        }
        case Phase::done:
            if (!campaign.snapshot()->checkpoint.world.founding || campaign.welcome()) {
                fail("Exact save continuation"); return;
            }
            if(partner) std::cout << "Cofounder UI passed: eligible directory, defer and accept, signed mission, company board reply and exact save continuation\n";
            else std::cout << "Prologue UI passed: hospital opening, resized 650×480 canvas, editable name, branching art and palette, "
                         "sampled formative chains, saved application and investor conversations at 200% text, appearance, paused office, continued company inbox and fresh-save/day continuation\n";
            timer->stop();
            QCoreApplication::exit(0);
            break;
        }
    });
    timer->start();
}
