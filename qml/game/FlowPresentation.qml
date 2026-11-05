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
            Button {
                required property var modelData
                text: modelData.label
                Accessible.name: text
                onClicked: card.chosen(modelData.choiceId)
            }
        }
    }
}
