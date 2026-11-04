#pragma once

#include <pgp/platform/audio.hpp>

#include <QObject>
#include <QTemporaryDir>

#include <memory>

// Application adapter only. No simulation state or business rules live here.
class ProbeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int activations READ activations NOTIFY controlsChanged)
    Q_PROPERTY(QString controlsMessage READ controls_message NOTIFY controlsChanged)
    Q_PROPERTY(QString audioMessage READ audio_message NOTIFY audioChanged)
    Q_PROPERTY(qreal volume READ volume WRITE set_volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE set_muted NOTIFY mutedChanged)
public:
    explicit ProbeController(QObject* parent = nullptr);
    [[nodiscard]] int activations() const { return activations_; }
    [[nodiscard]] QString controls_message() const;
    [[nodiscard]] QString audio_message() const { return audio_message_; }
    [[nodiscard]] qreal volume() const { return volume_; }
    [[nodiscard]] bool muted() const { return muted_; }
    [[nodiscard]] bool audio_ready() const { return player_ && player_->status().loaded; }
    void set_volume(qreal value);
    void set_muted(bool value);
    Q_INVOKABLE void checkControls();
    Q_INVOKABLE void checkSound();
signals:
    void controlsChanged();
    void audioChanged();
    void volumeChanged();
    void mutedChanged();
private:
    int activations_{};
    qreal volume_{0.35};
    bool muted_{};
    QString audio_message_{QStringLiteral("Sound is optional. Nothing plays until you ask.")};
    std::unique_ptr<QTemporaryDir> audio_directory_;
    std::unique_ptr<pgp::audio::Player> player_;
};
