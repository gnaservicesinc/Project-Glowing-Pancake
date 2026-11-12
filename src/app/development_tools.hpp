#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace pgp::app {
class DevelopmentTools final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString checkout READ checkout NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    explicit DevelopmentTools(QString settings_path,QStringList launch_arguments={});
    [[nodiscard]] QString checkout() const;
    [[nodiscard]] QString message() const { return message_; }
    Q_INVOKABLE bool selectCheckout(const QUrl& folder);
    Q_INVOKABLE bool openContentStudio();
    Q_INVOKABLE bool openGodMode();

signals:
    void changed();

private:
    void setMessage(QString message);
    QString settings_path_, remembered_checkout_, message_;
    QStringList launch_arguments_;
};
}
