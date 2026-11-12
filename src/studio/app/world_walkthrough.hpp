#pragma once
#include "walkthrough.hpp"
#include "world_controller.hpp"
#include <QJsonDocument>
#include <QJsonArray>

namespace pgp::studio {
template<class Browser>
QJsonObject run_world_walkthrough(Browser& browser,QQuickWindow& window,const QString& screenshot) {
    using namespace walkthrough;namespace fs=std::filesystem;
    const fs::path root{browser.checkout().toStdU16String()};
    constexpr auto source="data/runtime/first-playable.json";
    constexpr auto draft="data/drafts/e2-01-walkthrough.json";
    require(!fs::exists(root/draft),QStringLiteral("Choose a fresh copy; the world walkthrough draft already exists."));
    const auto original=persistence::read_bounded(root/source,content::maximum_document_bytes);
    const auto versions=persistence::read_bounded(root/"project-versions.json",16384);
    browser.beginDraft(QString::fromLatin1(source),QStringLiteral("scenario.first_playable"));settle();
    auto* author=qobject_cast<WorldController*>(browser.worldController());
    require(author && browser.worldMode() && author->states().size()==51,QStringLiteral("Scenario library entry did not open the world workspace."));
    const auto reveal=[&](QQuickItem* control) {
        for(auto* p=control->parentItem();p;p=p->parentItem()) if(p->property("contentY").isValid()) {
            auto* content=qobject_cast<QQuickItem*>(p->property("contentItem").value<QObject*>());
            if(!content) continue;
            const auto limit=std::max(0.0,p->property("contentHeight").toDouble()-p->height());
            p->setProperty("contentY",std::clamp(control->mapToItem(content,QPointF{}).y()-40.0,0.0,limit));
        }
        settle();
    };
    const auto item=[&](const QString& name) {
        auto* found=find(window.contentItem(),name);require(found,QStringLiteral("Missing world control: ")+name);reveal(found);
        const auto center=found->mapToScene(QPointF{found->width()/2,found->height()/2});
        require(center.x()>=0 && center.x()<window.width() && center.y()>=0 && center.y()<window.height(),
            QStringLiteral("World control is outside the window: ")+name);return found;
    };
    const auto click=[&](const QString& name) {
        auto* button=item(name);require(button->isVisible() && button->isEnabled(),QStringLiteral("World action is unavailable: ")+name);
        require(QMetaObject::invokeMethod(button,"clicked"),QStringLiteral("Missing world action handler: ")+name);settle();
    };
    int submissions=0;
    const auto input=[&](const QString& key,const QString& value) {
        auto* field=item(QStringLiteral("worldInput/")+key);field->setProperty("text",value);
        require(QMetaObject::invokeMethod(field,"editingFinished"),QStringLiteral("Missing world field handler: ")+key);
        ++submissions;settle();
    };
    const auto select=[&](const QString& path) {author->selectSection(path);settle();require(author->path()==path,QStringLiteral("Section selection failed: ")+path);};
    const auto apply=[&] {click(QStringLiteral("applyWorldSection"));require(author->message().isEmpty(),author->message());};
    const QString scenario=QStringLiteral("/definitions/scenario.first_playable");
    select(scenario);input(QStringLiteral("cash_cents"),QStringLiteral("120001234"));apply();
    const auto stale=author->ticket();
    select(scenario+QStringLiteral("/management"));input(QStringLiteral("default_policy_budget_cents"),QStringLiteral("5001234"));apply();
    require(!author->apply(stale,{{"cash_cents","1"}}),QStringLiteral("A stale rendered form was accepted."));
    select(scenario);input(QStringLiteral("cash_cents"),QStringLiteral("1.5"));click(QStringLiteral("applyWorldSection"));
    require(!author->message().isEmpty(),QStringLiteral("A decimal passed the money form."));
    input(QStringLiteral("cash_cents"),QStringLiteral("120001234"));
    // The rejected value never reached the draft; resetting the form is explicit.
    author->selectSection(scenario+QStringLiteral("/ownership_basis_points"));settle();
    QString founder,other;QStringList owner_keys;
    for(const auto& row:author->fields()) {
        const auto f=row.toMap();const auto key=f.value(QStringLiteral("key")).toString();
        if(!key.startsWith(QLatin1Char('@'))) owner_keys.push_back(key);
    }
    select(scenario+QStringLiteral("/management/scoring"));
    QString old_share;
    for(const auto& row:author->fields()) {
        const auto f=row.toMap();
        if(f.value(QStringLiteral("key"))==QStringLiteral("founder")) founder=f.value(QStringLiteral("value")).toString();
        if(f.value(QStringLiteral("key"))==QStringLiteral("opening_founder_share_bp")) old_share=f.value(QStringLiteral("value")).toString();
    }
    for(const auto& id:owner_keys) if(id!=founder) {other=id;break;}
    require(!founder.isEmpty() && !other.isEmpty(),QStringLiteral("The actual ownership fixture is missing."));
    select(scenario+QStringLiteral("/ownership_basis_points"));
    QString other_share;
    for(const auto& row:author->fields()) if(row.toMap().value(QStringLiteral("key"))==other) other_share=row.toMap().value(QStringLiteral("value")).toString();
    input(founder,QString::number(old_share.toInt()-100));input(other,QString::number(other_share.toInt()+100));
    click(QStringLiteral("stageWorldSection"));require(author->hasStaged(),QStringLiteral("Ownership section was not staged."));
    select(scenario+QStringLiteral("/management/scoring"));
    input(QStringLiteral("opening_founder_share_bp"),QString::number(old_share.toInt()-100));click(QStringLiteral("stageWorldSection"));
    click(QStringLiteral("applyWorldBatch"));require(!author->hasStaged() && author->message().isEmpty(),author->message());
    click(QStringLiteral("worldMapTab"));
    auto* list=item(QStringLiteral("worldSectionList"));int texas=-1;
    for(qsizetype i=0;i<author->states().size();++i) if(author->states()[i].toMap().value(QStringLiteral("id"))==QStringLiteral("US-TX")) texas=static_cast<int>(i);
    require(texas>=0,QStringLiteral("Texas is missing from the keyboard list."));
    list->forceActiveFocus();QKeyEvent home{QEvent::KeyPress,Qt::Key_Home,Qt::NoModifier};QCoreApplication::sendEvent(list,&home);
    for(int i=0;i<texas;++i) {QKeyEvent down{QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier};QCoreApplication::sendEvent(list,&down);}
    QKeyEvent enter{QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier};QCoreApplication::sendEvent(list,&enter);settle();
    require(author->selectedState()==QStringLiteral("US-TX"),QStringLiteral("Keyboard state selection failed."));
    input(QStringLiteral("grid_lead_days"),QStringLiteral("76"));apply();
    auto* map=item(QStringLiteral("worldDomesticMap"));
    const auto geometry=author->mapGeometry();QVariantMap washington;
    for(const auto& row:geometry.value(QStringLiteral("features")).toList()) if(row.toMap().value(QStringLiteral("id"))==QStringLiteral("US-WA")) washington=row.toMap();
    const auto point=washington.value(QStringLiteral("label")).toList();
    require(point.size()==2,QStringLiteral("The embedded map lacks Washington's label point."));
    const auto ratio=map->property("ratio").toDouble();
    const auto x=point[0].toDouble()*ratio+map->property("insetX").toDouble(),y=point[1].toDouble()*ratio+map->property("insetY").toDouble();
    require(QMetaObject::invokeMethod(map,"pick",Q_ARG(QVariant,QVariant{x}),Q_ARG(QVariant,QVariant{y})),QStringLiteral("Map pointer hit test is unavailable."));settle();
    require(author->selectedState()==QStringLiteral("US-WA"),QStringLiteral("Pointer map selection did not choose Washington."));
    input(QStringLiteral("grid_lead_days"),QStringLiteral("26"));apply();
    if(!screenshot.isEmpty()) {
        (void)item(QStringLiteral("worldInput/grid_lead_days"));
        settle();const auto image=window.grabWindow();
        require(!image.isNull() && image.save(screenshot,"PNG"),QStringLiteral("World map/fields screenshot failed."));
    }
    select(scenario+QStringLiteral("/management/financing"));
    click(QStringLiteral("appendWorldEntry"));require(author->adding(),QStringLiteral("The financing template did not open."));
    input(QStringLiteral("id"),QStringLiteral("finance.world_author"));input(QStringLiteral("name"),QStringLiteral("Author financing"));apply();
    click(QStringLiteral("previewWorldOpening"));
    require(author->report().contains(QStringLiteral("120001234")) && author->report().contains(QStringLiteral("Exact save reload")) &&
        author->report().contains(QStringLiteral("Enabled trigger targets")),QStringLiteral("Actual opening/target report is incomplete."));
    require(browser.saveDraft(QStringLiteral("e2-01-walkthrough.json")), browser.draftMessage());
    require(!browser.draftDirty(),QStringLiteral("Saved world draft remained dirty."));
    const auto saved=persistence::read_bounded(root/draft,content::maximum_document_bytes);
    browser.closeDraft();browser.beginDraft(QString::fromLatin1(draft),QStringLiteral("scenario.first_playable"));settle();
    require(browser.worldMode() && !browser.draftDirty() && author->preview(QStringLiteral("US-WA")),QStringLiteral("Saved scenario did not reopen through its ordinary workspace."));
    require(persistence::read_bounded(root/source,content::maximum_document_bytes)==original &&
        persistence::read_bounded(root/"project-versions.json",16384)==versions,QStringLiteral("World draft editing changed runtime content or versions."));
    window.resize(900,720);settle();
    require(item(QStringLiteral("worldScenario"))->isVisible(),QStringLiteral("Narrow workspace lost the scenario picker."));
    click(QStringLiteral("worldSectionsPane"));
    require(item(QStringLiteral("worldSectionList"))->isVisible(),QStringLiteral("Narrow workspace lost the section/map list."));
    click(QStringLiteral("worldFieldsPane"));
    require(item(QStringLiteral("applyWorldSection"))->isVisible(),QStringLiteral("Narrow workspace lost the section action."));
    return {{"draft",QString::fromLatin1(draft)},{"form_submissions",submissions},{"states",author->states().size()},
        {"keyboard_and_pointer_map",true},{"coupled_ownership_batch",true},{"actual_opening_and_exact_reload",true},
        {"stale_and_decimal_refused",true},{"runtime_and_versions_unchanged",true},{"narrow_width",900},{"saved_bytes",static_cast<qint64>(saved.size())}};
}
} // namespace pgp::studio
