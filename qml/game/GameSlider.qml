import QtQuick
import QtQuick.Controls

Slider {
    id: control
    property int textSize: 14
    font.pixelSize: textSize
    implicitHeight: Math.max(30, font.pixelSize * 1.5)
    background: Rectangle {
        x: control.leftPadding
        y: (control.height-height)/2
        width: control.availableWidth
        height: Math.max(5, control.font.pixelSize * 0.35)
        radius: height/2
        color: "#28494a"
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height/2
            color: "#87bba8"
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth-width)
        y: (control.height-height)/2
        implicitWidth: Math.max(22, control.font.pixelSize)
        implicitHeight: implicitWidth
        radius: width/2
        color: control.pressed ? "#f0cf8e" : "#b0d9c6"
        border.color: control.activeFocus ? "#f2c783" : "#547a70"
        border.width: control.activeFocus ? 3 : 1
    }
}
