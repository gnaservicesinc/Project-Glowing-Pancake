#include "menu_media.hpp"
#include "campaign_controller.hpp"
#include <QFile>
#include <QPainter>

MenuAudio::MenuAudio(pgp::bridge::CampaignController& campaign,pgp::audio::DeviceMode mode)
    : campaign_(campaign), player_({},mode) {
    QFile file{QStringLiteral(":/pgp/assets/menu/select.wav")};
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto bytes=file.readAll();
    const auto result=player_.load_bundled_wave({reinterpret_cast<const unsigned char*>(bytes.constData()),
        static_cast<std::size_t>(bytes.size())});
    status_=result.ok ? tr("Ready") : tr("Sound effects are unavailable.");
}
void MenuAudio::select() {
    if (campaign_.effectsVolume()==0) { player_.stop(); return; }
    player_.set_volume(static_cast<float>(campaign_.effectsVolume())/100.0F);
    const auto result=player_.play();
    status_=result.ok ? tr("Playing") : tr("Sound effects are unavailable.");
    emit statusChanged();
}
void MenuVideo::paint(QPainter* painter) {
    if (frame_.isNull()) return;
    const auto scale=std::max(width()/frame_.width(),height()/frame_.height());
    const QSizeF size{frame_.width()*scale,frame_.height()*scale};
    painter->drawImage(QRectF{(width()-size.width())/2,(height()-size.height())/2,size.width(),size.height()},frame_);
}

#ifndef Q_OS_MACOS
void pump_menu_media_events_for_check() {}
struct MenuVideo::Native {};
MenuVideo::MenuVideo(QQuickItem* parent):QQuickPaintedItem(parent) {}
MenuVideo::~MenuVideo()=default;
bool MenuVideo::supported() const { return false; }
void MenuVideo::setActive(bool active) { if (active_!=active) { active_=active; emit activeChanged(); } }
void MenuVideo::pollFrame() {}
#endif
