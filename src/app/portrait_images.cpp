#include "portrait_images.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QSvgRenderer>
#include <algorithm>
#include <stdexcept>

static void initialize_portrait_resources() {Q_INIT_RESOURCE(portrait_kit);}
namespace pgp::app {
PortraitImages::PortraitImages():QQuickImageProvider(QQuickImageProvider::Image) {
    initialize_portrait_resources();
    QFile source{QStringLiteral(":/pgp/assets/portraits/kit-v1.json")};
    if(!source.open(QIODevice::ReadOnly) || source.size()>256*1024) throw std::runtime_error("The bundled portrait kit is missing or oversized.");
    kit_=QJsonDocument::fromJson(source.readAll()).object();
    constexpr std::array names{"head","hairBack","nose","ears","mouth","chin","neck","eyes","brows","facialHair","glasses","skin","hairColor","shirtColor","background"};
    if(kit_.value("kit").toInt()!=1) throw std::runtime_error("Unsupported bundled portrait kit.");
    for(std::size_t i=0;i<names.size();++i) if(kit_.value(names[i]).toArray().size()!=core::portrait_part_counts[i])
        throw std::runtime_error("Bundled portrait layer IDs do not match their saved contract.");
    if(kit_.value("hairFront").toArray().size()!=core::portrait_part_counts[1]) throw std::runtime_error("Portrait hair pairs do not match.");
}
QByteArray PortraitImages::svg(const core::PortraitRecipe& recipe) const {
    core::validate_portrait(recipe);
    const auto layer=[&](const char* name,std::size_t part){return kit_.value(name).toArray()[recipe.parts[part]].toString();};
    QString result=QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 240 240\"><rect width=\"240\" height=\"240\" rx=\"28\" fill=\"@background\"/><circle cx=\"120\" cy=\"108\" r=\"92\" fill=\"#fff9e9\" opacity=\".18\"/>");
    result+=layer("hairBack",1)+layer("neck",6);
    result+=QStringLiteral("<path d=\"M26 240 Q27 197 65 191 L99 177 Q120 195 141 177 L175 191 Q213 197 214 240Z\" fill=\"@shirt\"/><path d=\"M99 177 L120 194 L107 209 L85 185 M141 177 L120 194 L133 209 L155 185\" fill=\"none\" stroke=\"#fff9e9\" stroke-opacity=\".4\" stroke-width=\"2\"/>");
    result+=layer("ears",3)+layer("head",0);
    result+=QStringLiteral("<path d=\"M81 120 Q82 153 112 164 Q84 156 78 136Z\" fill=\"@shade\" opacity=\".14\"/>");
    result+=layer("eyes",7)+layer("brows",8)+layer("nose",2)+layer("mouth",4)+layer("chin",5)+layer("facialHair",9)+layer("hairFront",1)+layer("glasses",10);
    result+=QStringLiteral("</svg>");
    result.replace("@skin",layer("skin",11)).replace("@hair",layer("hairColor",12)).replace("@shirt",layer("shirtColor",13)).replace("@background",layer("background",14));
    result.replace("@ink","#293e40").replace("@shade","#804d3d").replace("@lip","#804d4b").replace("@paper","#fff7e5").replace("@eye","#64766a");
    return result.toUtf8();
}
QImage PortraitImages::requestImage(const QString& id,QSize* size,const QSize& requested) {
    auto recipe=core::PortraitRecipe{};
    const auto tokens=id.split('/');
    try {
        if(tokens.size()!=16) throw std::invalid_argument("Portrait URL layer count");
        bool okay=false;const auto kit=tokens[0].toUInt(&okay);if(!okay || kit!=1) throw std::invalid_argument("Portrait kit");
        for(std::size_t i=0;i<recipe.parts.size();++i) {
            const auto part=tokens[static_cast<qsizetype>(i)+1].toUInt(&okay);
            if(!okay || part>=core::portrait_part_counts[i]) throw std::invalid_argument("Portrait part");
            recipe.parts[i]=static_cast<std::uint8_t>(part);
        }
    } catch(const std::invalid_argument&) {recipe={};}
    const auto edge=std::clamp(std::max(requested.width(),requested.height()),32,2048);
    QImage result{edge,edge,QImage::Format_ARGB32_Premultiplied};result.fill(Qt::transparent);
    QSvgRenderer renderer{svg(recipe)};
    QPainter painter{&result};painter.setRenderHint(QPainter::Antialiasing);renderer.render(&painter);painter.end();
    if(size) *size=QSize{240,240};
    return result;
}
}
