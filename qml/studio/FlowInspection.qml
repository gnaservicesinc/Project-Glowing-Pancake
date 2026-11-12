import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property QtObject inspector
    spacing: 8
    Label { text: qsTr("Preview this event"); font.bold: true }
    Label {
        Layout.fillWidth: true
        visible: panel.inspector.scenarios.length === 0
        text: qsTr("This collection needs a validated scenario fixture before full-simulation testing.")
        wrapMode: Text.Wrap
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.inspector.scenarios.length > 0
        GridLayout {
            Layout.fillWidth: true
            columns: width > 360 ? 2 : 1
            ComboBox {
                id: scenario
                Layout.fillWidth: true
                model: panel.inspector.scenarios
                Accessible.name: qsTr("Test scenario")
            }
            ComboBox {
                id: headquarters
                model: panel.inspector.headquarters(scenario.currentText)
                Accessible.name: qsTr("Test headquarters")
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: width > 520 ? 4 : 2
            TextField {
                id: seed
                objectName: "flowTestSeed"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                maximumLength: 20
                placeholderText: qsTr("Seed · blank uses scenario seed")
                Accessible.name: qsTr("Test seed")
            }
            CheckBox { id: isolate; text: qsTr("Show this event only"); checked: true }
            Button {
                objectName: "startFlowTest"
                text: qsTr("Start preview")
                enabled: !panel.inspector.busy && headquarters.currentIndex >= 0
                onClicked: panel.inspector.start(scenario.currentText, headquarters.currentText, seed.text, isolate.checked)
            }
        }
    }
    Label {
        Layout.fillWidth: true
        visible: panel.inspector.fixtureInfo.length > 0
        text: panel.inspector.fixtureInfo
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    GridLayout {
        Layout.fillWidth: true
        columns: width > 560 ? 4 : 2
        visible: panel.inspector.ready
        TextField {
            id: dateTarget
                objectName: "flowAdvanceDate"
            Layout.fillWidth: true
            maximumLength: 10
            text: panel.inspector.nextDate
            Accessible.name: qsTr("Preview through this date")
        }
        Button {
            objectName: "advanceFlowTest"
            text: qsTr("Advance to date")
            enabled: !panel.inspector.busy
            onClicked: panel.inspector.advance(dateTarget.text)
        }
        Button { text: qsTr("Stop"); enabled: panel.inspector.busy; onClicked: panel.inspector.stop() }
        Button { objectName: "reloadFlowTest"; text: qsTr("Reload checkpoint"); enabled: !panel.inspector.busy; onClicked: panel.inspector.reload() }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: width > 500 ? 2 : 1
        visible: panel.inspector.cleanupOwners.length > 0
        ComboBox { id: owner; model: panel.inspector.cleanupOwners; Accessible.name: qsTr("Synthetic cleanup owner") }
        Button {
            text: qsTr("Synthetic cleanup on separate copy")
            enabled: !panel.inspector.busy
            onClicked: panel.inspector.cleanup(owner.currentText)
        }
    }
    Label {
        objectName: "flowTestStatus"
        Layout.fillWidth: true
        text: panel.inspector.status
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Button {
        objectName: "inspectFlowBoundaries"
        text: qsTr("Inspect threshold boundaries on isolated facts")
        visible: panel.inspector.ready
        enabled: !panel.inspector.busy
        onClicked: panel.inspector.inspectBoundaries()
    }
    Repeater {
        model: panel.inspector.presentations
        Frame {
            required property var modelData
            Layout.fillWidth: true
            contentItem: ColumnLayout {
                Label {
                    Layout.fillWidth: true
                    text: modelData.source
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }
                FlowPresentation {
                    Layout.fillWidth: true
                    heading: modelData.title
                    detail: modelData.detail
                    choices: modelData.choices
                    enabled: !panel.inspector.busy
                    onChosen: choiceId => panel.inspector.choose(modelData.id, choiceId)
                }
            }
        }
    }
    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 320
        visible: panel.inspector.ready
        TextArea {
            objectName: "flowTestReport"
            text: panel.inspector.report
            textFormat: Text.PlainText
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            Accessible.name: qsTr("Trigger, effect and continuation inspection")
        }
    }
    Connections {
        target: panel.inspector
        function onChanged() {
            if (!dateTarget.activeFocus) dateTarget.text = panel.inspector.nextDate
        }
    }
}
