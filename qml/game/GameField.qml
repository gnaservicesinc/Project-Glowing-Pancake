import QtQuick
import QtQuick.Controls

TextField {
    id: control
    property int textSize:14
    GameInk { id: ink }
    font.pixelSize:textSize
    color:"#f0e8d3";selectionColor:"#486d89";selectedTextColor:"#fff6d8";placeholderTextColor:ink.muted
    implicitHeight:Math.max(40,font.pixelSize*2.6)
    leftPadding:12;rightPadding:12;topPadding:8;bottomPadding:8
    background:Rectangle {radius:7;color:control.activeFocus?"#2c4359":"#1c2f42";border.width:control.activeFocus?2:1;border.color:control.activeFocus?ink.focus:ink.border}
}
