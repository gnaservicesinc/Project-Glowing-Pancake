import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "apps"

Popup {
    id: desktop
    objectName: "deviceLauncher"
    required property int textSize
    property string selectedDevice: ""
    property bool allowDetach: true
    signal appRequested(string app, string section, string device)
    signal focusDetachedRequested(string deviceId)

    readonly property var physicalList: campaign.world.physicalDevices || []
    readonly property bool hasPhysical: physicalList.length > 0
    readonly property var deviceList: hasPhysical ? physicalList : (campaign.world.devices || [])
    readonly property var device: deviceList.find(row => (row.deviceId || row.id) === selectedDevice) || deviceList[0] || ({})
    readonly property string selectedKind: device.kind || device.id || "desktop"
    readonly property string selectedId: device.deviceId || device.id || ""
    readonly property bool selectedPoppedOut: !!device.poppedOut
    readonly property bool selectedUnavailable: hasPhysical && !!device.deviceId && !device.available

    readonly property var applications: [
        {id:"office",name:qsTr("Company"),tint:"#567f8c"},
        {id:"people",name:qsTr("People"),tint:"#95768f"},
        {id:"facilities",name:qsTr("Headquarters"),tint:"#a28961"},
        {id:"lab",name:qsTr("Research"),tint:"#648a9b",section:"research"},
        {id:"inbox",name:qsTr("News & decisions"),tint:"#b78964"},
        {id:"operations",name:qsTr("Operations"),tint:"#6d83a8"},
        {id:"civic",name:qsTr("Public affairs"),tint:"#9a9672"},
        {id:"society",name:qsTr("World"),tint:"#5e9292"},
        {id:"institutions",name:qsTr("Institutions"),tint:"#8c7eaa"},
        {id:"frontier",name:qsTr("Frontier"),tint:"#6c9a89"},
        {id:"leadership",name:qsTr("Leadership"),tint:"#a58572"},
        {id:"objectives",name:qsTr("World & legacy"),tint:"#6789a0"},
        {id:"devices",objectName:"equipmentApp",name:qsTr("Equipment"),tint:"#7d8d9b"}
    ]

    function firstOfKind(row) {
        const k = row.kind || row.id
        for (let i = 0; i < deviceList.length; ++i) {
            if ((deviceList[i].kind || deviceList[i].id) === k)
                return (deviceList[i].deviceId || deviceList[i].id) === (row.deviceId || row.id)
        }
        return false
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay.width - 16, textSize * (selectedKind === "phone" ? 34 : 90))
    height: Math.min(Overlay.overlay.height - 16, textSize * 54)
    padding: 0
    modal: true
    focus: true
    background: Item {}

    function showFor(preferred) {
        const list = deviceList
        const resolved = hasPhysical ? campaign.world.resolvePhysicalDevice(preferred || "", "") : (preferred || "")
        if (resolved && list.some(row => (row.deviceId || row.id) === resolved)) selectedDevice = resolved
        else if (list.some(row => (row.kind || row.id) === "laptop")) selectedDevice = (list.find(row => (row.kind || row.id) === "laptop").deviceId || "laptop")
        else selectedDevice = list.length ? (list[0].deviceId || list[0].id || "") : ""
        open()
    }

    contentItem: ColumnLayout {
        spacing: 2
        Flow {
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: desktop.deviceList
                WorldButton {
                    required property var modelData
                    readonly property string deviceKey: modelData.deviceId || modelData.id
                    objectName: "launcherDevice_" + (desktop.firstOfKind(modelData) ? (modelData.kind || modelData.id) : deviceKey.replace("/", "_"))
                    text: modelData.name
                    checked: desktop.selectedId === deviceKey
                    textSize: desktop.textSize * 0.8
                    padding: 5
                    onClicked: desktop.selectedDevice = deviceKey
                }
            }
        }
        DeviceSurfaceHost {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: desktop.hasPhysical
            allowDetach: desktop.allowDetach
            deviceId: desktop.selectedId
            textSize: desktop.textSize
            onSwitchDeviceRequested: id => desktop.selectedDevice = id
            onFocusDetachedRequested: id => desktop.focusDetachedRequested(id)
            onOpenEquipmentRequested: { desktop.close(); desktop.appRequested("devices", "", "") }
            onCloseSurfaceRequested: desktop.close()
        }
        DesktopSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !desktop.hasPhysical
            textSize: desktop.textSize
            applications: desktop.applications.map(row => Object.assign({}, row, {
                available: campaign.world.appAvailableOn(row.id, desktop.selectedId)
            }))
            onCloseRequested: desktop.close()
            onLaunched: (app, section) => {
                desktop.close()
                desktop.appRequested(app, section, desktop.selectedId)
            }
        }
    }
}
