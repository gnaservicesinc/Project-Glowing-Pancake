import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: desktop
    property int textSize: 14
    readonly property real headerHeight: hud.height
    readonly property real footerHeight: dock.height
    readonly property real uiScale:Math.max(1,textSize/14)
    readonly property bool compact: width < 1050*uiScale
    readonly property bool shortLayout:height<textSize*26
    readonly property var nav: campaign.world.navigation
    readonly property string place: nav.place || "workspace"
    signal openApp(string app, string section, string device)
    signal menuRequested()
    function visit(id) { campaign.world.navigate(id, nav.scale || "hq", "", "", "") }
    function placeApp() {return place==="lab" || place==="research_annex" || place==="evaluation_suite"?"lab":place==="meeting" || place==="collaboration"?"inbox":place==="compute"?"operations":"office"}
    function openPlace() {desktop.openApp(placeApp(),place==="evaluation_suite"?"evaluate":placeApp()==="lab"?"research":"",campaign.world.atHeadquarters && campaign.world.officeKind!=="home"?"desktop":"laptop")}
    function revealDock(item) {
        let left=item.mapToItem(dockRow,0,0).x,view=dockScroll.contentItem
        if(left<view.contentX) view.contentX=left
        else if(left+item.width>view.contentX+dockScroll.availableWidth) view.contentX=left+item.width-dockScroll.availableWidth
    }
    function showMap(scale) {campaign.world.navigate("map",scale,"","","")}

    Rectangle {anchors.fill:parent;color:"#17282c"}
    Component {id:flatWorld;RoomScene {room:"office"}}
    Loader {
        id: worldView
        objectName: "workspaceScene"
        anchors.top: hud.bottom
        anchors.bottom: dock.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: desktop.compact || desktop.place === "map" ? 0 : 270*desktop.uiScale
        active: true
        visible: desktop.place !== "map"
        source: softwareWorld ? "" : "Workspace3D.qml"
        sourceComponent: softwareWorld ? flatWorld : undefined
        onLoaded: {
            if(softwareWorld) item.room = "office"
            else {
                item.place = Qt.binding(() => desktop.place)
                item.atHeadquarters = Qt.binding(() => campaign.world.atHeadquarters)
                item.peopleCount = Qt.binding(() => campaign.world.people.length)
                item.officeKind = Qt.binding(() => campaign.world.officeKind)
                item.availableRooms = Qt.binding(() => campaign.world.rooms.map(row=>row.id))
                item.placePicked.connect(desktop.visit)
            }
        }
    }
    WorldMap {
        anchors.top: hud.bottom
        anchors.bottom: dock.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: desktop.shortLayout?8:18
        visible: desktop.place === "map"
        enabled: visible
        scale: desktop.nav.scale || "hq"
        textSize: desktop.textSize
        onOpenApp: (app,section) => desktop.openApp(app,section,"laptop")
        onVisitHeadquarters: desktop.visit("workspace")
    }
    Rectangle {
        id: hud
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: hudContent.implicitHeight + (desktop.shortLayout?16:28*desktop.uiScale)
        color: "#ef13252b"
        ColumnLayout {
            id: hudContent
            anchors.left: parent.left;anchors.right:parent.right;anchors.top:parent.top
            anchors.margins: desktop.shortLayout?8:14*desktop.uiScale
            spacing: 10
            RowLayout {
                spacing: 12
                EmployeePortrait {
                    objectName:"founderPortrait"
                    Layout.preferredWidth:desktop.shortLayout?44:Math.max(44,desktop.textSize*3);Layout.preferredHeight:Layout.preferredWidth
                    portraitSource:campaign.world.founder.portrait || "";personName:campaign.world.founder.name || ""
                    reducedMotion:campaign.reducedMotion
                    onClicked:desktop.openApp("people","","laptop")
                }
                ColumnLayout {
                    Layout.fillWidth:true;spacing:3
                    Label {Layout.fillWidth:true;text:campaign.companyName;font.pixelSize:desktop.textSize*1.2;font.bold:true;color:"#f0ecdd";elide:Text.ElideRight;textFormat:Text.PlainText}
                    Label {Layout.fillWidth:true;text:campaign.world.location;font.pixelSize:desktop.textSize*0.85;color:"#a7c1bb";elide:Text.ElideRight;textFormat:Text.PlainText}
                }
                ColumnLayout {
                    visible: !desktop.compact;spacing:3
                    Label {text:qsTr("AVAILABLE %1").arg(campaign.availableText);font.pixelSize:desktop.textSize;color:"#b7dfc7"}
                    Label {text:qsTr("Cash %1").arg(campaign.cashText);font.pixelSize:desktop.textSize*0.85;color:"#adbfbd"}
                }
                WorldButton {objectName:"mainMenuButton";text:qsTr("Menu");textSize:desktop.textSize;onClicked:desktop.menuRequested()}
            }
            Flow {
                Layout.fillWidth:true;spacing:8
                Label {height:desktop.shortLayout?desktop.textSize*2.2:42*desktop.uiScale;text:campaign.date;font.pixelSize:desktop.textSize;color:"#f0ecdd";verticalAlignment:Text.AlignVCenter;rightPadding:8}
                WorldButton {objectName:"pauseButton";text:campaign.paused?qsTr("▶ Play"):qsTr("Ⅱ Pause");textSize:desktop.textSize;primary:campaign.paused;enabled:!campaign.busy && !campaign.storageBlocked && (!campaign.paused || campaign.canPlay);onClicked:campaign.togglePaused()}
                WorldButton {objectName:"stepButton";visible:!desktop.shortLayout;text:qsTr("Next day");textSize:desktop.textSize;enabled:campaign.paused && campaign.canAdvance && !campaign.storageBlocked;onClicked:campaign.step()}
                ComboBox {objectName:"speedControl";visible:!desktop.shortLayout;model:[1,10,30,60];currentIndex:model.indexOf(campaign.daysPerSecond);displayText:qsTr("%1 days/s").arg(campaign.daysPerSecond);Accessible.name:qsTr("Days per second");font.pixelSize:desktop.textSize;implicitWidth:desktop.textSize*9;onActivated:campaign.daysPerSecond=model[currentIndex]}
                WorldButton {visible:!desktop.shortLayout;text:qsTr("Advance…");textSize:desktop.textSize;onClicked:advanceMenu.open()}
                WorldButton {visible:desktop.shortLayout;text:campaign.requiredKind!=="none"?qsTr("Review decision"):qsTr("Time & funds");textSize:desktop.textSize;primary:campaign.requiredKind!=="none";onClicked:campaign.requiredKind!=="none"?desktop.openApp("inbox","","phone"):compactTime.open()}
                Label {visible:desktop.compact && !desktop.shortLayout;height:42*desktop.uiScale;text:qsTr("Available %1").arg(campaign.availableText);font.pixelSize:desktop.textSize;color:"#b7dfc7";verticalAlignment:Text.AlignVCenter}
            }
        }
    }
    Flow {
        id: places
        anchors.top:hud.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:16*desktop.uiScale
        spacing:8
        visible:desktop.place!=="map" && !desktop.shortLayout
        Repeater {
            model:campaign.world.rooms
            WorldButton {required property var modelData;objectName:"place_"+modelData.id;text:modelData.name;textSize:desktop.textSize;checked:desktop.place===modelData.id;onClicked:desktop.visit(modelData.id)}
        }
    }
    ColumnLayout {
        anchors.left:parent.left;anchors.bottom:dock.top;anchors.margins:22*desktop.uiScale
        width:Math.max(200,Math.min(520*desktop.uiScale,desktop.width-(desktop.compact?44:320*desktop.uiScale)))
        visible:desktop.place!=="map" && !desktop.shortLayout
        spacing:7
        Label {Layout.fillWidth:true;text:({workspace:qsTr("An idea takes shape."),lab:qsTr("Make the next discovery."),meeting:qsTr("Every choice has a company behind it."),compute:qsTr("Keep the work running."),research_annex:qsTr("Space for the next idea."),evaluation_suite:qsTr("Put your evidence to work."),collaboration:qsTr("Bring the team together.")})[desktop.place] || "";font.pixelSize:desktop.textSize*1.7;font.bold:true;color:"#f4ead6";wrapMode:Text.Wrap}
        Label {Layout.fillWidth:true;text:campaign.world.setting;font.pixelSize:desktop.textSize;color:"#c7d7cf";wrapMode:Text.Wrap}
        WorldButton {objectName:"openPlaceApp";text:desktop.placeApp()==="lab"?qsTr("Open research notebook"):desktop.placeApp()==="inbox"?qsTr("Open decisions"):desktop.placeApp()==="operations"?qsTr("Open infrastructure"):qsTr("Open company desktop");textSize:desktop.textSize;primary:true;onClicked:desktop.openPlace()}
    }
    RowLayout {
        visible:desktop.shortLayout && desktop.place!=="map"
        anchors.top:hud.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:10
        ComboBox {objectName:"compactRoomPicker";Layout.fillWidth:true;model:campaign.world.rooms;textRole:"name";valueRole:"id";font.pixelSize:desktop.textSize;currentIndex:Math.max(0,model.findIndex(row=>row.id===desktop.place));Accessible.name:qsTr("Current room");onActivated:desktop.visit(currentValue)}
        WorldButton {text:qsTr("Open");textSize:desktop.textSize;onClicked:desktop.openPlace()}
    }
    Rectangle {
        id: agenda
        objectName:"worldAgenda"
        anchors.top:places.bottom;anchors.bottom:dock.top;anchors.right:parent.right
        anchors.margins:16*desktop.uiScale;width:244*desktop.uiScale;color:"#15292b";radius:12;border.color:"#3b5555"
        visible:!desktop.compact && desktop.place!=="map"
        ScrollView {
            id: agendaScroll
            anchors.fill:parent;anchors.margins:16*desktop.uiScale;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:agendaScroll.availableWidth;spacing:14
                Label {text:qsTr("TODAY");font.pixelSize:desktop.textSize*0.85;font.letterSpacing:2;color:"#a9d5c4"}
                Label {Layout.fillWidth:true;text:campaign.requiredKind!=="none"?campaign.requiredTitle:qsTr("Your next move");textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:desktop.textSize*1.2;font.bold:true;color:"#eee6d5"}
                Label {Layout.fillWidth:true;text:campaign.requiredKind!=="none"?qsTr("A decision needs your attention. Time waits for you."):qsTr("Review the team, check your research, or let the next day begin.");wrapMode:Text.Wrap;font.pixelSize:desktop.textSize;color:"#b7cac2"}
                WorldButton {objectName:"agendaDecisionButton";Layout.fillWidth:true;text:campaign.requiredKind!=="none"?qsTr("Review decision"):qsTr("Open journal (%1)").arg(campaign.inbox.count);textSize:desktop.textSize;primary:campaign.requiredKind!=="none";onClicked:desktop.openApp("inbox","","phone")}
                Rectangle {Layout.fillWidth:true;height:1;color:"#3c5554"}
                Label {Layout.fillWidth:true;text:qsTr("%1 projects in your notebook").arg(campaign.projects.count);wrapMode:Text.Wrap;font.pixelSize:desktop.textSize;color:"#d9e3d7"}
                WorldButton {Layout.fillWidth:true;text:qsTr("Review research");textSize:desktop.textSize;onClicked:desktop.openApp("lab","projects","laptop")}
                Label {Layout.fillWidth:true;text:campaign.world.founder.name || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:desktop.textSize;font.bold:true;color:"#eee6d5"}
                Label {Layout.fillWidth:true;text:campaign.world.founder.detail || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:desktop.textSize*0.9;color:"#b7cac2"}
                WorldButton {Layout.fillWidth:true;text:qsTr("Meet your team");textSize:desktop.textSize;onClicked:desktop.openApp("people","","laptop")}
            }
        }
    }
    Rectangle {
        id:dock
        anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom
        height:desktop.shortLayout?desktop.textSize*3.2+16:68*desktop.uiScale;color:"#11242a"
        ScrollView {
            id:dockScroll
            anchors.fill:parent;anchors.margins:desktop.shortLayout?8:10*desktop.uiScale;contentHeight:availableHeight;clip:true
            ScrollBar.vertical.policy:ScrollBar.AlwaysOff
            Row {
                id:dockRow
                spacing:8
                WorldButton {objectName:"officeTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Company");textSize:desktop.textSize;onClicked:desktop.openApp("office","","laptop")}
                WorldButton {objectName:"peopleTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("People");textSize:desktop.textSize;onClicked:desktop.openApp("people","","laptop")}
                WorldButton {objectName:"headquartersTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Headquarters");textSize:desktop.textSize;onClicked:desktop.openApp("facilities","","laptop")}
                WorldButton {objectName:"labTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Lab");textSize:desktop.textSize;onClicked:desktop.openApp("lab","research","laptop")}
                WorldButton {objectName:"inboxTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("News & decisions");textSize:desktop.textSize;onClicked:desktop.openApp("inbox","","phone")}
                WorldButton {objectName:"mapTab";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Map");textSize:desktop.textSize;checked:desktop.place==="map";onClicked:desktop.showMap(desktop.nav.scale || "hq")}
                WorldButton {objectName:"devicesButton";onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Devices & apps");textSize:desktop.textSize;onClicked:deviceMenu.open()}
            }
        }
    }
    Popup {
        id:compactTime;anchors.centerIn:Overlay.overlay;width:Math.min(520,desktop.width-32);height:Math.min(500,desktop.height-32);modal:true;focus:true
        contentItem:ScrollView {
            id:timeScroll;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:timeScroll.availableWidth;spacing:12
                Label {Layout.fillWidth:true;text:qsTr("Cash %1\nAvailable %2").arg(campaign.cashText,campaign.availableText);font.pixelSize:desktop.textSize;wrapMode:Text.Wrap}
                WorldButton {text:qsTr("Next day");textSize:desktop.textSize;enabled:campaign.canAdvance && !campaign.storageBlocked;onClicked:campaign.step()}
                ComboBox {Layout.fillWidth:true;model:[1,10,30,60];currentIndex:model.indexOf(campaign.daysPerSecond);displayText:qsTr("%1 days/s").arg(campaign.daysPerSecond);font.pixelSize:desktop.textSize;Accessible.name:qsTr("Days per second");onActivated:campaign.daysPerSecond=model[currentIndex]}
                WorldButton {text:qsTr("Advance until…");textSize:desktop.textSize;onClicked:{compactTime.close();advanceMenu.open()}}
                WorldButton {text:qsTr("Close");textSize:desktop.textSize;onClicked:compactTime.close()}
            }
        }
    }
    Popup {
        id:deviceMenu;objectName:"deviceLauncher"
        anchors.centerIn:Overlay.overlay;width:Math.min(700,desktop.width-32);height:Math.min(570,desktop.height-32)
        modal:true;focus:true;padding:24
        background:Rectangle {color:"#152d31";radius:14;border.color:"#7c9d91"}
        contentItem:ScrollView {
            id: launcherScroll
            contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:launcherScroll.availableWidth;spacing:14
                Label {text:qsTr("Your devices");font.pixelSize:desktop.textSize*1.5;color:"#f4ead6"}
                Flow {Layout.fillWidth:true;spacing:8
                    Repeater {model:campaign.world.devices;WorldButton {required property var modelData;text:modelData.name;textSize:desktop.textSize;checked:launcherDevice===modelData.id;onClicked:launcherDevice=modelData.id}}
                }
                Label {Layout.fillWidth:true;text:qsTr("Open an application. Close it to return to this place.");wrapMode:Text.Wrap;font.pixelSize:desktop.textSize;color:"#bbcec6"}
                Flow {
                    Layout.fillWidth:true;spacing:8
                    Repeater {
                        model:[{id:"office",name:qsTr("Company")},{id:"people",name:qsTr("People")},{id:"facilities",name:qsTr("Headquarters & blueprints")},{id:"lab",name:qsTr("Research")},{id:"inbox",name:qsTr("News & decisions")},{id:"operations",name:qsTr("Infrastructure & travel")},{id:"civic",name:qsTr("Public affairs")},{id:"society",name:qsTr("World")},{id:"institutions",name:qsTr("Institutions")},{id:"frontier",name:qsTr("Frontier")},{id:"leadership",name:qsTr("Leadership & retirement")},{id:"objectives",name:qsTr("Company mission")}]
                        WorldButton {required property var modelData;objectName:modelData.id+"App";text:modelData.name;textSize:desktop.textSize;onClicked:{deviceMenu.close();desktop.openApp(modelData.id,"",launcherDevice)}}
                    }
                }
                WorldButton {text:qsTr("Back to the room");textSize:desktop.textSize;onClicked:deviceMenu.close()}
            }
        }
    }
    property string launcherDevice:"laptop"
    Popup {
        id:advanceMenu;anchors.centerIn:Overlay.overlay;width:Math.min(410,desktop.width-32);modal:true;focus:true;padding:20
        background:Rectangle {color:"#183135";radius:12;border.color:"#72938a"}
        contentItem:ColumnLayout {
            Label {Layout.fillWidth:true;text:qsTr("Advance until…");font.pixelSize:desktop.textSize;color:"#f2e8d3"}
            Repeater {model:[{id:"project",name:qsTr("Next project")},{id:"evaluation",name:qsTr("Next evaluation")},{id:"deadline",name:qsTr("Next deadline")},{id:"leadership",name:qsTr("Next leadership date")}];WorldButton {required property var modelData;Layout.fillWidth:true;text:modelData.name;textSize:desktop.textSize;enabled:campaign.paused && campaign.canPlay && !campaign.storageBlocked;onClicked:{advanceMenu.close();campaign.advanceTo(modelData.id)}}}
            WorldButton {Layout.fillWidth:true;text:qsTr("Cancel");textSize:desktop.textSize;onClicked:advanceMenu.close()}
        }
    }
}
