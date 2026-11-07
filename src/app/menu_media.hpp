#pragma once

#include <pgp/platform/audio.hpp>
#include <QImage>
#include <QObject>
#include <QQuickPaintedItem>
#include <QTimer>
#include <memory>

namespace pgp::bridge { class CampaignController; }
// The offscreen Qt test platform has no Cocoa dispatcher for AVFoundation's
// main-loop callbacks. Used only by the explicit quiet menu check.
void pump_menu_media_events_for_check();

class MenuAudio final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    explicit MenuAudio(pgp::bridge::CampaignController& campaign,
        pgp::audio::DeviceMode mode = pgp::audio::DeviceMode::NativeIfAvailable);
    Q_INVOKABLE void select();
    QString status() const { return status_; }
signals:
    void statusChanged();
private:
    pgp::bridge::CampaignController& campaign_;
    pgp::audio::Player player_;
    QString status_;
};

// One local, silent video layer. QML supplies no URLs and the decoder has no
// remote-input path. A still image stays visible until a decoded frame arrives.
class MenuVideo : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool frameReady READ frameReady NOTIFY frameReadyChanged)
    Q_PROPERTY(int decodedFrames READ decodedFrames NOTIFY frameReadyChanged)
    Q_PROPERTY(QString error READ error NOTIFY frameReadyChanged)
public:
    explicit MenuVideo(QQuickItem* parent = nullptr);
    ~MenuVideo() override;
    void paint(QPainter* painter) override;
    bool active() const { return active_; }
    void setActive(bool active);
    bool supported() const;
    bool frameReady() const { return !frame_.isNull(); }
    int decodedFrames() const { return decoded_frames_; }
    QString error() const { return error_; }
signals:
    void activeChanged();
    void frameReadyChanged();
private:
    void pollFrame();
    struct Native;
    std::unique_ptr<Native> native_;
    QTimer timer_;
    QImage frame_;
    QString error_;
    bool active_ = false;
    int decoded_frames_ = 0;
};
