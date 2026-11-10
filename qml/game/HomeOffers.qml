import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property string mode
    required property int textSize
    required property var offers
    required property string selectedId
    property string errorText: ""
    readonly property var selected: mode === "messages" ? offers.find(offer => offer.id === selectedId) : undefined
    readonly property var inbox: offers.filter(offer => offer.status !== "listed")
    signal opened(string id)
    signal replied(string id, string response)
    signal closeThread()
    signal cofounderRequested()
    property bool canMeetCofounder: false
    spacing: 12

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: "#d2e5da"
    }
    component Heading: Copy {
        font.pixelSize: panel.textSize * 1.4
        font.bold: true
        color: "#f1e3bc"
    }
    component Sender: RowLayout {
        id: sender
        required property var offer
        Layout.fillWidth: true
        spacing: 12
        Rectangle {
            Layout.preferredWidth: panel.textSize * 2.25
            Layout.preferredHeight: Layout.preferredWidth
            Layout.alignment: Qt.AlignTop
            radius: 8
            color: sender.offer.kind === "career" ? "#ba946e" : sender.offer.kind === "donor" ? "#a1a0c0" : "#a1b79c"
            Label {
                anchors.centerIn: parent
                visible: !sender.offer.portrait
                text: sender.offer.name.split(" ").slice(0, 2).map(word => word[0]).join("")
                color: "#193136"
                font.pixelSize: panel.textSize * 0.95
                font.bold: true
            }
            Image {
                anchors.fill: parent; anchors.margins: 3
                visible: !!sender.offer.portrait
                source: sender.offer.portrait || ""
                fillMode: Image.PreserveAspectFit
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Copy { text: sender.offer.name; font.bold: true }
            Copy { text: sender.offer.organization; font.pixelSize: panel.textSize * 0.82; color: "#aed2c5" }
        }
    }

    Heading {
        visible: !panel.selected
        text: panel.mode === "labs" ? qsTr("Alder Research · Careers") : panel.mode === "investors" ? qsTr("Founders’ Exchange") : qsTr("Messages")
    }
    Copy {
        visible: panel.mode !== "messages" || (!panel.selected && panel.inbox.length === 0)
        text: panel.mode === "labs" ? qsTr("A small research lab is looking for a chief executive who can attract funding and talent.")
            : panel.mode === "investors" ? qsTr("Pitch investors for a corporation or approach a donor for a nonprofit. Review the capital and board terms in Messages.")
            : qsTr("Your inbox is empty. Visit the careers or investor websites to start a conversation.")
    }
    Repeater {
        model: panel.selected ? [] : panel.mode === "messages" ? panel.inbox
            : panel.offers.filter(offer => offer.available && (panel.mode === "labs" ? offer.kind === "career" : offer.kind !== "career"))
        Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: listing.implicitHeight + 24
            radius: 10
            color: "#23434a"
            border.color: "#567c76"
            ColumnLayout {
                id: listing
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 8
                Sender { offer: modelData }
                Copy {
                    text: modelData.status === "listed" ? modelData.subject : modelData.subject + " · " + modelData.statusText
                }
                WorldButton {
                    objectName: panel.mode === "labs" ? "homeApplyCEO" : "homeContact_" + modelData.id
                    text: modelData.status !== "listed" ? qsTr("Open conversation")
                        : modelData.kind === "career" ? qsTr("Send leadership application")
                        : modelData.kind === "donor" ? qsTr("Send grant proposal")
                        : modelData.kind === "partner" ? qsTr("Discuss a partnership") : qsTr("Send pitch")
                    textSize: panel.textSize
                    Layout.maximumWidth: listing.width
                    onClicked: panel.opened(modelData.id)
                }
            }
        }
    }
    WorldButton {
        visible: panel.mode === "investors" && panel.canMeetCofounder
        text: qsTr("Meet a potential cofounder")
        textSize: panel.textSize
        Layout.maximumWidth: panel.width
        onClicked: panel.cofounderRequested()
    }
    ColumnLayout {
        visible: !!panel.selected
        Layout.fillWidth: true
        spacing: 12
        WorldButton {
            objectName: "homeCloseThread"
            text: qsTr("← Inbox")
            Accessible.name: qsTr("Back to all conversations")
            textSize: panel.textSize
            onClicked: panel.closeThread()
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: thread.implicitHeight + 28
            radius: 12
            color: "#23434a"
            border.color: "#64877e"
            ColumnLayout {
                id: thread
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 14
                spacing: 10
                Sender { offer: panel.selected || ({kind:"career",name:"",organization:""}) }
                Heading { objectName: "homeOfferSubject"; text: panel.selected?.subject || "" }
                Copy { objectName: "homeOfferStatus"; text: panel.selected?.statusText || ""; color: "#bddda7"; font.bold: true }
                Copy {
                    objectName: "homeOfferTerms"
                    visible: !!panel.selected && panel.selected.status !== "rejected"
                    text: panel.selected?.terms || ""
                    font.bold: true
                    color: "#f1e3bc"
                }
                Copy { objectName: "homeOfferMessage"; text: panel.selected?.body || "" }
            }
        }
        Flow {
            visible: !!panel.selected && ["offered", "considering", "selected"].includes(panel.selected.status)
            Layout.fillWidth: true
            spacing: 8
            WorldButton {
                objectName: "homeAcceptOffer"
                text: panel.selected?.status === "selected" ? qsTr("Review signing") : qsTr("Accept and review terms")
                textSize: panel.textSize
                primary: true
                width: Math.min(implicitWidth, parent.width)
                onClicked: panel.replied(panel.selected.id, "accept")
            }
            WorldButton {
                objectName: "homeDeferOffer"
                visible: panel.selected?.status !== "considering"
                text: qsTr("Think about it")
                textSize: panel.textSize
                onClicked: panel.replied(panel.selected.id, "later")
            }
            WorldButton {
                objectName: "homeDeclineOffer"
                text: qsTr("Decline")
                textSize: panel.textSize
                onClicked: panel.replied(panel.selected.id, "decline")
            }
        }
    }
    Copy { visible: panel.errorText.length > 0; text: panel.errorText; color: "#f1c492" }
}
