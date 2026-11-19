import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property string mission
    required property bool editable
    required property int textSize
    readonly property var authority: campaign.world.companyAuthority || ({})
    readonly property var choices: [
        {id:"research", name:qsTr("Research")},
        {id:"public benefit", name:qsTr("Public benefit")},
        {id:"commercial", name:qsTr("Commercial")}
    ]
    function nameFor(id) { return (choices.find(choice => choice.id === id) || {}).name || id }
    spacing: 8

    Flow {
        Layout.fillWidth: true
        spacing: 8
        Label {
            text: qsTr("Company mission · %1").arg(panel.nameFor(panel.mission))
            font.pixelSize: panel.textSize
            color: "#dbe4e3"
        }
        WorldButton {
            objectName: "reviewCompanyMission"
            text: qsTr("Change direction")
            textSize: panel.textSize
            enabled: panel.editable
            onClicked: {
                review.revision = campaign.world.reviewToken
                review.open()
            }
        }
    }

    Rectangle {
        visible: !!panel.authority.totalSeats
        Layout.fillWidth: true
        implicitHeight: authCol.implicitHeight + 16
        radius: 8
        color: "#1b2e3d"
        border.color: panel.authority.canChangeMission ? "#4f7c6a" : "#9c7c54"
        ColumnLayout {
            id: authCol
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: qsTr("Authority: Economic ownership %1 · Board votes %2 of %3%4")
                    .arg(panel.authority.founderEquityText || "0%")
                    .arg(panel.authority.founderSeats || 0)
                    .arg(panel.authority.totalSeats || 0)
                    .arg(panel.authority.inheritedPurpose ? qsTr(" · Inherited purpose") : "")
                font.pixelSize: panel.textSize * 0.88
                color: "#d4e6e2"
                wrapMode: Text.Wrap
            }
            Label {
                visible: !!panel.authority.charterConstraints
                Layout.fillWidth: true
                text: panel.authority.charterConstraints || ""
                font.pixelSize: panel.textSize * 0.82
                color: "#aec9c3"
                wrapMode: Text.Wrap
            }
            Label {
                objectName: "missionAuthorityBlocker"
                visible: !panel.authority.canChangeMission && !!panel.authority.blocker
                Layout.fillWidth: true
                text: panel.authority.blocker || ""
                font.pixelSize: panel.textSize * 0.84
                color: "#f1c492"
                wrapMode: Text.Wrap
            }
            Flow {
                visible: !panel.authority.hasVotingControl && ((panel.authority.economicStakeShares || 0) > 0 || !!panel.authority.hasEconomicControl)
                Layout.fillWidth: true
                spacing: 8
                WorldButton {
                    objectName: "buyEconomicStakeButton"
                    visible: !panel.authority.hasEconomicControl && (panel.authority.economicStakeShares || 0) > 0
                    text: qsTr("Buy economic stake (%1 personal)").arg(panel.authority.economicStakePrice || "")
                    textSize: panel.textSize * 0.88
                    enabled: panel.editable && !!panel.authority.canBuyEconomicStake
                    onClicked: campaign.world.acquireCompanyControl("buy_economic_stake", panel.authority.revision)
                }
                WorldButton {
                    objectName: "acquireVotingControlButton"
                    visible: !!panel.authority.hasEconomicControl && !panel.authority.hasVotingControl
                    text: qsTr("Acquire voting control (%1 personal + %2 fee)").arg(panel.authority.votingControlPrice || "").arg(panel.authority.legalFee || "")
                    textSize: panel.textSize * 0.88
                    enabled: panel.editable && !!panel.authority.canAcquireVotingControl
                    onClicked: campaign.world.acquireCompanyControl("acquire_voting_control", panel.authority.revision)
                }
            }
        }
    }

    Rectangle {
        objectName: "missionScandalBanner"
        visible: !!panel.authority.scandalSummary
        Layout.fillWidth: true
        implicitHeight: scandalCol.implicitHeight + 20
        radius: 8
        color: panel.authority.scandalActive ? "#3f2626" : "#22333b"
        border.color: panel.authority.scandalActive ? "#cf7e6a" : "#6b8882"
        ColumnLayout {
            id: scandalCol
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 10
            spacing: 6
            Label {
                Layout.fillWidth: true
                text: panel.authority.scandalActive ? qsTr("Public Scandal · Inherited Purpose Reversal") : qsTr("Mission Reversal Record")
                font.bold: true
                font.pixelSize: panel.textSize * 0.95
                color: panel.authority.scandalActive ? "#ffd3b6" : "#d4e6e2"
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: panel.authority.scandalSummary || ""
                font.pixelSize: panel.textSize * 0.86
                color: "#f2e5d5"
                wrapMode: Text.Wrap
            }
            Flow {
                visible: !!panel.authority.scandalActive
                Layout.fillWidth: true
                spacing: 8
                WorldButton {
                    objectName: "scandalDefendButton"
                    text: qsTr("Defend pivot & honor contracts ($0)")
                    textSize: panel.textSize * 0.86
                    enabled: panel.editable
                    onClicked: campaign.world.respondToMissionScandal("defend_and_honor", panel.authority.revision)
                }
                WorldButton {
                    objectName: "scandalTransitionReserveButton"
                    text: qsTr("Fund transition reserve ($50,000)")
                    textSize: panel.textSize * 0.86
                    enabled: panel.editable
                    onClicked: campaign.world.respondToMissionScandal("transition_reserve", panel.authority.revision)
                }
                WorldButton {
                    objectName: "scandalSeveranceButton"
                    text: qsTr("Offer voluntary severance ($30,000)")
                    textSize: panel.textSize * 0.86
                    enabled: panel.editable
                    onClicked: campaign.world.respondToMissionScandal("voluntary_severance", panel.authority.revision)
                }
            }
        }
    }

    GameDialog {
        id: review
        objectName: "missionReviewDialog"
        title: qsTr("Change the company mission")
        anchors.centerIn: Overlay.overlay
        width: Math.min(panel.textSize*37,(Overlay.overlay?.width ?? 650)-32)
        height: Math.min(panel.textSize*26,(Overlay.overlay?.height ?? 480)-32)
        modal: true
        focus: true
        property string revision: ""
        property string selected: ""
        readonly property var options: panel.choices.filter(choice => choice.id !== panel.mission)
        readonly property var affected: selected ? campaign.world.missionPreview(selected) : []
        onOpened: selected = (options[0] || {}).id || ""

        contentItem: ScrollView {
            id: scroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12
                Label {
                    visible: !panel.authority.canChangeMission
                    Layout.fillWidth: true
                    text: panel.authority.blocker || qsTr("Board voting control is required before changing the company mission.")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: panel.textSize
                    color: "#f1c492"
                }
                GameSelect {
                    objectName: "newCompanyMission"
                    Layout.fillWidth: true
                    textSize: panel.textSize
                    textRole: "name"
                    model: review.options
                    enabled: !!panel.authority.canChangeMission
                    currentIndex: Math.max(0, review.options.findIndex(choice => choice.id === review.selected))
                    onActivated: review.selected = review.options[currentIndex].id
                    Accessible.name: qsTr("New company mission")
                }
                Label {
                    visible: !!panel.authority.inheritedPurpose && !!panel.authority.canChangeMission
                    Layout.fillWidth: true
                    text: qsTr("Warning: Reversing this company's inherited mission will trigger a public scandal, customer backlash, and immediate staff concerns.")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: panel.textSize * 0.9
                    color: "#ffd3b6"
                }
                Label {
                    Layout.fillWidth: true
                    visible: review.affected.length > 0
                    text: qsTr("Likely to raise concerns\n%1").arg(review.affected.join(", "))
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: panel.textSize
                    color: "#eed69e"
                }
            }
        }
        footer: Flow {
            spacing: 8
            padding: 10
            WorldButton {
                text: qsTr("Cancel")
                textSize: panel.textSize
                onClicked: review.close()
            }
            WorldButton {
                objectName: "confirmCompanyMission"
                text: qsTr("Change mission")
                textSize: panel.textSize
                enabled: panel.editable && !!panel.authority.canChangeMission && review.selected.length > 0
                onClicked: {
                    campaign.world.changeCompanyMission(review.selected,review.revision)
                    review.close()
                }
            }
        }
    }
}
