import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../"

Window {
    id: win
    objectName: "detachedResearchTreeWindow"
    property string deviceId: ""
    property string deviceName: ""
    property bool deviceAvailable: true
    property string unavailabilityReason: ""
    property int textSize: 14
    property bool mainExiting: false

    title: qsTr("Technology Tree · %1 · %2").arg(deviceName || deviceId).arg(campaign.companyName || qsTr("Project Glowing Pancake"))
    minimumWidth: 620
    minimumHeight: 460
    color: "#111d29"
    visible: true

    function focusWindow() {
        show()
        raise()
        requestActivate()
    }

    Component.onCompleted: {
        const g = campaign.world.clampedWindowGeometry("researchTree", 1180, 760)
        x = g.x
        y = g.y
        width = g.w
        height = g.h
    }

    Timer {
        id: saveGeomTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (!win.mainExiting && win.visible)
                campaign.world.saveWindowGeometry("researchTree", win.x, win.y, win.width, win.height)
        }
    }
    onXChanged: saveGeomTimer.restart()
    onYChanged: saveGeomTimer.restart()
    onWidthChanged: saveGeomTimer.restart()
    onHeightChanged: saveGeomTimer.restart()

    onClosing: close => {
        if (!win.mainExiting) {
            close.accepted = false
            campaign.world.closeDetachedResearchTree()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: qsTr("Technology Tree · synchronized with %1").arg(win.deviceName || win.deviceId)
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: win.textSize * 1.1
                font.bold: true
                color: "#ffd897"
            }
            WorldButton {
                objectName: "dockResearchTreeButton"
                text: qsTr("Dock back")
                textSize: win.textSize * 0.9
                onClicked: campaign.world.closeDetachedResearchTree()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !win.deviceAvailable
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 480)
                spacing: 10
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 is currently unavailable").arg(win.deviceName || win.deviceId)
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: win.textSize * 1.15
                    font.bold: true
                    color: "#f2b8a0"
                }
                Label {
                    Layout.fillWidth: true
                    text: win.unavailabilityReason || qsTr("Restore power or return to this device to resume editing.")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: win.textSize
                    color: "#d4e0e8"
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: win.deviceAvailable
            contentWidth: availableWidth
            clip: true
            ResearchTree {
                width: parent.width
                textSize: win.textSize
                editable: campaign.paused && !campaign.busy && campaign.companyEditable
                deviceId: win.deviceId
                detachedTree: true
            }
        }
    }
}
