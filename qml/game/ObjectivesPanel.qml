import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var desk: campaign.objectives
    readonly property var career: panel.visible && desk.retrospective ? desk.career : ({})
    property bool choosing: false
    DraftState {
        id: choosingDraft
        draftKey: "objectives/choosing"
        accepts: value => typeof value === "boolean"
        onRestored: value => panel.choosing = value
        onReset: panel.choosing = false
    }
    spacing: 10
    function focusRetrospective() {
        if (careerDesk.visible) {
            careerHeading.forceActiveFocus(Qt.OtherFocusReason)
            return careerHeading
        }
        return edition.focusHeadline()
    }
    ColumnLayout {
        id: careerDesk
        Layout.fillWidth: true
        visible: panel.career.open === true
        spacing: 8
        component CareerCopy: Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.pixelSize: panel.textSize
        }
        component CareerAction: WorldButton {
            Layout.maximumWidth: careerDesk.width
            font.pixelSize: panel.textSize
            enabled: !campaign.busy && campaign.paused
        }
        CareerCopy {
            id: careerHeading
            objectName: "careerHeading"
            property bool revealAtTop: true
            text: panel.career.title || ""
            font.bold: true
        }
        CareerCopy { text: panel.career.detail || ""; visible: text.length > 0 }
        DraftComboBox {
            id: jobs
            objectName: "careerJobPicker"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: count > 0
            model: panel.career.jobs || []
            textRole: "label"
            valueRole: "id"
            font.pixelSize: panel.textSize
            Accessible.name: qsTr("Research appointment")
        }
        CareerCopy {
            text: jobs.currentIndex >= 0 ? jobs.model[jobs.currentIndex].detail : ""
            visible: jobs.visible && text.length > 0
        }
        CareerAction {
            objectName: "reviewCareerAppointment"
            visible: jobs.currentIndex >= 0
            text: qsTr("Review appointment")
            onClicked: panel.desk.reviewCareer("accept", jobs.model[jobs.currentIndex].id)
        }
        CareerAction {
            objectName: "reviewCareerSearch"
            visible: panel.career.looking === true
            text: qsTr("Continue looking for work")
            onClicked: panel.desk.reviewCareer("search", "")
        }
        CareerAction {
            objectName: "advanceCareerDay"
            visible: panel.career.employed === true || (panel.career.open === true && campaign.canAdvance)
            enabled: campaign.canAdvance && !campaign.storageBlocked
            text: qsTr("Next day")
            onClicked: campaign.step()
        }
        DraftComboBox {
            id: topics
            objectName: "careerResearchPicker"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            visible: count > 0
            model: panel.career.options || []
            textRole: "label"
            valueRole: "id"
            font.pixelSize: panel.textSize
            Accessible.name: qsTr("Research topic")
        }
        CareerCopy {
            visible: panel.career.employed === true
            text: topics.currentIndex >= 0 ? topics.model[topics.currentIndex].detail :
                campaign.paused ? qsTr("No research is ready for funding with the lab's current staff, compute and budget.") :
                                  qsTr("Pause to review available research.")
        }
        CareerAction {
            objectName: "reviewCareerResearch"
            visible: topics.currentIndex >= 0
            text: qsTr("Review research")
            onClicked: panel.desk.reviewCareer("research", topics.model[topics.currentIndex].id)
        }
        Repeater {
            model: panel.career.projects || []
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                CareerCopy { text: modelData.label + " · " + modelData.detail }
                CareerAction {
                    objectName: "reviewCareerPublication"
                    visible: modelData.publish
                    text: qsTr("Review publication")
                    onClicked: panel.desk.reviewCareer("publish", modelData.id)
                }
                CareerAction {
                    visible: modelData.resume
                    text: qsTr("Review resuming research")
                    onClicked: panel.desk.reviewCareer("resume", modelData.project)
                }
                CareerAction {
                    visible: modelData.cancel
                    text: qsTr("Review cancellation")
                    onClicked: panel.desk.reviewCareer("cancel", modelData.project)
                }
            }
        }
        GameDialog {
            id: careerReview
            modal: true
            title: qsTr("Review career decision")
            width: Math.min(620, (Overlay.overlay ? Overlay.overlay.width : 650) - 32)
            anchors.centerIn: Overlay.overlay
            visible: careerDesk.visible && panel.desk.reviewText.length > 0
            contentItem: ScrollView {
                implicitHeight: Math.min(reviewCopy.implicitHeight + 8, (Overlay.overlay ? Overlay.overlay.height : 480) * 0.48)
                contentWidth: availableWidth
                clip: true
                Label {
                    id: reviewCopy
                    width: parent.width
                    text: panel.desk.reviewText
                    wrapMode: Text.Wrap
                    font.pixelSize: panel.textSize
                }
            }
            footer: DialogButtonBox {
                CareerAction { objectName: "confirmCareerAction"; text: qsTr("Confirm"); onClicked: panel.desk.confirm() }
                CareerAction { text: qsTr("Back"); onClicked: panel.desk.clearReview() }
            }
            onClosed: panel.desk.clearReview()
        }
    }
    LifeReport {
        id: edition
        Layout.fillWidth: true
        visible: panel.desk.retrospective
        textSize: panel.textSize
        report: panel.visible && panel.desk.retrospective ? panel.desk.report : ({})
    }
    ColumnLayout {
        id: scored
        visible: !panel.desk.retrospective
        Layout.fillWidth: true
        component Copy: Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font.pixelSize: panel.textSize
        }
        component Action: WorldButton { font.pixelSize: panel.textSize; enabled: !campaign.busy }

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
        DraftComboBox {
            id: selected
            objectName: "objectiveResultPicker"
            draftDefaultIndex: Math.max(0, indexOfValue(panel.desk.primary))
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
        Copy { objectName: "objectiveRules"; text: scored.result.rules || "" }
        Copy { objectName: "objectiveProgress"; text: scored.result.progress || "" }
        Action {
            objectName: "editObjectiveSelection"
            visible: panel.desk.editable
            text: panel.choosing ? qsTr("Hide selection") : qsTr("Change objectives…")
            onClicked: { panel.choosing = !panel.choosing; choosingDraft.remember(panel.choosing); panel.desk.clearReview() }
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
}
