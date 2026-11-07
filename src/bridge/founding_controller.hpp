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
    Q_PROPERTY(QVariantList offices READ offices NOTIFY changed)
    Q_PROPERTY(QVariantList researchers READ researchers NOTIFY changed)
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY changed)
    Q_PROPERTY(QString preview READ preview NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    FoundingController(const content::Collection&,std::filesystem::path,QObject* parent=nullptr);
    bool available() const {return catalog_.has_value();}
    QVariantMap form() const {return form_;}
    QVariantList chapters() const;
    QVariantList families() const;
    QVariantList legs() const;
    QVariantList routes() const;
    QVariantList backers() const;
    QVariantList offices() const;
    QVariantList researchers() const;
    int page() const {return page_;}
    void setPage(int);
    QString preview() const;
    QString error() const {return error_;}
    Q_INVOKABLE void setField(QString,QVariant);
    Q_INVOKABLE void choose(QString,QString);
    Q_INVOKABLE void back(QString,bool);
    Q_INVOKABLE void reset();
    [[nodiscard]] content::FoundingSelection selection() const;
signals:
    void changed();
private:
    QVariantList options(const std::vector<content::FoundingOption>&) const;
    void saveDraft();
    std::optional<content::FoundingCatalog> catalog_;
    std::vector<content::CampaignDifficulty> difficulties_;
    std::filesystem::path draft_path_;
    QVariantMap form_;
    int page_=0;
    QString error_;
};
}
