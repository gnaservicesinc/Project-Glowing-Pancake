import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    property bool inPerson: false
    property bool showHeading: false
    property int room: 1
    property string selected: ""
    property string pendingTopic: ""
    signal revealRequested(Item target)
    readonly property var rooms: campaign.world.headquarters.rooms || []
    readonly property var contacts: {
        campaign.world.reviewToken
        return inPerson ? campaign.world.roomContacts(room) : campaign.world.contacts
    }
    readonly property var thread: {
        campaign.world.reviewToken
        return contacts.some(person => person.id === selected) ? campaign.world.contactThread(selected) : ({})
    }
    readonly property bool editable: campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    readonly property int conversationRoom: inPerson ? room : 0
    spacing: 12

    function ask(topic) {
        pendingTopic = topic
        campaign.world.messageContact(selected, topic, conversationRoom, campaign.world.reviewToken)
    }
    onThreadChanged: {
        if ((pendingTopic === "check_in" && thread.checkedIn) ||
            (pendingTopic === "workstation" && thread.checkedWorkstation)) {
            pendingTopic = ""
            Qt.callLater(() => {
                const latest = messageCards.itemAt(0)
                if (visible && latest) {
                    latest.forceActiveFocus(Qt.OtherFocusReason)
                    revealRequested(latest)
                }
            })
        }
    }
    onSelectedChanged: pendingTopic = ""

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        color: "#d2e5da"
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Copy {
        visible: panel.showHeading
        text: qsTr("Contacts & messages")
        font.pixelSize: panel.textSize * 1.35
        font.bold: true
        color: "#f0e6cf"
    }
    GameSelect {
        objectName: "contactRoomPicker"
        visible: panel.inPerson
        Layout.fillWidth: true
        textSize: panel.textSize
        model: panel.rooms
        textRole: "name"
        valueRole: "id"
        currentIndex: model.findIndex(row => row.id === panel.room)
        Accessible.name: qsTr("Headquarters room")
        onActivated: {
            panel.room = currentValue
            panel.selected = ""
        }
    }
    Copy {
        visible: panel.contacts.length === 0
        text: panel.inPerson ? qsTr("No onboarded employees have an assigned desk in this room.")
                             : qsTr("No saved contacts yet. Open Talk at HQ from the company workspace.")
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: panel.contacts
            WorldButton {
                required property var modelData
                objectName: "contactPerson_" + modelData.id
                width: Math.min(panel.width, Math.max(180,panel.textSize*9))
                height: portraitRow.implicitHeight + 16
                padding: 8
                text: modelData.name
                textSize: panel.textSize
                checked: panel.selected === modelData.id
                contentItem: RowLayout {
                    id: portraitRow
                    spacing: 8
                    Image {
                        Layout.preferredWidth: panel.textSize * 1.8
                        Layout.preferredHeight: Layout.preferredWidth
                        source: modelData.portrait
                        fillMode: Image.PreserveAspectFit
                    }
                    Copy { text: modelData.name }
                }
                onClicked: panel.selected = modelData.id
            }
        }
    }
    ColumnLayout {
        visible: !!panel.thread.name
        Layout.fillWidth: true
        spacing: 8
        Copy { text: panel.thread.name || ""; font.bold: true; font.pixelSize: panel.textSize * 1.22 }
        Copy { text: panel.thread.role || ""; color: "#9fc2b5" }
        WorldButton {
            objectName: "exchangeContactButton"
            visible: panel.inPerson && !panel.thread.exchanged
            text: qsTr("Exchange contact details")
            textSize: panel.textSize
            enabled: panel.editable && panel.thread.room === panel.room
            primary: true
            onClicked: campaign.world.exchangeContact(panel.selected, panel.room, campaign.world.reviewToken)
        }
        Flow {
            visible: !!panel.thread.exchanged
            Layout.fillWidth: true
            spacing: 8
            WorldButton {
                objectName: "contactCheckIn"
                text: qsTr("How's work going?")
                textSize: panel.textSize
                enabled: panel.editable && !panel.thread.checkedIn
                onClicked: panel.ask("check_in")
            }
            WorldButton {
                objectName: "contactWorkstation"
                text: qsTr("Ask about their computer")
                textSize: panel.textSize
                enabled: panel.editable && !panel.thread.checkedWorkstation
                onClicked: panel.ask("workstation")
            }
        }
        Repeater {
            id: messageCards
            model: (panel.thread.messages || []).slice().reverse()
            Rectangle {
                required property var modelData
                objectName: "contactMessageCard"
                Layout.fillWidth: true
                implicitHeight: messageCopy.implicitHeight + 20
                color: "#26464a"
                border.color: activeFocus ? "#f2c784" : "#4e7067"
                radius: 8
                Accessible.role: Accessible.StaticText
                Accessible.name: modelData.prompt + " " + modelData.reply
                ColumnLayout {
                    id: messageCopy
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    Copy { text: qsTr("%1 · You").arg(modelData.date); color: "#a7c4b9"; font.pixelSize: panel.textSize*0.86 }
                    Copy { text: modelData.prompt; font.bold: true }
                    Copy { text: modelData.reply }
                }
            }
        }
        Repeater {
            model: panel.thread.exchanged && panel.thread.current
                ? (campaign.world.headquarters.items || []).filter(item => item.portable && item.delivered && !item.placed && !item.person)
                : []
            WorldButton {
                required property var modelData
                objectName: "contactIssueLaptop_" + modelData.id
                text: qsTr("Issue %1 to %2").arg(modelData.name).arg(panel.thread.name || "")
                textSize: panel.textSize
                Layout.maximumWidth: panel.width
                enabled: panel.editable
                onClicked: campaign.world.assignHQLaptop(modelData.id, panel.selected, campaign.world.reviewToken)
            }
        }
    }
}
