#include <pgp/core/build_identity.hpp>
#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/resources/resource_guards.hpp>
#include <pgp/studio/draft.hpp>
#include <pgp/studio/library.hpp>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsConfiguration>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QVariant>

#include <algorithm>
#include <iostream>
#include <memory>

namespace {
using namespace pgp;
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
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
class Browser final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QString checkout READ checkout NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY draftChanged)
    Q_PROPERTY(QString workingFlow READ workingFlow NOTIFY draftChanged)
    Q_PROPERTY(QString workingFile READ workingFile NOTIFY draftChanged)
    Q_PROPERTY(QStringList workingTags READ workingTags NOTIFY draftChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY draftChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY draftChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool existingDraft READ existingDraft NOTIFY draftChanged)
    Q_PROPERTY(QString draftMessage READ draftMessage NOTIFY draftChanged)
public:
    [[nodiscard]] const QVariantList& rows() const noexcept { return visible_; }
    [[nodiscard]] QString checkout() const { return checkout_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] bool editing() const noexcept { return draft_ != nullptr; }
    [[nodiscard]] QString workingFlow() const { return text(flow_); }
    [[nodiscard]] QString workingFile() const {
        return draft_ ? QString::fromStdU16String(draft_->relative_file().generic_u16string()) : QString{};
    }
    [[nodiscard]] QStringList workingTags() const {
        return draft_ ? strings(std::get<content::FlowDefinition>(draft_->collection().definitions.at(flow_).data).tags) : QStringList{};
    }
    [[nodiscard]] bool canUndo() const noexcept { return draft_ && draft_->can_undo(); }
    [[nodiscard]] bool canRedo() const noexcept { return draft_ && draft_->can_redo(); }
    [[nodiscard]] bool draftDirty() const noexcept { return draft_ && draft_->dirty(); }
    [[nodiscard]] bool existingDraft() const { return draft_ && draft_->is_draft(); }
    [[nodiscard]] QString draftMessage() const { return draft_message_; }
    Q_INVOKABLE bool openCheckout(const QString& folder) {
        if (draftDirty()) {
            message_ = QStringLiteral("Save or close the working draft before opening a checkout.");
            emit changed();
            return false;
        }
        try {
            const auto library = studio::inspect_checkout(std::filesystem::path{folder.toStdU16String()});
            QVariantList next;
            for (const auto& document : library.documents) {
                const auto file = QString::fromStdU16String(document.relative_path.generic_u16string());
                const QVariantMap common{{QStringLiteral("file"), file}, {QStringLiteral("origin"), origin(document.origin)},
                    {QStringLiteral("validation"), status(document.validation)}, {QStringLiteral("diagnostic"), text(document.diagnostic)},
                    {QStringLiteral("version"), text(document.content_version)}, {QStringLiteral("fingerprint"), text(document.source_hash)}};
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
                    row.insert(QStringLiteral("revision"), definition.revision);
                    row.insert(QStringLiteral("tags"), strings(definition.tags));
                    row.insert(QStringLiteral("uses"), strings(definition.uses));
                    row.insert(QStringLiteral("usedBy"), strings(definition.used_by));
                    next.push_back(std::move(row));
                }
            }
            const auto opened = QString::fromStdU16String(library.checkout.u16string());
            if (opened != checkout_) closeDraft();
            checkout_ = opened;
            message_ = QStringLiteral("%1 collections · checkout schema %2")
                .arg(static_cast<qulonglong>(library.documents.size()))
                .arg(library.checkout_schema == 0 ? QStringLiteral("unknown") : QString::number(library.checkout_schema));
            source_ = std::move(next);
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
        refresh();
    }
    Q_INVOKABLE qsizetype locate(const QString& file, const QString& id) const {
        for (qsizetype index = 0; index < visible_.size(); ++index) {
            const auto row = visible_.at(index).toMap();
            if (row.value(QStringLiteral("file")) == file && row.value(QStringLiteral("id")) == id) return index;
        }
        return -1;
    }
    Q_INVOKABLE void beginDraft(const QString& file, const QString& id) {
        if (draftDirty()) {
            draft_message_ = QStringLiteral("Save or close the working draft first.");
            emit draftChanged();
            return;
        }
        edit([&] {
            auto next = std::make_unique<studio::DraftDocument>(std::filesystem::path{checkout_.toStdU16String()},
                std::filesystem::path{file.toStdU16String()});
            const auto key = id.toStdString();
            const auto* definition = next->collection().find(key);
            if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
                throw std::invalid_argument("Select a validated flow to edit its tags.");
            flow_ = key;
            draft_ = std::move(next);
        });
    }
    Q_INVOKABLE bool changeTag(const QString& tag, bool remove) {
        if (!draft_) return false;
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
    Q_INVOKABLE bool saveDraft(const QString& filename) {
        if (!draft_ || !edit([&] { draft_->save(std::filesystem::path{filename.toStdU16String()}); })) return false;
        const auto current = checkout_;
        (void)openCheckout(current);
        draft_message_ = QStringLiteral("Saved %1").arg(workingFile());
        emit draftChanged();
        return true;
    }
    Q_INVOKABLE void closeDraft() {
        draft_.reset();
        flow_.clear();
        draft_message_.clear();
        emit draftChanged();
    }
signals:
    void changed();
    void draftChanged();
private:
    template<class Action> bool edit(Action action) {
        try {
            action();
            draft_message_.clear();
            emit draftChanged();
            return true;
        } catch (const std::exception& error) {
            draft_message_ = text(error.what());
            emit draftChanged();
            return false;
        }
    }
    void refresh() {
        QVariantList visible;
        for (const auto& entry : source_) {
            const auto row = entry.toMap();
            if ((!kind_.isEmpty() && row.value(QStringLiteral("kind")) != kind_) ||
                (!origin_.isEmpty() && row.value(QStringLiteral("origin")) != origin_)) continue;
            if (!search_.isEmpty()) {
                const QStringList searchable{row.value(QStringLiteral("id")).toString(), row.value(QStringLiteral("file")).toString(),
                    row.value(QStringLiteral("tags")).toStringList().join(u' '), row.value(QStringLiteral("validation")).toString()};
                if (!searchable.join(u' ').contains(search_, Qt::CaseInsensitive)) continue;
            }
            visible.push_back(entry);
        }
        visible_ = std::move(visible);
        emit changed();
    }
    QVariantList source_, visible_;
    QString checkout_, message_, search_, kind_, origin_;
    std::unique_ptr<studio::DraftDocument> draft_;
    std::string flow_;
    QString draft_message_;
};
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QCoreApplication::setApplicationName(QStringLiteral("Glowing Pancake Content Studio"));
    QCoreApplication::setApplicationVersion(text(core::build_identity().editor_version));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline development checkout library."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("project"), QStringLiteral("Open an explicitly selected absolute checkout."), QStringLiteral("folder")});
    parser.addOption({QStringLiteral("software"), QStringLiteral("Use software rendering.")});
    parser.addOption({QStringLiteral("check-view"), QStringLiteral("Load and inspect the library view without showing a window.")});
    parser.process(application);
    if (!parser.positionalArguments().isEmpty() ||
        (parser.isSet(QStringLiteral("check-view")) && !parser.isSet(QStringLiteral("project")))) parser.showHelp(2);
    try {
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
        if (parser.isSet(QStringLiteral("check-view"))) {
            const auto* list = window->findChild<QObject*>(QStringLiteral("studioLibrary"));
            const auto all_rows = browser.rows().size();
            if (!list || list->property("count").toLongLong() != all_rows || guards->denied_requests.load() != 0) return 3;
            browser.filter({}, QStringLiteral("invalid_type"), {});
            if (list->property("count").toLongLong() != 0) return 3;
            browser.filter({}, {}, {});
            if (list->property("count").toLongLong() != all_rows) return 3;
            const auto flow = std::find_if(browser.rows().begin(), browser.rows().end(), [](const auto& entry) {
                return entry.toMap().value(QStringLiteral("kind")) == QStringLiteral("flow");
            });
            if (flow != browser.rows().end()) {
                const auto row = flow->toMap();
                browser.beginDraft(row.value(QStringLiteral("file")).toString(), row.value(QStringLiteral("id")).toString());
                if (!browser.editing()) return 3;
                const auto tags = browser.workingTags();
                const auto tag = tags.isEmpty() ? QStringLiteral("studio_view_probe") : tags.front();
                if (!browser.changeTag(tag, !tags.isEmpty()) || !browser.draftDirty()) return 3;
                browser.undo();
                if (browser.draftDirty() || browser.workingTags() != tags) return 3;
                browser.redo();
                if (!browser.draftDirty()) return 3;
                browser.closeDraft();
            }
            std::cout << "Studio library view loaded and filtered " << all_rows << " rows\n";
            return 0;
        }
        auto configuration = window->graphicsConfiguration();
        configuration.setAutomaticPipelineCache(false);
        window->setGraphicsConfiguration(configuration);
        QObject::connect(window, &QQuickWindow::sceneGraphError, &application,
            [](QQuickWindow::SceneGraphError, const QString& message) {
                qCritical().noquote() << message;
                QCoreApplication::exit(4);
            });
        window->show();
        return application.exec();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}

#include "main.moc"
