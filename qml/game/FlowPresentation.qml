import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: card
    objectName: "flowPresentation"
    property string heading
    property string detail
    property var choices: []
    signal chosen(string choiceId)
    spacing: 8
    Label {
        Layout.fillWidth: true
        text: card.heading
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.bold: true
    }
    Label {
        Layout.fillWidth: true
        text: card.detail
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: card.choices
            WorldButton {
                id: choiceButton
                width: Math.min(implicitWidth, card.width)
                padding: font.pixelSize * 0.65
                implicitHeight: Math.max(font.pixelSize * 2.6, contentItem.implicitHeight + topPadding + bottomPadding)
                contentItem: Label {text:choiceButton.text;font:choiceButton.font;color:choiceButton.enabled?"#e4ebe3":"#829594";wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                required property var modelData
                objectName: "flowChoice_" + modelData.choiceId
                text: modelData.label
                Accessible.name: text
                onClicked: card.chosen(modelData.choiceId)
            }
        }
    }
}
