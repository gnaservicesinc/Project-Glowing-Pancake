import QtQuick
import QtQuick.Window

Window {
    id: win
    objectName: "detachedDeviceWindow_" + deviceId.replace("/", "_")
    property string deviceId: ""
    property string deviceName: ""
    property int textSize: 14
    property bool mainExiting: false
    signal openEquipmentRequested()

    title: qsTr("%1 · %2").arg(deviceName || deviceId).arg(campaign.companyName || qsTr("Project Glowing Pancake"))
    minimumWidth: 520
    minimumHeight: 400
    color: "#111d29"
    visible: true

    function focusWindow() {
        show()
        raise()
        requestActivate()
    }

    Component.onCompleted: {
        const g = campaign.world.clampedWindowGeometry("device:" + deviceId, 1080, 700)
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
                campaign.world.saveWindowGeometry("device:" + win.deviceId, win.x, win.y, win.width, win.height)
        }
    }
    onXChanged: saveGeomTimer.restart()
    onYChanged: saveGeomTimer.restart()
    onWidthChanged: saveGeomTimer.restart()
    onHeightChanged: saveGeomTimer.restart()

    onClosing: close => {
        if (!win.mainExiting) {
            close.accepted = false
            campaign.world.setDevicePoppedOut(win.deviceId, false)
        }
    }

    DeviceSurfaceHost {
        anchors.fill: parent
        deviceId: win.deviceId
        textSize: win.textSize
        detached: true
        onDockRequested: id => campaign.world.setDevicePoppedOut(id, false)
        onOpenEquipmentRequested: win.openEquipmentRequested()
        onSwitchDeviceRequested: id => {
            campaign.world.setDevicePoppedOut(win.deviceId, false)
            campaign.world.setDevicePoppedOut(id, true)
        }
    }
}
