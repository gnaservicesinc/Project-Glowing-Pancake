#pragma once

#include <pgp/studio/revision_review.hpp>

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace pgp::studio {
class RevisionController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList revisions READ revisions NOTIFY changed)
    Q_PROPERTY(QString comparison READ comparison NOTIFY changed)
    Q_PROPERTY(QStringList sections READ sections NOTIFY changed)
    Q_PROPERTY(QVariantList differences READ differences NOTIFY changed)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY changed)
    Q_PROPERTY(QStringList beforeScenarios READ beforeScenarios NOTIFY changed)
    Q_PROPERTY(QStringList afterScenarios READ afterScenarios NOTIFY changed)
    Q_PROPERTY(QString beforeTrace READ beforeTrace NOTIFY changed)
    Q_PROPERTY(QString afterTrace READ afterTrace NOTIFY changed)
    Q_PROPERTY(QVariantList beforeChoices READ beforeChoices NOTIFY changed)
    Q_PROPERTY(QVariantList afterChoices READ afterChoices NOTIFY changed)
    Q_PROPERTY(bool fixtureReady READ fixtureReady NOTIFY changed)
    Q_PROPERTY(bool canMigrate READ canMigrate NOTIFY changed)
    Q_PROPERTY(QVariantList mappings READ mappings NOTIFY changed)
    Q_PROPERTY(QString planId READ planId NOTIFY changed)
    Q_PROPERTY(QString migrationReport READ migrationReport NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString exportedPath READ exportedPath NOTIFY changed)
public:
    [[nodiscard]] QStringList revisions() const { return revisions_; }
    [[nodiscard]] QString comparison() const { return comparison_; }
    [[nodiscard]] QStringList sections() const { return sections_; }
    [[nodiscard]] QVariantList differences() const { return differences_; }
    [[nodiscard]] int pageCount() const { return pages_; }
    [[nodiscard]] QStringList beforeScenarios() const;
    [[nodiscard]] QStringList afterScenarios() const;
    [[nodiscard]] QString beforeTrace() const { return before_trace_; }
    [[nodiscard]] QString afterTrace() const { return after_trace_; }
    [[nodiscard]] QVariantList beforeChoices() const { return before_choices_; }
    [[nodiscard]] QVariantList afterChoices() const { return after_choices_; }
    [[nodiscard]] bool fixtureReady() const { return fixture_.has_value(); }
    [[nodiscard]] bool canMigrate() const;
    [[nodiscard]] QVariantList mappings() const;
    [[nodiscard]] QString planId() const { return plan_id_; }
    [[nodiscard]] QString migrationReport() const { return migration_report_; }
    [[nodiscard]] QString message() const { return message_; }
    [[nodiscard]] QString exportedPath() const { return exported_path_; }

    Q_INVOKABLE bool refresh(const QString& checkout);
    Q_INVOKABLE bool compare(int beforeIndex, int afterIndex);
    Q_INVOKABLE void filter(const QString& section, int page);
    Q_INVOKABLE QStringList headquarters(const QString& before, const QString& after) const;
    Q_INVOKABLE bool startFixture(const QString& before, const QString& after, const QString& hq, const QString& seed);
    Q_INVOKABLE bool advanceFixture();
    Q_INVOKABLE bool chooseFixture(int beforeChoice, int afterChoice);
    Q_INVOKABLE QStringList mappingIds(int kind, const QString& owner, bool destination) const;
    Q_INVOKABLE bool addMapping(int kind, const QString& owner, const QString& from, const QString& to);
    Q_INVOKABLE void removeMapping(int index);
    Q_INVOKABLE void clearMigration();
    Q_INVOKABLE bool reviewMigration(const QString& source, const QString& summary);
    Q_INVOKABLE bool exportCopy(const QString& plan, const QString& name);

signals:
    void changed();

private:
    template<class Action> bool perform(Action action);
    void reset_comparison();
    void reset_migration();
    void show_steps(const MatchedSteps& steps);

    std::optional<RevisionArchive> archive_;
    std::optional<FinalizedRevision> before_, after_;
    std::optional<MatchedFixture> fixture_;
    std::vector<RevisionDifference> changes_;
    std::vector<IdentityMapping> maps_;
    std::optional<SaveMigrationRequest> request_;
    QStringList revisions_, sections_;
    QVariantList differences_, before_choices_, after_choices_;
    QString comparison_, before_trace_, after_trace_, plan_id_, migration_report_, message_, exported_path_;
    int pages_ = 0;
};
} // namespace pgp::studio
