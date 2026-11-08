import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: picker
    required property int textSize
    required property var choices
    required property string defaultPrimary
    required property var defaultSecondary
    property string namePrefix: "objective"
    readonly property string primary: primaryChoice.currentValue || ""
    property var checkedGoals: []
    readonly property string openingKey: defaultPrimary + "|" + JSON.stringify(defaultSecondary) + "|" + JSON.stringify(choices)
    onOpeningKeyChanged: Qt.callLater(reset)
    Component.onCompleted: reset()
    function reset() {
        checkedGoals = Array.from(defaultSecondary || [])
        if (choices.length > 0)
            primaryChoice.currentIndex = Math.max(0, choices.findIndex(row => row.id === defaultPrimary))
        Qt.callLater(primaryChoice.restoreDraft)
        Qt.callLater(secondaryDraft.restoreDraft)
    }
    DraftState {
        id: secondaryDraft
        draftKey: "objectives/" + picker.namePrefix
        accepts: value => Array.isArray(value) && value.length <= 16 && value.every(id => typeof id === "string") && new Set(value).size === value.length
        onRestored: value => picker.checkedGoals = value.filter(id => picker.choices.some(choice => choice.id === id))
        onReset: picker.checkedGoals = Array.from(picker.defaultSecondary || [])
    }
    function secondary() {
        return choices.filter(row => row.id !== primary && checkedGoals.includes(row.id)).map(row => row.id)
    }
    Label { text: qsTr("Primary objective"); font.pixelSize: picker.textSize }
    DraftComboBox {
        id: primaryChoice
        objectName: picker.namePrefix + "Primary"
        draftDefaultIndex: Math.max(0, picker.choices.findIndex(row => row.id === picker.defaultPrimary))
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        textRole: "label"
        valueRole: "id"
        font.pixelSize: picker.textSize
        model: picker.choices
        Accessible.name: qsTr("Primary objective")
    }
    Label { text: qsTr("Optional secondary objectives"); font.pixelSize: picker.textSize; Layout.fillWidth: true; wrapMode: Text.Wrap }
    Flow {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredHeight: implicitHeight
        spacing: 4
        Repeater {
            model: picker.choices
            delegate: CheckBox {
                required property var modelData
                objectName: picker.namePrefix + "Secondary_" + modelData.id
                visible: modelData.id !== picker.primary
                text: modelData.label
                font.pixelSize: picker.textSize
                Accessible.name: qsTr("Secondary objective: %1").arg(text)
                checked: picker.checkedGoals.includes(modelData.id)
                onClicked: {
                    const goals = picker.checkedGoals.filter(id => id !== modelData.id)
                    if (checked)
                        goals.push(modelData.id)
                    picker.checkedGoals = goals
                    secondaryDraft.remember(goals)
                }
            }
        }
    }
}
