import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var desk: campaign.leadership
    readonly property bool editable: desk.editable && !campaign.busy && campaign.companyEditable
    spacing: 10
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Action: Button { font.pixelSize: panel.textSize }
    component Field: ColumnLayout {
        id: field
        required property string label
        required property string inputName
        property string initialValue: ""
        readonly property string value: entry.text
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Copy { text: field.label }
        TextField {
            id: entry
            objectName: field.inputName
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            Accessible.name: field.label
            text: field.initialValue
            onTextEdited: panel.desk.clearReview()
        }
    }
    component Picker: ComboBox {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        textRole: "label"
        valueRole: "id"
        onActivated: panel.desk.clearReview()
    }
    component Records: ColumnLayout {
        id: records
        required property var rows
        required property string listName
        required property string emptyText
        Layout.fillWidth: true
        StableEntityList {
            objectName: records.listName
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, Math.max(90, records.rows.count * panel.textSize * 4))
            rows: records.rows
            emptyText: records.emptyText
            reducedMotion: campaign.reducedMotion
            textSize: panel.textSize
            onSelected: id => records.rows.selectedId = id
        }
        Copy { objectName: records.listName + "Detail"; text: records.rows.selectedRow.detail || ""; visible: text.length > 0 }
    }
    Copy { text: qsTr("Leadership and founder legacy"); font.bold: true }
    Copy { objectName: "leadershipOverview"; text: panel.desk.overview }
    Copy { objectName: "leadershipNotice"; text: panel.desk.noticeText; visible: text.length > 0 }
    Action {
        objectName: "acknowledgeLeadership"
        text: qsTr("Acknowledge leadership notice")
        visible: panel.desk.noticeText.length > 0
        enabled: panel.editable
        onClicked: panel.desk.acknowledge()
    }
    Copy { text: qsTr("Plan a successor"); font.bold: true }
    Field { id: handover; inputName: "leadershipHandoverDate"; label: qsTr("Handover date (YYYY-MM-DD)"); initialValue: panel.desk.recommendedDate }
    Picker {
        id: candidate
        objectName: "leadershipCandidate"
        model: panel.desk.available ? panel.desk.candidatesAt(handover.value) : []
        Accessible.name: qsTr("Successor eligible at the handover date")
    }
    Copy { text: candidate.currentIndex >= 0 ? candidate.model[candidate.currentIndex].detail : qsTr("Choose a future date with an eligible leadership cohort.") }
    CheckBox {
        id: chair
        objectName: "leadershipRetainChair"
        visible: panel.desk.founderExecutive
        font.pixelSize: panel.textSize
        text: qsTr("Founder moves to the personal chair role")
        onToggled: panel.desk.clearReview()
    }
    Copy { text: qsTr("Transition exercises use spare staff and compute while existing company work continues.") }
    Picker {
        id: source
        objectName: "leadershipSource"
        model: campaign.management.sources
        Accessible.name: qsTr("Transition compute source")
    }
    GridLayout {
        Layout.fillWidth: true
        columns: panel.width >= panel.textSize * 29 ? 2 : 1
        Field { id: rate; inputName: "leadershipRate"; label: qsTr("Compute per day (CU)") }
        Field { id: staff; inputName: "leadershipStaff"; label: qsTr("Staff for preparation") }
    }
    Action {
        objectName: "reviewLeadershipNomination"
        text: qsTr("Review successor nomination")
        enabled: panel.editable && candidate.currentIndex >= 0 && source.currentIndex >= 0
        onClicked: panel.desk.reviewNomination(candidate.currentValue, handover.value, panel.desk.founderExecutive && chair.checked,
                                               source.currentValue, rate.value, staff.value)
    }
    Records { listName: "leadershipPlans"; rows: panel.desk.plans; emptyText: qsTr("No leadership proposal") }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Action {
            objectName: "reviewLeadershipSupport"
            text: qsTr("Review supporting this plan")
            enabled: panel.editable && (panel.desk.plans.selectedRow.fields?.vote ?? false)
            onClicked: panel.desk.reviewVote(true)
        }
        Action {
            objectName: "reviewLeadershipOpposition"
            text: qsTr("Review opposing this plan")
            enabled: panel.editable && (panel.desk.plans.selectedRow.fields?.vote ?? false)
            onClicked: panel.desk.reviewVote(false)
        }
        Action {
            objectName: "reviewLeadershipCancellation"
            text: qsTr("Review cancelling this plan")
            enabled: panel.editable && (panel.desk.plans.selectedRow.fields?.cancel ?? false)
            onClicked: panel.desk.reviewCancellation()
        }
    }
    Action {
        objectName: "advanceLeadership"
        text: qsTr("Advance to next leadership date")
        enabled: !campaign.busy && campaign.paused && campaign.requiredKind === "none"
        onClicked: campaign.advanceTo("leadership")
    }
    Action {
        objectName: "reviewFounderChairRetirement"
        text: qsTr("Review retiring the founder chair")
        visible: panel.desk.founderChair
        enabled: panel.editable
        onClicked: panel.desk.reviewRetirement()
    }
    Copy { text: qsTr("Executive history"); font.bold: true }
    Records { listName: "leadershipTerms"; rows: panel.desk.terms; emptyText: qsTr("Executive terms") }
    Dialog {
        id: review
        title: qsTr("Review leadership decision")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(820, (Overlay.overlay ? Overlay.overlay.width : 820) - 32)
        font.pixelSize: panel.textSize
        contentItem: ScrollView {
            implicitHeight: Math.min(copy.implicitHeight + 8, (Overlay.overlay ? Overlay.overlay.height : 720) * 0.53)
            contentWidth: availableWidth
            clip: true
            Label {
                id: copy
                objectName: "leadershipReview"
                width: parent.width
                text: panel.desk.reviewText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: panel.textSize
            }
        }
        footer: DialogButtonBox {
            Action {
                objectName: "confirmLeadershipAction"
                text: qsTr("Confirm")
                enabled: panel.editable && panel.desk.reviewText.length > 0
                onClicked: { panel.desk.confirm(); review.close() }
            }
            Action { text: qsTr("Back"); onClicked: review.close() }
        }
        onClosed: panel.desk.clearReview()
    }
    Connections { target: panel.desk; function onReviewReady() { review.open() } }
}
