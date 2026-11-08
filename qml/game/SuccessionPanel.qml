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
    component Action: Button {
        id: action
        font.pixelSize: panel.textSize
        padding: panel.textSize * 0.65
        implicitHeight: Math.max(panel.textSize * 2.6, contentItem.implicitHeight + topPadding + bottomPadding)
        width: Math.min(implicitWidth, panel.width)
        Layout.maximumWidth: panel.width
        contentItem: Text { text: action.text; font: action.font; color: action.palette.buttonText; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.Wrap }
    }
    component Field: ColumnLayout {
        id: field
        required property string label
        required property string inputName
        property string initialValue: ""
        readonly property string value: entry.text
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Copy { text: field.label }
        DraftField {
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
        property string chosenId: ""
        currentIndex: model.length ? Math.max(0, model.findIndex(row => row.id === chosenId)) : -1
        Component.onCompleted: chosenId = campaign.world.draft(objectName + "/selection", "")
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        textRole: "label"
        valueRole: "id"
        onActivated: { chosenId = currentValue; campaign.world.setDraft(objectName + "/selection", currentValue); panel.desk.clearReview() }
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
        Layout.fillWidth: true
        text: qsTr("Founder moves to the personal chair role")
        contentItem: Label { text: chair.text; font: chair.font; leftPadding: chair.indicator.width + chair.spacing; wrapMode: Text.Wrap }
        checked: campaign.world.draft("leadership/chair", "false") === "true"
        onToggled: { campaign.world.setDraft("leadership/chair", checked ? "true" : "false"); panel.desk.clearReview() }
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
        Field { id: rate; inputName: "leadershipRate"; label: qsTr("Compute per day (CU)"); initialValue: panel.desk.recommendedRate }
        Field { id: staff; inputName: "leadershipStaff"; label: qsTr("Staff for preparation"); initialValue: "1" }
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
    Copy { text: qsTr("Announce the founder handover"); font.bold: true; visible: message.model.length > 0 }
    Copy { text: qsTr("Fund and approve a founder successor plan, then choose its public message. Complete the preparation and handover before retiring. Keeping the personal chair role lets you continue guiding the company."); visible: message.model.length > 0 }
    Picker { id: message; objectName: "departureMessage"; model: panel.desk.departureMessages; visible: model.length > 0; Accessible.name: qsTr("Public handover message") }
    Copy { text: message.currentIndex >= 0 ? message.model[message.currentIndex].detail : ""; visible: text.length > 0 }
    Action { objectName: "reviewDepartureAnnouncement"; text: qsTr("Review public announcement"); visible: message.model.length > 0; enabled: panel.editable && (panel.desk.plans.selectedRow.fields?.announce ?? false); onClicked: panel.desk.reviewAnnouncement(message.currentValue) }
    Copy { objectName: "departureHistory"; text: panel.desk.departureHistory; visible: text.length > 0 }
    Action { objectName: "reviewFounderRunRetirement"; text: qsTr("Review retirement"); visible: message.model.length > 0; enabled: panel.editable && panel.desk.canFinish; onClicked: panel.desk.reviewFinish() }
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
