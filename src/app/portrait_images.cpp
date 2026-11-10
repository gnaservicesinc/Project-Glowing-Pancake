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
    const int build=recipe.style[0]%5;const bool shaped=recipe.style[0]>=5;
    const double sx=build==1?0.88:build==2?1.12:build==3?1.04:build==4?0.96:1.0;
    const double sy=recipe.style[1]==1?0.93:recipe.style[1]==2?1.07:1.0;
    result+=QStringLiteral("<g transform=\"translate(120 240) scale(%1 %2) translate(-120 -240)\">").arg(sx).arg(sy);
    result+=layer("hairBack",1)+layer("neck",6);
    result+=QStringLiteral("<path d=\"M26 240 Q27 197 65 191 L99 177 Q120 195 141 177 L175 191 Q213 197 214 240Z\" fill=\"@shirt\"/><path d=\"M99 177 L120 194 L107 209 L85 185 M141 177 L120 194 L133 209 L155 185\" fill=\"none\" stroke=\"#fff9e9\" stroke-opacity=\".4\" stroke-width=\"2\"/>");
    const QStringList palette{"#ead4ae","#374f67","#bd8581","#80ac91","#b8a2ca","#e4dfd3"};
    const auto accent=palette[recipe.style[7]];
    if(recipe.style[3]) {
        // Keep every pattern stroke inside the shirt silhouette. QtSvg's
        // supported subset does not reliably clip arbitrary paths.
        result+=QStringLiteral("<g opacity=\".45\" stroke=\"%1\" fill=\"%1\" stroke-width=\"2\">").arg(accent);
        for(int n=0;n<5;++n) {
            const int y=202+n*8,left=51-n*5,right=240-left;
            if(recipe.style[3]==1 || recipe.style[3]==3) result+=QStringLiteral("<path d=\"M%1 %2 H%3\"/>").arg(left).arg(y).arg(right);
            if(recipe.style[3]==2) for(int x=left+7;x<right-4;x+=17) result+=QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"1.5\"/>").arg(x).arg(y);
        }
        if(recipe.style[3]==3) for(int x=52;x<=188;x+=17) result+=QStringLiteral("<path d=\"M%1 202 V239\"/>").arg(x);
        result+="</g>";
    }
    if(shaped) result+="<path d=\"M57 210 Q78 218 70 240 M183 210 Q162 218 170 240\" stroke=\"@ink\" stroke-opacity=\".25\" fill=\"none\"/>";
    if(recipe.style[2]==1) result+="<path d=\"M92 182 L120 220 L148 182\" fill=\"none\" stroke=\"@paper\" stroke-width=\"4\"/>";
    if(recipe.style[2]==2) result+="<path d=\"M80 184 L104 240 H45 M160 184 L136 240 H195\" fill=\"@jacket\"/><path d=\"M116 197 L124 197 L129 234 L120 240 L111 234Z\" fill=\"@tie\"/>";
    if(recipe.style[2]==3) result+="<path d=\"M79 186 Q120 226 161 186 M100 206 V230 M140 206 V230\" stroke=\"@paper\" stroke-width=\"3\" fill=\"none\"/>";
    result+=layer("ears",3)+layer("head",0);
    result+=QStringLiteral("<path d=\"M81 120 Q82 153 112 164 Q84 156 78 136Z\" fill=\"@shade\" opacity=\".14\"/>");
    result+=layer("eyes",7)+layer("brows",8).replace("@hair",kit_.value("hairColor").toArray()[recipe.style[8]].toString())+layer("nose",2)+layer("mouth",4)+layer("chin",5)+layer("facialHair",9).replace("@hair",kit_.value("hairColor").toArray()[recipe.style[9]].toString())+layer("hairFront",1)+layer("glasses",10).replace("@ink",palette[recipe.style[6]]);
    result+=QStringLiteral("</g></svg>");
    result.replace("@skin",layer("skin",11)).replace("@hair",layer("hairColor",12)).replace("@shirt",layer("shirtColor",13)).replace("@background",layer("background",14));
    result.replace("@jacket",kit_.value("hairColor").toArray()[recipe.style[10]].toString()).replace("@tie",kit_.value("hairColor").toArray()[recipe.style[11]].toString());
    result.replace("@ink","#293e40").replace("@shade","#804d3d").replace("@lip",QStringList{"#804d4b","#a86e65","#be8a7a","#804c68","#604848","#c18d89"}[recipe.style[5]]).replace("@paper","#fff7e5").replace("@eye",QStringList{"#64766a","#527c9c","#714e38","#443c38","#a4a177","#888e92","#548a78","#7b6590"}[recipe.style[4]]);
    return result.toUtf8();
}
QImage PortraitImages::requestImage(const QString& id,QSize* size,const QSize& requested) {
    auto recipe=core::PortraitRecipe{};
    const auto tokens=id.split('/');
    try {
        if(tokens.size()!=16 && tokens.size()!=28) throw std::invalid_argument("Portrait URL layer count");
        bool okay=false;const auto kit=tokens[0].toUInt(&okay);if(!okay || kit!=1) throw std::invalid_argument("Portrait kit");
        for(std::size_t i=0;i<recipe.parts.size();++i) {
            const auto part=tokens[static_cast<qsizetype>(i)+1].toUInt(&okay);
            if(!okay || part>=core::portrait_part_counts[i]) throw std::invalid_argument("Portrait part");
            recipe.parts[i]=static_cast<std::uint8_t>(part);
        }
        if(tokens.size()==28) for(std::size_t i=0;i<recipe.style.size();++i) {const auto part=tokens[static_cast<qsizetype>(i)+16].toUInt(&okay);if(!okay || part>=core::portrait_style_counts[i]) throw std::invalid_argument("Portrait styling");recipe.style[i]=static_cast<std::uint8_t>(part);}
    } catch(const std::invalid_argument&) {recipe={};}
    const auto edge=std::clamp(requested.isValid()?std::max(requested.width(),requested.height()):480,32,2048);
    QImage result{edge,edge,QImage::Format_ARGB32_Premultiplied};result.fill(Qt::transparent);
    QSvgRenderer renderer{svg(recipe)};
    QPainter painter{&result};painter.setRenderHint(QPainter::Antialiasing);renderer.render(&painter);painter.end();
    if(size) *size=QSize{240,240};
    return result;
}
}
