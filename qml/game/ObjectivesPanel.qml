import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var desk: campaign.objectives
    property bool choosing: false
    spacing: 10
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Action: Button { font.pixelSize: panel.textSize; enabled: !campaign.busy }

    Copy { objectName: "objectivesOverview"; text: panel.desk.overview; font.bold: true }
    Copy { text: qsTr("Postmortem"); font.bold: true; visible: postmortem.visible }
    Copy { id: postmortem; objectName: "objectivesPostmortem"; text: panel.desk.postmortem; visible: text.length > 0 }
    Flow {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredHeight: implicitHeight
        spacing: 6
        Action {
            objectName: "continueScoredObjectives"
            visible: panel.desk.breachText.length > 0
            text: qsTr("Continue scored play")
            onClicked: panel.desk.acknowledge()
        }
        Action {
            objectName: "endScoredObjectives"
            visible: panel.desk.canEndEarly
            text: qsTr("End at this postmortem")
            onClicked: panel.desk.reviewEnd()
        }
        Action {
            objectName: "continueAfterObjectiveEnding"
            visible: panel.desk.canContinue
            text: qsTr("Review continuation…")
            onClicked: panel.desk.reviewContinue()
        }
    }
    Copy { text: qsTr("Results and exact rules"); font.bold: true }
    ComboBox {
        id: selected
        objectName: "objectiveResultPicker"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        model: panel.desk.rows.map(row => ({id: row.id, label: row.label + " · " + row.role + " · " + row.status}))
        textRole: "label"
        valueRole: "id"
        font.pixelSize: panel.textSize
        Accessible.name: qsTr("Objective result to inspect")
        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(panel.desk.primary))
    }
    readonly property var result: desk.rows.find(row => row.id === selected.currentValue) || {}
    Copy { objectName: "objectiveRules"; text: panel.result.rules || "" }
    Copy { objectName: "objectiveProgress"; text: panel.result.progress || "" }
    Action {
        objectName: "editObjectiveSelection"
        visible: panel.desk.editable
        text: panel.choosing ? qsTr("Hide selection") : qsTr("Change objectives…")
        onClicked: { panel.choosing = !panel.choosing; panel.desk.clearReview() }
    }
    ObjectivePicker {
        id: selection
        visible: panel.choosing && panel.desk.editable
        Layout.fillWidth: true
        textSize: panel.textSize
        namePrefix: "changeObjective"
        choices: panel.desk.options
        defaultPrimary: panel.desk.primary
        defaultSecondary: panel.desk.secondary
    }
    Action {
        objectName: "reviewObjectiveSelection"
        visible: selection.visible
        text: qsTr("Review changes")
        onClicked: panel.desk.reviewSelection(selection.primary, selection.secondary())
    }
    Copy { objectName: "objectiveReviewText"; visible: text.length > 0; text: panel.desk.reviewText }
    Flow {
        visible: panel.desk.reviewText.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight
        spacing: 8
        Action { objectName: "confirmObjectiveAction"; text: qsTr("Confirm"); onClicked: panel.desk.confirm() }
        Action { text: qsTr("Cancel"); onClicked: panel.desk.clearReview() }
    }
}
