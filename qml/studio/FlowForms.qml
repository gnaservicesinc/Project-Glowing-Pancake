import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: forms
    required property QtObject author
    property string errorKey
    property string errorText
    property bool showOutline: true
    property bool showFields: true
    property bool fillOutline: false
    property string outlineObjectName: "flowOutline"
    property alias outlineView: outline
    property QtObject outlineFocusTarget

    function selectAt(index) {
        const row = author.flowOutline[index]
        if (row)
            author.selectOutline(row.key)
    }

    function commit(ticket, key, value) {
        if (!author.changeForm(ticket, key, value)) {
            errorKey = key
            errorText = author.draftMessage
            return false
        }
        Qt.callLater(() => {
            for (let index = 0; index < fieldRepeater.count; ++index) {
                const row = fieldRepeater.itemAt(index)
                if (row && row.field.key === key && row.loader.item && row.loader.item.focusEditor) {
                    row.loader.item.focusEditor()
                    return
                }
            }
            if (forms.showOutline) outline.forceActiveFocus(Qt.OtherFocusReason)
            else if (forms.outlineFocusTarget && forms.outlineFocusTarget.visible) forms.outlineFocusTarget.forceActiveFocus(Qt.OtherFocusReason)
        })
        return true
    }
    Connections {
        target: forms.author
        function onFormChanged() { forms.errorKey = ""; forms.errorText = "" }
    }

    Label { visible: forms.showOutline; text: qsTr("Flow outline"); font.bold: true }
    ScrollView {
        visible: forms.showOutline
        Layout.fillWidth: true
        Layout.fillHeight: forms.fillOutline
        Layout.preferredHeight: 190
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ListView {
            id: outline
            objectName: forms.outlineObjectName
            clip: true
            activeFocusOnTab: true
            keyNavigationEnabled: false
            model: forms.author.flowOutline
            currentIndex: forms.author.outlineIndex
            Accessible.name: qsTr("Flow outline")
            Keys.onPressed: event => {
                switch (event.key) {
                case Qt.Key_Up: forms.selectAt(currentIndex - 1); break
                case Qt.Key_Down: forms.selectAt(currentIndex + 1); break
                case Qt.Key_Home: forms.selectAt(0); break
                case Qt.Key_End: forms.selectAt(count - 1); break
                default: return
                }
                event.accepted = true
            }
            onCurrentIndexChanged: {
                if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            }
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.label
                highlighted: ListView.isCurrentItem
                focusPolicy: Qt.ClickFocus
                onClicked: {
                    outline.forceActiveFocus(Qt.MouseFocusReason)
                    forms.author.selectOutline(modelData.key)
                }
            }
        }
    }
    Repeater {
        id: fieldRepeater
        model: forms.showFields ? forms.author.formFields : []
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
            Label {
                Layout.fillWidth: true
                visible: forms.errorText.length > 0 && forms.errorKey === row.field.key
                text: forms.errorText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.bold: true
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
                    objectName: "formAction/" + row.field.key
                    text: row.field.label
                    Accessible.description: row.field.help
                    onClicked: forms.commit(row.ticket, row.field.key, "")
                }
            }
            Component {
                id: picker
                ComboBox {
                    id: choices
                    objectName: "formChoice/" + row.field.key
                    function focusEditor() { forceActiveFocus(Qt.OtherFocusReason) }
                    model: row.field.options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: indexOfValue(row.field.value)
                    Accessible.name: row.field.label
                    Accessible.description: row.field.help
                    onActivated: {
                        if (!forms.commit(row.ticket, row.field.key, currentValue))
                            currentIndex = indexOfValue(row.field.value)
                    }
                }
            }
            Component {
                id: input
                RowLayout {
                    function focusEditor() { entered.forceActiveFocus(Qt.OtherFocusReason) }
                    TextField {
                        id: entered
                        objectName: "formInput/" + row.field.key
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
