import QtQuick
import "EquipmentArt.js" as Art
Canvas {
    id:glyph
    property string kind:"desk"
    property bool powered:true
    implicitWidth:80;implicitHeight:80
    onKindChanged:requestPaint()
    onPoweredChanged:requestPaint()
    onWidthChanged:requestPaint()
    onHeightChanged:requestPaint()
    onPaint:{const c=getContext("2d");c.reset();Art.draw(c,kind,0,0,width,height,powered)}
    Accessible.role:Accessible.Graphic
    Accessible.name:kind
}
