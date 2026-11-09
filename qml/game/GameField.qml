import QtQuick
import QtQuick.Controls

TextField {
    id: control
    property int textSize:14
    font.pixelSize:textSize
    color:"#f0e8d3";selectionColor:"#52746b";selectedTextColor:"#fff6d8";placeholderTextColor:"#90aaa1"
    implicitHeight:Math.max(40,font.pixelSize*2.6)
    leftPadding:12;rightPadding:12;topPadding:8;bottomPadding:8
    background:Rectangle {radius:7;color:control.activeFocus?"#28484a":"#1b363b";border.width:control.activeFocus?2:1;border.color:control.activeFocus?"#dabe83":"#56746c"}
}
