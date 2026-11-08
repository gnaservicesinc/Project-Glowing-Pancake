import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: card
    required property Item host
    property string heading: ""
    property string detail: ""
    property point pointer: Qt.point(0,0)
    property int textSize: 14
    readonly property real padding: textSize*0.65
    parent: host && host.Window.window ? host.Window.window.contentItem : null
    enabled: false
    z: 10000
    x: parent ? Math.max(16,Math.min(pointer.x+textSize,parent.width-width-16)) : 0
    y: parent ? (pointer.y+textSize+height<=parent.height-16 ? pointer.y+textSize : Math.max(16,pointer.y-height-textSize)) : 0
    width: parent ? Math.min(parent.width-32,textSize*32) : 0
    height: copy.implicitHeight+2*padding
    Rectangle {anchors.fill:parent;color:"#152b31";radius:card.textSize*0.4;border.color:"#708d85";border.width:Math.max(1,card.textSize/14)}
    ColumnLayout {
        id: copy
        x: card.padding;y: card.padding
        width: card.width-2*card.padding
        spacing: card.textSize*0.3
        Label {Layout.fillWidth:true;text:card.heading;textFormat:Text.PlainText;font.pixelSize:card.textSize;font.bold:true;wrapMode:Text.Wrap;color:"#f2ead6"}
        Label {Layout.fillWidth:true;text:card.detail;textFormat:Text.PlainText;font.pixelSize:card.textSize*0.85;wrapMode:Text.Wrap;color:"#d1e3db"}
    }
}
