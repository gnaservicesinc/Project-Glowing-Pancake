#include "probe_controller.hpp"
#include "canvas_controller.hpp"
#include "snapshot_controller.hpp"
#include "revision_controller.hpp"
#if PGP_TEST_HOOKS
#include "revision_walkthrough.hpp"
#endif
#include "world_controller.hpp"
#if PGP_TEST_HOOKS
#include "world_walkthrough.hpp"
#endif
#include "asset_controller.hpp"
#if PGP_TEST_HOOKS
#include "catalog_walkthrough.hpp"
#endif
#if PGP_TEST_HOOKS
#include "walkthrough.hpp"
#endif
#if PGP_TEST_HOOKS
#include "canvas_walkthrough.hpp"
#endif
#if PGP_TEST_HOOKS
#include "snapshot_walkthrough.hpp"
#endif

#include <pgp/core/build_identity.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/resources/resource_guards.hpp>
#include <pgp/studio/draft.hpp>
#include <pgp/studio/flow_forms.hpp>
#include <pgp/studio/library.hpp>
#include <pgp/studio/reusable_flow.hpp>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QFont>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsConfiguration>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QUuid>
#include <QSGRendererInterface>
#include <QStyleHints>
#include <QVariant>

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

namespace {
using namespace pgp;
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
#if PGP_TEST_HOOKS
QQuickItem* view_item(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto* child : root->childItems())
        if (auto* found = view_item(child, name)) return found;
    return nullptr;
}
#endif
QStringList strings(const std::vector<std::string>& values) {
    QStringList result;
    for (const auto& value : values) result.push_back(text(value));
    return result;
}
QString origin(studio::Origin value) {
    switch (value) {
    case studio::Origin::reviewed: return QStringLiteral("Reviewed");
    case studio::Origin::draft: return QStringLiteral("Draft");
    case studio::Origin::engineering_fixture: return QStringLiteral("Engineering fixture");
    }
    throw std::invalid_argument("Unknown library origin");
}
QString status(studio::Validation value) {
    switch (value) {
    case studio::Validation::validated: return QStringLiteral("Validated");
    case studio::Validation::invalid: return QStringLiteral("Invalid");
    case studio::Validation::unsupported: return QStringLiteral("Unsupported schema");
    case studio::Validation::blocked: return QStringLiteral("Blocked path or input");
    }
    throw std::invalid_argument("Unknown library validation state");
}
QString control(studio::FormControl kind) {
    switch (kind) {
    case studio::FormControl::note: return QStringLiteral("note");
    case studio::FormControl::text: return QStringLiteral("text");
    case studio::FormControl::paragraph: return QStringLiteral("paragraph");
    case studio::FormControl::integer: return QStringLiteral("integer");
    case studio::FormControl::choice: return QStringLiteral("choice");
    case studio::FormControl::button: return QStringLiteral("button");
    }
    throw std::invalid_argument("Unknown form control.");
}
class Browser final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* probeController READ probeController CONSTANT)
    Q_PROPERTY(QObject* canvasController READ canvasController CONSTANT)
    Q_PROPERTY(QObject* snapshotController READ snapshotController CONSTANT)
    Q_PROPERTY(QObject* revisionController READ revisionController CONSTANT)
    Q_PROPERTY(QObject* worldController READ worldController CONSTANT)
    Q_PROPERTY(QObject* assetController READ assetController CONSTANT)
    Q_PROPERTY(bool worldMode READ worldMode NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap chapter READ chapter NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap eventStory READ eventStory NOTIFY draftChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QString checkout READ checkout NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY draftChanged)
    Q_PROPERTY(QString workingFlow READ workingFlow NOTIFY draftChanged)
    Q_PROPERTY(QString workingTitle READ workingTitle NOTIFY draftChanged)
    Q_PROPERTY(QString workingVersion READ workingVersion NOTIFY draftChanged)
    Q_PROPERTY(QStringList workingFlows READ workingFlows NOTIFY formChanged)
    Q_PROPERTY(QVariantList flowScopes READ flowScopes NOTIFY formChanged)
    Q_PROPERTY(QString workingFile READ workingFile NOTIFY draftChanged)
    Q_PROPERTY(QStringList workingTags READ workingTags NOTIFY draftChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY draftChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY draftChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(bool existingDraft READ existingDraft NOTIFY draftChanged)
    Q_PROPERTY(QString draftMessage READ draftMessage NOTIFY draftChanged)
    Q_PROPERTY(QVariantList flowOutline READ flowOutline NOTIFY formChanged)
    Q_PROPERTY(QVariantList formFields READ formFields NOTIFY formChanged)
    Q_PROPERTY(qsizetype outlineIndex READ outlineIndex NOTIFY formChanged)
    Q_PROPERTY(QString formTicket READ formTicket NOTIFY formChanged)
    Q_PROPERTY(QVariantList groupOptions READ groupOptions NOTIFY formChanged)
    Q_PROPERTY(QVariantList groupContinuations READ groupContinuations NOTIFY formChanged)
public:
    Browser() {
        connect(&world_, &studio::WorldController::documentEdited, this, [this] {
            draft_message_.clear(); refreshForm(); emit draftChanged();
        });
        connect(&world_, &studio::WorldController::flowRequested, this, [this](const QString& id) {
            selectWorkingFlow(id); selectWorkspace(false);
        });
        connect(&world_, &studio::WorldController::identityRenamed, this, [this](const QString& family,const QString& before,const QString& after) {
            if (family == QStringLiteral("flow") && workingFlow() == before) flow_=after.toStdString();
        });
        connect(&world_, &studio::WorldController::changed, this, [this] { refreshSnapshotContext(); emit draftChanged(); });
        connect(&world_, &studio::WorldController::pendingChanged, this, [this] { refreshSnapshotContext(); emit draftChanged(); });
        connect(&canvas_, &studio::CanvasController::documentEdited, this, [this] {
            draft_message_.clear(); refreshForm(); emit draftChanged();
        });
        connect(&canvas_, &studio::CanvasController::selectionRequested, this, [this](const QString& key) {
            outline_key_ = key.toStdString(); refreshForm(); canvas_.remember_selection();
        });
        connect(&canvas_, &studio::CanvasController::layoutChanged, this, [this] { refreshSnapshotContext(); });
        connect(&snapshot_, &studio::SnapshotController::finalized, this, [this](const QString& target) { reopenFinalized(target); });
        connect(&snapshot_, &studio::SnapshotController::recovered, this, [this] {
            if (!draftDirty() && !canvas_.hasLayoutEdits()) reopenFinalized(QStringLiteral("data/runtime/first-playable.json"));
            else { const auto current = checkout_; (void)openCheckout(current); }
        });
    }
    [[nodiscard]] QObject* probeController() noexcept { return &probe_; }
    [[nodiscard]] QObject* canvasController() noexcept { return &canvas_; }
    [[nodiscard]] QObject* snapshotController() noexcept { return &snapshot_; }
    [[nodiscard]] QObject* revisionController() noexcept { return &revisions_; }
    [[nodiscard]] QObject* worldController() noexcept { return &world_; }
    [[nodiscard]] QObject* assetController() noexcept { return &assets_; }
    [[nodiscard]] bool worldMode() const noexcept { return world_mode_; }
    [[nodiscard]] QVariantMap chapter() const {
        if (!draft_ || chapter_id_.empty()) return {};
        const auto* definition = draft_->collection().find(chapter_owner_);
        if (!definition) return {};
        const auto& catalog = std::get<content::ScenarioDefinition>(definition->data).founding.value();
        const auto scene = std::ranges::find(catalog.scenes, chapter_id_, &content::FormativeScene::id);
        if (scene == catalog.scenes.end()) return {};
        QVariantList choices;
        for (const auto& choice : scene->choices) {
            const auto skill = catalog.skill_rewards.find(scene->id + "/" + choice.id);
            choices.push_back(QVariantMap{{"name", text(choice.name)}, {"detail", text(choice.detail)},
                {"skill", skill == catalog.skill_rewards.end() ? QString{} : strings(skill->second).join(", ")}});
        }
        return {{"title", text(scene->title)}, {"date", text(scene->date)}, {"body", text(scene->text)},
            {"choices", choices}, {"pool", text(studio::readable_name(scene->pool))}};
    }
    [[nodiscard]] QVariantMap eventStory() const {
        const auto* definition = draft_ ? draft_->collection().find(flow_) : nullptr;
        if (!definition || !chapter_id_.empty()) return {};
        const auto& collection = draft_->collection();
        const auto& event = std::get<content::FlowDefinition>(definition->data);
        const auto key = studio::event_presentation(event);
        const auto node = event.nodes.find(key);
        const auto copy = [&](const auto& message) {
            const auto* value = collection.message(message);
            return value ? text(*value) : QString{};
        };
        QVariantList choices;
        if (node != event.nodes.end()) for (const auto& choice : node->second.choices)
            choices.push_back(QVariantMap{{"text", copy(choice.label_key)},
                {"next", text(studio::readable_name(choice.next))}});
        QString timing;
        if (event.group) timing = QStringLiteral("Another event starts these steps.");
        else if (!event.on_committed.empty())
            timing = QStringLiteral("Starts after %1.").arg(text(studio::readable_name(event.on_committed)).toLower());
        else switch (event.lifecycle.mode) {
            case content::LifecycleMode::once: timing = QStringLiteral("Happens once, when its conditions are met."); break;
            case content::LifecycleMode::recurring: timing = QStringLiteral("Can happen again after %1 game days.").arg(event.lifecycle.cooldown_days); break;
            case content::LifecycleMode::interval: timing = QStringLiteral("Checks every %1 %2, up to %3 times.")
                .arg(event.lifecycle.interval->value).arg(event.lifecycle.interval->unit == content::IntervalUnit::days
                    ? QStringLiteral("game day(s)") : QStringLiteral("calendar month(s)")).arg(event.lifecycle.max_occurrences); break;
            case content::LifecycleMode::scheduled: timing = QStringLiteral("Checks on %1.").arg(text(core::iso_date(*event.lifecycle.due_date))); break;
            case content::LifecycleMode::continuous: timing = QStringLiteral("Stays active until its end conditions are met."); break;
        }
        QStringList scenarios;
        for (const auto& [id, item] : collection.definitions) {
            const auto* scenario = std::get_if<content::ScenarioDefinition>(&item.data);
            if (scenario && std::ranges::find(scenario->flows, flow_) != scenario->flows.end())
                scenarios << text(studio::event_name(collection, item));
        }
        return {{"hasMessage", node != event.nodes.end()},
            {"dialogue", node != event.nodes.end() && node->second.kind == content::NodeKind::dialogue},
            {"title", node == event.nodes.end() ? QString{} : copy(node->second.title_key)},
            {"body", node == event.nodes.end() ? QString{} : copy(node->second.text_key)},
            {"choices", choices}, {"timing", timing}, {"scenarios", scenarios}};
    }
    Q_INVOKABLE bool changeEventStory(const QString& name, const QString& title, const QString& body,
                                      const QVariantList& labels) {
        if (!draft_ || flow_.empty()) return false;
        return edit([&] {
            auto candidate = draft_->collection();
            auto& event = std::get<content::FlowDefinition>(candidate.definitions.at(flow_).data);
            const auto new_name = name.trimmed();
            if (new_name.isEmpty()) throw std::invalid_argument("Give this event a name.");
            const auto editor_key = flow_ + ".editor_title";
            const auto node = event.nodes.find(studio::event_presentation(event));
            if (node != event.nodes.end()) {
                auto& message = node->second;
                if (body.trimmed().isEmpty()) throw std::invalid_argument("Write the message players will read.");
                if (!message.title_key.empty()) {
                    if (title.trimmed().isEmpty()) throw std::invalid_argument("Give the dialogue a title.");
                    if (message.title_key == editor_key && title != new_name)
                        message.title_key = flow_ + ".dialogue_title";
                    candidate.messages[message.title_key] = title.trimmed().toStdString();
                }
                if (message.choices.size() != static_cast<std::size_t>(labels.size()))
                    throw std::invalid_argument("The choices have changed. Reopen the event and try again.");
                for (qsizetype index = 0; index < labels.size(); ++index) {
                    const auto label = labels[index].toString().trimmed();
                    if (label.isEmpty()) throw std::invalid_argument("Give each player choice a button label.");
                    candidate.messages[message.choices[static_cast<std::size_t>(index)].label_key] = label.toStdString();
                }
                candidate.messages[message.text_key] = body.trimmed().toStdString();
            }
            if (studio::event_name(candidate, candidate.definitions.at(flow_)) != new_name.toStdString())
                candidate.messages[editor_key] = new_name.toStdString();
            (void)draft_->replace(candidate);
        });
    }
    Q_INVOKABLE bool appendStoryChoice() {
        if (!draft_ || flow_.empty()) return false;
        const auto& event = std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data);
        const auto node = studio::event_presentation(event);
        if (node.empty()) return false;
        return edit([&] {
            (void)studio::FlowForm::open(*draft_, flow_, "node/" + std::string(node)).apply(*draft_, "append", {});
        });
    }
    Q_INVOKABLE void selectEventStep(const QString& step, int choice = -1) {
        if (!draft_ || flow_.empty()) return;
        if (step != QStringLiteral("story")) {
            selectOutline(step);
            return;
        }
        const auto& event = std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data);
        const auto key = studio::event_presentation(event);
        if (key.empty()) { selectOutline(QStringLiteral("flow")); return; }
        auto row = QStringLiteral("node/") + text(key);
        if (choice >= 0) row += QStringLiteral("/choice/") + QString::number(choice);
        selectOutline(row);
    }
    Q_INVOKABLE bool openChapter(const QString& file, const QString& owner, const QString& id) {
        if (!beginDraft(file, owner, true)) return false;
        chapter_owner_ = owner.toStdString();
        chapter_id_ = id.toStdString();
        world_mode_ = false;
        emit draftChanged();
        return !chapter().isEmpty();
    }
    Q_INVOKABLE bool changeChapter(const QString& title, const QString& date, const QString& body,
                                   const QVariantList& choices) {
        return editChapter([&](auto&, auto& scene) {
            const auto day = core::parse_iso_date(date.toStdString());
            if (day < core::parse_iso_date("2003-09-01") || day >= core::parse_iso_date("2015-01-01"))
                throw std::invalid_argument("Choose a scene date between September 2003 and the end of 2014.");
            scene.title = title.trimmed().toStdString();
            scene.date = core::iso_date(day);
            scene.text = body.simplified().toStdString();
            if (scene.choices.size() != static_cast<std::size_t>(choices.size()))
                throw std::invalid_argument("The scene choices changed. Reopen the scene and try again.");
            for (qsizetype index = 0; index < choices.size(); ++index) {
                const auto row = choices[index].toMap();
                auto& choice = scene.choices[static_cast<std::size_t>(index)];
                choice.name = row.value("name").toString().trimmed().toStdString();
                choice.detail = row.value("detail").toString().trimmed().toStdString();
            }
        });
    }
    Q_INVOKABLE bool changeChapterChoice(int index, const QString& name, const QString& detail) {
        return editChapter([&](auto&, auto& scene) {
            auto& choice = scene.choices.at(static_cast<std::size_t>(index));
            choice.name = name.trimmed().toStdString();
            choice.detail = detail.trimmed().toStdString();
        });
    }
    Q_INVOKABLE bool addChapterChoice() {
        return editChapter([](auto&, auto& scene) {
            if (scene.choices.size() >= 16) throw std::invalid_argument("A scene can have up to 16 choices.");
            content::FoundingOption choice{};
            choice.id = ("choice_" + QUuid::createUuid().toString(QUuid::Id128).left(12)).toStdString();
            choice.name = "New choice";
            choice.detail = "Describe what happens.";
            scene.choices.push_back(std::move(choice));
        });
    }
    Q_INVOKABLE bool createChapter(const QString& title, const QString& body) {
        const auto owner = QStringLiteral("scenario.founder_campaign");
        if (!editing() && !beginDraft(QStringLiteral("data/runtime/first-playable.json"), owner, true)) return false;
        return edit([&] {
            auto candidate = draft_->collection();
            auto& scenario = std::get<content::ScenarioDefinition>(candidate.definitions.at(owner.toStdString()).data);
            if (!scenario.founding) throw std::invalid_argument("This project has no founder story.");
            const auto key = authorKey(title, 38);
            content::FoundingOption choice{};
            choice.id = "continue";
            choice.name = "Continue";
            choice.detail = "The experience stays with you.";
            content::FormativeScene created{};
            created.id = key;
            created.title = title.trimmed().toStdString();
            created.date = "2014-06-01";
            created.text = body.simplified().toStdString();
            created.choices = {std::move(choice)};
            scenario.founding->scenes.push_back(std::move(created));
            if (draft_->replace(candidate)) {
                chapter_owner_ = owner.toStdString();
                chapter_id_ = key;
                world_mode_ = false;
            }
        });
    }
    Q_INVOKABLE void selectWorkspace(bool world, bool catalogs = false) {
        if (world) {
            try {
                world_.set_document(draft_.get());
                if (!world_.setCatalogMode(catalogs)) return;
            } catch (const std::exception& error) {
                draft_message_ = text(error.what());
                emit draftChanged();
                return;
            }
        }
        world_mode_=world;
        emit draftChanged();
    }
    Q_INVOKABLE bool openAssets() { return assets_.openRegister(checkout_); }
    Q_INVOKABLE bool pinAsset(const QString& id, bool include) {
        return edit([&] {
            if (!draft_) throw std::invalid_argument("Open the content draft that should carry the asset record.");
            auto next=draft_->collection();
            if (include) {
                studio::AssetRegister records{std::filesystem::path{checkout_.toStdU16String()}};
                const auto seal=records.seal(id.toStdString());
                next.asset_records[seal.record.id]=seal.fingerprint;
            } else next.asset_records.erase(id.toStdString());
            (void)draft_->replace(next);
        });
    }
    Q_INVOKABLE bool assetPinned(const QString& id) const {
        return draft_ && draft_->collection().asset_records.contains(id.toStdString());
    }
    [[nodiscard]] const QVariantList& rows() const noexcept { return visible_; }
    [[nodiscard]] QString checkout() const { return checkout_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] bool editing() const noexcept { return draft_ != nullptr; }
    [[nodiscard]] QString workingFlow() const { return text(flow_); }
    [[nodiscard]] QString workingTitle() const {
        const auto* definition = draft_ ? draft_->collection().find(flow_) : nullptr;
        return definition ? text(studio::event_name(draft_->collection(), *definition)) : QString{};
    }
    [[nodiscard]] QString workingVersion() const { return draft_ ? text(draft_->collection().content_version) : QString{}; }
    [[nodiscard]] const QStringList& workingFlows() const noexcept { return working_flows_; }
    [[nodiscard]] const QVariantList& flowScopes() const noexcept { return flow_scopes_; }
    [[nodiscard]] QString workingFile() const {
        return draft_ ? QString::fromStdU16String(draft_->relative_file().generic_u16string()) : QString{};
    }
    [[nodiscard]] QStringList workingTags() const {
        return draft_ && !flow_.empty() ? strings(std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data).tags) : QStringList{};
    }
    [[nodiscard]] bool canUndo() const noexcept { return draft_ && !world_.hasStaged() && !world_.hasUnapplied() && draft_->can_undo(); }
    [[nodiscard]] bool canRedo() const noexcept { return draft_ && !world_.hasStaged() && !world_.hasUnapplied() && draft_->can_redo(); }
    [[nodiscard]] bool draftDirty() const noexcept { return draft_ && (draft_->dirty() || world_.hasStaged() || world_.hasUnapplied()); }
    [[nodiscard]] bool saving() const noexcept { return saving_; }
    [[nodiscard]] bool existingDraft() const { return draft_ && draft_->is_draft(); }
    [[nodiscard]] QString draftMessage() const { return draft_message_; }
    [[nodiscard]] const QVariantList& flowOutline() const noexcept { return outline_; }
    [[nodiscard]] const QVariantList& formFields() const noexcept { return fields_; }
    [[nodiscard]] qsizetype outlineIndex() const noexcept { return outline_index_; }
    [[nodiscard]] QString formTicket() const { return QString::number(static_cast<qulonglong>(form_ticket_)); }
    [[nodiscard]] QVariantList groupOptions() const {
        QVariantList groups;
        if (!draft_ || flow_.empty()) return groups;
        const auto& caller=std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data);
        for (const auto& [id, definition]:draft_->collection().definitions) {
            const auto* group=std::get_if<content::FlowDefinition>(&definition.data);
            if (!group || !group->group || group->scope!=caller.scope || id==flow_) continue;
            QVariantList ports;
            for (const auto& port:*group->group) {
                QVariantList options;
                for (const auto& option:studio::group_variable_options(*draft_,caller.scope,&port,&caller))
                    options.push_back(QVariantMap{{"value",text(option.value)},{"label",text(option.label)}});
                ports.push_back(QVariantMap{{"name",text(port.name)},{"label",text((port.output?"Output · ":"Input · ")+port.name)},
                    {"options",std::move(options)}});
            }
            groups.push_back(QVariantMap{{"value",text(id)},{"label",text(studio::event_name(draft_->collection(), definition))},
                {"ports",std::move(ports)}});
        }
        return groups;
    }
    [[nodiscard]] QVariantList groupContinuations() const {
        QVariantList nodes;
        if (!draft_ || flow_.empty()) return nodes;
        const auto& flow=std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data);
        for (const auto& [id, node]:flow.nodes) {
            (void)node;
            nodes.push_back(QVariantMap{{"value",text(id)},{"label",text(studio::readable_name(id)+(id==flow.entry_node?" · first step":""))}});
        }
        return nodes;
    }
    Q_INVOKABLE bool createGroup(const QString& id) {
        return edit([&] {
            if (!draft_ || flow_.empty()) throw std::invalid_argument("Open a working flow first.");
            const auto key = authorKey(id);
            if (studio::create_flow_group(*draft_,flow_,key,id.trimmed().toStdString())) {
                flow_=key;
                outline_key_="group";
            }
        });
    }
    Q_INVOKABLE bool insertGroup(const QString& group,const QString& before,const QVariantMap& fields) {
        return edit([&] {
            if (!draft_ || flow_.empty()) throw std::invalid_argument("Open the caller flow first.");
            std::map<std::string,std::string,std::less<>> bindings;
            for(auto i=fields.cbegin();i!=fields.cend();++i) bindings.emplace(i.key().toStdString(),i.value().toString().toStdString());
            (void)studio::insert_group_call(*draft_,flow_,before.toStdString(),group.toStdString(),std::move(bindings));
        });
    }
    Q_INVOKABLE bool openCheckout(const QString& folder) {
        if (draftDirty() || (canvas_.hasLayoutEdits() && folder != checkout_)) {
            message_ = QStringLiteral("Save or close the working content/layout drafts before opening another checkout.");
            emit changed();
            return false;
        }
        try {
            const auto library = studio::inspect_checkout(std::filesystem::path{folder.toStdU16String()}, studio::Origin::reviewed);
            const auto opened = QString::fromStdU16String(library.checkout.u16string());
            if (opened != checkout_) closeDraft();
            checkout_ = opened;
            refreshSnapshotContext();
            message_.clear();
            source_ = libraryRows(library);
            indexed_ = {studio::Origin::reviewed};
            loadSources();
            refresh();
            return true;
        } catch (const std::exception& error) {
            message_ = text(error.what());
            emit changed();
            return false;
        }
    }
    Q_INVOKABLE void filter(QString search, QString kind, QString source) {
        search_ = search.trimmed();
        kind_ = std::move(kind);
        origin_ = std::move(source);
        try {
            loadSources();
        } catch (const std::exception& error) {
            message_ = text(error.what());
        }
        refresh();
    }
    Q_INVOKABLE qsizetype locate(const QString& file, const QString& id) const {
        for (qsizetype index = 0; index < visible_.size(); ++index) {
            const auto row = visible_.at(index).toMap();
            if (row.value(QStringLiteral("file")) == file && row.value(QStringLiteral("id")) == id) return index;
        }
        return -1;
    }
    Q_INVOKABLE bool beginDraft(const QString& file, const QString& id, bool chapter = false) {
        if (draftDirty() || canvas_.hasLayoutEdits()) {
            draft_message_ = QStringLiteral("Save or close the working draft first.");
            emit draftChanged();
            return false;
        }
        return edit([&] {
            auto next = std::make_unique<studio::DraftDocument>(std::filesystem::path{checkout_.toStdU16String()},
                std::filesystem::path{file.toStdU16String()});
            const auto key = id.toStdString();
            const auto* definition = next->collection().find(key);
            if (!definition) throw std::invalid_argument("Select a validated definition to open its collection.");
            chapter_owner_.clear();
            chapter_id_.clear();
            const auto is_flow = std::holds_alternative<content::FlowDefinition>(definition->data);
            world_mode_=!chapter && !is_flow;
            flow_ = is_flow ? key : std::string{};
            canvas_.set_document(nullptr, {}, {});
            world_.set_document(nullptr);
            draft_ = std::move(next);
            if (world_mode_)
                world_.set_document(draft_.get(),std::holds_alternative<content::ScenarioDefinition>(definition->data)?key:std::string{});
            if (world_mode_ && !std::holds_alternative<content::ScenarioDefinition>(definition->data)) {
                (void)world_.setCatalogMode(true);
                world_.selectSection(text("/definitions/"+key));
            }
            outline_key_.clear();
        });
    }
    Q_INVOKABLE void selectWorkingFlow(const QString& id) {
        if (!draft_ || !working_flows_.contains(id) || id == workingFlow()) return;
        flow_ = id.toStdString();
        outline_key_.clear();
        refreshForm();
        emit draftChanged();
    }
    Q_INVOKABLE bool createEvent(const QString& title, const QString& presentation, const QString& body,
                                 const QString& scope = QStringLiteral("company")) {
        if (!editing()) {
            const auto row = std::find_if(source_.cbegin(), source_.cend(), [](const auto& entry) {
                const auto item = entry.toMap();
                return item.value("file") == QStringLiteral("data/runtime/first-playable.json") &&
                    item.value("kind") == QStringLiteral("flow");
            });
            if (row == source_.cend()) {
                draft_message_ = QStringLiteral("Open a project with reviewed events first.");
                emit draftChanged();
                return false;
            }
            if (!beginDraft(row->toMap().value("file").toString(), row->toMap().value("id").toString())) return false;
        }
        return edit([&] {
            const auto name = title.trimmed();
            if (name.isEmpty()) throw std::invalid_argument("Enter a name for this event.");
            const auto key = authorKey(name);
            chapter_owner_.clear();
            chapter_id_.clear();
            if (studio::create_flow(*draft_, key, scope.toStdString(), name.toStdString(), presentation.toStdString(), body.trimmed().toStdString())) {
                flow_ = key;
                world_mode_ = false;
                outline_key_ = presentation == QStringLiteral("logic") ? "flow" : "node/message";
            }
        });
    }
    Q_INVOKABLE bool saveEvent() {
        if (!draft_ || saving_) return false;
        if (world_.hasStaged() || world_.hasUnapplied()) {
            draft_message_ = QStringLiteral("Apply the edited values and staged sections before saving the project.");
            emit draftChanged();
            return false;
        }
        if (!draft_->dirty()) return true;
        auto next = std::make_shared<studio::DraftDocument>(*draft_);
        const auto active_chapter = chapter();
        auto name = active_chapter.value("title").toString();
        if (name.isEmpty()) name = world_mode_ ? QStringLiteral("game data") : workingTitle();
        const auto summary = QStringLiteral("Edit %1 in Content Studio.").arg(name.left(300)).toStdString();
        saving_ = true;
        emit savingChanged();
        save_thread_ = std::jthread([this, next, summary] {
            QString error;
            try {
                next->save_reviewed(summary);
            } catch (const std::exception& failure) {
                error = text(failure.what());
            }
            QMetaObject::invokeMethod(this, [this, next, error] {
                if (error.isEmpty()) {
                    const auto scenario = world_.scenario();
                    const auto section = world_.path();
                    const auto browsing_data = world_.catalogMode();
                    canvas_.set_document(nullptr, {}, {});
                    world_.set_document(nullptr);
                    draft_ = std::make_unique<studio::DraftDocument>(std::move(*next));
                    if (world_mode_) {
                        world_.set_document(draft_.get(), scenario.toStdString());
                        if (browsing_data) (void)world_.setCatalogMode(true);
                        if (!section.isEmpty()) world_.selectSection(section);
                    }
                    refreshForm();
                    (void)openCheckout(checkout_);
                    draft_message_ = QStringLiteral("Saved to the project.");
                } else draft_message_ = error;
                saving_ = false;
                emit savingChanged();
                emit draftChanged();
            }, Qt::QueuedConnection);
        });
        return true;
    }
    Q_INVOKABLE bool createFlow(const QString& id, const QString& scope) {
        if (!draft_) return false;
        return edit([&] {
            auto key = id.trimmed().toStdString();
            if (studio::create_flow(*draft_, key, scope.toStdString())) {
                flow_ = std::move(key);
                outline_key_ = "flow";
            }
        });
    }
    Q_INVOKABLE bool changeTag(const QString& tag, bool remove) {
        if (!draft_ || flow_.empty()) return false;
        return edit([&] {
            auto candidate = draft_->collection();
            auto& tags = std::get<content::FlowDefinition>(candidate.definitions.at(flow_).data).tags;
            const auto value = tag.trimmed().toStdString();
            if (remove) {
                const auto found = std::find(tags.begin(), tags.end(), value);
                if (found != tags.end()) tags.erase(found);
            } else {
                if (!content::valid_id(value)) throw std::invalid_argument("Use a stable lowercase tag ID.");
                if (std::find(tags.begin(), tags.end(), value) != tags.end())
                    throw std::invalid_argument("This flow already has that tag.");
                tags.push_back(value);
            }
            (void)draft_->replace(candidate);
        });
    }
    Q_INVOKABLE void undo() { if (draft_) edit([&] { (void)draft_->undo(); }); }
    Q_INVOKABLE void redo() { if (draft_) edit([&] { (void)draft_->redo(); }); }
    Q_INVOKABLE void selectOutline(const QString& key) {
        if (!draft_) return;
        outline_key_ = key.toStdString();
        canvas_.set_selection(key, true);
        refreshForm();
        canvas_.remember_selection();
    }
    Q_INVOKABLE bool changeForm(const QString& ticket, const QString& key, const QString& value) {
        return edit([&] {
            if (!draft_ || !form_ || ticket != formTicket()) throw std::invalid_argument("Select the current outline row again.");
            const auto changed = form_->apply(*draft_, key.toStdString(), value.toStdString());
            if (changed && key == QStringLiteral("id")) {
                if (outline_key_.starts_with("node/") && outline_key_.find('/', 5) == std::string::npos)
                    outline_key_ = "node/" + value.toStdString();
                else if (outline_key_.starts_with("variables/"))
                    outline_key_ = "variables/" + value.toStdString();
            }
        });
    }
    Q_INVOKABLE bool saveDraft(const QString& filename) {
        if(world_.hasStaged() || world_.hasUnapplied()) {draft_message_=QStringLiteral("Apply or reset the pending world fields and staged sections before saving.");emit draftChanged();return false;}
        if (!draft_ || !edit([&] { draft_->save(std::filesystem::path{filename.toStdU16String()}); })) return false;
        const auto current = checkout_;
        (void)openCheckout(current);
        draft_message_ = QStringLiteral("Saved %1").arg(workingFile());
        emit draftChanged();
        return true;
    }
    Q_INVOKABLE void closeDraft() {
        if (saving_) return;
        canvas_.set_document(nullptr, {}, {});
        canvas_.discard_layouts();
        world_.set_document(nullptr);
        draft_.reset();
        chapter_owner_.clear();
        chapter_id_.clear();
        flow_.clear();
        draft_message_.clear();
        refreshForm();
        emit draftChanged();
    }
signals:
    void changed();
    void draftChanged();
    void formChanged();
    void savingChanged();
private:
    static QVariantList libraryRows(const studio::Library& library) {
        QVariantList next;
        for (const auto& document : library.documents) {
            const auto file = QString::fromStdU16String(document.relative_path.generic_u16string());
            const QVariantMap common{{QStringLiteral("file"), file}, {QStringLiteral("origin"), origin(document.origin)},
                {QStringLiteral("validation"), status(document.validation)}, {QStringLiteral("diagnostic"), text(document.diagnostic)},
                {QStringLiteral("version"), text(document.content_version)}};
            if (document.definitions.empty()) {
                auto row = common;
                row.insert(QStringLiteral("id"), file);
                row.insert(QStringLiteral("kind"), QStringLiteral("document"));
                next.push_back(std::move(row));
            }
            for (const auto& definition : document.definitions) {
                auto row = common;
                row.insert(QStringLiteral("id"), text(definition.id));
                row.insert(QStringLiteral("kind"), text(definition.kind));
                row.insert(QStringLiteral("title"), text(definition.title));
                row.insert(QStringLiteral("synopsis"), text(definition.synopsis));
                row.insert(QStringLiteral("owner"), text(definition.owner));
                row.insert(QStringLiteral("revision"), definition.revision);
                row.insert(QStringLiteral("tags"), strings(definition.tags));
                row.insert(QStringLiteral("uses"), strings(definition.uses));
                row.insert(QStringLiteral("usedBy"), strings(definition.used_by));
                row.insert(QStringLiteral("state"), text(definition.state));
                next.push_back(std::move(row));
            }
        }
        return next;
    }
    void loadSources() {
        if (checkout_.isEmpty()) return;
        for (auto candidate : {studio::Origin::draft, studio::Origin::engineering_fixture}) {
            if (indexed_.contains(candidate) || (!origin_.isEmpty() && origin(candidate) != origin_)) continue;
            const auto library = studio::inspect_checkout(std::filesystem::path{checkout_.toStdU16String()}, candidate);
            source_.append(libraryRows(library));
            indexed_.insert(candidate);
        }
    }
    template<class Action> bool editChapter(Action action) {
        if (!draft_ || chapter_id_.empty()) return false;
        return edit([&] {
            auto candidate = draft_->collection();
            auto& catalog = std::get<content::ScenarioDefinition>(candidate.definitions.at(chapter_owner_).data).founding.value();
            const auto scene = std::ranges::find(catalog.scenes, chapter_id_, &content::FormativeScene::id);
            if (scene == catalog.scenes.end()) throw std::invalid_argument("This childhood scene is unavailable.");
            action(catalog, *scene);
            (void)draft_->replace(candidate);
        });
    }
    static std::string authorKey(const QString& name, qsizetype maximum = 70) {
        auto stem = name.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_")).left(maximum);
        return (QStringLiteral("author.") + stem + QStringLiteral(".") +
            QUuid::createUuid().toString(QUuid::Id128).left(12)).toStdString();
    }
    void refreshSnapshotContext() {
        snapshot_.set_context(checkout_, workingFile(), draft_ ? draft_->edit_revision() : 0, draftDirty(), canvas_.hasLayoutEdits());
    }
    void reopenFinalized(const QString& target) {
        const auto selected = workingFlow();
        const auto selected_scenario=world_.scenario();
        const bool selected_world=world_mode_, selected_catalogs=world_.catalogMode();
        const auto selected_section=world_.path();
        const bool had_draft = editing();
        closeDraft();
        const auto current = checkout_;
        if (!openCheckout(current) || !had_draft) return;
        const auto preferred=selected_world?selected_scenario:selected;
        QString next;
        for (const auto& entry:source_) {
            const auto row=entry.toMap();
            if (row.value(QStringLiteral("file")).toString()!=target || !row.contains(QStringLiteral("revision"))) continue;
            const auto id=row.value(QStringLiteral("id")).toString();
            if (next.isEmpty()) next=id;
            if (id==preferred) {next=id;break;}
        }
        if (next.isEmpty()) return;
        beginDraft(target,next);
        if (working_flows_.contains(selected)) selectWorkingFlow(selected);
        if (world_.headquarters().size()>0 && !selected_scenario.isEmpty()) world_.selectScenario(selected_scenario);
        selectWorkspace(selected_world,selected_catalogs);
        if (selected_world && !selected_section.isEmpty()) {
            const auto sections=world_.sections();
            if (std::any_of(sections.begin(),sections.end(),[&](const auto& row){return row.toMap().value(QStringLiteral("key"))==selected_section;}))
                world_.selectSection(selected_section);
        }
    }
    template<class Action> bool edit(Action action) {
        if (saving_) return false;
        try {
            if(world_.hasStaged() || world_.hasUnapplied()) throw std::invalid_argument("Apply or reset the pending world fields and staged sections before editing another surface.");
            action();
            draft_message_.clear();
            refreshForm();
            emit draftChanged();
            return true;
        } catch (const std::exception& error) {
            draft_message_ = text(error.what());
            emit draftChanged();
            return false;
        }
    }
    void refreshForm() {
        ++form_ticket_;
        outline_.clear();
        fields_.clear();
        form_.reset();
        outline_index_ = -1;
        refreshWorkingFlows();
        if (draft_ && !flow_.empty() && chapter_id_.empty()) {
            canvas_.set_document(draft_.get(), flow_, std::filesystem::path{checkout_.toStdU16String()});
            if (outline_key_.empty()) outline_key_ = canvas_.restored_row().toStdString();
            const auto rows = studio::flow_outline(*draft_, flow_);
            const auto find_row = [&] {
                return std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.key == outline_key_; });
            };
            auto selected = find_row();
            while (selected == rows.end()) {
                const auto separator = outline_key_.rfind('/');
                if (outline_key_ == "node") outline_key_ = "execution";
                else outline_key_ = separator == std::string::npos ? "flow" : outline_key_.substr(0, separator);
                selected = find_row();
            }
            outline_index_ = selected - rows.begin();
            for (const auto& row : rows) {
                const QString indent(static_cast<qsizetype>(row.depth * 2), QChar{0x00a0});
                outline_.push_back(QVariantMap{{QStringLiteral("key"), text(row.key)},
                    {QStringLiteral("label"), indent + text(row.label)}});
            }
            form_ = studio::FlowForm::open(*draft_, flow_, selected->key);
            canvas_.set_selection(text(selected->key));
            for (const auto& field : form_->fields()) {
                QVariantList options;
                for (const auto& option : field.options)
                    options.push_back(QVariantMap{{QStringLiteral("value"), text(option.value)},
                        {QStringLiteral("label"), text(option.label)}, {QStringLiteral("help"), text(option.help)}});
                fields_.push_back(QVariantMap{{QStringLiteral("key"), text(field.key)}, {QStringLiteral("label"), text(field.label)},
                    {QStringLiteral("value"), text(field.value)}, {QStringLiteral("help"), text(field.help)},
                    {QStringLiteral("control"), control(field.control)}, {QStringLiteral("options"), std::move(options)},
                    {QStringLiteral("ticket"), formTicket()}});
            }
        }
        else canvas_.set_document(nullptr, {}, {});
        probe_.set_document(draft_ && chapter_id_.empty() ? &draft_->program() : nullptr, flow_);
        if (world_mode_) world_.set_document(draft_.get());
        refreshSnapshotContext();
        emit formChanged();
    }
    void refreshWorkingFlows() {
        const auto previous = std::move(working_flows_);
        working_flows_ = {};
        flow_scopes_.clear();
        if (!draft_) return;
        for (const auto& [id, definition] : draft_->collection().definitions)
            if (std::holds_alternative<content::FlowDefinition>(definition.data)) working_flows_.push_back(text(id));
        for (const auto& scope : studio::flow_scope_options(*draft_))
            flow_scopes_.push_back(QVariantMap{{QStringLiteral("value"), text(scope.value)}, {QStringLiteral("label"), text(scope.label)}});
        if (working_flows_.contains(workingFlow())) return;
        // A rename and its undo/redo both replace exactly one outline root.
        QStringList added;
        for (const auto& id : working_flows_)
            if (!previous.contains(id)) added.push_back(id);
        if (added.size() == 1) flow_ = added.front().toStdString();
        else flow_ = working_flows_.isEmpty() ? std::string{} : working_flows_.front().toStdString();
        outline_key_ = "flow";
    }
    void refresh() {
        QVariantList visible;
        for (const auto& entry : source_) {
            const auto row = entry.toMap();
            const auto kind = row.value(QStringLiteral("kind")).toString();
            const bool event = kind == QStringLiteral("flow") || kind == QStringLiteral("chapter");
            if ((!kind_.isEmpty() && (kind_ == QStringLiteral("events") ? !event : kind != kind_)) ||
                (!origin_.isEmpty() && row.value(QStringLiteral("origin")) != origin_)) continue;
            if (!search_.isEmpty()) {
                const QStringList searchable{row.value(QStringLiteral("id")).toString(), row.value(QStringLiteral("file")).toString(),
                    row.value(QStringLiteral("title")).toString(), row.value(QStringLiteral("synopsis")).toString(),
                    row.value(QStringLiteral("tags")).toStringList().join(u' '), row.value(QStringLiteral("validation")).toString(),
                    row.value(QStringLiteral("state")).toString()};
                if (!searchable.join(u' ').contains(search_, Qt::CaseInsensitive)) continue;
            }
            visible.push_back(entry);
        }
        std::stable_sort(visible.begin(), visible.end(), [](const auto& left, const auto& right) {
            return QString::compare(left.toMap().value("title").toString(),
                right.toMap().value("title").toString(), Qt::CaseInsensitive) < 0;
        });
        visible_ = std::move(visible);
        emit changed();
    }
    studio::ProbeController probe_;
    studio::SnapshotController snapshot_;
    studio::RevisionController revisions_;
    studio::AssetController assets_;
    studio::WorldController world_;
    bool world_mode_=false;
    QVariantList source_, visible_;
    QString checkout_, message_, search_, kind_, origin_ = QStringLiteral("Reviewed");
    std::set<studio::Origin> indexed_;
    std::unique_ptr<studio::DraftDocument> draft_;
    std::string flow_, chapter_owner_, chapter_id_;
    QString draft_message_;
    QVariantList outline_, fields_;
    QStringList working_flows_;
    QVariantList flow_scopes_;
    std::optional<studio::FlowForm> form_;
    std::string outline_key_;
    qsizetype outline_index_ = -1;
    std::uint64_t form_ticket_ = 0;
    // Destruct before the draft so the final ignored view flush has live data.
    studio::CanvasController canvas_;
    bool saving_ = false;
    std::jthread save_thread_;
};
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QCoreApplication::setApplicationName(QStringLiteral("Glowing Pancake Content Studio"));
    QCoreApplication::setApplicationVersion(text(core::build_identity().editor_version));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline game event editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("project"), QStringLiteral("Open an explicitly selected absolute checkout."), QStringLiteral("folder")});
    parser.addOption({QStringLiteral("software"), QStringLiteral("Use software rendering.")});
    parser.addOption({QStringLiteral("check-view"), QStringLiteral("Load and inspect the library view without showing a window.")});
    parser.addOption({QStringLiteral("walkthrough"), QStringLiteral("Run the engineering authoring walkthrough in an explicit fresh checkout copy and write a new local report."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("canvas-walkthrough"), QStringLiteral("Run the forms and typed canvas engineering walkthrough on a fresh checkout copy."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("finalizer-walkthrough"), QStringLiteral("Run graphical review/finalization, conflicts and recovery after the forms/canvas walkthrough on a fresh checkout copy."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("world-walkthrough"), QStringLiteral("Run scenario/world forms, atomic batches, map/list and actual opening checks on a fresh checkout copy."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("catalog-walkthrough"), QStringLiteral("Run catalog editing, reference review, localization, asset pins and group calls on a fresh checkout copy."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("revision-walkthrough"), QStringLiteral("Run finalized comparison, matched fixtures and reviewed save export on the prepared scratch checkout."), QStringLiteral("report.json")});
    parser.addOption({QStringLiteral("text-scale"), QStringLiteral("Editor text scale: 100 or 200 percent."), QStringLiteral("percent"), QStringLiteral("100")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save the walkthrough's rendered editor window to a new PNG."), QStringLiteral("file.png")});
    parser.process(application);
    const bool canvas_walkthrough = parser.isSet(QStringLiteral("canvas-walkthrough"));
    const bool finalizer_walkthrough = parser.isSet(QStringLiteral("finalizer-walkthrough"));
    const bool world_walkthrough = parser.isSet(QStringLiteral("world-walkthrough"));
    const bool catalog_walkthrough = parser.isSet(QStringLiteral("catalog-walkthrough"));
    const bool revision_walkthrough = parser.isSet(QStringLiteral("revision-walkthrough"));
    const bool run_walkthrough = parser.isSet(QStringLiteral("walkthrough")) || canvas_walkthrough || finalizer_walkthrough || world_walkthrough || catalog_walkthrough || revision_walkthrough;
    if (!parser.positionalArguments().isEmpty() ||
        ((parser.isSet(QStringLiteral("check-view")) || run_walkthrough) && !parser.isSet(QStringLiteral("project"))) ||
        (parser.isSet(QStringLiteral("check-view")) && run_walkthrough) ||
        (static_cast<int>(parser.isSet(QStringLiteral("walkthrough"))) + static_cast<int>(canvas_walkthrough) +
            static_cast<int>(finalizer_walkthrough) + static_cast<int>(world_walkthrough) + static_cast<int>(catalog_walkthrough) + static_cast<int>(revision_walkthrough) > 1) ||
        (parser.isSet(QStringLiteral("screenshot")) && !run_walkthrough)) parser.showHelp(2);
    try {
#if !PGP_TEST_HOOKS
        if(run_walkthrough || parser.isSet(QStringLiteral("check-view"))) throw std::invalid_argument("Editor checks require an explicit --enable-tests build.");
#endif
        const auto scale=parser.value(QStringLiteral("text-scale"));
        if(scale!=QStringLiteral("100") && scale!=QStringLiteral("200")) throw std::invalid_argument("Choose editor text scale 100 or 200.");
        auto font=QGuiApplication::font();font.setPointSizeF(font.pointSizeF()*(scale==QStringLiteral("200")?2.0:1.0));QGuiApplication::setFont(font);
        for (const auto& option : {QStringLiteral("walkthrough"), QStringLiteral("canvas-walkthrough"), QStringLiteral("finalizer-walkthrough"),
                QStringLiteral("world-walkthrough"), QStringLiteral("catalog-walkthrough"), QStringLiteral("revision-walkthrough"), QStringLiteral("screenshot")})
            if (parser.isSet(option) && std::filesystem::exists(std::filesystem::path{parser.value(option).toStdU16String()}))
                throw std::invalid_argument("Choose new walkthrough report/screenshot filenames; existing output was preserved.");
        Browser browser;
        if (parser.isSet(QStringLiteral("project")) && !browser.openCheckout(parser.value(QStringLiteral("project")))) {
            std::cerr << browser.message().toStdString() << '\n';
            return 2;
        }
        (void)graphics::configure(parser.isSet(QStringLiteral("software")));
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QQmlApplicationEngine engine;
        const auto imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        engine.setImportPathList({imports, QStringLiteral("qrc:/qt/qml")});
        resources::ResourcePolicy policy;
        if (!policy.allow_local_root(imports) || !policy.allow_resource_prefix(QStringLiteral("/qt-project.org/imports/")) ||
            !policy.allow_resource_prefix(QStringLiteral("/qt/qml/"))) throw std::runtime_error("Trusted local Qt imports are unavailable.");
        const auto guards = resources::install_resource_guards(engine, std::move(policy));
        engine.rootContext()->setContextProperty(QStringLiteral("studio"), &browser);
        engine.load(QUrl(QStringLiteral("qrc:/pgp/studio/Main.qml")));
        if (engine.rootObjects().isEmpty()) return 2;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window) return 2;
#if PGP_TEST_HOOKS
        if (parser.isSet(QStringLiteral("check-view"))) {
            const auto* list = window->findChild<QObject*>(QStringLiteral("studioLibrary"));
            const auto all_rows = browser.rows().size();
            if (!list || list->property("count").toLongLong() != all_rows || guards->denied_requests.load() != 0) throw std::runtime_error("Studio view check: Library row count/filter or resource guard failed.");
            browser.filter({}, QStringLiteral("invalid_type"), {});
            if (list->property("count").toLongLong() != 0) throw std::runtime_error("Studio view check: Library type filtering failed.");
            browser.filter({}, QStringLiteral("events"), QStringLiteral("Reviewed"));
            if (list->property("count").toLongLong() != all_rows) throw std::runtime_error("Studio view check: Library filter reset failed.");
            if (browser.locate(QStringLiteral("data/runtime/first-playable.json"), QStringLiteral("curiosity")) >= 0) {
                const auto created = browser.createChapter(QStringLiteral("An afternoon at the fair"), QStringLiteral("Your class takes the bus to the fair."));
                const auto added = created && browser.addChapterChoice();
                const auto edited = added && browser.changeChapterChoice(1, QStringLiteral("I enter the art show"), QStringLiteral("A teacher gives you a sketchbook."));
                if (!edited) throw std::runtime_error("Childhood scene edit failed: " + browser.draftMessage().toStdString());
                browser.closeDraft();
            }
            const auto flow = std::find_if(browser.rows().begin(), browser.rows().end(), [](const auto& entry) {
                const auto row = entry.toMap();
                return row.value(QStringLiteral("kind")) == QStringLiteral("flow") &&
                    row.value(QStringLiteral("file")) == QStringLiteral("data/runtime/first-playable.json");
            });
            if (flow != browser.rows().end()) {
                const auto row = flow->toMap();
                browser.beginDraft(row.value(QStringLiteral("file")).toString(), row.value(QStringLiteral("id")).toString());
                if (!browser.editing()) throw std::runtime_error("Studio view check: The selected library flow did not open.");
                auto* outline = window->findChild<QObject*>(QStringLiteral("flowOutline"));
                if (!outline || outline->property("count").toLongLong() != browser.flowOutline().size()) throw std::runtime_error("Studio view check: The shared outline count does not match its document.");
                QKeyEvent home(QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier);
                QCoreApplication::sendEvent(outline, &home);
                QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
                QCoreApplication::sendEvent(outline, &down);
                if (browser.outlineIndex() != 1) throw std::runtime_error("Studio view check: Keyboard outline selection failed.");
                const auto tags = browser.workingTags();
                const auto tag = tags.isEmpty() ? QStringLiteral("studio_view_probe") : tags.front();
                if (!browser.changeTag(tag, !tags.isEmpty()) || !browser.draftDirty()) throw std::runtime_error("Studio view check: Tag edit did not dirty the working draft.");
                browser.undo();
                if (browser.draftDirty() || browser.workingTags() != tags) throw std::runtime_error("Studio view check: Undo did not restore the saved tags.");
                browser.redo();
                if (!browser.draftDirty()) throw std::runtime_error("Studio view check: Redo did not restore the tag edit.");
                const auto trigger = std::find_if(browser.flowOutline().begin(), browser.flowOutline().end(), [](const auto& entry) {
                    const auto key = entry.toMap().value(QStringLiteral("key")).toString();
                    return key.startsWith(QStringLiteral("trigger/")) && !key.contains(QStringLiteral("/expression"));
                });
                if (trigger != browser.flowOutline().end()) {
                    browser.selectOutline(trigger->toMap().value(QStringLiteral("key")).toString());
                    const auto comparison = std::find_if(browser.formFields().begin(), browser.formFields().end(), [](const auto& entry) {
                        return entry.toMap().value(QStringLiteral("key")) == QStringLiteral("comparison");
                    });
                    if (comparison != browser.formFields().end()) {
                        const auto key = comparison->toMap().value(QStringLiteral("value")).toString();
                        if (!browser.changeForm(browser.formTicket(), QStringLiteral("comparison"),
                                                key == QStringLiteral("eq") ? QStringLiteral("ne") : QStringLiteral("eq"))) throw std::runtime_error("Studio view check: Trigger form edit failed.");
                        browser.undo();
                    }
                }
                const auto verify = [](bool valid, const char* detail) {
                    if (!valid) throw std::runtime_error(detail);
                };
                auto* inspector = qobject_cast<studio::ProbeController*>(browser.probeController());
                verify(inspector && !inspector->scenarios().isEmpty(), "Inspection view needs a scenario fixture.");
                const auto scenario = inspector->scenarios().front();
                const auto headquarters = inspector->headquarters(scenario);
                verify(!headquarters.isEmpty() && inspector->start(scenario, headquarters.front(), QStringLiteral("18446744073709551615"), true), "The inspection fixture did not start.");
                verify(inspector->ready() && !inspector->report().isEmpty() && inspector->reload(), "The inspection report or checkpoint reload failed.");
                const auto original_date = inspector->date();
                verify(!inspector->start(scenario, headquarters.front(), QStringLiteral("1.5"), true) && inspector->date() == original_date, "An invalid seed changed the fixture.");
                verify(inspector->inspectBoundaries(), "Boundary inspection failed.");
                inspector->advance(inspector->nextDate());
                QElapsedTimer timeout;
                timeout.start();
                while (inspector->busy() && timeout.elapsed() < 3000) QCoreApplication::processEvents();
                verify(!inspector->busy() && inspector->date() != original_date, "The isolated date did not advance.");
                if (browser.workingFlows().contains(QStringLiteral("flow.supplier.exclusivity_offer"))) {
                    browser.selectWorkingFlow(QStringLiteral("flow.supplier.exclusivity_offer"));
                    browser.selectOutline(QStringLiteral("trigger/0"));
                    verify(browser.changeForm(browser.formTicket(), QStringLiteral("comparison"), QStringLiteral("lte")), "The fixture comparison edit failed.");
                    verify(inspector->start(scenario, headquarters.front(), QStringLiteral("42"), true), "The dialogue fixture did not start.");
                    const auto presentations = inspector->presentations();
                    verify(presentations.size() == 1, "The dialogue fixture has an unexpected presentation count.");
                    QCoreApplication::processEvents();
                    verify(view_item(window->contentItem(), QStringLiteral("flowPresentation")), "The shared dialogue card did not load.");
                    const auto presentation = presentations.front().toMap().value(QStringLiteral("id")).toString();
                    verify(inspector->choose(presentation, QStringLiteral("decline")) && inspector->presentations().size() == 2, "The dialogue result failed.");
                    browser.undo();
                    verify(!inspector->ready(), "A document edit retained a stale fixture.");
                }
                browser.closeDraft();
                verify(browser.createEvent(QStringLiteral("The mayor sends a letter"), QStringLiteral("dialogue"),
                    QStringLiteral("The mayor invites you to the town hall.")), "A named dialogue could not be created.");
                verify(browser.workingTitle() == QStringLiteral("The mayor sends a letter") &&
                    browser.changeForm(browser.formTicket(), QStringLiteral("body_copy"), QStringLiteral("Meet the mayor after lunch.")),
                    "The event must expose editable player text.");
                auto* canvas = qobject_cast<studio::CanvasController*>(browser.canvasController());
                verify(canvas && canvas->prepareInsert() && canvas->insertNode(canvas->ticket(), QStringLiteral("insert/delay")),
                    "The graph must add a box on an execution route.");
                verify(inspector->start(scenario, headquarters.front(), QStringLiteral("42"), true) &&
                    inspector->presentations().size() == 1 &&
                    inspector->presentations().front().toMap().value("detail").toString().startsWith(QStringLiteral("Meet the mayor after lunch.")),
                    "The shared runtime preview must show the edited copy.");
                browser.closeDraft();
            }
            std::cout << "Studio library view loaded and filtered " << all_rows << " rows\n";
            return 0;
        }
#endif
        auto configuration = window->graphicsConfiguration();
        configuration.setAutomaticPipelineCache(false);
        window->setGraphicsConfiguration(configuration);
        QObject::connect(window, &QQuickWindow::sceneGraphError, &application,
            [](QQuickWindow::SceneGraphError, const QString& message) {
                qCritical().noquote() << message;
                QCoreApplication::exit(4);
            });
        if (run_walkthrough) window->resize(1280, 720);
        window->show();
#if PGP_TEST_HOOKS
        if(world_walkthrough || catalog_walkthrough || revision_walkthrough) {
            const auto screenshot=parser.value(QStringLiteral("screenshot"));
            auto report=world_walkthrough?studio::run_world_walkthrough(browser,*window,screenshot):
                catalog_walkthrough ? studio::run_catalog_walkthrough(browser,*window,screenshot) :
                studio::run_revision_walkthrough(browser,*window,screenshot);
            if(guards->denied_requests.load()!=0) throw std::runtime_error("The editor walkthrough attempted a blocked resource request.");
            const auto renderer=window->rendererInterface()->graphicsApi()==QSGRendererInterface::Metal?QStringLiteral("Metal"):
                window->rendererInterface()->graphicsApi()==QSGRendererInterface::Software?QStringLiteral("Software"):QStringLiteral("Other native backend");
            report.insert(QStringLiteral("renderer"),renderer);report.insert(QStringLiteral("text_scale"),scale.toInt());
            const auto bytes=QJsonDocument{report}.toJson(QJsonDocument::Indented);
            const auto destination=parser.value(world_walkthrough ? QStringLiteral("world-walkthrough") :
                catalog_walkthrough ? QStringLiteral("catalog-walkthrough") : QStringLiteral("revision-walkthrough"));
            persistence::atomic_write(std::filesystem::path{destination.toStdU16String()},
                std::string_view{bytes.constData(),static_cast<std::size_t>(bytes.size())},
                [](std::string_view){throw std::invalid_argument("Choose a new report filename.");},4U*1024U*1024U,false);
            if(world_walkthrough) std::cout<<"Studio world walkthrough passed: "<<report.value(QStringLiteral("form_submissions")).toInt()
                <<" QML inputs, coupled batches, 51-state keyboard/pointer map, actual opening/save, draft reopen and 900px workspace; ";
            else if(catalog_walkthrough) std::cout<<"Studio catalog walkthrough passed: catalog edits, reviewed references, locale fallback, verified asset pin, typed group call and draft reopen; ";
            else std::cout<<"Studio revision walkthrough passed: finalized differences, matched day, explicit mapping, preserved original and exact migrated reload; ";
            std::cout<<renderer.toStdString()<<" text "<<scale.toStdString()<<'\n';
            return 0;
        }
        if (run_walkthrough) {
            window->setProperty("page", 0);
            auto report = studio::run_authoring_walkthrough(browser, *window);
            if (canvas_walkthrough || finalizer_walkthrough) { report.insert(QStringLiteral("canvas"), studio::run_canvas_walkthrough(browser, *window)); }
            if (finalizer_walkthrough) {
                report.insert(QStringLiteral("forms_stage_preserved_runtime_and_versions"), report.take(QStringLiteral("runtime_and_versions_unchanged")));
                report.insert(QStringLiteral("finalizer"), studio::run_snapshot_walkthrough(browser, *window, parser.value(QStringLiteral("screenshot"))));
            }
            if (guards->denied_requests.load() != 0) throw std::runtime_error("The walkthrough attempted a blocked resource request.");
            report.insert(QStringLiteral("renderer"), window->rendererInterface()->graphicsApi() == QSGRendererInterface::Metal ? QStringLiteral("Metal") :
                window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software ? QStringLiteral("Software") : QStringLiteral("Other native backend"));
            if (parser.isSet(QStringLiteral("screenshot")) && !finalizer_walkthrough) {
                studio::walkthrough::reveal_report(*window);
                QImage image;
                QElapsedTimer timer; timer.start();
                while (image.isNull() && timer.elapsed() < 3000) {
                    QCoreApplication::processEvents();
                    image = window->grabWindow();
                }
                if (image.isNull() || !image.save(parser.value(QStringLiteral("screenshot")), "PNG")) throw std::runtime_error("The editor walkthrough screenshot failed.");
            }
            const auto bytes = QJsonDocument{report}.toJson(QJsonDocument::Indented);
            persistence::atomic_write(std::filesystem::path{parser.value(finalizer_walkthrough ? QStringLiteral("finalizer-walkthrough") : canvas_walkthrough ? QStringLiteral("canvas-walkthrough") : QStringLiteral("walkthrough")).toStdU16String()},
                std::string_view{bytes.constData(), static_cast<std::size_t>(bytes.size())},
                [](std::string_view) { throw std::invalid_argument("Choose a new walkthrough report filename."); }, 4U * 1024U * 1024U, false);
            std::cout << "Studio engineering walkthrough passed: QML form edits, keyboard outline, sign/scope inspection, branching dialogue, delayed exact reload, five lifecycle reopens and separate draft; "
                << report.value(QStringLiteral("form_submissions")).toInt() << " form submissions; renderer " << report.value(QStringLiteral("renderer")).toString().toStdString()
                << ((canvas_walkthrough || finalizer_walkthrough) ? "; canvas ports/palette, typed refusals, shared undo, integer layouts and local viewport reopen passed" : "")
                << (finalizer_walkthrough ? "; graphical review/finalize/reopen, external conflicts, no-op, journal recovery and explicit game version passed" : "") << '\n';
            return 0;
        }
#endif
        return application.exec();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}

#include "main.moc"
