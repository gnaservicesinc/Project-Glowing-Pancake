import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Button {
    id: control
    property color accent: "#e5bc7e"
    property int textSize: 14
    property bool primary: false
    property string iconApp: ""
    TextMetrics { id: metrics; font: control.font; text: control.text }
    implicitWidth: Math.ceil(metrics.advanceWidth)+leftPadding+rightPadding+4+(iconApp?textSize*1.8:0)
    implicitHeight: Math.max(36,metrics.boundingRect.height+topPadding+bottomPadding)
    font.pixelSize: textSize
    padding: Math.max(12,font.pixelSize*0.85)
    activeFocusOnTab: true
    Accessible.name: text
    contentItem: RowLayout {
        spacing: control.iconApp ? 8 : 0
        DeviceGlyph { visible: !!control.iconApp; Layout.preferredWidth: control.textSize*1.3; Layout.preferredHeight: Layout.preferredWidth; app: control.iconApp; ink: control.primary ? "#24374a" : "#e4d2b1" }
        Text { Layout.fillWidth: true; text:control.text;textFormat:Text.PlainText;font:control.font;color:!control.enabled?"#829594":control.primary?"#1b2c3a":"#e4ebe3";wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter }
    }
    background: Rectangle {radius:8;color:control.primary?control.accent:control.down||control.checked?"#43546a":control.hovered?"#35475a":"#233548";border.color:control.activeFocus?"#f2c784":control.checked?control.accent:"#637180";border.width:control.activeFocus?2:1}
}
