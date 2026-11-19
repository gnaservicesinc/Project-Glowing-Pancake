import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

Rectangle {
    required property var workspace
    Layout.fillWidth: true
    implicitHeight: topBarCol.implicitHeight + 10
    color: "#162535"
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#395066" }
    ColumnLayout {
        id: topBarCol
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            WorldButton {
                objectName: "surfaceHomeButton"
                text: qsTr("Desktop")
                textSize: workspace.textSize * 0.85
                padding: 6
                enabled: !workspace.isPoppedOutElsewhere
                onClicked: campaign.world.suspendDeviceApp(workspace.deviceId)
            }
            Label {
                Layout.fillWidth: true
                text: (workspace.device.name || workspace.deviceId) +
                    (workspace.device.ownerLabel ? " · " + workspace.device.ownerLabel : "") +
                    (workspace.device.available ? "" : " · " + qsTr("Offline"))
                textFormat: Text.PlainText
                elide: Text.ElideRight
                font.pixelSize: workspace.textSize * 0.92
                font.bold: true
                color: workspace.device.available ? "#f2e7cd" : "#f0b29b"
            }
            WorldButton {
                objectName: "surfacePopOutButton"
                visible: workspace.allowDetach && !workspace.detached && !workspace.isPoppedOutElsewhere && !!workspace.device.deviceId
                text: qsTr("Pop out")
                textSize: workspace.textSize * 0.82
                padding: 6
                onClicked: campaign.world.setDevicePoppedOut(workspace.deviceId, true)
            }
            WorldButton {
                objectName: "surfaceDockButton"
                visible: workspace.detached
                text: qsTr("Dock")
                textSize: workspace.textSize * 0.82
                padding: 6
                onClicked: workspace.dockRequested(workspace.deviceId)
            }
            WorldButton {
                objectName: "workspaceTimeButton"
                text: campaign.paused ? qsTr("Resume") : qsTr("Pause")
                enabled: !campaign.busy
                textSize: workspace.textSize * 0.82
                padding: 6
                onClicked: campaign.togglePaused()
            }
            WorldButton {
                objectName: "workspaceCloseButton"
                visible: !workspace.detached
                text: qsTr("Close")
                textSize: workspace.textSize * 0.82
                padding: 6
                onClicked: workspace.closeSurfaceRequested()
            }
        }

        // Open-App tabs for this physical device (switch without losing drafts, or explicit close)
        Flow {
            Layout.fillWidth: true
            visible: workspace.openApps.length > 0 && !workspace.isPoppedOutElsewhere
            spacing: 4
            Repeater {
                model: workspace.openApps
                Rectangle {
                    required property string modelData
                    radius: 5
                    color: workspace.activeApp === modelData ? "#33546f" : "#1e3144"
                    border.color: workspace.activeApp === modelData ? "#efc389" : "#445e75"
                    implicitWidth: tabRow.implicitWidth + 10
                    implicitHeight: Math.max(28, tabRow.implicitHeight + 6)
                    RowLayout {
                        id: tabRow
                        anchors.centerIn: parent
                        spacing: 4
                        WorldButton {
                            objectName: "deviceTab_" + modelData
                            text: workspace.appName(modelData)
                            textSize: workspace.textSize * 0.8
                            quiet: true
                            padding: 4
                            onClicked: campaign.world.switchDeviceApp(workspace.deviceId, modelData)
                        }
                        WorldButton {
                            objectName: "deviceTabClose_" + modelData
                            text: "×"
                            Accessible.name: qsTr("Close %1").arg(workspace.appName(modelData))
                            textSize: workspace.textSize * 0.8
                            quiet: true
                            padding: 4
                            onClicked: campaign.world.closeDeviceApp(workspace.deviceId, modelData)
                        }
                    }
                }
            }
        }
    }
}
