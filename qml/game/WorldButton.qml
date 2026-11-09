import QtQuick
import QtQuick.Controls
Button {
    id: control
    property color accent: "#aad8c9"
    property int textSize: 14
    property bool primary: false
    font.pixelSize: textSize
    padding: Math.max(12,font.pixelSize*0.85)
    activeFocusOnTab: true
    Accessible.name: text
    contentItem: Text {text:control.text;textFormat:Text.PlainText;font:control.font;color:!control.enabled?"#829594":control.primary?"#122b2e":"#e4ebe3";wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
    background: Rectangle {radius:8;color:control.primary?control.accent:control.down||control.checked?"#365553":control.hovered?"#2b4447":"#1d3338";border.color:control.activeFocus?"#f2c784":control.checked?control.accent:"#49605d";border.width:control.activeFocus?2:1}
}
