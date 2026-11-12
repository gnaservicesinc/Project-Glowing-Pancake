#pragma once
#include <pgp/content/collection.hpp>
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <filesystem>

namespace pgp::bridge {
class FoundingController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QVariantMap form READ form NOTIFY changed)
    Q_PROPERTY(QVariantList chapters READ chapters CONSTANT)
    Q_PROPERTY(QVariantList families READ families NOTIFY changed)
    Q_PROPERTY(QVariantList legs READ legs NOTIFY changed)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY changed)
    Q_PROPERTY(QVariantList backers READ backers NOTIFY changed)
    Q_PROPERTY(QVariantList homeOffers READ homeOffers NOTIFY changed)
    Q_PROPERTY(QVariantList headquarters READ headquarters CONSTANT)
    Q_PROPERTY(QString beat READ beat WRITE setBeat NOTIFY beatChanged)
    Q_PROPERTY(bool hasDraft READ hasDraft NOTIFY beatChanged)
    Q_PROPERTY(QVariantMap personalFunds READ personalFunds NOTIFY changed)
    Q_PROPERTY(QVariantMap familyHome READ familyHome NOTIFY changed)
    Q_PROPERTY(QVariantList charters READ charters CONSTANT)
    Q_PROPERTY(QVariantMap startupLoan READ startupLoan CONSTANT)
    Q_PROPERTY(QString preview READ preview NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString portrait READ portrait NOTIFY changed)
    Q_PROPERTY(QStringList pronounOptions READ pronounOptions CONSTANT)
    Q_PROPERTY(QVariantList appearanceParts READ appearanceParts CONSTANT)
public:
    FoundingController(const content::Collection&,std::filesystem::path,QObject* parent=nullptr);
    bool available() const {return catalog_.has_value();}
    QVariantMap form() const {return form_;}
    QVariantList chapters() const;
    QVariantList families() const;
    QVariantList legs() const;
    QVariantList routes() const;
    QVariantList backers() const;
    QVariantList homeOffers() const;
    QVariantList headquarters() const;
    QString beat() const { return beat_; }
    bool hasDraft() const { return available() && beat_ != QStringLiteral("opening"); }
    void setBeat(QString);
    QString preview() const;
    QVariantMap personalFunds() const;
    QVariantMap familyHome() const;
    QVariantList charters() const;
    QVariantMap startupLoan() const;
    QString error() const {return error_;}
    Q_INVOKABLE QString signingError() const;
    Q_INVOKABLE QString genderAudience(QString gender) const;
    Q_INVOKABLE QString communitySupport(QString gender) const;
    Q_INVOKABLE QString orientationAudience(QString orientation) const;
    Q_INVOKABLE QString recognitionTerms() const;
    Q_INVOKABLE void setField(QString,QVariant);
    Q_INVOKABLE QString answerText(QString field, QString value);
    Q_INVOKABLE void choose(QString,QString);
    Q_INVOKABLE QString answerOffer(QString id, bool accept);
    Q_INVOKABLE QString contactHomeOffer(QString id);
    Q_INVOKABLE QString respondHomeOffer(QString id, QString response);
    Q_INVOKABLE QString closeHomeOffer();
    Q_INVOKABLE void cycleAppearance(int part,int direction);
    Q_INVOKABLE void setAppearance(int part,int value);
    QString portrait() const;
    QVariantList appearanceParts() const;
    QStringList pronounOptions() const;
    void reset(bool save = true);
    [[nodiscard]] content::FoundingSelection selection() const;
    [[nodiscard]] std::vector<core::PeopleContact> openingContacts() const;
    static QVariantMap presentOffer(const content::FoundingCatalog&,content::FoundingSelection,
                                    const QString& id,const QString& source,const QString& status);
signals:
    void changed();
    void beatChanged();
private:
    QVariantList options(const std::vector<content::FoundingOption>&) const;
    QVariantMap homeOffer(const QString& id) const;
    QString commitHomeDraft(QVariantMap next);
    void saveDraft();
    void deriveOpening();
    std::optional<content::FoundingCatalog> catalog_;
    std::vector<std::string> headquarters_;
    std::optional<core::HeadquartersRules> premises_rules_;
    std::vector<core::OrganizationalCharter> charters_;
    std::vector<core::FounderAudienceRule> audiences_;
    core::OrdinalDay starts_on_{0};
    std::filesystem::path draft_path_;
    QVariantMap form_;
    QString beat_ = QStringLiteral("opening");
    QString error_;
};
}
