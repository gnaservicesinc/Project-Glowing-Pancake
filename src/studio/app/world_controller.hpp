#pragma once
#include <pgp/studio/world_forms.hpp>
#include <pgp/studio/catalog.hpp>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace pgp::studio {
class WorldController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList scenarios READ scenarios NOTIFY changed)
    Q_PROPERTY(QString scenario READ scenario NOTIFY changed)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString ticket READ ticket NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString report READ report NOTIFY changed)
    Q_PROPERTY(QStringList headquarters READ headquarters NOTIFY changed)
    Q_PROPERTY(bool adding READ adding NOTIFY changed)
    Q_PROPERTY(bool hasStaged READ hasStaged NOTIFY changed)
    Q_PROPERTY(bool hasUnapplied READ hasUnapplied NOTIFY pendingChanged)
    Q_PROPERTY(QString stagedSummary READ stagedSummary NOTIFY changed)
    Q_PROPERTY(bool canAppend READ canAppend NOTIFY changed)
    Q_PROPERTY(bool canRemove READ canRemove NOTIFY changed)
    Q_PROPERTY(QVariantList templates READ templates NOTIFY changed)
    Q_PROPERTY(QVariantList states READ states NOTIFY changed)
    Q_PROPERTY(QString selectedState READ selectedState NOTIFY changed)
    Q_PROPERTY(QVariantMap mapGeometry READ mapGeometry NOTIFY changed)
    Q_PROPERTY(QString mapCredit READ mapCredit NOTIFY changed)
    Q_PROPERTY(bool catalogMode READ catalogMode NOTIFY changed)
    Q_PROPERTY(QVariantList catalogEntries READ catalogEntries NOTIFY changed)
    Q_PROPERTY(QVariantList catalogTemplates READ catalogTemplates NOTIFY changed)
    Q_PROPERTY(QVariantList referenceUses READ referenceUses NOTIFY changed)
    Q_PROPERTY(QVariantList renameChanges READ renameChanges NOTIFY changed)
    Q_PROPERTY(QString selectedIdentity READ selectedIdentity NOTIFY changed)
    Q_PROPERTY(QString selectedFamily READ selectedFamily NOTIFY changed)
    Q_PROPERTY(bool selectedDeprecated READ selectedDeprecated NOTIFY changed)
    Q_PROPERTY(bool canDeprecate READ canDeprecate NOTIFY changed)
public:
    WorldController();
    void set_document(DraftDocument*,std::string_view scenario={});
    QVariantList scenarios() const {return scenarios_;}
    QString scenario() const;
    QVariantList sections() const {return sections_;}
    QVariantList fields() const {return fields_;}
    QString path() const;
    QString ticket() const {return QString::number(static_cast<qulonglong>(ticket_));}
    QString message() const {return message_;}
    QString report() const {return report_;}
    QStringList headquarters() const;
    bool adding() const {return adding_;}
    bool hasStaged() const {return !staged_.empty();}
    bool hasUnapplied() const {return !unapplied_.isEmpty();}
    QString stagedSummary() const;
    bool canAppend() const {return form_ && form_->can_append();}
    bool canRemove() const {return form_ && form_->can_remove();}
    QVariantList templates() const {return templates_;}
    QVariantList states() const {return states_;}
    QString selectedState() const {return selected_state_;}
    QVariantMap mapGeometry() const;
    QString mapCredit() const;
    bool catalogMode() const {return catalog_mode_;}
    QVariantList catalogEntries() const {return catalog_entries_;}
    QVariantList catalogTemplates() const {return catalog_templates_;}
    QVariantList referenceUses() const {return reference_uses_;}
    QVariantList renameChanges() const {return rename_changes_;}
    QString selectedIdentity() const {return QString::fromStdString(identity_);}
    QString selectedFamily() const {return QString::fromStdString(family_);}
    bool selectedDeprecated() const;
    bool canDeprecate() const;
    Q_INVOKABLE bool setCatalogMode(bool);
    Q_INVOKABLE void selectCatalogEntry(QString path);
    Q_INVOKABLE bool reviewRename(QString newId);
    Q_INVOKABLE bool applyRename();
    Q_INVOKABLE bool cloneCatalogEntry(QString newId);
    Q_INVOKABLE bool beginCatalogTemplate(QString sourcePath);
    Q_INVOKABLE bool removeIdentity();
    Q_INVOKABLE bool deprecateIdentity(bool);
    Q_INVOKABLE void followReference(QString owner, QString path);
    Q_INVOKABLE void openSelectedFlow();
    Q_INVOKABLE QString messageValue(QString key,QString locale,bool fallback=false) const;
    Q_INVOKABLE bool saveMessage(QString key,QString locale,QString value);
    Q_INVOKABLE void selectScenario(QString);
    Q_INVOKABLE void selectSection(QString);
    Q_INVOKABLE void filter(QString);
    Q_INVOKABLE void selectState(QString);
    Q_INVOKABLE bool apply(QString ticket,QVariantMap values);
    Q_INVOKABLE void setUnapplied(QVariantMap values);
    Q_INVOKABLE bool stage(QString ticket,QVariantMap values);
    Q_INVOKABLE bool applyStaged();
    Q_INVOKABLE void discardStaged();
    Q_INVOKABLE bool beginAppend(QString templateIndex);
    Q_INVOKABLE void cancelAppend();
    Q_INVOKABLE bool removeEntry();
    Q_INVOKABLE bool duplicate(QString id);
    Q_INVOKABLE bool addMissing(QString source);
    Q_INVOKABLE bool preview(QString headquarters);
signals:
    void changed();
    void pendingChanged();
    void documentEdited();
    void identityRenamed(QString family,QString before,QString after);
    void flowRequested(QString id);
private:
    template<class Action> bool perform(Action action) {
        try {message_.clear();action();return true;}
        catch(const std::exception& e) {message_=QString::fromUtf8(e.what());emit changed();return false;}
    }
    void refresh();
    void project_form();
    void project_catalog();
    const CatalogIndex& catalog();
    DraftDocument* draft_=nullptr;
    std::uint64_t revision_=0,ticket_=0,staged_revision_=0;
    std::string scenario_,path_,search_;
    bool adding_=false;
    bool catalog_mode_=false;
    std::optional<WorldForm> form_;
    std::optional<CatalogIndex> catalog_index_;
    std::uint64_t catalog_revision_=UINT64_MAX;
    std::optional<RefactorPlan> rename_plan_;
    std::string identity_,family_,identity_path_,rename_target_;
    QVariantList catalog_entries_,catalog_templates_,reference_uses_,rename_changes_;
    std::map<std::string,std::map<std::string,std::string,std::less<>>,std::less<>> staged_;
    QVariantList scenarios_,sections_,fields_,templates_,states_;
    QVariantMap map_;
    QVariantMap unapplied_;
    QString message_,report_,selected_state_,map_credit_,map_hash_;
};
} // namespace pgp::studio
