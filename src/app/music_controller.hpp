#pragma once

#include <pgp/platform/audio.hpp>

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantAnimation>

class MusicController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList credits READ credits CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit MusicController(QObject* parent = nullptr,
        pgp::audio::DeviceMode mode = pgp::audio::DeviceMode::NativeIfAvailable);
    [[nodiscard]] QVariantList credits() const { return credits_; }
    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] bool playing() const { return player_.status().playing; }
    [[nodiscard]] bool loaded() const { return player_.status().loaded; }
    [[nodiscard]] QString backend() const;
    void configure(bool enabled, int volume_percent, bool urgent, bool introduction = false);
    Q_INVOKABLE void playExcerpt(int index);
    Q_INVOKABLE void openTrackSource(int index) const;
    Q_INVOKABLE QString licenseText(int index) const;
signals:
    void statusChanged();
private:
    void playNext();
    void setStatus(QString status);
    void rampVolume(float target, int duration);
    pgp::audio::Player player_;
    QTimer timer_;
    QVariantAnimation gain_;
    QVariantList credits_;
    QList<int> ambient_, tension_;
    qsizetype next_ambient_ = 0, next_tension_ = 0;
    QString status_;
    bool enabled_ = false, urgent_ = false, tension_pending_ = false;
    bool introduction_ = false, pending_context_ = false;
    qsizetype next_intro_ = 0;
    float target_volume_ = 0.4F;
};
