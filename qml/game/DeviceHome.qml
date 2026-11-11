import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: desktop
    objectName: "deviceLauncher"
    required property int textSize
    property string selectedDevice: "laptop"
    signal appRequested(string app,string section,string device)
    readonly property var device: campaign.world.devices.find(row=>row.id===selectedDevice) || ({})
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
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay.width-16,textSize*(selectedDevice==="phone"?34:90))
    height: Math.min(Overlay.overlay.height-16,textSize*54)
    padding: 0; modal: true; focus: true
    background: Item {}
    function showFor(preferred) {
        const devices=campaign.world.devices
        selectedDevice=devices.some(row=>row.id===preferred)?preferred:devices.some(row=>row.id==="laptop")?"laptop":(devices[0] || {}).id || ""
        open()
    }
    contentItem: DeviceFrame {
        kind: desktop.selectedDevice || "desktop"; textSize: desktop.textSize
        ColumnLayout {
            anchors.fill:parent;spacing:0
            Rectangle {
                Layout.fillWidth:true;implicitHeight:deviceRow.implicitHeight+8;color:"#172638"
                Flow {
                    id:deviceRow;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:4;spacing:6
                    Repeater {model:campaign.world.devices
                        WorldButton {required property var modelData;objectName:"launcherDevice_"+modelData.id;text:modelData.name;textSize:desktop.textSize*0.8;padding:6;checked:desktop.selectedDevice===modelData.id;onClicked:desktop.selectedDevice=modelData.id}
                    }
                }
            }
        DesktopSurface {
            Layout.fillWidth:true;Layout.fillHeight:true;textSize:desktop.textSize
            owner: campaign.companyName; date: campaign.date
            status: desktop.device.name || qsTr("Your workspace")
            applications: desktop.applications.map(row=>Object.assign({},row,{available: row.id==="devices" || campaign.world.appAvailableOn(row.id,desktop.selectedDevice)}))
            onCloseRequested: desktop.close()
            onLaunched: (app,section)=>{desktop.close();desktop.appRequested(app,section,app==="devices"?"":desktop.selectedDevice)}
        }
        }
    }
}
