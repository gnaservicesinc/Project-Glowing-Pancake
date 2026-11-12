import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Button {
    id: control
    property color accent: ink.accent
    property int textSize: 14
    property bool primary: false
    property bool quiet: false
    property string hint: ""
    hoverEnabled: true
    ToolTip.visible: hovered && hint.length > 0
    ToolTip.delay: 600
    ToolTip.text: hint
    GameInk { id: ink }
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
    background: Rectangle {
        radius: 7
        color: !control.enabled ? "#172431" : control.down ? "#496074" : control.primary ? (control.hovered ? Qt.lighter(control.accent,1.12) : control.accent) : control.checked ? "#384c5c" : control.hovered ? ink.hover : control.quiet ? "transparent" : ink.control
        border.color: control.activeFocus ? ink.paper : control.checked ? control.accent : control.hovered ? "#80929b" : control.quiet ? "transparent" : ink.border
        border.width: control.activeFocus ? 2 : 1
        Behavior on color { ColorAnimation { duration: typeof campaign !== "undefined" && campaign && campaign.reducedMotion ? 0 : 100 } }
        Rectangle { anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:1; height:1; radius:1; color:"#22ffffff"; visible: !control.quiet && !control.down }
    }
}
