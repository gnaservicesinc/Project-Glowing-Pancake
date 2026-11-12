#pragma once

#include "walkthrough.hpp"
#include "world_controller.hpp"
#include <QImage>

namespace pgp::studio {
template<class Browser>
QJsonObject run_catalog_walkthrough(Browser& browser,QQuickWindow& window,const QString& screenshot) {
    using namespace walkthrough;
    namespace fs=std::filesystem;
    const fs::path root{browser.checkout().toStdU16String()};
    constexpr auto source="data/runtime/first-playable.json";
    constexpr auto draft="data/drafts/e2-02-walkthrough.json";
    require(!fs::exists(root/draft),QStringLiteral("Choose a fresh copy for the catalog walkthrough."));
    const auto original=persistence::read_bounded(root/source,content::maximum_document_bytes);
    const auto versions=persistence::read_bounded(root/"project-versions.json",16384);
    const auto record=persistence::read_bounded(root/"assets/manifest.json",content::maximum_document_bytes);
    const auto reveal=[&](QQuickItem* control) {
        for(auto* item=control->parentItem();item;item=item->parentItem()) if(item->property("contentY").isValid()) {
            auto* content=qobject_cast<QQuickItem*>(item->property("contentItem").value<QObject*>());
            if(!content) continue;
            const auto limit=std::max(0.0,item->property("contentHeight").toDouble()-item->height());
            item->setProperty("contentY",std::clamp(control->mapToItem(content,QPointF{}).y()-32.0,0.0,limit));
        }
        settle();
    };
    const auto item=[&](const QString& name) {
        auto* found=find(window.contentItem(),name);
        require(found,QStringLiteral("Missing catalog control ")+name);
        reveal(found);
        return found;
    };
    const auto click=[&](const QString& name) {
        auto* button=item(name);
        require(button->isVisible() && button->isEnabled(),QStringLiteral("Catalog action is unavailable ")+name);
        require(QMetaObject::invokeMethod(button,"clicked"),QStringLiteral("Missing catalog handler ")+name);
        settle();
    };
    const auto enter=[&](const QString& name,const QString& value,const char* signal=nullptr) {
        auto* input=item(name);
        input->setProperty("text",value);
        if(signal) require(QMetaObject::invokeMethod(input,signal),QStringLiteral("Missing input handler ")+name);
        settle();
    };
    const auto choose=[&](const QString& name,const QString& value,const char* role="value") {
        auto* picker=item(name);
        const auto entries=picker->property("model").toList();
        int index=-1;
        for(qsizetype i=0;i<entries.size();++i)
            if(entries[i].toMap().value(QString::fromLatin1(role)).toString()==value) index=static_cast<int>(i);
        require(index>=0,QStringLiteral("Missing typed option ")+value);
        picker->setProperty("currentIndex",index);
        require(QMetaObject::invokeMethod(picker,"activated",Q_ARG(int,index)),QStringLiteral("Picker could not select ")+value);
        settle();
    };
    auto* author=qobject_cast<WorldController*>(browser.worldController());
    browser.beginDraft(QString::fromLatin1(source),QStringLiteral("scenario.first_playable"));
    settle();
    browser.selectWorkspace(true, true);
    settle();
    require(author && author->catalogMode() && !author->catalogEntries().isEmpty(),QStringLiteral("The catalog workspace did not open."));
    author->selectCatalogEntry(QStringLiteral("/definitions/research.efficient_serving"));
    settle();
    enter(QStringLiteral("worldInput/cash_cents"),QStringLiteral("123456"),"editingFinished");
    click(QStringLiteral("applyWorldSection"));
    require(author->message().isEmpty(),author->message());
    enter(QStringLiteral("catalogNewId"),QStringLiteral("research.walkthrough_serving"));
    click(QStringLiteral("catalogReviewRename"));
    require(author->renameChanges().size()>1,QStringLiteral("Research rename did not disclose the affected references."));
    if(!screenshot.isEmpty()) {
        auto captured=window.grabWindow();
        require(!captured.isNull() && captured.save(screenshot,"PNG"),QStringLiteral("Catalog reference review screenshot failed."));
    }
    click(QStringLiteral("catalogApplyRename"));
    require(author->selectedIdentity()==QStringLiteral("research.walkthrough_serving"),QStringLiteral("Reviewed rename did not select the new identity."));
    click(QStringLiteral("catalogNewMessage"));
    enter(QStringLiteral("catalogMessageKey"),QStringLiteral("walkthrough.local_text"),"editingFinished");
    enter(QStringLiteral("catalogMessageText"),QStringLiteral("English fixture."));
    click(QStringLiteral("catalogSaveMessage"));
    click(QStringLiteral("catalogTranslations"));
    enter(QStringLiteral("catalogLocale"),QStringLiteral("fr"),"editingFinished");
    enter(QStringLiteral("catalogMessageText"),QStringLiteral("Texte de vérification."));
    click(QStringLiteral("catalogSaveMessage"));
    require(author->messageValue(QStringLiteral("walkthrough.local_text"),QStringLiteral("fr-CA"),true)==QStringLiteral("Texte de vérification."),
        QStringLiteral("Localized text did not resolve through the language fallback."));
    require(browser.openAssets(), QStringLiteral("The asset register did not open."));
    auto* assets = window.findChild<QObject*>(QStringLiteral("assetRegisterDialog"));
    require(assets && QMetaObject::invokeMethod(assets, "open"), QStringLiteral("The asset dialog did not open."));
    settle();
    choose(QStringLiteral("registeredAssets"),QStringLiteral("map.us_states_2024"),"id");
    click(QStringLiteral("verifyAssetRecord"));
    auto* credit=item(QStringLiteral("assetField/credit_text"));
    enter(QStringLiteral("assetField/credit_text"),credit->property("text").toString()+QStringLiteral(" Studio verification copy."),"textEdited");
    click(QStringLiteral("saveAssetRecord"));
    require(persistence::read_bounded(root/"assets/manifest.json",content::maximum_document_bytes).find("Studio verification copy.")!=std::string::npos,
        QStringLiteral("Credit editing did not save the reviewed record."));
    click(QStringLiteral("pinAssetRecord"));
    require(browser.assetPinned(QStringLiteral("map.us_states_2024")),browser.draftMessage());
    auto* asset=window.findChild<QObject*>(QStringLiteral("assetRegisterDialog"));
    require(asset && QMetaObject::invokeMethod(asset,"close"),QStringLiteral("Asset editor did not close."));
    settle();
    browser.selectWorkspace(false);
    require(browser.createFlow(QStringLiteral("walkthrough.group_caller"),QStringLiteral("company")),browser.draftMessage());
    for(unsigned i=1;i<=4;++i) {
        browser.selectOutline(QStringLiteral("variables"));
        require(browser.changeForm(browser.formTicket(),QStringLiteral("append"),{}),browser.draftMessage());
    }
    settle();
    click(QStringLiteral("openGroupTools"));
    enter(QStringLiteral("newReusableGroupId"),QStringLiteral("group.walkthrough"));
    click(QStringLiteral("createReusableGroup"));
    const auto group = browser.workingFlow();
    require(group != QStringLiteral("walkthrough.group_caller"),QStringLiteral("The new group did not open in its typed outline."));
    browser.selectOutline(QStringLiteral("group"));
    require(browser.changeForm(browser.formTicket(),QStringLiteral("add_input"),QStringLiteral("story.flag")),browser.draftMessage());
    require(browser.changeForm(browser.formTicket(),QStringLiteral("add_output"),QStringLiteral("story.flag.2")),browser.draftMessage());
    browser.selectWorkingFlow(QStringLiteral("walkthrough.group_caller"));
    settle();
    click(QStringLiteral("openGroupTools"));
    choose(QStringLiteral("selectReusableGroup"),group);
    choose(QStringLiteral("selectGroupContinuation"),QStringLiteral("end"));
    choose(QStringLiteral("groupBinding/story.flag"),QStringLiteral("story.flag.3"));
    choose(QStringLiteral("groupBinding/story.flag.2"),QStringLiteral("story.flag.4"));
    click(QStringLiteral("insertReusableGroupCall"));
    require(browser.draftMessage().isEmpty(),browser.draftMessage());
    browser.selectWorkspace(true, true);
    settle();
    require(browser.saveDraft(QStringLiteral("e2-02-walkthrough.json")), browser.draftMessage());
    const auto saved=persistence::read_bounded(root/draft,content::maximum_document_bytes);
    DraftDocument reopened{root,draft};
    require(reopened.collection().find(group.toStdString()) && reopened.collection().find("research.walkthrough_serving") &&
        reopened.collection().asset_records.contains("map.us_states_2024") &&
        reopened.collection().translations.contains("fr"),QStringLiteral("The catalog/group/asset draft failed to reopen."));
    require(persistence::read_bounded(root/source,content::maximum_document_bytes)==original &&
        persistence::read_bounded(root/"project-versions.json",16384)==versions &&
        persistence::read_bounded(root/"assets/manifest.json.bak",content::maximum_document_bytes)==record,
        QStringLiteral("The walkthrough changed finalized files while drafting."));
    return {{"draft",QString::fromLatin1(draft)},{"saved_bytes",static_cast<qint64>(saved.size())},
        {"catalog_edit_and_refactor",true},{"locale_fallback",true},{"asset_credit_saved_and_pinned",true},{"typed_group_call",true}};
}
} // namespace pgp::studio
