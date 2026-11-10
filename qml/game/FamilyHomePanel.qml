import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    required property var home
    signal messagesRequested()
    signal realtorRequested()
    property int roomId: Number(campaign.world.draft("personal/family-room", "0"))
    readonly property var rooms: [{id: 0, name: qsTr("Bedroom")}].concat(home.rooms || [])
    readonly property var selectedRoom: rooms.find(room => room.id === roomId) || rooms[0]
    spacing: 12
    Connections {
        target: campaign.world
        function onDraftsReset() { panel.roomId = Number(campaign.world.draft("personal/family-room", "0")) }
    }

    function selectRoom(id) {
        roomId = id
        campaign.world.setDraft("personal/family-room", String(id))
    }
    function showWorkspace(x, y) {
        const room = rooms.slice(1).find(room => x >= room.x && y >= room.y && x < room.x + room.width && y < room.y + room.depth)
        if (room) selectRoom(room.id)
    }
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: panel.textSize
        color: "#d0e0d4"
    }
    Copy {
        objectName: "familyHomeTitle"
        text: qsTr("%1 · %2").arg(panel.home.name || "").arg(panel.home.city || "")
        font.bold: true; font.pixelSize: panel.textSize * 1.18; color: "#f0e3c2"
    }
    Copy {
        text: panel.home.atHome ? qsTr("The company works in the family’s free rooms. Your childhood bedroom is still yours.")
                               : qsTr("Your parents keep your bedroom for you. The family workspaces are available if you decide to move the company home.")
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: panel.rooms
            WorldButton {
                required property var modelData
                objectName: "familyRoom_" + modelData.id
                text: modelData.name; textSize: panel.textSize
                width: Math.min(implicitWidth, parent.width)
                checked: panel.selectedRoom.id === modelData.id
                onClicked: panel.selectRoom(modelData.id)
            }
        }
    }
    Copy {
        objectName: "familySelectedRoom"
        text: panel.selectedRoom.id === 0 ? qsTr("Your childhood bedroom")
             : qsTr("%1 · %2 × %3 m").arg(panel.selectedRoom.name).arg(panel.selectedRoom.width).arg(panel.selectedRoom.depth)
        color: "#f0e3c2"; font.bold: true
    }
    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: scene.visible ? scene.Layout.preferredHeight : Math.max(175, panel.textSize * 6.5)
        FamilyRoomScene {
            id: scene
            anchors.fill: parent
            visible: panel.selectedRoom.id === 0
            textSize: panel.textSize
            bedroom: true
            showRecord: !!panel.home.founder
            computerLabel: qsTr("Open Messages")
            onComputerRequested: panel.messagesRequested()
            onRecordRequested: certificate.open()
        }
        HeadquartersPlan {
            objectName: "familyWorkspacePlan"
            anchors.fill: parent
            visible: panel.selectedRoom.id !== 0
            headquarters: panel.home
            highlightedRoom: panel.selectedRoom.id
            Accessible.name: qsTr("Family workspace plan. Selected room: %1").arg(panel.selectedRoom.name)
            TapHandler {
                onTapped: point => panel.showWorkspace(Math.floor((point.position.x - parent.leftEdge) / parent.cell),
                                                       Math.floor((point.position.y - parent.topEdge) / parent.cell))
            }
        }
    }
    WorldButton {
        objectName: "familyHomeRealtor"
        visible: !panel.home.atHome
        text: qsTr("Review moving the company home")
        textSize: panel.textSize; Layout.maximumWidth: panel.width
        onClicked: panel.realtorRequested()
    }
    GameDialog {
        id: certificate
        title: qsTr("Family keepsake")
        anchors.centerIn: Overlay.overlay
        width: Math.min((Overlay.overlay ? Overlay.overlay.width : 650) - 32, panel.textSize * 27)
        height: Math.min((Overlay.overlay ? Overlay.overlay.height : 480) - 32, panel.textSize * 22)
        modal: true; focus: true; standardButtons: Dialog.Close
        font.pixelSize: panel.textSize
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: campaign.reducedMotion ? 0 : 150 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: campaign.reducedMotion ? 0 : 100 } }
        contentItem: ScrollView {
            id: recordScroll
            contentWidth: availableWidth; clip: true
            Rectangle {
                objectName: "familyCertificate"
                width: recordScroll.availableWidth
                height: recordCopy.implicitHeight + 32
                color: "#f2e4be"; border.width: 6; border.color: "#a9794b"; radius: 3
                ColumnLayout {
                    id: recordCopy
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
                    spacing: 8
                    Copy { text: qsTr("CALIFORNIA  /  JANUARY 1997"); color: "#52432c"; font.pixelSize: panel.textSize * 0.85 }
                    Copy { text: qsTr("Birth record"); color: "#493b26"; font.bold: true }
                    Copy { text: panel.home.founder || ""; color: "#493b26"; font.pixelSize: panel.textSize * 1.15 }
                    Copy { text: [panel.home.gender, panel.home.pronouns].filter(Boolean).join(" · "); color: "#52432c" }
                    Copy { text: qsTr("Signed by your family"); color: "#604730"; font.italic: true }
                }
            }
        }
    }
}
