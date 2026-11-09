import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    property int textSize: 14
    font.pixelSize: textSize
    padding: 5
    spacing: 10
    Accessible.name: text
    indicator: Rectangle {
        implicitWidth: Math.max(22, control.font.pixelSize * 1.1)
        implicitHeight: implicitWidth
        x: control.leftPadding
        y: (control.height-height)/2
        radius: 5
        color: control.checked ? "#aad8c9" : control.hovered ? "#304f4d" : "#1b3538"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#f0c888" : "#68857b"
        Text {
            anchors.centerIn: parent
            text: control.checked ? "✓" : ""
            font.pixelSize: control.font.pixelSize
            font.bold: true
            color: "#172e30"
        }
    }
    contentItem: Text {
        leftPadding: control.indicator.width+control.spacing
        text: control.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        verticalAlignment: Text.AlignVCenter
        color: control.enabled ? "#e3e8d8" : "#839b92"
        font: control.font
    }
}
