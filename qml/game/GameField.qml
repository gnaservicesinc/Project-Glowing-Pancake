import QtQuick
import QtQuick.Controls

TextField {
    id: control
    property int textSize: 14
    property bool parchment: false
    GameInk { id: ink }
    font.pixelSize: textSize
    font.family: parchment ? ink.displayFace : font.family
    color: parchment ? "#273945" : "#f0e8d3"
    selectionColor: parchment ? "#c4a374" : "#486d89"
    selectedTextColor: parchment ? "#182732" : "#fff6d8"
    placeholderTextColor: ink.muted
    implicitHeight: Math.max(40, font.pixelSize * 2.6)
    leftPadding: 12; rightPadding: 12; topPadding: 8; bottomPadding: 8
    background: Rectangle {
        radius: parchment ? 2 : 7
        color: parchment ? (control.activeFocus ? "#fffcf1" : "#e5d8be") : (control.activeFocus ? "#2c4359" : "#1c2f42")
        border.width: control.activeFocus ? 2 : 1
        border.color: parchment ? (control.activeFocus ? "#293f51" : "#aa8b5e") : (control.activeFocus ? ink.focus : ink.border)
    }
}
