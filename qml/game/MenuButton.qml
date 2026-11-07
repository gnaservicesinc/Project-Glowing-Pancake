import QtQuick
import QtQuick.Controls

Button {
    id: control
    property int textSize: 22
    property string number: ""
    property bool primary: false
    implicitHeight: Math.max(48, textSize * 2.1)
    implicitWidth: 320
    hoverEnabled: true
    Accessible.name: text
    GameInk { id: ink }
    padding: 8
    leftPadding: 0
    contentItem: Row {
        spacing: 18
        Text {
            visible: control.number.length > 0
            width: visible ? 24 : 0
            anchors.verticalCenter: parent.verticalCenter
            text: control.number
            font.pixelSize: 11
            font.letterSpacing: 1
            color: control.hovered || control.activeFocus ? ink.accent : ink.muted
        }
        Text {
            text: control.text
            font.pixelSize: control.textSize
            font.weight: control.primary ? Font.DemiBold : Font.Normal
            textFormat: Text.PlainText
            color: !control.enabled ? ink.muted : control.hovered || control.activeFocus ? ink.accent : ink.paper
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "→"
            font.pixelSize: control.textSize
            anchors.verticalCenter: parent.verticalCenter
            color: ink.accent
            opacity: control.hovered || control.activeFocus ? 1 : 0
        }
    }
    background: Rectangle {
        color: control.down ? ink.stage : "transparent"
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Qt.alpha(ink.accent, control.activeFocus ? 1.0 : control.hovered ? 0.55 : 0.25)
        }
    }
}
