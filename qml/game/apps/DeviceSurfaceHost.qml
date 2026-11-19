import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

Item {
    id: host
    objectName: "deviceSurfaceHost_" + deviceId.replace("/", "_")
    property string deviceId: ""
    property int textSize: 14
    property bool detached: false
    signal focusDetachedRequested(string deviceId)
    property bool allowDetach: true
    signal dockRequested(string deviceId)
    signal switchDeviceRequested(string deviceId)
    signal openEquipmentRequested()
    signal closeSurfaceRequested()

    readonly property var device: {
        campaign.world.physicalDevices
        return campaign.world.physicalDevice(host.deviceId)
    }
    readonly property var workspace: {
        campaign.world.physicalDevices
        return campaign.world.deviceWorkspace(host.deviceId)
    }
    readonly property string activeApp: workspace.activeApp || ""
    readonly property var openApps: workspace.openApps || []
    readonly property bool isPoppedOutElsewhere: allowDetach && !detached && !!device.poppedOut
    readonly property bool isUnavailable: !!device.deviceId && !device.available

    readonly property var catalogApps: [
        {id:"lab",name:qsTr("Research"),tint:"#648a9b",section:"research"},
        {id:"data",name:qsTr("Data"),glyph:"lab",tint:"#648e77"},
        {id:"models",name:qsTr("Models"),glyph:"lab",tint:"#687eab"},
        {id:"products",name:qsTr("Products"),glyph:"office",tint:"#a1765a"},
        {id:"accounts",name:qsTr("Accounts"),glyph:"office",tint:"#6b8d70"},
        {id:"messages",name:qsTr("Messages"),glyph:"inbox",tint:"#8e7493"},
        {id:"personal",name:qsTr("My life"),glyph:"leadership",tint:"#9c8069"},
        {id:"office",name:qsTr("Company"),tint:"#567f8c",section:"overview"},
        {id:"people",name:qsTr("People"),tint:"#95768f",section:""},
        {id:"facilities",name:qsTr("Headquarters"),tint:"#a28961",section:""},
        {id:"inbox",name:qsTr("News & decisions"),tint:"#b78964",section:""},
        {id:"operations",name:qsTr("Operations"),tint:"#6d83a8",section:""},
        {id:"civic",name:qsTr("Public affairs"),tint:"#9a9672",section:""},
        {id:"society",name:qsTr("World"),tint:"#5e9292",section:""},
        {id:"institutions",name:qsTr("Institutions"),tint:"#8c7eaa",section:""},
        {id:"frontier",name:qsTr("Frontier"),tint:"#6c9a89",section:""},
        {id:"leadership",name:qsTr("Leadership"),tint:"#a58572",section:""},
        {id:"objectives",name:qsTr("World & legacy"),tint:"#6789a0",section:""},
        {id:"devices",objectName:"equipmentApp",name:qsTr("Equipment"),tint:"#7d8d9b",section:""}
    ]

    onDeviceIdChanged: {
        if (appLoader && appLoader.item) {
            appLoader.active = false
            Qt.callLater(() => appLoader.active = Qt.binding(() => host.visible && !host.isPoppedOutElsewhere && !host.isUnavailable && host.activeApp !== ""))
        }
    }
    function appName(appId) {
        for (let i = 0; i < catalogApps.length; ++i)
            if (catalogApps[i].id === appId) return catalogApps[i].name
        return appId
    }

    DeviceFrame {
        objectName: "workspaceDeviceFrame"
        anchors.fill: parent
        kind: host.device.kind || "laptop"
        textSize: host.textSize

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            AppShell { workspace: host; Layout.fillWidth: true }

            // State 1: Observe-in-place placeholder when popped out to a detached OS window
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: host.isPoppedOutElsewhere
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 460)
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 is open in a detached window").arg(host.device.name || host.deviceId)
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: host.textSize * 1.15
                        font.bold: true
                        color: "#ffd897"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Only one live surface runs per physical device so drafts and controls stay synchronized. Focus the detached window or dock this device back here.")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: host.textSize * 0.92
                        color: "#c4d4de"
                    }
                    Flow {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        WorldButton {
                            objectName: "focusDetachedDeviceButton"
                            text: qsTr("Focus window")
                            primary: true
                            textSize: host.textSize
                            onClicked: host.focusDetachedRequested(host.deviceId)
                        }
                        WorldButton {
                            objectName: "dockBackDeviceButton"
                            text: qsTr("Dock back here")
                            textSize: host.textSize
                            onClicked: campaign.world.setDevicePoppedOut(host.deviceId, false)
                        }
                    }
                }
            }

            // State 2: Honest unavailability screen (powered off, in service, retired, unmounted, or at HQ while traveling)
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !host.isPoppedOutElsewhere && host.isUnavailable
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width - 24
                    x: 12; y: 16
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 is currently unavailable").arg(host.device.name || host.deviceId)
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: host.textSize * 1.2
                        font.bold: true
                        color: "#f2b8a0"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: host.device.unavailabilityReason || qsTr("This device cannot be used right now.")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: host.textSize
                        color: "#e6eef3"
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: host.openApps.length > 0
                        text: qsTr("Preserved workspace: %1. Your open apps and unfinished drafts on this device stay intact until it is available again.").arg(host.openApps.map(a => host.appName(a)).join(", "))
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: host.textSize * 0.92
                        color: "#b7d5c8"
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        WorldButton {
                            objectName: "powerOnUnavailableDevice"
                            visible: !!host.device.canPower && !host.device.powered
                            text: qsTr("Power on %1").arg(host.device.name || "")
                            primary: true
                            textSize: host.textSize
                            enabled: !campaign.busy
                            onClicked: campaign.world.powerPhysicalDevice(host.deviceId, true, host.device.revision || campaign.world.reviewToken)
                        }
                        WorldButton {
                            objectName: "unavailableEquipmentControls"
                            text: qsTr("Equipment controls")
                            textSize: host.textSize
                            onClicked: host.openEquipmentRequested()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: altRepeater.count > 0
                        text: qsTr("Or switch to another available device (its own workspace opens separately):")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: host.textSize * 0.9
                        color: "#c0d1dc"
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            id: altRepeater
                            model: {
                                campaign.world.physicalDevices
                                return campaign.world.alternateDevicesForApp(host.activeApp, host.deviceId)
                            }
                            WorldButton {
                                required property var modelData
                                text: qsTr("Use %1").arg(modelData.name)
                                textSize: host.textSize * 0.9
                                onClicked: {
                                    // Switching hardware resumes the destination workspace.
                                    host.switchDeviceRequested(modelData.deviceId)
                                }
                            }
                        }
                    }
                }
            }

            // State 3: Device Desktop Home (no active app selected — open apps remain suspended in tabs above)
            DesktopSurface {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !host.isPoppedOutElsewhere && !host.isUnavailable && host.activeApp === ""
                textSize: host.textSize
                showClose: false
                owner: campaign.companyName
                date: campaign.date
                status: (host.device.name || qsTr("Device")) + " · " + campaign.availableText
                applications: host.catalogApps.map(row => Object.assign({}, row, {
                    available: row.id === "devices" || campaign.world.appAvailableOn(row.id, host.deviceId)
                }))
                onLaunched: (app, section) => campaign.world.openDeviceApp(host.deviceId, app, section)
            }

            // State 4: Live App inside this physical device
            ScrollView {
                id: appScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !host.isPoppedOutElsewhere && !host.isUnavailable && host.activeApp !== ""
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                Connections {
                    target: appScroll.contentItem
                    function onMovementEnded() {
                        if (appScroll.visible) campaign.world.setDeviceAppState(host.deviceId, host.activeApp, "scrollY", appScroll.contentItem.contentY)
                    }
                }
                ColumnLayout {
                    width: appScroll.availableWidth
                    spacing: 8
                    Loader {
                        id: appLoader
                        Layout.fillWidth: true
                        Layout.margins: 10
                        active: host.visible && !host.isPoppedOutElsewhere && !host.isUnavailable && host.activeApp !== ""
                        onLoaded: {
                            const state = campaign.world.deviceAppState(host.deviceId, host.activeApp)
                            Qt.callLater(() => {
                                if (appScroll.contentItem) appScroll.contentItem.contentY = state.scrollY || 0
                            })
                        }
                        sourceComponent: {
                            switch (host.activeApp) {
                            case "lab": return researchComp
                            case "data": return dataComp
                            case "models": return modelsComp
                            case "accounts": return accountsComp
                            case "products": return productsComp
                            case "messages": return messagesComp
                            case "personal": return personalComp
                            case "office": return officeComp
                            case "people": return peopleComp
                            case "facilities": return facilitiesComp
                            case "devices": return devicesComp
                            case "operations": return operationsComp
                            case "civic": return civicComp
                            case "society": return societyComp
                            case "institutions": return institutionsComp
                            case "frontier": return frontierComp
                            case "leadership": return leadershipComp
                            case "objectives": return objectivesComp
                            default: return inboxComp
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: researchComp
        ResearchApp {
            deviceId: host.deviceId
            textSize: host.textSize
            canDetach: host.allowDetach
            onSuppliersRequested: campaign.world.openDeviceApp(host.deviceId, "facilities", "equipment")
            editable: campaign.paused && !campaign.busy && campaign.companyEditable
        }
    }
    component CompanyApp: ManagementPanel {
        id: deviceSession
        manager: campaign.createManagementView(deviceSession)
        deviceId: host.deviceId
        application: host.activeApp
        textSize: host.textSize
        onOpenSuppliers: campaign.world.openDeviceApp(host.deviceId, "facilities", "equipment")
    }
    Component { id: dataComp; CompanyApp { room: "lab"; section: "data" } }
    Component { id: modelsComp; CompanyApp { room: "lab"; section: "models" } }
    Component { id: accountsComp; CompanyApp { room: "office"; officeSection: "accounts" } }
    Component { id: productsComp; CompanyApp { room: "office"; officeSection: "products" } }
    Component { id: officeComp; CompanyApp { room: "office" } }
    Component {
        id: personalComp
        PersonalLifePanel {
            textSize: host.textSize
            onOpenApp: (app, section, device) => campaign.world.openDeviceApp(host.deviceId, app, section)
        }
    }
    Component { id: messagesComp; PeoplePanel { deviceId: host.deviceId; application: "messages"; textSize: host.textSize } }
    Component {
        id: peopleComp
        PeoplePanel {
            deviceId: host.deviceId
            textSize: host.textSize
            onOpenApp: (app, section) => campaign.world.openDeviceApp(host.deviceId, app, section)
        }
    }
    Component {
        id: facilitiesComp
        WorkplacePanel {
            textSize: host.textSize
            onOpenInfrastructure: campaign.world.openDeviceApp(host.deviceId, "operations", "")
        }
    }
    Component { id: devicesComp; DevicePanel { textSize: host.textSize } }
    Component { id: operationsComp; InfrastructurePanel { textSize: host.textSize } }
    Component { id: civicComp; CivicPanel { textSize: host.textSize } }
    Component { id: societyComp; SocietyPanel { textSize: host.textSize } }
    Component { id: institutionsComp; InstitutionsPanel { textSize: host.textSize } }
    Component { id: frontierComp; FrontierPanel { textSize: host.textSize } }
    Component { id: leadershipComp; SuccessionPanel { textSize: host.textSize } }
    Component { id: objectivesComp; ObjectivesPanel { textSize: host.textSize } }
    Component {
        id: inboxComp
        ColumnLayout {
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: qsTr("Campaign journal & required decisions")
                font.pixelSize: host.textSize * 1.15
                font.bold: true
                color: "#ffd897"
            }
            StableEntityList {
                Layout.fillWidth: true
                Layout.preferredHeight: host.textSize * 16
                rows: campaign.inbox
                reducedMotion: campaign.reducedMotion
                textSize: host.textSize
                emptyText: qsTr("No matching messages")
                onSelected: id => { campaign.selectedInboxId = id }
            }
            Label {
                Layout.fillWidth: true
                visible: !!(campaign.inbox.selectedRow && campaign.inbox.selectedRow.detail)
                text: (campaign.inbox.selectedRow && campaign.inbox.selectedRow.detail) || ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: host.textSize
                color: "#e6eef3"
            }
        }
    }
}
