#pragma once
#include <pgp/studio/world_forms.hpp>
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
private:
    template<class Action> bool perform(Action action) {
        try {message_.clear();action();return true;}
        catch(const std::exception& e) {message_=QString::fromUtf8(e.what());emit changed();return false;}
    }
    void refresh();
    void project_form();
    DraftDocument* draft_=nullptr;
    std::uint64_t revision_=0,ticket_=0,staged_revision_=0;
    std::string scenario_,path_,search_;
    bool adding_=false;
    std::optional<WorldForm> form_;
    std::map<std::string,std::map<std::string,std::string,std::less<>>,std::less<>> staged_;
    QVariantList scenarios_,sections_,fields_,templates_,states_;
    QVariantMap map_;
    QVariantMap unapplied_;
    QString message_,report_,selected_state_,map_credit_,map_hash_;
};
} // namespace pgp::studio
