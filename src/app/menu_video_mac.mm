#include "menu_media.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreVideo/CoreVideo.h>

void pump_menu_media_events_for_check() {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.001,false);
}

struct MenuVideo::Native {
    AVQueuePlayer* player = nil;
    AVPlayerLooper* looper = nil;
    AVPlayerItemVideoOutput* output = nil;
    bool failed = false;
};
MenuVideo::MenuVideo(QQuickItem* parent):QQuickPaintedItem(parent),native_(std::make_unique<Native>()) {
    setAntialiasing(false);
    setTextureSize(QSize{1280,720});
    timer_.setInterval(42); // The menu asset is 720p/24fps; no animation while hidden.
    connect(&timer_,&QTimer::timeout,this,&MenuVideo::pollFrame);
}
MenuVideo::~MenuVideo() {
    timer_.stop();
    [native_->player pause];
    [native_->looper disableLooping];
    [native_->player removeAllItems];
}
namespace {
QString menu_video_path() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/Menu/horizon.mp4");
}
}
bool MenuVideo::supported() const { return QFileInfo::exists(menu_video_path()); }
void MenuVideo::setActive(bool active) {
    if (active_==active) return;
    active_=active;
    if (active_ && !native_->failed) {
        if (!native_->player) {
            const auto path=QFileInfo{menu_video_path()}.canonicalFilePath();
            if (path.isEmpty()) {
                native_->failed=true; error_=tr("The background video is unavailable. Showing the still image.");
                emit frameReadyChanged(); emit activeChanged(); return;
            }
            @autoreleasepool {
                NSURL* url=[NSURL fileURLWithPath:path.toNSString()];
                AVPlayerItem* item=[AVPlayerItem playerItemWithURL:url];
                native_->player=[AVQueuePlayer queuePlayerWithItems:@[]];
                native_->player.muted=YES; // Soundtrack/effects have independent volume controls.
                native_->looper=[AVPlayerLooper playerLooperWithPlayer:native_->player templateItem:item];
            }
        }
        [native_->player play]; timer_.start();
    } else { [native_->player pause]; timer_.stop(); }
    emit activeChanged();
}
void MenuVideo::pollFrame() {
    @autoreleasepool {
        auto* item=native_->player.currentItem;
        if (item.status==AVPlayerItemStatusFailed || native_->looper.status==AVPlayerLooperStatusFailed) {
            timer_.stop(); [native_->player pause]; native_->failed=true;
            frame_={}; error_=tr("The background video could not be played. Showing the still image.");
            update(); emit frameReadyChanged(); return;
        }
        // Attach to each actual queue item; the looper's template is copied.
        AVPlayerItemVideoOutput* output=nil;
        for (AVPlayerItemOutput* candidate in item.outputs)
            if ([candidate isKindOfClass:[AVPlayerItemVideoOutput class]]) { output=(AVPlayerItemVideoOutput*)candidate; break; }
        if (!item) return;
        if (!output) {
            output=[[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:
                @{(id)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA)}];
            [item addOutput:output];
            native_->output=output;
        }
        const auto time=[output itemTimeForHostTime:CACurrentMediaTime()];
        if (![output hasNewPixelBufferForItemTime:time]) return;
        CVPixelBufferRef buffer=[output copyPixelBufferForItemTime:time itemTimeForDisplay:nullptr];
        if (!buffer) return;
        CVPixelBufferLockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);
        const auto width=CVPixelBufferGetWidth(buffer),height=CVPixelBufferGetHeight(buffer);
        if (width<=3840 && height<=2160) {
            QImage view{static_cast<const unsigned char*>(CVPixelBufferGetBaseAddress(buffer)),
                static_cast<int>(width),static_cast<int>(height),static_cast<qsizetype>(CVPixelBufferGetBytesPerRow(buffer)),QImage::Format_ARGB32};
            frame_=view.copy(); ++decoded_frames_;
        }
        CVPixelBufferUnlockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);
        CVPixelBufferRelease(buffer);
        update(); emit frameReadyChanged();
    }
}
