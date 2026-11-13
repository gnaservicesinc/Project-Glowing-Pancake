import QtQuick
import QtQuick.Controls

SpinBox {
    id: control
    property int textSize: 14
    GameInk { id: ink }
    font.pixelSize: textSize
    editable: true
    implicitHeight: Math.max(40, font.pixelSize * 2.6)
    implicitWidth: Math.max(132, font.pixelSize * 8)
    leftPadding: (down.indicator ? down.indicator.width : 0) + 4
    rightPadding: (up.indicator ? up.indicator.width : 0) + 4

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        padding: 6
        font: control.font
        color: control.enabled ? "#f0e8d3" : "#7b9690"
        selectionColor: "#486d89"
        selectedTextColor: "#fff6d8"
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 1 : control.width - width - 1
        y: 1
        height: control.height - 2
        implicitWidth: Math.max(34, control.font.pixelSize * 2.1)
        implicitHeight: Math.max(38, control.font.pixelSize * 2.4)
        radius: 6
        color: control.up.pressed ? "#385268" : control.up.hovered ? ink.hover : ink.control
        border.width: 0
        Text {
            anchors.centerIn: parent
            text: "+"
            font.pixelSize: control.font.pixelSize * 1.15
            font.bold: true
            color: control.enabled ? "#bed7c7" : "#6c8581"
            Accessible.ignored: true
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? control.width - width - 1 : 1
        y: 1
        height: control.height - 2
        implicitWidth: Math.max(34, control.font.pixelSize * 2.1)
        implicitHeight: Math.max(38, control.font.pixelSize * 2.4)
        radius: 6
        color: control.down.pressed ? "#385268" : control.down.hovered ? ink.hover : ink.control
        border.width: 0
        Text {
            anchors.centerIn: parent
            text: "−"
            font.pixelSize: control.font.pixelSize * 1.15
            font.bold: true
            color: control.enabled ? "#bed7c7" : "#6c8581"
            Accessible.ignored: true
        }
    }

    background: Rectangle {
        radius: 7
        color: control.activeFocus ? "#2c4359" : "#1c2f42"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? ink.focus : ink.border
    }
}
