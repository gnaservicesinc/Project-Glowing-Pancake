#include "probe_smoke.hpp"
#include "probe_controller.hpp"

#include <pgp/graphics/graphics_policy.hpp>

#include <QAccessible>
#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

void schedule_probe_smoke(QQuickWindow& window, ProbeController& controller,
                          QSGRendererInterface::GraphicsApi expected,
                          const QString& screenshot) {
    auto* timer = new QTimer(&window);
    timer->setInterval(100);
    auto attempts = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, &window, [&, timer, attempts, expected, screenshot] {
        ++*attempts;
        auto* png = window.findChild<QObject*>(QStringLiteral("localPng"));
        auto* jpeg = window.findChild<QObject*>(QStringLiteral("localJpeg"));
        if (!window.isSceneGraphInitialized() || !png || !jpeg ||
            png->property("status").toInt() != 1 || jpeg->property("status").toInt() != 1) {
            if (*attempts < 80) {
                return;
            }
            qCritical() << "Probe did not render both local image codecs before timeout";
            QCoreApplication::exit(3);
            timer->stop();
            return;
        }
        timer->stop();
        auto* button = window.findChild<QQuickItem*>(QStringLiteral("controlsButton"));
        auto* sound = window.findChild<QQuickItem*>(QStringLiteral("soundButton"));
        if (!button || !sound || window.rendererInterface()->graphicsApi() != expected) {
            qCritical() << "Probe objects or requested graphics backend mismatch";
            QCoreApplication::exit(3);
            return;
        }
        button->forceActiveFocus(Qt::TabFocusReason);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &press);
        QCoreApplication::sendEvent(&window, &release);
        auto* accessible = QAccessible::queryAccessibleInterface(button);
        if (controller.activations() != 1 || !accessible ||
            accessible->text(QAccessible::Name).isEmpty()) {
            qCritical() << "Keyboard activation or accessible button name failed";
            QCoreApplication::exit(3);
            return;
        }
        QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &tab);
        if (window.activeFocusItem() != sound) {
            qCritical() << "Keyboard tab order failed";
            QCoreApplication::exit(3);
            return;
        }
        // Let the view's focus-reveal scroll settle before checking visibility.
        QTimer::singleShot(100, &window, [&, sound, expected, screenshot] {
            const auto location = sound->mapToScene(QPointF{});
            if (location.y() < 0 || location.y() + sound->height() > window.height()) {
                qCritical() << "Keyboard focus is outside the visible viewport";
                QCoreApplication::exit(3);
                return;
            }
            QKeyEvent sound_press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
            QKeyEvent sound_release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &sound_press);
            QCoreApplication::sendEvent(&window, &sound_release);
            if (!controller.audio_ready()) {
                qCritical() << "Sound button did not load the bundled local fixture";
                QCoreApplication::exit(3);
                return;
            }
            if (!screenshot.isEmpty() && !window.grabWindow().save(screenshot)) {
                qCritical() << "Could not save explicit probe screenshot";
                QCoreApplication::exit(3);
                return;
            }
            qInfo().noquote() << "Probe passed: renderer=" + pgp::graphics::name(expected)
                              << "PNG/JPEG=ready keyboard=passed focus-visible=passed accessible-name=passed audio-button=passed";
            QCoreApplication::exit(0);
        });
    });
    timer->start();
}
