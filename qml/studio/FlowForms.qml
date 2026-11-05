import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: forms
    required property QtObject author

    function commit(ticket, key, value) {
        if (!author.changeForm(ticket, key, value))
            return
        Qt.callLater(() => {
            for (let index = 0; index < fieldRepeater.count; ++index) {
                const row = fieldRepeater.itemAt(index)
                if (row && row.field.key === key && row.loader.item && row.loader.item.focusEditor) {
                    row.loader.item.focusEditor()
                    return
                }
            }
            outline.forceActiveFocus(Qt.OtherFocusReason)
        })
    }

    Label { text: qsTr("Flow outline"); font.bold: true }
    ComboBox {
        id: outline
        objectName: "flowOutline"
        Layout.fillWidth: true
        textRole: "label"
        valueRole: "key"
        model: forms.author.flowOutline
        currentIndex: forms.author.outlineIndex
        onActivated: forms.author.selectOutline(currentValue)
        Accessible.name: qsTr("Flow outline")
    }
    Repeater {
        id: fieldRepeater
        model: forms.author.formFields
        delegate: ColumnLayout {
            id: row
            required property var modelData
            readonly property var field: modelData
            readonly property alias loader: editor
            readonly property string ticket: modelData.ticket
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                visible: row.field.control !== "button"
                text: row.field.label
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.bold: true
            }
            Loader {
                id: editor
                Layout.fillWidth: true
                sourceComponent: {
                    if (row.field.control === "choice") return picker
                    if (row.field.control === "button") return action
                    if (row.field.control === "note") return note
                    return input
                }
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: row.field.help
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            Component {
                id: note
                Label {
                    text: row.field.value
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                }
            }
            Component {
                id: action
                Button {
                    text: row.field.label
                    Accessible.description: row.field.help
                    onClicked: forms.commit(row.ticket, row.field.key, "")
                }
            }
            Component {
                id: picker
                ComboBox {
                    id: choices
                    function focusEditor() { forceActiveFocus(Qt.OtherFocusReason) }
                    model: row.field.options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: indexOfValue(row.field.value)
                    Accessible.name: row.field.label
                    Accessible.description: row.field.help
                    onActivated: forms.commit(row.ticket, row.field.key, currentValue)
                }
            }
            Component {
                id: input
                RowLayout {
                    function focusEditor() { entered.forceActiveFocus(Qt.OtherFocusReason) }
                    TextField {
                        id: entered
                        Layout.fillWidth: true
                        text: row.field.value
                        maximumLength: row.field.control === "integer" ? 21 : 4096
                        inputMethodHints: row.field.control === "integer" ? Qt.ImhPreferNumbers : Qt.ImhNone
                        Accessible.name: row.field.label
                        Accessible.description: row.field.help
                        onAccepted: forms.commit(row.ticket, row.field.key, text)
                    }
                    Button {
                        text: qsTr("Apply")
                        enabled: entered.text !== row.field.value
                        onClicked: forms.commit(row.ticket, row.field.key, entered.text)
                    }
                }
            }
        }
    }
}
