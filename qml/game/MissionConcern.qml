import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var person
    required property var mediators
    required property int textSize
    required property bool editable
    readonly property var concern: person.concern || ({})
    readonly property var availableMediators: mediators.filter(row => row.id !== person.id)
    property int mediatorIndex: 0
    onPersonChanged: mediatorIndex = 0

    visible: !!person.concern
    color: "#40323c"
    border.color: "#ae837e"
    radius: 12
    implicitHeight: body.implicitHeight+28

    component Copy: Label {
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: "#ecdcd4"
    }
    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10
        Copy {
            text: panel.concern.rival ? qsTr("A promise broken") : qsTr("A difficult conversation")
            font.bold: true
            color: "#f1c5a0"
        }
        Copy {
            text: qsTr("%1 backed %2. The new direction is %3.").arg(panel.person.name || "")
                .arg(panel.concern.before || "").arg(panel.concern.after || "")
        }
        Copy {
            visible: !panel.concern.rival || !!panel.concern.atWork || !!panel.concern.resigned
            text: panel.concern.resigned ? qsTr("Left the company") :
                panel.concern.rival ? (panel.concern.noticeDue ? qsTr("Notice given · waiting for a handover") : qsTr("Plans to leave on %1").arg(panel.concern.deadline || "")) :
                    qsTr("Respond before %1").arg(panel.concern.deadline || "")
        }
        Copy {
            visible: !!panel.concern.blocker
            text: panel.concern.blocker || ""
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            WorldButton {
                objectName: "hearMissionConcern"
                text: panel.concern.heard ? qsTr("Talk it through") : qsTr("Hear them out")
                textSize: panel.textSize
                enabled: panel.editable && !panel.concern.blocker
                onClicked: campaign.world.respondToMissionConcern(panel.person.id,"",campaign.world.reviewToken)
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: !panel.concern.rival && panel.availableMediators.length > 0
            GameSelect {
                objectName: "missionMediator"
                Layout.fillWidth: true
                model: panel.availableMediators
                textRole: "name"
                textSize: panel.textSize
                currentIndex: Math.min(panel.mediatorIndex,Math.max(0,panel.availableMediators.length-1))
                onActivated: panel.mediatorIndex = currentIndex
                Accessible.name: qsTr("HR mediator")
            }
            WorldButton {
                objectName: "mediateMissionConcern"
                text: qsTr("Ask to mediate")
                textSize: panel.textSize
                enabled: panel.editable && panel.availableMediators.length > 0 && !panel.concern.busyToday
                onClicked: {
                    const mediator = panel.availableMediators[panel.mediatorIndex]
                    if (mediator) campaign.world.respondToMissionConcern(panel.person.id,mediator.id,campaign.world.reviewToken)
                }
            }
        }
    }
}
