import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var civic: campaign.civic
    readonly property bool editable: !campaign.busy && campaign.paused && campaign.companyEditable
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Copy { font.bold: true }
    component Action: Button { font.pixelSize: panel.textSize }

    Copy {text:campaign.world.accessSummary}
    Heading { text: qsTr("Civic · elections and rules") }
    Copy { text: panel.civic.assumptions }
    ComboBox {
        id: jurisdiction
        objectName: "civicJurisdiction"
        Layout.fillWidth: true
        model: panel.civic.jurisdictions
        textRole: "label"
        valueRole: "id"
        currentIndex: indexOfValue(panel.civic.jurisdiction)
        font.pixelSize: panel.textSize
        Accessible.name: qsTr("Inspect a civic jurisdiction")
        onActivated: panel.civic.jurisdiction = currentValue
    }
    GridLayout {
        Layout.fillWidth: true
        columns: panel.width >= 850 ? 2 : 1
        columnSpacing: 20
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            Heading { text: qsTr("Current officeholders and seats") }
            Copy { objectName: "civicInstitutions"; text: panel.civic.institutions }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            Heading { text: qsTr("Calendar and latest count") }
            Copy { objectName: "civicCalendar"; text: panel.civic.calendar }
            Action {
                objectName: "advanceCivic"
                text: qsTr("Advance to next civic event")
                enabled: panel.civic.hasUpcoming && campaign.canAdvance
                onClicked: campaign.advanceTo("civic")
            }
        }
    }

    Heading { text: qsTr("Public docket") }
    DraftField {
        objectName: "civicDocketFilter"
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        placeholderText: qsTr("Filter by policy, date or status")
        Accessible.name: qsTr("Filter the public docket")
        onTextChanged: panel.civic.dockets.filterText = text
    }
    StableEntityList {
        objectName: "civicDockets"
        Layout.fillWidth: true
        Layout.preferredHeight: 200
        rows: panel.civic.dockets
        textSize: panel.textSize
        reducedMotion: campaign.reducedMotion
        emptyText: qsTr("Policy proposals and rules")
        onSelected: entityId => panel.civic.dockets.selectedId = entityId
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.civic.dockets.selectedId.length > 0
        Heading { text: panel.civic.dockets.selectedRow.title || "" }
        Copy { objectName: "civicPolicyDetail"; text: panel.civic.dockets.selectedRow.detail || "" }
        Heading { text: qsTr("Public submissions") }
        Copy { objectName: "civicAdvocacyTerms"; text: panel.civic.advocacy }
        ComboBox {
            id: stance
            objectName: "civicStance"
            Layout.minimumWidth: panel.textSize * 7
            model: [qsTr("Support"), qsTr("Oppose")]
            font.pixelSize: panel.textSize
            Accessible.name: qsTr("Position on the selected proposal")
            onActivated: panel.civic.clearReview()
        }
        Action {
            objectName: "reviewCivicAdvocacy"
            text: qsTr("Review paid advocacy")
            enabled: panel.editable && panel.civic.canAdvocate
            onClicked: panel.civic.reviewAdvocacy(stance.currentIndex === 0)
        }
        Frame {
            Layout.fillWidth: true
            visible: panel.civic.reviewText.length > 0
            contentItem: ColumnLayout {
                Copy { text: panel.civic.reviewText }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Action {
                        objectName: "confirmCivicAdvocacy"
                        text: qsTr("Confirm payment and disclosure")
                        enabled: panel.editable && panel.civic.canAdvocate
                        onClicked: panel.civic.confirmAdvocacy()
                    }
                    Action { text: qsTr("Cancel review"); onClicked: panel.civic.clearReview() }
                }
            }
        }
        Copy {
            text: panel.civic.evidence.length > 0 ? qsTr("The listed reports or connected sites meet this docket’s evidence requirement.")
                : qsTr("No additional eligible evidence submission is available at this stage.")
        }
        ComboBox {
            id: evidence
            visible: count > 0
            Layout.fillWidth: true
            model: panel.civic.evidence
            font.pixelSize: panel.textSize
            Accessible.name: qsTr("Eligible independent evaluation or site record")
        }
        Action {
            visible: evidence.count > 0
            text: qsTr("Disclose selected evidence")
            enabled: panel.editable && evidence.currentIndex >= 0
            onClicked: panel.civic.discloseEvidence(evidence.currentText, stance.currentIndex === 0)
        }
    }
}
