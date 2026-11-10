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
    property var messageRows: []
    signal revealRequested(Item target)
    signal governanceRequested()
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
    function revealReply() {
        if (!pendingTopic || !(thread.topics || []).some(topic => topic.id === pendingTopic && topic.asked)) return
        const latest = messageCards.itemAt(0)
        if (!latest || latest.modelData.topic !== pendingTopic) return
        pendingTopic = ""
        if (visible) {
            latest.forceActiveFocus(Qt.OtherFocusReason)
            revealRequested(latest)
        }
    }
    onThreadChanged: {
        const rows = (thread.messages || []).slice().reverse()
        if (JSON.stringify(rows) !== JSON.stringify(messageRows)) messageRows = rows
        Qt.callLater(revealReply)
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
                        visible: !!modelData.portrait
                        Layout.preferredWidth: panel.textSize * 1.8
                        Layout.preferredHeight: Layout.preferredWidth
                        source: modelData.portrait || ""
                        fillMode: Image.PreserveAspectFit
                    }
                    Rectangle {
                        visible: !modelData.portrait
                        Layout.preferredWidth: panel.textSize * 1.8
                        Layout.preferredHeight: Layout.preferredWidth
                        radius: 6
                        color: "#adbea2"
                        Label {
                            anchors.centerIn: parent
                            text: modelData.initials || ""
                            color: "#193136"
                            font.pixelSize: panel.textSize * 0.85
                            font.bold: true
                        }
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
        Copy { visible: !!panel.thread.status; text: panel.thread.status || ""; color: "#bddda7" }
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
            Repeater {
                model: ["check_in", "workstation", "opening_terms", "board_role"]
                WorldButton {
                    required property string modelData
                    readonly property var option: (panel.thread.topics || []).find(topic => topic.id === modelData)
                    visible: !!option
                    objectName: option?.button || ""
                    text: option?.label || ""
                    textSize: panel.textSize
                    width: Math.min(implicitWidth, panel.width)
                    enabled: panel.editable && !!option && !option.asked
                    onClicked: panel.ask(modelData)
                }
            }
        }
        WorldButton {
            objectName: "contactReviewBoard"
            visible: !!panel.thread.governance
            text: qsTr("Open company governance")
            textSize: panel.textSize
            onClicked: panel.governanceRequested()
        }
        Repeater {
            id: messageCards
            model: panel.messageRows
            onItemAdded: (index, item) => { if (index === 0) Qt.callLater(panel.revealReply) }
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
                    Copy { text: modelData.incoming ? qsTr("Before signing · %1").arg(panel.thread.name) : qsTr("%1 · You").arg(modelData.date || ""); color: "#a7c4b9"; font.pixelSize: panel.textSize*0.86 }
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
