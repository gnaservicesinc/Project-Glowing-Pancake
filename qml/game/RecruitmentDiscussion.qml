import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: discussion
    required property string applicantId
    required property int textSize
    required property bool editable
    readonly property var offer: {
        const revision = campaign.world.reviewToken
        return applicantId.length ? campaign.world.recruitmentOffer(applicantId) : ({})
    }
    property int selectedDesk: 0
    signal closeRequested()
    signal suppliersRequested()
    color: "#1e393b"
    border.color: "#8f9c7c"
    radius: 12
    implicitHeight: content.implicitHeight + 28

    function focusDiscussion() {
        if (visible) heading.forceActiveFocus(Qt.TabFocusReason)
    }
    function restoreOffer() {
        if (!applicantId.length || !offer.id) return
        salary.text = campaign.world.draft("recruitment/salary/" + applicantId, offer.salary)
        signing.text = campaign.world.draft("recruitment/signing/" + applicantId, offer.signing)
        selectedDesk = Number(campaign.world.draft("recruitment/desk/" + applicantId, "0"))
    }
    onApplicantIdChanged: Qt.callLater(restoreOffer)
    Component.onCompleted: Qt.callLater(restoreOffer)
    Connections {
        target: campaign.world
        function onDraftsReset() { Qt.callLater(discussion.restoreOffer) }
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 12
        component Copy: Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.pixelSize: discussion.textSize
            color: "#c9dacc"
        }
        Copy {
            id: heading
            text: qsTr("Compensation discussion · %1").arg(discussion.offer.name || "")
            font.bold: true
            font.pixelSize: discussion.textSize * 1.2
            color: "#f2e3be"
        }
        Copy { objectName: "employmentStatus"; text: discussion.offer.summary || ""; color: "#f0cf91" }
        Copy { visible: !!discussion.offer.canOffer || !!discussion.offer.canHire; text: discussion.offer.terms || "" }
        ColumnLayout {
            visible: !!discussion.offer.canOffer
            Layout.fillWidth: true
            spacing: 6
            Copy { text: qsTr("Monthly salary in dollars") }
            GameField {
                id: salary
                objectName: "employmentSalary"
                Layout.fillWidth: true
                textSize: discussion.textSize
                maximumLength: 11
                Accessible.name: qsTr("Monthly salary in dollars")
                onTextEdited: campaign.world.setDraft("recruitment/salary/" + discussion.applicantId, text)
            }
            Copy { text: qsTr("One-time signing payment in dollars") }
            GameField {
                id: signing
                objectName: "employmentSigning"
                Layout.fillWidth: true
                textSize: discussion.textSize
                maximumLength: 11
                Accessible.name: qsTr("Signing payment in dollars")
                onTextEdited: campaign.world.setDraft("recruitment/signing/" + discussion.applicantId, text)
            }
        }
        ColumnLayout {
            visible: !!discussion.offer.canHire
            Layout.fillWidth: true
            Copy { text: qsTr("Choose a furnished desk for this hire") }
            GameSelect {
                objectName: "employmentDesk"
                Layout.fillWidth: true
                textSize: discussion.textSize
                model: [{id:0,name:qsTr("Choose an unassigned desk")}].concat(discussion.offer.desks || [])
                textRole: "name"
                valueRole: "id"
                currentIndex: Math.max(0, model.findIndex(row => row.id === discussion.selectedDesk))
                Accessible.name: qsTr("Desk for the new employee")
                onActivated: {
                    discussion.selectedDesk = currentValue
                    campaign.world.setDraft("recruitment/desk/" + discussion.applicantId, String(currentValue))
                }
            }
            Copy {
                visible: (discussion.offer.desks || []).length === 0
                text: qsTr("There is no free furnished desk. Buy and place another desk, or free an existing one before the terms expire.")
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            WorldButton {
                objectName: "employmentBack"
                text: qsTr("Back to applicants")
                textSize: discussion.textSize
                onClicked: discussion.closeRequested()
            }
            WorldButton {
                objectName: "employmentSend"
                visible: !!discussion.offer.canOffer
                text: discussion.offer.status === "counter" ? qsTr("Send revised offer") : qsTr("Send offer")
                textSize: discussion.textSize
                primary: true
                enabled: discussion.editable
                onClicked: campaign.world.proposeEmployment(discussion.applicantId, salary.text, signing.text, discussion.offer.revision)
            }
            WorldButton {
                objectName: "employmentWait"
                visible: discussion.offer.status === "waiting"
                text: qsTr("Advance one day")
                textSize: discussion.textSize
                enabled: discussion.editable && campaign.canAdvance
                onClicked: campaign.step()
            }
            WorldButton {
                objectName: "employmentAcceptCounter"
                visible: !!discussion.offer.canAccept
                text: qsTr("Agree to counteroffer")
                textSize: discussion.textSize
                primary: true
                enabled: discussion.editable
                onClicked: campaign.world.acceptEmploymentCounter(discussion.applicantId, discussion.offer.revision)
            }
            WorldButton {
                objectName: "employmentDecline"
                visible: !!discussion.offer.canDecline
                text: qsTr("Decline applicant")
                textSize: discussion.textSize
                enabled: discussion.editable
                onClicked: campaign.world.declineApplicant(discussion.applicantId, discussion.offer.revision)
            }
            WorldButton {
                visible: !!discussion.offer.canHire && (discussion.offer.desks || []).length === 0
                text: qsTr("Open suppliers")
                textSize: discussion.textSize
                onClicked: discussion.suppliersRequested()
            }
            WorldButton {
                objectName: "employmentSign"
                visible: !!discussion.offer.canHire
                text: qsTr("Sign and assign desk")
                textSize: discussion.textSize
                primary: true
                enabled: discussion.editable && (discussion.offer.desks || []).some(row => row.id === discussion.selectedDesk)
                onClicked: campaign.world.hireNegotiatedEmployee(discussion.applicantId, discussion.selectedDesk, discussion.offer.revision)
            }
        }
        Repeater {
            model: discussion.offer.history || []
            Copy { required property var modelData; text: modelData.detail }
        }
    }
}
