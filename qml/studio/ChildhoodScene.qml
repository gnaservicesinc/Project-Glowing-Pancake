import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ScrollView {
    id: editor
    required property QtObject author
    property bool changed: false
    property bool submitting: false
    property var scene: ({})
    contentWidth: availableWidth

    function loadScene() {
        scene = author.chapter
        title.text = scene.title || ""
        date.text = scene.date || ""
        body.text = scene.body || ""
        changed = false
    }
    function applyEdits() {
        if (!changed) return true
        const choices = []
        for (let i = 0; i < answers.count; ++i) {
            const row = answers.itemAt(i)
            choices.push({ name: row.buttonText, detail: row.description })
        }
        submitting = true
        const saved = author.changeChapter(title.text, date.text, body.text, choices)
        submitting = false
        return saved
    }
    Connections {
        target: editor.author
        function onDraftChanged() {
            if (!editor.submitting || editor.author.draftMessage.length === 0) editor.loadScene()
        }
    }
    Component.onCompleted: loadScene()

    ColumnLayout {
        width: editor.availableWidth
        spacing: 9
        Label { text: qsTr("Scene title"); font.bold: true }
        TextField { id: title; Layout.fillWidth: true; text: editor.scene.title || ""; Accessible.name: qsTr("Scene title"); onTextEdited: editor.changed = true }
        Label { text: qsTr("When it happens"); font.bold: true }
        TextField {
            id: date
            Layout.preferredWidth: 190
            text: editor.scene.date || ""
            onTextEdited: editor.changed = true
            placeholderText: qsTr("YYYY-MM-DD")
            Accessible.name: qsTr("Scene date")
        }
        Label {
            visible: !!editor.scene.pool
            text: qsTr("Appears in the %1 group of scenes.").arg(editor.scene.pool || "")
        }
        Label { text: qsTr("What happens"); font.bold: true }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            TextArea {
                id: body
                text: editor.scene.body || ""
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                Accessible.name: qsTr("Scene narration")
                onTextChanged: if (activeFocus) editor.changed = true
            }
        }
        Button {
            text: qsTr("Apply scene edits")
            enabled: editor.changed
            onClicked: editor.applyEdits()
        }
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; text: qsTr("What can the player do?"); font.bold: true }
            Button { text: qsTr("Add a choice"); onClicked: if (editor.applyEdits()) author.addChapterChoice() }
        }
        Repeater {
            id: answers
            model: editor.scene.choices || []
            delegate: ColumnLayout {
                required property int index
                required property var modelData
                readonly property alias buttonText: answer.text
                readonly property alias description: outcome.text
                Layout.fillWidth: true
                TextField {
                    id: answer
                    Layout.fillWidth: true
                    text: modelData.name
                    onTextEdited: editor.changed = true
                    Accessible.name: qsTr("Choice %1 button text").arg(index + 1)
                }
                TextField {
                    id: outcome
                    Layout.fillWidth: true
                    text: modelData.detail
                    onTextEdited: editor.changed = true
                    Accessible.name: qsTr("Choice %1 description").arg(index + 1)
                }
                Label {
                    visible: !!modelData.skill
                    text: qsTr("Teaches %1").arg(modelData.skill || "")
                }
            }
        }
    }
}
