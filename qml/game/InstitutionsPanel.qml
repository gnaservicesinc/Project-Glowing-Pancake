import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var institutions: campaign.institutions
    readonly property bool editable: institutions.editable && !campaign.busy && campaign.companyEditable
    spacing: 10

    function showDisputes() {
        institutions.selectRequiredCase()
        section.currentIndex = 2
    }

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Copy { font.bold: true }
    component Action: WorldButton { font.pixelSize: panel.textSize }
    component Picker: DraftComboBox {
        id: picker
        draftOwner: picker
        draftProperty: "rememberedId"
        draftResetValue: ""
        property string rememberedId: ""
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        textRole: "label"
        valueRole: "id"
        currentValue: model && model.some(row => row.id === rememberedId)
                      ? rememberedId : count > 0 ? model[0].id : ""
        onActivated: {
            rememberedId = currentValue
            panel.institutions.clearReview()
        }
    }
    component Records: ColumnLayout {
        id: records
        required property var rows
        required property string emptyText
        required property string listName
        StableEntityList {
            objectName: records.listName
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, Math.max(100, records.rows.count * panel.textSize * 4))
            rows: records.rows
            textSize: panel.textSize
            emptyText: records.emptyText
            reducedMotion: campaign.reducedMotion
            onSelected: id => records.rows.selectedId = id
        }
        Copy { text: records.rows.selectedRow.detail || ""; visible: text.length > 0 }
    }

    Copy {text:campaign.world.accessSummary}
    Heading { text: qsTr("Institutions · agreements and public commitments") }
    Copy { objectName: "institutionCoverage"; text: panel.institutions.coverage }
    Action {
        objectName: "advanceInstitutions"
        text: qsTr("Advance to next institutional date")
        enabled: panel.institutions.hasUpcoming && campaign.canAdvance
        onClicked: campaign.advanceTo("institutions")
    }
    DraftComboBox {
        id: section
        objectName: "institutionsSection"
        draftIds: ["authority", "public-record", "disputes"]
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        model: [qsTr("Authority"), qsTr("Public record"), qsTr("Disputes")]
        Accessible.name: qsTr("Institutions section")
        onActivated: panel.institutions.clearReview()
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex !== 2
        Heading { text: qsTr("Region and issuing institution") }
        Records {
            Layout.fillWidth: true
            listName: "institutionRegions"
            rows: panel.institutions.regions
            emptyText: qsTr("Select a region")
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 0
        Heading { text: qsTr("Apply for a funded agreement") }
        Picker {
            id: role
            objectName: "institutionRole"
            model: panel.institutions.roles
            Accessible.name: qsTr("Institutional role")
        }
        Copy { text: role.currentIndex >= 0 ? role.model[role.currentIndex].terms : "" }
        Copy { text: qsTr("Deploy a service with public customers in this region, then disclose its independent evaluation under Public record.") }
        Picker {
            id: service
            objectName: "institutionService"
            model: panel.institutions.services
            Accessible.name: qsTr("Public service for the agreement")
        }
        Picker {
            id: authoritySource
            objectName: "institutionSource"
            model: panel.institutions.sources
            Accessible.name: qsTr("Compute source for the agreement")
        }
        Action {
            objectName: "reviewInstitutionMandate"
            text: qsTr("Review application terms")
            enabled: panel.editable && role.currentIndex >= 0 && service.currentIndex >= 0 && authoritySource.currentIndex >= 0
            onClicked: panel.institutions.reviewMandate(role.currentValue, service.currentValue, authoritySource.currentValue)
        }
        Heading { text: qsTr("Agreements") }
        Records {
            Layout.fillWidth: true
            listName: "institutionMandates"
            rows: panel.institutions.mandates
            emptyText: qsTr("Your company has no agreements")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Action {
                objectName: "reviewInstitutionProcurement"
                text: panel.institutions.mandates.selectedRow.fields?.restricted
                      ? qsTr("Review ending procurement rule") : qsTr("Review procurement rule")
                enabled: panel.editable && (panel.institutions.mandates.selectedRow.fields?.directive ?? false)
                onClicked: panel.institutions.reviewProcurement()
            }
            Action {
                objectName: "reviewInstitutionRelinquishment"
                text: qsTr("Review relinquishing agreement")
                enabled: panel.editable && (panel.institutions.mandates.selectedRow.fields?.relinquish ?? false)
                onClicked: panel.institutions.reviewRelinquishment()
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 1
        Heading { text: qsTr("Disclose independent evidence") }
        Picker {
            id: testimony
            objectName: "institutionReport"
            model: panel.institutions.reports
            Accessible.name: qsTr("Independent report to disclose")
        }
        Action {
            objectName: "reviewInstitutionTestimony"
            text: qsTr("Review public testimony")
            enabled: panel.editable && testimony.currentIndex >= 0
            onClicked: panel.institutions.reviewTestimony(testimony.currentValue, false)
        }
        Heading { text: qsTr("Make a dated public commitment") }
        Picker {
            id: pledgeService
            objectName: "institutionPledgeService"
            model: panel.institutions.services
            Accessible.name: qsTr("Deployed service to commit in the selected region")
        }
        Action {
            objectName: "reviewInstitutionServicePromise"
            text: qsTr("Review public delivery commitment")
            enabled: panel.editable && pledgeService.currentIndex >= 0
            onClicked: panel.institutions.reviewPromise(pledgeService.currentValue, false)
        }
        Picker {
            id: research
            model: panel.institutions.research
            Accessible.name: qsTr("Research to commit to publishing")
        }
        Action {
            objectName: "reviewInstitutionPublicationPromise"
            text: qsTr("Review publication commitment")
            enabled: panel.editable && research.currentIndex >= 0
            onClicked: panel.institutions.reviewPromise(research.currentValue, true)
        }
        Records {
            Layout.fillWidth: true
            listName: "institutionPromises"
            rows: panel.institutions.promises
            emptyText: qsTr("Your company has no public commitments")
        }
        Heading { text: qsTr("Audience confidence and recent evidence") }
        Copy { objectName: "institutionIdentity"; text: parent.visible ? panel.institutions.identity : "" }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 2
        Heading { text: qsTr("Public and executive disputes") }
        Records {
            Layout.fillWidth: true
            listName: "institutionCases"
            rows: panel.institutions.cases
            emptyText: qsTr("There are no recorded disputes")
        }
        Action {
            text: qsTr("Acknowledge selected notice")
            enabled: panel.editable && (panel.institutions.cases.selectedRow.required ?? false)
            onClicked: panel.institutions.acknowledgeCase()
        }
        Picker {
            id: citation
            model: panel.institutions.reports
            Accessible.name: qsTr("Independent report to cite in the selected case")
        }
        Action {
            objectName: "reviewInstitutionCitation"
            text: qsTr("Review citing report")
            enabled: panel.editable && citation.currentIndex >= 0 && (panel.institutions.cases.selectedRow.fields?.cite ?? false)
            onClicked: panel.institutions.reviewTestimony(citation.currentValue, true)
        }
        Picker {
            id: responseSource
            model: panel.institutions.sources
            Accessible.name: qsTr("Compute source for negotiated review")
        }
        Action {
            objectName: "reviewInstitutionNegotiation"
            text: qsTr("Review funded response")
            enabled: panel.editable && responseSource.currentIndex >= 0 && (panel.institutions.cases.selectedRow.fields?.negotiate ?? false)
            onClicked: panel.institutions.reviewNegotiation(responseSource.currentValue)
        }
    }

    GameDialog {
        id: review
        objectName: "institutionsReview"
        title: qsTr("Review institutional terms")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(720, (Overlay.overlay ? Overlay.overlay.width : 720) - 32)
        font.pixelSize: panel.textSize
        contentItem: ScrollView {
            implicitHeight: Math.min(reviewCopy.implicitHeight + 8, (Overlay.overlay ? Overlay.overlay.height : 720) * 0.53)
            contentWidth: availableWidth
            clip: true
            Label {
                id: reviewCopy
                objectName: "institutionsReviewText"
                width: parent.width
                text: panel.institutions.reviewText
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                font.pixelSize: panel.textSize
            }
        }
        footer: DialogButtonBox {
            Action {
                objectName: "confirmInstitutionAction"
                text: qsTr("Confirm")
                enabled: panel.editable && panel.institutions.reviewText.length > 0
                onClicked: { panel.institutions.confirmAction(); review.close() }
            }
            Action { text: qsTr("Back"); onClicked: review.close() }
        }
        onClosed: panel.institutions.clearReview()
    }
    Connections {
        target: panel.institutions
        function onReviewReady() { review.open() }
    }
}
