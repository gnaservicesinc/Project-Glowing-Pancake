import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    required property string person
    required property bool editable
    property bool pendingUpdate: false
    property int pendingDiscussion: 0
    property string pendingStatus: ""
    property int pendingPitchCount: -1
    property bool showRequirements: false
    property int historyLimit: 4
    property var discussionRows: []
    readonly property var funding: {
        campaign.world.reviewToken
        return campaign.world.contactFunding(person)
    }
    signal revealRequested(Item target)
    signal governanceRequested()
    spacing: 10
    visible: !!funding.available

    function revealLatest() {
        if (!pendingUpdate || confirmation.visible || !visible) return
        const rows = funding.discussions || []
        if (pendingPitchCount >= 0 && rows.length < pendingPitchCount) return
        if (pendingDiscussion && (!rows.length || rows[0].id !== pendingDiscussion || rows[0].status !== pendingStatus)) return
        const card = discussions.itemAt(0)
        if (!card) return
        pendingUpdate = false
        card.heading.forceActiveFocus(Qt.OtherFocusReason)
        revealRequested(card.heading)
    }
    function respond(discussion, reply, terms) {
        pendingUpdate = true
        pendingPitchCount = -1
        pendingDiscussion = discussion
        pendingStatus = reply === "submit" ? "board" : reply === "defer" ? "held" : "declined"
        campaign.world.respondContactFunding(person, discussion, reply, terms, campaign.world.reviewToken)
    }
    function revealCurrent() {
        pendingDiscussion = 0
        pendingStatus = ""
        pendingPitchCount = -1
        pendingUpdate = true
        revealTimer.restart()
    }
    onFundingChanged: {
        const rows = funding.discussions || []
        if (JSON.stringify(rows) !== JSON.stringify(discussionRows)) discussionRows = rows
        revealTimer.restart()
    }
    onPersonChanged: { historyLimit = 4; showRequirements = false; revealCurrent() }
    onVisibleChanged: if (visible) { if (!pendingUpdate) revealCurrent(); else revealTimer.restart() }
    Timer { id: revealTimer; interval: 50; onTriggered: panel.revealLatest() }

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        color: "#d2e5da"
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Copy { text: panel.funding.donor ? qsTr("Discuss a donor gift") : qsTr("Discuss the next round"); font.bold: true; color: "#e7c891" }
    Copy { text: panel.funding.record || ""; color: "#a9c6b8" }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        WorldButton {
            objectName: "contactFundingPitch"
            text: panel.funding.donor ? qsTr("Apply for a gift") : qsTr("Pitch for funding")
            textSize: panel.textSize
            enabled: panel.editable && !!panel.funding.canRequest
            onClicked: {
                panel.pendingUpdate = true
                panel.pendingDiscussion = 0
                panel.pendingPitchCount = (panel.funding.discussions || []).length + 1
                campaign.world.requestContactFunding(panel.person, campaign.world.reviewToken)
            }
        }
        WorldButton {
            text: panel.showRequirements ? qsTr("Hide requirements") : qsTr("Review requirements")
            textSize: panel.textSize
            onClicked: panel.showRequirements = !panel.showRequirements
        }
    }
    Copy { visible: !panel.funding.canRequest; text: (panel.funding.donor ? qsTr("Next application from %1.") : qsTr("Next pitch from %1.")).arg(panel.funding.next || "") }
    Copy { visible: panel.showRequirements; text: panel.funding.requirements || "" }
    Repeater {
        id: discussions
        model: panel.discussionRows.slice(0, panel.historyLimit)
        onItemAdded: (index, item) => { if (index === 0) revealTimer.restart() }
        Rectangle {
            id: discussionCard
            required property var modelData
            property alias heading: statusCopy
            Layout.fillWidth: true
            implicitHeight: discussionBody.implicitHeight + 24
            radius: 10
            color: "#26464a"
            border.color: "#65877a"
            ColumnLayout {
                id: discussionBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 10
                Copy { text: (panel.funding.donor ? qsTr("%1 · Donor reply") : qsTr("%1 · Investor reply")).arg(discussionCard.modelData.date); color: "#a9c6b8"; font.pixelSize: panel.textSize * 0.85 }
                Copy { id: statusCopy; objectName: "contactFundingStatus"; property bool revealAtTop: true; text: discussionCard.modelData.label; font.bold: true; color: "#e7c891" }
                Copy { objectName: "contactFundingReply"; text: discussionCard.modelData.reply }
                Copy { text: discussionCard.modelData.record; color: "#b9cdbd"; font.pixelSize: panel.textSize * 0.9 }
                Copy { visible: discussionCard.modelData.open; text: qsTr("Choose before %1. Board approval has its own voting window.").arg(discussionCard.modelData.expires) }
                Repeater {
                    model: discussionCard.modelData.offers
                    Rectangle {
                        id: offerCard
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: offerBody.implicitHeight + 20
                        color: "#19383d"
                        radius: 8
                        border.color: discussionCard.modelData.selected === modelData.id ? "#d1bc7b" : "#537166"
                        ColumnLayout {
                            id: offerBody
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 10
                            spacing: 8
                            Copy { text: offerCard.modelData.name; font.bold: true }
                            Copy { text: (offerCard.modelData.donation ? qsTr("%1 institutional gift") : qsTr("%1 company capital")).arg(offerCard.modelData.cash); color: "#bfe0a5"; font.pixelSize: panel.textSize * 1.1 }
                            Copy { text: offerCard.modelData.donation ? qsTr("No shares sold · %1").arg(offerCard.modelData.seat ? qsTr("one trustee seat") : qsTr("no new trustee seat")) : qsTr("%1 post-issue ownership · %2").arg(offerCard.modelData.equity).arg(offerCard.modelData.seat ? qsTr("one board seat") : qsTr("no new board seat")) }
                            Copy {
                                visible: discussionCard.modelData.open && !offerCard.modelData.donation
                                text: qsTr("Your current stake: %1 → %2 on approval").arg(offerCard.modelData.founderBefore).arg(offerCard.modelData.founderAfter)
                            }
                            Rectangle {
                                visible: discussionCard.modelData.open && !offerCard.modelData.donation
                                Layout.fillWidth: true
                                implicitHeight: 10
                                radius: 5
                                color: "#64756b"
                                Rectangle { width: parent.width * offerCard.modelData.fraction; height: parent.height; radius: 5; color: "#b8d99a" }
                                Accessible.role: Accessible.Indicator
                                Accessible.name: qsTr("Your ownership after this round: %1").arg(offerCard.modelData.founderAfter)
                            }
                            WorldButton {
                                objectName: "contactFundingReview_" + offerCard.modelData.id
                                visible: discussionCard.modelData.open
                                text: qsTr("Review these terms…")
                                textSize: panel.textSize
                                Layout.maximumWidth: panel.width - 44
                                enabled: panel.editable
                                onClicked: {
                                    confirmation.person = panel.person
                                    confirmation.discussion = discussionCard.modelData.id
                                    confirmation.terms = offerCard.modelData.id
                                    confirmation.revision = campaign.world.reviewToken
                                    confirmation.quote = campaign.world.contactFundingQuote(confirmation.person, confirmation.discussion, confirmation.terms)
                                    confirmation.open()
                                }
                            }
                        }
                    }
                }
                Flow {
                    visible: discussionCard.modelData.open
                    Layout.fillWidth: true
                    spacing: 8
                    WorldButton {
                        objectName: "contactFundingHold"
                        text: qsTr("Think it over")
                        textSize: panel.textSize
                        enabled: panel.editable && !discussionCard.modelData.held
                        onClicked: panel.respond(discussionCard.modelData.id, "defer", "")
                    }
                    WorldButton {
                        objectName: "contactFundingDecline"
                        text: qsTr("Decline these offers")
                        textSize: panel.textSize
                        width: Math.min(implicitWidth, parent.width)
                        enabled: panel.editable
                        onClicked: panel.respond(discussionCard.modelData.id, "decline", "")
                    }
                }
                WorldButton {
                    objectName: "contactFundingBoard"
                    visible: discussionCard.modelData.submitted
                    text: qsTr("Open the board")
                    textSize: panel.textSize
                    onClicked: panel.governanceRequested()
                }
            }
        }
    }
    WorldButton {
        visible: (panel.funding.discussions || []).length > panel.historyLimit
        text: qsTr("Show older discussions")
        textSize: panel.textSize
        onClicked: panel.historyLimit += 8
    }
    GameDialog {
        id: confirmation
        property string person: ""
        property int discussion: 0
        property string terms: ""
        property string revision: ""
        property var quote: ({})
        title: panel.funding.donor ? qsTr("Review donor gift") : qsTr("Review investor terms")
        modal: true
        focus: true
        anchors.centerIn: Overlay.overlay
        width: Math.min((Overlay.overlay ? Overlay.overlay.width : 650) - 32, panel.textSize * 36)
        height: Math.min((Overlay.overlay ? Overlay.overlay.height : 480) - 32, panel.textSize * 28)
        header: Item { visible: false; implicitHeight: 0 }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: campaign.reducedMotion ? 0 : 150 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: campaign.reducedMotion ? 0 : 100 } }
        onOpened: if (reviewScroll.contentItem) reviewScroll.contentItem.contentY = 0
        onClosed: revealTimer.restart()
        contentItem: ScrollView {
            id: reviewScroll
            objectName: "contactFundingReviewScroll"
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            ColumnLayout {
                width: reviewScroll.availableWidth
                Copy { text: confirmation.title; font.bold: true; color: "#e7c891" }
                Copy { objectName: "contactFundingTerms"; text: confirmation.quote.detail || "" }
            }
        }
        footer: Flow {
            width: confirmation.availableWidth
            padding: 8
            spacing: 8
            WorldButton { objectName: "contactFundingBack"; text: qsTr("Back"); textSize: panel.textSize; onClicked: confirmation.close() }
            WorldButton {
                objectName: "contactFundingConfirm"
                text: panel.funding.donor ? qsTr("Send to trustees") : qsTr("Send to the board")
                textSize: panel.textSize
                width: Math.min(implicitWidth, parent.width - 16)
                primary: true
                enabled: panel.editable && !!confirmation.quote.allowed && confirmation.revision === campaign.world.reviewToken
                onClicked: {
                    panel.respond(confirmation.discussion, "submit", confirmation.terms)
                    confirmation.close()
                }
            }
        }
    }
}
