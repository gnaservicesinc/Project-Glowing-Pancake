#pragma once

#include <pgp/studio/assets.hpp>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

namespace pgp::studio {
class AssetController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    QVariantList entries() const;
    QString message() const { return message_; }
    Q_INVOKABLE bool openRegister(QString checkout);
    Q_INVOKABLE QVariantMap record(QString id) const;
    Q_INVOKABLE bool inspect(QString id);
    Q_INVOKABLE bool save(QVariantMap fields, QString importPath);
signals:
    void changed();
private:
    std::optional<AssetRegister> register_;
    QString message_;
};
} // namespace pgp::studio
