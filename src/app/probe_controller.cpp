#include "probe_controller.hpp"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <filesystem>

ProbeController::ProbeController(QObject* parent) : QObject(parent) {}

QString ProbeController::controls_message() const {
    return activations_ == 0 ? QStringLiteral("Try the button with a click or Space.")
                             : QStringLiteral("Controls are responding. You are ready to continue.");
}

void ProbeController::checkControls() {
    ++activations_;
    emit controlsChanged();
}

void ProbeController::set_volume(qreal value) {
    if (!std::isfinite(value)) {
        return;
    }
    volume_ = std::clamp(value, 0.0, 1.0);
    if (player_) {
        player_->set_volume(static_cast<float>(volume_));
    }
    emit volumeChanged();
}

void ProbeController::set_muted(bool value) {
    muted_ = value;
    if (player_) {
        player_->set_muted(value);
    }
    emit mutedChanged();
}

void ProbeController::checkSound() {
    if (!player_) {
        audio_directory_ = std::make_unique<QTemporaryDir>();
        QFile source(QStringLiteral(":/pgp/assets/probe/audio/tone.wav"));
        QFile output(audio_directory_->filePath(QStringLiteral("tone.wav")));
        if (!audio_directory_->isValid() || !source.open(QIODevice::ReadOnly) ||
            !output.open(QIODevice::WriteOnly)) {
            audio_message_ = QStringLiteral("Sound is unavailable. You can continue in silence.");
            emit audioChanged();
            return;
        }
        const auto bytes = source.readAll();
        if (output.write(bytes) != bytes.size() || !output.flush()) {
            audio_message_ = QStringLiteral("Sound is unavailable. You can continue in silence.");
            emit audioChanged();
            return;
        }
        output.close();
#if PGP_ENABLE_AUDIO
        constexpr auto mode = pgp::audio::DeviceMode::NativeIfAvailable;
#else
        constexpr auto mode = pgp::audio::DeviceMode::Silent;
#endif
        player_ = std::make_unique<pgp::audio::Player>(
            std::filesystem::path{audio_directory_->path().toStdU16String()}, mode);
    }
    const auto loaded = player_->load("tone.wav");
    player_->set_volume(static_cast<float>(volume_));
    player_->set_muted(muted_);
    const auto result = loaded.ok ? player_->play() : loaded;
    if (!result.ok || player_->backend_name().starts_with("silent")) {
        audio_message_ = QStringLiteral("Sound is off. All controls remain available.");
    } else {
        audio_message_ = muted_ ? QStringLiteral("Sound check is muted.")
                                 : QStringLiteral("A short tone is playing. Sound is working.");
    }
    emit audioChanged();
}
