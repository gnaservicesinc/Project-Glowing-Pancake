#include "portrait_images.hpp"
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <iostream>
#include <stdexcept>
#include <set>

namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
QString url(const pgp::core::PortraitRecipe& p) {
    QString result=QString::number(p.kit);for(auto id:p.parts) result+='/'+QString::number(id);for(auto id:p.style) result+='/'+QString::number(id);return result;
}
}
int main(int argc,char** argv) {
    QGuiApplication application{argc,argv};
    try {
        pgp::app::PortraitImages provider;
        QImage gallery{1440,1000,QImage::Format_ARGB32_Premultiplied};gallery.fill(QColor{"#173035"});QPainter painter{&gallery};
        std::set<QByteArray> distinct;
        for(unsigned i=0;i<24;++i) {
            auto recipe=pgp::core::make_portrait(718,"person/"+std::to_string(i));
            require(recipe==pgp::core::make_portrait(718,"person/"+std::to_string(i)),"Portrait generation must be stable.");
            const auto svg=provider.svg(recipe);require(!svg.contains('@'),"All vector palette tokens must resolve.");
            QSvgRenderer renderer{svg};require(renderer.isValid(),"Composed employee vector must be valid SVG.");
            const auto image=provider.requestImage(url(recipe),nullptr,{480,480});
            require(image.size()==QSize{480,480} && image.pixelColor(240,240).alpha()==255,"Portrait must rasterize at the requested resolution.");
            distinct.insert(svg);
            painter.drawImage(QRect{static_cast<int>(i%6)*240+20,static_cast<int>(i/6)*250+10,200,200},image);
            painter.setPen(QColor{"#f0e7d1"});painter.drawText(QRect{static_cast<int>(i%6)*240,static_cast<int>(i/6)*250+215,240,25},Qt::AlignCenter,QStringLiteral("Team member %1").arg(i+1));
        }
        require(distinct.size()==24,"Different identities need distinct portrait recipes.");
        for(std::size_t slot=0;slot<pgp::core::portrait_part_counts.size();++slot)
            for(std::uint8_t id=0;id<pgp::core::portrait_part_counts[slot];++id) {
                auto recipe=pgp::core::PortraitRecipe{};recipe.parts[slot]=id;
                require(QSvgRenderer{provider.svg(recipe)}.isValid(),"Every published layer must compose.");
            }
        const auto recipe=pgp::core::make_portrait(718,"pixel-density-check");
        const auto small=provider.requestImage(url(recipe),nullptr,{64,64});
        const auto large=provider.requestImage(url(recipe),nullptr,{1024,1024});
        require(large!=small.scaled(1024,1024,Qt::IgnoreAspectRatio,Qt::SmoothTransformation),"Large portraits must render from vectors instead of upscaling a thumbnail.");
        require(provider.requestImage(url(recipe),nullptr,{-1,-1}).width()>=240,"An unspecified image size must never fall back to a blurry 32px thumbnail.");
        const auto fallback=provider.requestImage("../../file.svg",nullptr,{64,64});
        require(!fallback.isNull(),"Malformed portrait URLs must render a local safe fallback.");
        {
            using pgp::core::LifeStage;
            auto baby_a=pgp::core::make_portrait(91,"alpha");
            auto baby_b=pgp::core::make_portrait(91,"beta");
            for(std::size_t slot=11;slot<15;++slot) baby_b.parts[slot]=baby_a.parts[slot];
            for(std::size_t slot=0;slot<baby_a.style.size();++slot) baby_b.style[slot]=baby_a.style[slot];
            baby_b.parts[0]=(baby_a.parts[0]+1)%pgp::core::portrait_part_counts[0];
            baby_b.parts[1]=(baby_a.parts[1]+2)%pgp::core::portrait_part_counts[1];
            baby_b.parts[9]=3;
            baby_b.parts[10]=2;
            require(provider.svg(baby_a,LifeStage::baby)==provider.svg(baby_b,LifeStage::baby),
                    "Babies should render uniformly regardless of head/hair/facial-hair/glasses parts.");
            require(provider.svg(baby_a,LifeStage::kid)!=provider.svg(baby_b,LifeStage::kid),
                    "Kids should preserve distinguishing genetic traits.");
            require(provider.svg(baby_a,LifeStage::adult)!=provider.svg(baby_b,LifeStage::adult),
                    "Adults should preserve distinguishing genetic traits.");
            for(const auto stage : {LifeStage::baby,LifeStage::toddler,LifeStage::kid,LifeStage::teen,
                                    LifeStage::adult,LifeStage::mature,LifeStage::senior,LifeStage::elder}) {
                const auto stage_svg=provider.svg(recipe,stage);
                require(!stage_svg.contains('@'),"All stage portrait palette tokens must resolve.");
                require(QSvgRenderer{stage_svg}.isValid(),"Every life-stage portrait must compose as valid SVG.");
                const auto key=pgp::core::life_stage_key(stage);
                const auto stage_img=provider.requestImage(
                    url(recipe)+'/'+QString::fromUtf8(key.data(),static_cast<qsizetype>(key.size())),
                    nullptr,{240,240});
                require(stage_img.size()==QSize{240,240} && stage_img.pixelColor(120,120).alpha()==255,
                        "29-token life-stage portrait URLs must rasterize cleanly.");
            }
            require(provider.svg(recipe,LifeStage::senior)!=provider.svg(recipe,LifeStage::adult),
                    "Senior portraits should show age and hair graying differences from adults.");
            require(provider.svg(recipe,LifeStage::elder)!=provider.svg(recipe,LifeStage::senior),
                    "Elder portraits should continue graying beyond seniors.");
            const auto adult_img=provider.requestImage(url(recipe)+"/25",nullptr,{240,240});
            const auto senior_img=provider.requestImage(url(recipe)+"/70",nullptr,{240,240});
            require(adult_img!=senior_img,"Numeric age suffixes in portrait URLs must switch rendered life stage.");
        }
        painter.end();
        if(argc>1) require(gallery.save(QString::fromLocal8Bit(argv[1])),"Could not save the portrait contact sheet.");
        std::cout<<"All portrait layers, stable recipes, native-size SVG rendering and safe URL fallback passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
