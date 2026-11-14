import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: frame
    objectName: "flowPresentation"
    property alias heading: card.heading
    property alias detail: card.detail
    property alias choices: card.choices
    property alias participants: cast.participants
    readonly property bool profileOpen: cast.profileOpen
    required property int textSize
    property bool reducedMotion: true
    function focusHeading() { title.forceActiveFocus(Qt.OtherFocusReason); return title }
    signal chosen(string choiceId)
    Layout.fillWidth: true
    implicitHeight: card.implicitHeight + 28
    radius: 12
    color: "#172737"
    border.color: "#4a6375"
    clip: true
    Rectangle {
        width: 4; height: parent.height - 20; radius: 2
        anchors.left: parent.left; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter
        color: "#efc389"
    }
    ColumnLayout {
        id: card
        property string heading
        property string detail
        property var choices: []
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.margins: 14; anchors.leftMargin: 20
        spacing: 10
        Label {
            id: title
            objectName: "flowDecisionHeading"
            readonly property bool revealAtTop: true
            Layout.fillWidth: true
            text: card.heading
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: "#f5ead8"
            font.pixelSize: Math.round(frame.textSize * 1.22)
            font.bold: true
            Accessible.role: Accessible.Heading
        }
        Label {
            Layout.fillWidth: true
            text: card.detail
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: "#cbd7d3"
            font.pixelSize: frame.textSize
        }
        DecisionParticipants {
            id: cast
            Layout.fillWidth: true
            textSize: frame.textSize
            reducedMotion: frame.reducedMotion
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: card.choices
                WorldButton {
                    id: choiceButton
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 0
                    textSize: frame.textSize
                    padding: font.pixelSize * 0.65
                    implicitHeight: Math.max(font.pixelSize * 2.6, contentItem.implicitHeight + topPadding + bottomPadding)
                    contentItem: Label {text:choiceButton.text;textFormat:Text.PlainText;font:choiceButton.font;color:choiceButton.enabled?"#e4ebe3":"#829594";wrapMode:Text.Wrap;horizontalAlignment:Text.AlignLeft;verticalAlignment:Text.AlignVCenter}
                    required property var modelData
                    objectName: "flowChoice_" + modelData.choiceId
                    text: modelData.label
                    Accessible.name: text
                    onClicked: frame.chosen(modelData.choiceId)
                }
            }
        }
    }
}
