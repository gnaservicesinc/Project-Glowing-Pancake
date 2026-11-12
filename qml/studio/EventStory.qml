import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ScrollView {
    id: editor
    required property QtObject author
    signal stepRequested(string step, int choice)
    property var story: ({})
    property bool submitting: false
    property bool changed: false
    contentWidth: availableWidth

    function labels() {
        const result = []
        for (let i = 0; i < answers.count; ++i)
            result.push(answers.itemAt(i).buttonText)
        return result
    }
    function applyEdits() {
        if (!changed) return true
        submitting = true
        const saved = author.changeEventStory(eventName.text, playerTitle.text, body.text, labels())
        submitting = false
        return saved
    }
    function openStep(step, choice) {
        if (applyEdits()) stepRequested(step, choice)
    }
    function loadStory() {
        story = author.eventStory
        eventName.text = author.workingTitle
        playerTitle.text = story.title || ""
        body.text = story.body || ""
        changed = false
    }
    Connections {
        target: editor.author
        function onDraftChanged() {
            if (!editor.submitting || editor.author.draftMessage.length === 0) editor.loadStory()
        }
    }
    Component.onCompleted: loadStory()

    ColumnLayout {
        width: editor.availableWidth
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("When it happens"); font.bold: true }
                Label {
                    Layout.fillWidth: true
                    text: editor.story.timing || ""
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
            }
            Button { text: qsTr("Edit timing"); onClicked: editor.openStep("lifecycle", -1); enabled: !!editor.story.timing && !editor.story.timing.startsWith("Another event") }
            Button { text: qsTr("Edit conditions"); onClicked: editor.openStep("trigger", -1); enabled: !!editor.story.timing && !editor.story.timing.startsWith("Another event") }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: editor.story.scenarios && editor.story.scenarios.length
                    ? qsTr("Used in %1").arg(editor.story.scenarios.join(", "))
                    : qsTr("This event is available to use in a scenario.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            Button { text: qsTr("Choose scenarios"); onClicked: editor.openStep("flow", -1) }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#455264" }
        Label { text: qsTr("Name in Studio"); font.bold: true }
        TextField {
            id: eventName
            Layout.fillWidth: true
            Accessible.name: qsTr("Name in Studio")
            text: editor.author.workingTitle
            onTextEdited: editor.changed = true
        }
        Label { visible: !!editor.story.dialogue; text: qsTr("Title the player sees"); font.bold: true }
        TextField {
            id: playerTitle
            visible: !!editor.story.dialogue
            Layout.fillWidth: true
            text: editor.story.title || ""
            Accessible.name: qsTr("Title the player sees")
            onTextEdited: editor.changed = true
        }
        Label { visible: !!editor.story.hasMessage; text: qsTr("What the player reads"); font.bold: true }
        ScrollView {
            visible: !!editor.story.hasMessage
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            TextArea {
                id: body
                objectName: "eventStoryBody"
                text: editor.story.body || ""
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                Accessible.name: qsTr("What the player reads")
                onTextChanged: if (activeFocus) editor.changed = true
            }
        }
        Label {
            visible: !editor.story.hasMessage
            Layout.fillWidth: true
            text: qsTr("This event runs in the background. Open Steps to see what it changes or to add a message.")
            wrapMode: Text.Wrap
        }
        Label { visible: !!editor.story.dialogue; text: qsTr("What the player can choose"); font.bold: true }
        Repeater {
            id: answers
            model: editor.story.choices || []
            delegate: RowLayout {
                required property int index
                required property var modelData
                readonly property alias buttonText: answer.text
                Layout.fillWidth: true
                TextField {
                    id: answer
                    Layout.fillWidth: true
                    text: modelData.text
                    Accessible.name: qsTr("Choice %1 button text").arg(index + 1)
                    onTextEdited: editor.changed = true
                }
                Button { text: qsTr("What happens next"); onClicked: editor.openStep("story", index) }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                visible: !!editor.story.dialogue
                text: qsTr("Add a choice")
                onClicked: if (editor.applyEdits()) editor.author.appendStoryChoice()
            }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Apply story edits"); enabled: editor.changed; onClicked: editor.applyEdits() }
        }
    }
}
