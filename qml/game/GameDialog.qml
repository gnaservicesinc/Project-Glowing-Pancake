import QtQuick
import QtQuick.Controls

Dialog {
    id: dialog
    GameInk { id: ink }
    padding: 16
    spacing: 8
    background: Rectangle {
        radius: 12
        color: ink.panel
        border.color: ink.border
    }
    header: Label {
        visible: dialog.title.length>0
        text: dialog.title
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: dialog.font.pixelSize * 1.25
        font.family: ink.displayFace
        font.bold: true
        color: "#f1e4c5"
        padding: 16
    }
    footer: DialogButtonBox {
        visible: count>0
        alignment: Qt.AlignRight
        spacing: 8
        padding: 12
        delegate: WorldButton { textSize: dialog.font.pixelSize }
        background: Item {}
    }
}
