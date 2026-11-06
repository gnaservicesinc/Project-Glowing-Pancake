import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var society: campaign.society
    readonly property bool editable: !campaign.busy && campaign.paused
    spacing: 10
    component Copy: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Copy { font.bold: true }
    component Action: Button { font.pixelSize: panel.textSize }

    Heading { text: qsTr("World · delivery, welfare and standards") }
    Copy { text: panel.society.assumptions }
    Copy { objectName: "societyGlobal"; text: panel.society.global }
    Action {
        objectName: "advanceSociety"
        text: qsTr("Advance to next regional or forum date")
        enabled: panel.society.hasUpcoming && campaign.canAdvance
        onClicked: campaign.advanceTo("society")
    }
    Heading { text: qsTr("Regional outcomes") }
    TextField {
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        placeholderText: qsTr("Filter regions")
        Accessible.name: qsTr("Filter regional outcomes")
        onTextChanged: panel.society.regions.filterText = text
    }
    StableEntityList {
        objectName: "societyRegions"
        Layout.fillWidth: true
        Layout.preferredHeight: 200
        rows: panel.society.regions
        textSize: panel.textSize
        reducedMotion: campaign.reducedMotion
        emptyText: qsTr("Regional profiles")
        onSelected: entityId => panel.society.regions.selectedId = entityId
    }
    Heading { text: panel.society.regions.selectedRow.title || "" }
    Copy { objectName: "societyRegionDetail"; text: panel.society.regions.selectedRow.detail || "" }
    GridLayout {
        Layout.fillWidth: true
        columns: panel.width >= 850 ? 2 : 1
        columnSpacing: 20
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            Heading { text: qsTr("Standards forum") }
            Copy { objectName: "societyForum"; text: panel.society.forum }
            Copy { text: qsTr("Select a region above. A completed independent report can support one paid disclosure to that member during the hearing.") }
            ComboBox {
                id: evidence
                objectName: "societyEvidence"
                Layout.fillWidth: true
                model: panel.society.evidence
                font.pixelSize: panel.textSize
                Accessible.name: qsTr("Independent report for the selected forum member")
                onActivated: panel.society.clearReview()
            }
            ComboBox {
                id: stance
                Layout.fillWidth: true
                model: [qsTr("Support"), qsTr("Oppose")]
                font.pixelSize: panel.textSize
                Accessible.name: qsTr("Position on the standards proposal")
                onActivated: panel.society.clearReview()
            }
            Action {
                objectName: "reviewForumEndorsement"
                text: qsTr("Review forum disclosure")
                enabled: panel.editable && evidence.currentIndex >= 0
                onClicked: panel.society.reviewEndorsement(evidence.currentText, stance.currentIndex === 0)
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            Heading { text: qsTr("Audience reputation") }
            Copy { objectName: "societyAudiences"; text: panel.society.audiences }
            Heading { text: qsTr("Disclosed media campaigns") }
            Copy { objectName: "societyMedia"; text: panel.society.media }
            ComboBox {
                id: service
                objectName: "societyService"
                Layout.fillWidth: true
                model: panel.society.services
                textRole: "label"
                valueRole: "id"
                font.pixelSize: panel.textSize
                Accessible.name: qsTr("Deployed service to advertise in the selected region")
                onActivated: panel.society.clearReview()
            }
            Copy { visible: service.count === 0; text: qsTr("Deploy a service with completed independent evidence in this region to prepare a campaign.") }
            ComboBox {
                id: audience
                objectName: "societyAudience"
                Layout.fillWidth: true
                model: [qsTr("Investors"), qsTr("Researchers"), qsTr("Customers"), qsTr("Staff"), qsTr("Regulators"), qsTr("Communities")]
                currentIndex: 2
                font.pixelSize: panel.textSize
                Accessible.name: qsTr("Audience for the media campaign")
                onActivated: panel.society.clearReview()
            }
            Action {
                objectName: "reviewMediaCampaign"
                text: qsTr("Review media campaign")
                enabled: panel.editable && service.currentIndex >= 0
                onClicked: panel.society.reviewMedia(service.currentValue, audience.currentIndex)
            }
        }
    }
    Frame {
        Layout.fillWidth: true
        visible: panel.society.reviewText.length > 0
        contentItem: ColumnLayout {
            Copy { objectName: "societyReview"; text: panel.society.reviewText }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Action {
                    objectName: "confirmSocietyAction"
                    text: qsTr("Confirm payment and disclosure")
                    enabled: panel.editable
                    onClicked: panel.society.confirmAction()
                }
                Action { text: qsTr("Cancel review"); onClicked: panel.society.clearReview() }
            }
        }
    }
}
