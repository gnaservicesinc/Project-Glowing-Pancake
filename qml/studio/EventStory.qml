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
        passage.currentIndex = (story.passages || []).findIndex(row => row.id === story.passage)
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
        Rectangle {
            Layout.fillWidth: true
            color: "#1b2531"
            border.color: "#2e3d4f"
            radius: 8
            implicitHeight: timingContent.implicitHeight + 24
            RowLayout {
                id: timingContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: qsTr("When it happens"); font.bold: true; color: "#a8c1d8" }
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
        }
        Rectangle {
            Layout.fillWidth: true
            color: "#1b2531"
            border.color: "#2e3d4f"
            radius: 8
            implicitHeight: storyCard.implicitHeight + 24
            ColumnLayout {
                id: storyCard
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                Label { text: qsTr("Name in Studio"); font.bold: true }
                TextField {
                    id: eventName
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Name in Studio")
                    text: editor.author.workingTitle
                    onTextEdited: editor.changed = true
                }
                RowLayout {
                    visible: passage.visible
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: qsTr("Story passage"); font.bold: true }
                    Label {
                        visible: passage.count > 1
                        text: qsTr("Passage %1 of %2").arg(passage.currentIndex + 1).arg(passage.count)
                        color: "#a8c1d8"
                        font.pixelSize: 12
                    }
                }
                ComboBox {
                    id: passage
                    objectName: "eventStoryPassage"
                    visible: count > 1
                    Layout.fillWidth: true
                    textRole: "label"
                    model: editor.story.passages || []
                    popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
                    Accessible.name: qsTr("Choose a decision, news story or outcome to edit")
                    onActivated: index => {
                        const next = model[index].id
                        if (editor.applyEdits()) editor.author.selectPassage(next)
                        else currentIndex = model.findIndex(row => row.id === editor.story.passage)
                    }
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
                    Layout.preferredHeight: 140
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
                    color: "#a8c1d8"
                }
            }
        }
        Rectangle {
            visible: !!editor.story.dialogue
            Layout.fillWidth: true
            color: "#1b2531"
            border.color: "#2e3d4f"
            radius: 8
            implicitHeight: choicesCard.implicitHeight + 24
            ColumnLayout {
                id: choicesCard
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10
                Label { text: qsTr("What the player can choose"); font.bold: true }
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
                        Button {
                            visible: !!modelData.nextPassage
                            text: qsTr("Read next")
                            onClicked: {
                                const next = modelData.nextPassage
                                if (editor.applyEdits()) editor.author.selectPassage(next)
                            }
                        }
                        Button { text: qsTr("Edit route"); onClicked: editor.openStep("story", index) }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: qsTr("Add a choice")
                        onClicked: if (editor.applyEdits()) editor.author.appendStoryChoice()
                    }
                    Item { Layout.fillWidth: true }
                    Button { text: qsTr("Apply story edits"); enabled: editor.changed; onClicked: editor.applyEdits() }
                }
            }
        }
        RowLayout {
            visible: !editor.story.dialogue && !!editor.story.hasMessage
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Apply story edits"); enabled: editor.changed; onClicked: editor.applyEdits() }
        }
    }
}
