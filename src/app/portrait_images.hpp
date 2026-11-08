#pragma once
#include <pgp/core/portrait.hpp>
#include <QJsonObject>
#include <QQuickImageProvider>

namespace pgp::app {
// Trusted bundled vector fragments; only validated numeric recipes reach SVG.
class PortraitImages final : public QQuickImageProvider {
public:
    PortraitImages();
    QImage requestImage(const QString&,QSize*,const QSize&) override;
    QByteArray svg(const core::PortraitRecipe&) const;
private:
    QJsonObject kit_;
};
}
