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
            color: control.hovered || control.activeFocus ? "#f3cb8d" : "#8c969d"
        }
        Text {
            text: control.text
            font.pixelSize: control.textSize
            font.weight: control.primary ? Font.DemiBold : Font.Normal
            textFormat: Text.PlainText
            color: !control.enabled ? "#899198" : control.hovered || control.activeFocus ? "#f3cb8d" : "#f4eee4"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "→"
            font.pixelSize: control.textSize
            anchors.verticalCenter: parent.verticalCenter
            color: "#f3cb8d"
            opacity: control.hovered || control.activeFocus ? 1 : 0
        }
    }
    background: Rectangle {
        color: control.down ? "#24313a" : "transparent"
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: control.activeFocus ? "#f3cb8d" : control.hovered ? "#66776f" : "#30404a"
        }
    }
}
