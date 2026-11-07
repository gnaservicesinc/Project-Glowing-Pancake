#include "music_controller.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace {
QByteArray local_resource(const QString& path, qsizetype limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > limit) return {};
    return file.readAll();
}
}

MusicController::MusicController(QObject* parent,pgp::audio::DeviceMode mode)
    : QObject(parent), player_({}, mode) {
    const auto manifest = QJsonDocument::fromJson(
        local_resource(QStringLiteral(":/pgp/assets/audio/CREDITS.json"), 64 * 1024)).object();
    const auto tracks = manifest.value(QStringLiteral("tracks")).toArray();
    const QRegularExpression path_pattern(QStringLiteral("^assets/audio/music/[a-z0-9_]+\\.wav$"));
    const QRegularExpression hash_pattern(QStringLiteral("^[0-9a-f]{64}$"));
    if (manifest.value(QStringLiteral("format")).toInt() == 1 && tracks.size() <= 16) {
        for (const auto& item : tracks) {
            const auto row = item.toObject();
            const auto role = row.value(QStringLiteral("role")).toString();
            if (!path_pattern.match(row.value(QStringLiteral("path")).toString()).hasMatch() ||
                !hash_pattern.match(row.value(QStringLiteral("sha256")).toString()).hasMatch() ||
                row.value(QStringLiteral("title")).toString().isEmpty() ||
                (role != QStringLiteral("ambient") && role != QStringLiteral("tension"))) continue;
            (role == QStringLiteral("ambient") ? ambient_ : tension_).push_back(static_cast<int>(credits_.size()));
            credits_.push_back(row.toVariantMap());
        }
    }
    status_ = tr("Music is off.");
    connect(&gain_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value) {player_.set_volume(value.toFloat());});
    connect(&gain_,&QVariantAnimation::finished,this,[this] {
        if(pending_context_ && enabled_) {pending_context_=false;playNext();rampVolume(target_volume_,1800);}
    });
    timer_.setInterval(350);
    connect(&timer_, &QTimer::timeout, this, [this] {
        const auto state = player_.status();
        if (state.device_lost) {
            timer_.stop();
            player_.stop();
            setStatus(tr("Audio output was interrupted. Music is silent."));
        } else if (!state.playing && !pending_context_) playNext();
    });
}

QString MusicController::backend() const { return QString::fromStdString(player_.backend_name()); }

void MusicController::rampVolume(float target,int duration) {
    gain_.stop();gain_.setStartValue(player_.status().volume);gain_.setEndValue(target);
    gain_.setDuration(duration);gain_.setEasingCurve(QEasingCurve::InOutSine);gain_.start();
}
void MusicController::setIntroduction(bool active) {
    if (active == introduction_) return;
    introduction_ = active;
    target_volume_ = static_cast<float>(volume_percent_) / 100.0F * (active ? 0.6F : 1.0F);
    tension_pending_ = urgent_ && !introduction_;
    if (enabled_) {
        pending_context_ = true;
        rampVolume(0, 900);
    }
}
void MusicController::configure(bool enabled, int volume_percent, bool urgent) {
    if (urgent && !urgent_ && !introduction_) tension_pending_ = true;
    if (!urgent || introduction_) tension_pending_ = false;
    urgent_ = urgent;
    volume_percent_ = volume_percent;
    const auto target=static_cast<float>(volume_percent_)/100.0F*(introduction_?0.6F:1.0F);
    const bool volume_changed=target_volume_!=target;target_volume_=target;
    if (enabled_ == enabled) {
        if(enabled_ && volume_changed && !pending_context_) rampVolume(target_volume_,1200);
        return;
    }
    gain_.stop();pending_context_=false;
    enabled_ = enabled;
    if (enabled_) {
        tension_pending_ = urgent_ && !introduction_;
        player_.set_volume(0);
        playNext();
        rampVolume(target_volume_,1800);
    } else {
        timer_.stop();
        player_.stop();
        setStatus(tr("Music is off."));
    }
}

void MusicController::playNext() {
    if(introduction_) {
        const auto id=next_intro_++%2==0?QStringLiteral("music.gedanklichkeit"):QStringLiteral("music.atemubungen");
        for(qsizetype i=0;i<credits_.size();++i) if(credits_.at(i).toMap().value("id").toString()==id) {playExcerpt(static_cast<int>(i));return;}
    }
    if (tension_pending_ && !tension_.empty()) {
        tension_pending_ = false;
        playExcerpt(tension_.at(next_tension_));
        next_tension_ = (next_tension_ + 1) % tension_.size();
    } else if (!ambient_.empty()) {
        playExcerpt(ambient_.at(next_ambient_));
        next_ambient_ = (next_ambient_ + 1) % ambient_.size();
    } else setStatus(tr("The local soundtrack is unavailable."));
}

void MusicController::playExcerpt(int index) {
    if (!enabled_ || index < 0 || index >= credits_.size()) return;
    const auto credit = credits_.at(index).toMap();
    const auto bytes = local_resource(QStringLiteral(":/pgp/") + credit.value(QStringLiteral("path")).toString(),
                                      4 * 1024 * 1024);
    const auto expected = credit.value(QStringLiteral("sha256")).toByteArray();
    const auto loaded = !bytes.isEmpty() &&
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() == expected &&
        player_.load_bundled_wave({reinterpret_cast<const unsigned char*>(bytes.constData()),
                                   static_cast<std::size_t>(bytes.size())}).ok;
    if (loaded && player_.play().ok && player_.status().playing) {
        timer_.start();
        setStatus(tr("Playing %1").arg(credit.value(QStringLiteral("title")).toString()));
    } else {
        timer_.stop();
        player_.stop();
        setStatus(loaded ? tr("Audio output is unavailable. Music is silent.")
                         : tr("The local excerpt is unavailable."));
    }
}

void MusicController::openTrackSource(int index) const {
    if (index < 0 || index >= credits_.size()) return;
    const auto url = QUrl(credits_.at(index).toMap().value(QStringLiteral("source_url")).toString());
    if (url.isValid() && url.scheme() == QStringLiteral("https") && url.host() == QStringLiteral("ende.app") &&
        url.port() == -1 && url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment() &&
        url.path().startsWith(QStringLiteral("/en/song/")))
        QDesktopServices::openUrl(url);
}

QString MusicController::licenseText(int index) const {
    static const QStringList paths{
        QStringLiteral("assets/audio/CC-BY-4.0.txt"),
        QStringLiteral("assets/maps/CENSUS_TERMS.txt"),
        QStringLiteral("LICENSE"),
        QStringLiteral("assets/notices/Qt-LGPL-3.0.txt"),
        QStringLiteral("third_party/json/LICENSE.MIT"),
        QStringLiteral("third_party/audio/miniaudio/LICENSE"),
        QStringLiteral("third_party/audio/stb/LICENSE")
    };
    if (index < 0 || index >= paths.size()) return {};
    const auto bytes = local_resource(QStringLiteral(":/pgp/notices/") + paths.at(index), 64 * 1024);
    return bytes.isEmpty() ? tr("The bundled notice could not be read.") : QString::fromUtf8(bytes);
}

void MusicController::setStatus(QString status) {
    if (status_ == status) return;
    status_ = std::move(status);
    emit statusChanged();
}
