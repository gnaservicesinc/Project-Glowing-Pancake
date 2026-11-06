#pragma once

#include <pgp/platform/audio.hpp>

#include <QObject>
#include <QTimer>
#include <QVariantList>

class MusicController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList credits READ credits CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit MusicController(QObject* parent = nullptr);
    [[nodiscard]] QVariantList credits() const { return credits_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool playing() const { return player_.status().playing; }
    [[nodiscard]] QString backend() const;
    void configure(bool enabled, int volume_percent, bool urgent);
    Q_INVOKABLE void playExcerpt(int index);
    Q_INVOKABLE void openTrackSource(int index) const;
    Q_INVOKABLE QString licenseText(int index) const;
signals:
    void statusChanged();
private:
    void playNext();
    void setStatus(QString status);
    pgp::audio::Player player_;
    QTimer timer_;
    QVariantList credits_;
    QList<int> ambient_, tension_;
    qsizetype next_ambient_ = 0, next_tension_ = 0;
    QString status_;
    bool enabled_ = false, urgent_ = false, tension_pending_ = false;
};
