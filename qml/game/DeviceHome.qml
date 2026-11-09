import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: desktop
    objectName: "deviceLauncher"
    required property int textSize
    property string selectedDevice: "laptop"
    signal appRequested(string app, string section, string device)
    readonly property var device: campaign.world.devices.find(row => row.id === selectedDevice) || ({})
    readonly property string deviceLabel: device.name || qsTr("Equipment access")
    readonly property bool compact: height < textSize*22
    readonly property var applications: [
        {id:"office",name:qsTr("Company"),tint:"#3d6663"},
        {id:"people",name:qsTr("People"),tint:"#6c5f79"},
        {id:"facilities",name:qsTr("Headquarters"),tint:"#756346"},
        {id:"lab",name:qsTr("Research"),tint:"#416e79",section:"research"},
        {id:"inbox",name:qsTr("News & decisions"),tint:"#78604f"},
        {id:"operations",name:qsTr("Operations"),tint:"#4b647b"},
        {id:"civic",name:qsTr("Public affairs"),tint:"#675f4d"},
        {id:"society",name:qsTr("World"),tint:"#375d61"},
        {id:"institutions",name:qsTr("Institutions"),tint:"#68627d"},
        {id:"frontier",name:qsTr("Frontier"),tint:"#426b68"},
        {id:"leadership",name:qsTr("Leadership"),tint:"#6c5d51"},
        {id:"objectives",name:qsTr("Company mission"),tint:"#4d6779"}
    ]
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay.width - 24, textSize * (selectedDevice === "phone" ? 34 : 72))
    height: Math.min(Overlay.overlay.height - 24, textSize * 46)
    padding: 0
    modal: true
    focus: true
    background: Item {}

    function reveal(item) {
        const flick = view.contentItem
        if (!flick || !item) return
        const point = item.mapToItem(view,0,0)
        if (point.y<0) flick.contentY=Math.max(0,flick.contentY+point.y)
        else if (point.y+item.height>view.availableHeight)
            flick.contentY=Math.min(Math.max(0,flick.contentHeight-view.availableHeight),flick.contentY+point.y+item.height-view.availableHeight)
    }
    function showFor(preferred) {
        const devices = campaign.world.devices
        selectedDevice = devices.some(row => row.id === preferred) ? preferred :
            devices.some(row => row.id === "laptop") ? "laptop" : (devices[0] || {}).id || ""
        open()
    }

    contentItem: DeviceFrame {
        kind: desktop.selectedDevice || "desktop"
        textSize: desktop.textSize
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {position:0;color:"#21454a"}
                GradientStop {position:0.60;color:"#17333b"}
                GradientStop {position:1;color:"#102329"}
            }
            Rectangle {
                width: parent.width*0.9
                height: width
                x: parent.width*0.58
                y: -height*0.48
                radius: width/2
                color: "transparent"
                border.width: Math.max(1,desktop.textSize*0.6)
                border.color: "#26686960"
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: parent.width*0.1
                    radius: width/2
                    color: "transparent"
                    border.width: Math.max(1,desktop.textSize*0.25)
                    border.color: "#376f7d70"
                }
            }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: systemBar.implicitHeight+(desktop.compact ? 2 : 10)
                color: "#132d33"
                RowLayout {
                    id: systemBar
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 10
                    DeviceGlyph {Layout.preferredWidth:desktop.textSize*1.5; Layout.preferredHeight:width}
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Pancake Desktop")
                        elide: Text.ElideRight
                        font.pixelSize: desktop.textSize
                        font.bold: true
                        color: "#f2e7c9"
                    }
                    Label {
                        visible: desktop.width>desktop.textSize*35
                        text: campaign.date
                        font.pixelSize: desktop.textSize * 0.9
                        color: "#b6d1c7"
                    }
                    WorldButton {text:qsTr("Close");textSize:desktop.textSize;onClicked:desktop.close()}
                }
            }
            ScrollView {
                id: view
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: desktop.compact ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                ColumnLayout {
                    width: view.availableWidth
                    spacing: desktop.compact ? 8 : 14
                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: desktop.compact ? 8 : 14
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        text: desktop.deviceLabel
                        wrapMode: Text.Wrap
                        font.pixelSize: desktop.textSize*(desktop.compact ? 1.15 : 1.35)
                        font.bold: true
                        color: "#ebdfc0"
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        spacing: 6
                        Repeater {
                            model: campaign.world.devices
                            WorldButton {
                                required property var modelData
                                objectName:"launcherDevice_"+modelData.id
                                text: modelData.name
                                textSize: desktop.textSize
                                checked: desktop.selectedDevice===modelData.id
                                onClicked:desktop.selectedDevice=modelData.id
                            }
                        }
                    }
                    GridLayout {
                        id: iconGrid
                        Layout.fillWidth: true
                        Layout.leftMargin: 18
                        Layout.rightMargin: 18
                        columns: Math.max(1, Math.min(6, Math.floor(width/(desktop.textSize*8.8))))
                        rowSpacing: 8
                        columnSpacing: 8
                        Repeater {
                            model: desktop.applications
                            WorldButton {
                                id: tile
                                required property var modelData
                                objectName: modelData.id+"App"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredWidth: 1
                                Layout.preferredHeight: desktop.textSize*(desktop.compact ? 4.6 : 7.2)
                                text: modelData.name
                                textSize: desktop.textSize
                                enabled: (campaign.world.companyArchive || !!desktop.selectedDevice) &&
                                    campaign.world.appAvailableOn(modelData.id, desktop.selectedDevice)
                                contentItem: ColumnLayout {
                                    spacing: 5
                                    Rectangle {
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.preferredWidth: desktop.textSize*(desktop.compact ? 1.65 : 2.7)
                                        Layout.preferredHeight: Layout.preferredWidth
                                        radius: 8
                                        color: tile.modelData.tint
                                        opacity: tile.enabled ? 1 : 0.45
                                        border.color: "#b1b5a4"
                                        DeviceGlyph {anchors.fill:parent; anchors.margins:desktop.textSize*0.36; app:tile.modelData.id}
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: tile.text
                                        wrapMode: Text.Wrap
                                        horizontalAlignment: Text.AlignHCenter
                                        font.pixelSize: desktop.textSize
                                        color: tile.enabled ? "#e7e9d6" : "#91a59d"
                                    }
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: tile.hovered || tile.activeFocus ? "#31514d" : "#20393e"
                                    border.color: tile.activeFocus ? "#efc383" : "#456761"
                                    border.width: tile.activeFocus ? 2 : 1
                                }
                                onActiveFocusChanged: if (activeFocus) desktop.reveal(tile)
                                onClicked: {
                                    desktop.close()
                                    desktop.appRequested(modelData.id, modelData.section || "", desktop.selectedDevice)
                                }
                            }
                        }
                    }
                    WorldButton {
                        id: equipmentButton
                        objectName: "equipmentApp"
                        Layout.leftMargin: 18
                        Layout.bottomMargin: 16
                        text: qsTr("Manage equipment")
                        textSize: desktop.textSize
                        onActiveFocusChanged: if (activeFocus) desktop.reveal(equipmentButton)
                        onClicked: {desktop.close();desktop.appRequested("devices","","")}
                    }
                }
            }
        }
    }
}
