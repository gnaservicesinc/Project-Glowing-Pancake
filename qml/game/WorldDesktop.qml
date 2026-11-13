import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: desktop
    property int textSize: 14
    readonly property real headerHeight: hud.height
    readonly property real footerHeight: dock.height
    readonly property bool dockOverflow: dockRow.width > dock.width-20*uiScale
    function scrollDock(direction) { const view=dockScroll.contentItem; view.contentX=Math.max(0,Math.min(dockRow.width-dockScroll.availableWidth,view.contentX+direction*dockScroll.availableWidth*0.7)) }
    readonly property real uiScale:Math.max(1,textSize/14)
    readonly property bool compact: width < 1050*uiScale
    readonly property bool shortLayout:height<textSize*26
    readonly property bool physicalHQ: Object.keys(campaign.world.headquarters).length>0 && campaign.world.atHeadquarters
    property bool furnishing:false
    property bool planView: softwareWorld
    property bool inspectingWalks: false
    readonly property bool layoutPlan: furnishing && !!hqTools.traffic.summary
    readonly property bool showingPlan: planView || layoutPlan
    onFurnishingChanged: if(!furnishing) inspectingWalks=false
    readonly property var hoveredEquipment: (campaign.world.headquarters.items || []).find(row => row.id === hqTools.hoverTarget) || null
    GameInk { id: ink }
    function zoomScene(delta) { if(worldView.item && !showingPlan) worldView.item.zoom=Math.max(0.5,Math.min(2,worldView.item.zoom+delta)) }
    function fitScene() { if(worldView.item && !showingPlan) worldView.item.zoom=1 }
    readonly property bool placementView:physicalHQ && furnishing && compact && hqTools.placing
    readonly property var nav: campaign.world.navigation
    readonly property string place: nav.place || "workspace"
    signal openApp(string app, string section, string device)
    signal launcherRequested()
    signal journalRequested()
    signal newspaperRequested()
    signal firstStepRequested()
    readonly property bool newspaperReady: campaign.objectives.retrospective && campaign.objectives.ended
    readonly property var firstStep: visible && campaign.showGuide && campaign.requiredKind === "none" &&
        !newspaperReady && !furnishing && campaign.world.navigation.app === "" ? campaign.management.firstCompanyStep : ({})
    function visit(id) { campaign.world.navigate(id, nav.scale || "hq", "", "", "") }
    function placeApp() {return place==="lab" || place==="research_annex" || place==="evaluation_suite"?"lab":place==="meeting" || place==="collaboration"?"inbox":place==="compute"?"operations":"office"}
    function openPlace() {desktop.openApp(placeApp(),place==="evaluation_suite"?"evaluate":placeApp()==="lab"?"research":"",campaign.world.atHeadquarters && campaign.world.officeKind!=="home"?"desktop":"laptop")}
    function revealDock(item) {
        let left=item.mapToItem(dockRow,0,0).x,view=dockScroll.contentItem
        if(left<view.contentX) view.contentX=left
        else if(left+item.width>view.contentX+dockScroll.availableWidth) view.contentX=left+item.width-dockScroll.availableWidth
    }
    function openTime() { compactTime.open() }
    function arrangeHeadquarters() {
        if (!physicalHQ) return
        furnishing = true
        if (!campaign.paused) campaign.togglePaused()
    }
    function showMap(scale) {campaign.world.navigate("map",scale,"","","")}

    Rectangle {
        anchors.fill:parent
        gradient: Gradient { GradientStop {position:0;color:"#263c4c"} GradientStop {position:0.65;color:"#172a38"} GradientStop {position:1;color:"#101c29"} }
    }
    Loader {
        id: worldView
        objectName: "workspaceScene"
        anchors.top: desktop.placementView || desktop.inspectingWalks?parent.top:desktop.physicalHQ?places.bottom:hud.bottom
        anchors.bottom: dock.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: desktop.physicalHQ ? (desktop.furnishing && !desktop.compact ? Math.min(360*desktop.uiScale,desktop.width*0.38)+24 : 0) : desktop.compact || desktop.place === "map" ? 0 : 270*desktop.uiScale
        active: true
        visible: desktop.place !== "map"
        source: desktop.physicalHQ ? (desktop.showingPlan ? "HeadquartersPlan.qml" : "Headquarters3D.qml") : softwareWorld ? "RoomScene.qml" : "Workspace3D.qml"
        onLoaded: {
            if(desktop.physicalHQ) {
                if(desktop.showingPlan) {
                    item.objectName="hqFloorPlan"
                    item.inspectionEnabled=true
                    item.traffic=Qt.binding(()=>desktop.layoutPlan?hqTools.traffic:({}))
                    item.topInset=Qt.binding(()=>desktop.furnishing && hqTools.placing?placementControls.height+8
                                               :desktop.inspectingWalks?backFromWalks.height+8:0)
                }
                item.headquarters=Qt.binding(()=>campaign.world.headquarters)
                item.editing=Qt.binding(()=>desktop.furnishing)
                item.selectedItem=Qt.binding(()=>desktop.furnishing ? hqTools.selectedItem : 0)
                item.hoverItem=Qt.binding(()=>hqTools.hoverTarget)
                item.placing=Qt.binding(()=>desktop.furnishing && hqTools.placing)
                item.preview=Qt.binding(()=>hqTools.preview)
                item.hovered.connect((x,y,target)=>hqTools.hover(x,y,target))
                item.cancelRequested.connect(()=>hqTools.placing=false)
                item.rotateRequested.connect(()=>hqTools.rotated=!hqTools.rotated)
                item.tilePicked.connect((x,y,id)=>{if(!desktop.furnishing && id){desktop.furnishing=true;hqTools.select(id);if(!campaign.paused)campaign.togglePaused()}else hqTools.tile(x,y,id)})
                item.itemDragged.connect((id,x,y,target)=>hqTools.drag(id,x,y,target))
            } else if(softwareWorld) item.room = "office"
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
        visible:!desktop.placementView && !desktop.inspectingWalks
        height:visible?hudContent.implicitHeight + (desktop.shortLayout?16:28*desktop.uiScale):0
        color: ink.chrome
        Rectangle {anchors.bottom:parent.bottom;width:parent.width;height:1;color:"#425263"}
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
                    Label {Layout.fillWidth:true;text:campaign.companyName;font.family:"Georgia";font.pixelSize:desktop.textSize*1.4;font.bold:true;color:"#f0ecdd";elide:Text.ElideRight;textFormat:Text.PlainText}
                    Label {Layout.fillWidth:true;text:campaign.world.location;font.pixelSize:desktop.textSize*0.85;color:"#a7c1bb";elide:Text.ElideRight;textFormat:Text.PlainText}
                }
                Rectangle {
                    visible: !desktop.compact
                    implicitWidth: financeHud.implicitWidth + 24; implicitHeight: financeHud.implicitHeight + 14
                    radius: 9; color: "#172738"; border.color: "#435d70"
                    ColumnLayout {
                        id: financeHud; anchors.centerIn: parent; spacing: 2
                        Label {text:qsTr("AVAILABLE %1").arg(campaign.availableText);font.pixelSize:desktop.textSize;font.bold:true;color:"#b7dfc7"}
                        Label {text:qsTr("Cash %1").arg(campaign.cashText);font.pixelSize:desktop.textSize*0.82;color:"#b3c5c3"}
                    }
                }
            }
            Flow {
                Layout.fillWidth:true;spacing:8
                Rectangle {
                    implicitWidth: dateBadge.implicitWidth + 18; implicitHeight: desktop.shortLayout ? desktop.textSize*2.2 : 42*desktop.uiScale
                    radius: 8; color: "#182838"; border.color: campaign.paused ? "#7d6b4e" : "#4f7c6c"
                    Row {
                        id: dateBadge; anchors.centerIn: parent; spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: campaign.paused ? "#efc389" : "#7ee0b8" }
                        Label {text:campaign.date;font.pixelSize:desktop.textSize;font.bold:true;color:"#f0ecdd";anchors.verticalCenter:parent.verticalCenter}
                    }
                }
                WorldButton {objectName:"pauseButton";text:campaign.paused?qsTr("▶ Resume"):qsTr("Ⅱ Pause");textSize:desktop.textSize;primary:campaign.paused;enabled:!campaign.busy && !campaign.storageBlocked && (!campaign.paused || campaign.canPlay);onClicked:campaign.togglePaused()}
                WorldButton {objectName:"stepButton";visible:!desktop.shortLayout;text:qsTr("Next day");textSize:desktop.textSize;enabled:campaign.paused && campaign.canAdvance && !campaign.storageBlocked;onClicked:campaign.step()}
                GameSelect {textSize:desktop.textSize;objectName:"speedControl";visible:!desktop.shortLayout;model:[1,10,30,60];currentIndex:model.indexOf(campaign.daysPerSecond);displayText:qsTr("%1 days/s").arg(campaign.daysPerSecond);Accessible.name:qsTr("Days per second");font.pixelSize:desktop.textSize;implicitWidth:desktop.textSize*9;onActivated:campaign.daysPerSecond=model[currentIndex]}
                WorldButton {objectName:"hudDecisionButton";visible:campaign.requiredKind!=="none";text:qsTr("Review decision");hint:campaign.requiredTitle;textSize:desktop.textSize;primary:true;onClicked:desktop.journalRequested()}
                WorldButton {objectName:"hudNewspaperButton";visible:desktop.newspaperReady;text:qsTr("Read the newspaper");textSize:desktop.textSize;primary:true;onClicked:desktop.newspaperRequested()}
                WorldButton {
                    objectName: "worldFirstCompanyStep"
                    visible: !!desktop.firstStep.action
                    enabled: !campaign.busy
                    text: qsTr("Next step")
                    hint: desktop.firstStep.title || ""
                    textSize: desktop.textSize
                    onClicked: desktop.firstStepRequested()
                }
                WorldButton {visible:!desktop.shortLayout;text:qsTr("Advance…");textSize:desktop.textSize;onClicked:advanceMenu.open()}
                WorldButton {visible:desktop.shortLayout && campaign.requiredKind==="none" && !desktop.newspaperReady;text:qsTr("Time & funds");textSize:desktop.textSize;onClicked:compactTime.open()}
                Label {visible:desktop.compact && !desktop.shortLayout;height:42*desktop.uiScale;text:qsTr("Available %1").arg(campaign.availableText);font.pixelSize:desktop.textSize;color:"#b7dfc7";verticalAlignment:Text.AlignVCenter}
            }
        }
    }
    Flow {
        id: places
        anchors.top:hud.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:16*desktop.uiScale
        spacing:8
        visible:!desktop.placementView && !desktop.inspectingWalks && desktop.place!=="map" && (!desktop.shortLayout || desktop.physicalHQ)
        Repeater {
            model:desktop.physicalHQ?[]:campaign.world.rooms
            WorldButton {required property var modelData;objectName:"place_"+modelData.id;text:modelData.name;textSize:desktop.textSize;checked:desktop.place===modelData.id;onClicked:desktop.visit(modelData.id)}
        }
        WorldButton {objectName:"hqFurnishButton";visible:desktop.physicalHQ;primary:desktop.furnishing;iconApp:"facilities";text:desktop.shortLayout?(desktop.furnishing?qsTr("Done arranging"):qsTr("Arrange HQ")):desktop.furnishing?qsTr("Done arranging"):qsTr("Arrange headquarters");textSize:desktop.textSize;onClicked:{desktop.furnishing=!desktop.furnishing;if(desktop.furnishing && !campaign.paused)campaign.togglePaused()}}
        WorldButton {visible:desktop.physicalHQ && !desktop.shortLayout;quiet:true;iconApp:"devices";text:qsTr("Suppliers");textSize:desktop.textSize;onClicked:desktop.openApp("facilities","equipment","laptop")}
        WorldButton {visible:desktop.physicalHQ && !desktop.shortLayout;quiet:true;text:qsTr("Realtor");textSize:desktop.textSize;onClicked:desktop.openApp("facilities","property","laptop")}
        WorldButton {
            objectName:"hqTalkButton"
            quiet:true;iconApp:"people"
            visible:desktop.physicalHQ && campaign.world.contactsAvailable && !desktop.furnishing
            text:qsTr("Talk at HQ")
            textSize:desktop.textSize
            onClicked:{if(!campaign.paused)campaign.togglePaused();roomConversation.open()}
        }
    }
    Column {
        anchors.left:worldView.left; anchors.top:worldView.top; anchors.margins:24*desktop.uiScale
        visible:desktop.physicalHQ && desktop.place!=="map" && !desktop.compact && !desktop.furnishing
        width:Math.min(350*desktop.uiScale,worldView.width*0.32)
        spacing:7
        Label {text:qsTr("YOUR HEADQUARTERS");font.pixelSize:desktop.textSize*0.74;font.letterSpacing:2.2;color:ink.accent}
        Label {width:parent.width;text:campaign.world.headquarters.title || qsTr("Your workspace");textFormat:Text.PlainText;wrapMode:Text.Wrap;font.family:ink.displayFace;font.pixelSize:desktop.textSize*1.7;color:ink.paper}
        Label {text:qsTr("%1 rooms · %2 placed furnishings").arg((campaign.world.headquarters.rooms || []).filter(room=>!room.passage).length).arg((campaign.world.headquarters.items || []).filter(row=>row.placed && !row.mount).length);font.pixelSize:desktop.textSize*0.9;color:ink.secondary}
        Label {width:parent.width;text:[...new Set((campaign.world.headquarters.rooms || []).map(row=>row.flooringLabel))].join(" · ");wrapMode:Text.Wrap;font.pixelSize:desktop.textSize*0.8;color:ink.muted}
    }
    Row {
        anchors.right:worldView.right;anchors.bottom:worldView.bottom;anchors.margins:16
        visible:desktop.physicalHQ && desktop.place!=="map" && !desktop.shortLayout && !hqTools.placing && !desktop.layoutPlan
        spacing:6
        WorldButton {objectName:"hqPlanToggle";text:desktop.planView?qsTr("3D view"):qsTr("Floor plan");textSize:desktop.textSize;visible:!softwareWorld;checked:desktop.planView;onClicked:desktop.planView=!desktop.planView}
        WorldButton {objectName:"hqZoomOut";text:"−";hint:qsTr("Zoom out");Accessible.name:hint;textSize:desktop.textSize;visible:!desktop.planView;onClicked:desktop.zoomScene(-0.15)}
        WorldButton {objectName:"hqFitView";text:qsTr("Fit");hint:qsTr("Fit the headquarters in view");textSize:desktop.textSize;visible:!desktop.planView;onClicked:desktop.fitScene()}
        WorldButton {objectName:"hqZoomIn";text:"+";hint:qsTr("Zoom in");Accessible.name:hint;textSize:desktop.textSize;visible:!desktop.planView;onClicked:desktop.zoomScene(0.15)}
    }
    Rectangle {
        anchors.left:worldView.left;anchors.bottom:worldView.bottom;anchors.margins:20
        width:Math.min(360*desktop.uiScale,worldView.width-40);height:hoverCopy.implicitHeight+24
        visible:desktop.physicalHQ && desktop.place!=="map" && !!desktop.hoveredEquipment && !desktop.compact && !desktop.furnishing
        radius:9;color:"#ee122330";border.color:"#617b86"
        Column {
            id:hoverCopy;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:5
            Label {width:parent.width;text:desktop.hoveredEquipment?desktop.hoveredEquipment.name:"";font.pixelSize:desktop.textSize;font.bold:true;color:ink.paper;wrapMode:Text.Wrap;textFormat:Text.PlainText}
            Label {width:parent.width;text:desktop.hoveredEquipment ? desktop.hoveredEquipment.detail + (desktop.hoveredEquipment.personName ? " · " + desktop.hoveredEquipment.personName : "") : "";textFormat:Text.PlainText;font.pixelSize:desktop.textSize*0.9;color:ink.secondary;wrapMode:Text.Wrap}
            Label {width:parent.width;text:qsTr("Click to inspect and arrange");font.pixelSize:desktop.textSize*0.85;color:ink.secondary;wrapMode:Text.Wrap}
        }
    }
    GameDialog {
        id:roomConversation
        objectName:"hqRoomConversation"
        title:qsTr("Talk at headquarters")
        anchors.centerIn:Overlay.overlay
        width:Math.min(Overlay.overlay.width-32,desktop.textSize*46)
        height:Math.min(Overlay.overlay.height-32,desktop.textSize*40)
        modal:true
        focus:true
        enter: Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:campaign.reducedMotion?0:150}}
        exit: Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:campaign.reducedMotion?0:110}}
        contentItem:ScrollView {
            id:roomConversationScroll
            objectName:"roomConversationScroll"
            contentWidth:availableWidth
            clip:true
            ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
            function reveal(target) {
                let ancestor=target
                while(ancestor && ancestor!==roomContactsPanel) ancestor=ancestor.parent
                if(!ancestor || !contentItem) return
                const y=target.mapToItem(roomContactsPanel,0,0).y
                const top=contentItem.contentY
                const bottom=top+availableHeight
                const destination=y<top || target.height>availableHeight ? y : y+target.height>bottom ? y+target.height-availableHeight : top
                contentItem.contentY=Math.max(0,Math.min(destination,roomContactsPanel.height-availableHeight))
            }
            Connections {
                target: desktop.Window.window
                enabled: roomConversation.opened
                function onActiveFocusItemChanged(){roomConversationScroll.reveal(target.activeFocusItem)}
            }
            ContactsPanel {
                id:roomContactsPanel
                width:roomConversationScroll.availableWidth
                inPerson:true
                textSize:desktop.textSize
                onRevealRequested: target=>roomConversationScroll.reveal(target)
            }
        }
        footer:WorldButton {objectName:"hqCloseTalk";text:qsTr("Back to headquarters");textSize:desktop.textSize;onClicked:roomConversation.close()}
    }
    HeadquartersTools {
        id:hqTools
        objectName:"headquartersTools"
        anchors.right:parent.right;anchors.top:desktop.compact?parent.top:places.bottom;anchors.bottom:dock.top
        anchors.margins:12
        width:Math.min(360*desktop.uiScale,desktop.width-24)
        visible:desktop.physicalHQ && desktop.furnishing && desktop.place!=="map" && (!desktop.compact || !placing)
                && !desktop.inspectingWalks
        textSize:desktop.textSize
        arranging:desktop.physicalHQ && desktop.furnishing
        compactOverlay:desktop.compact
        onDoneRequested:desktop.furnishing=false
        onWalksRequested:desktop.inspectingWalks=true
        editable:campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
        onShopRequested:desktop.openApp("facilities","equipment","laptop")
        onPropertiesRequested:desktop.openApp("facilities","property","laptop")
    }
    Rectangle {anchors.fill:placementControls;anchors.margins:-6;visible:placementControls.visible;radius:9;color:"#ed142633";border.color:"#627a86"}
    ColumnLayout {
        id:placementControls
        visible:desktop.physicalHQ && desktop.furnishing && hqTools.placing
        anchors.horizontalCenter:worldView.horizontalCenter;anchors.top:worldView.top;anchors.topMargin:8
        width:Math.min(worldView.width-24,580)
        Label {
            objectName:"hqPlacementTraffic"
            Layout.fillWidth:true
            Layout.preferredHeight:desktop.textSize*2.6
            text:hqTools.preview.traffic?.summary || hqTools.preview.reason || qsTr("Move over the floor to preview placement")
            font.pixelSize:desktop.textSize;color:hqTools.preview.valid?"#b9f2ce":"#ffd0bc"
            wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight
            horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
        }
        RowLayout {
            Layout.alignment:Qt.AlignHCenter
            WorldButton {objectName:"hqRotatePreview";text:qsTr("Rotate · R");textSize:desktop.textSize;onClicked:hqTools.rotated=!hqTools.rotated}
            WorldButton {objectName:"hqCancelPreview";text:qsTr("Cancel · Esc");textSize:desktop.textSize;onClicked:hqTools.placing=false}
        }
    }
    WorldButton {
        id:backFromWalks
        objectName:"hqBackFromWalks"
        anchors.top:worldView.top;anchors.horizontalCenter:worldView.horizontalCenter;anchors.topMargin:8
        visible:desktop.physicalHQ && desktop.furnishing && desktop.inspectingWalks
        text:qsTr("Back to arranging")
        textSize:desktop.textSize
        onClicked:desktop.inspectingWalks=false
    }
    Shortcut {sequence:"R";enabled:desktop.visible && desktop.furnishing && hqTools.placing;onActivated:hqTools.rotated=!hqTools.rotated}
    Shortcut {sequence:"Escape";enabled:desktop.visible && desktop.furnishing && hqTools.placing;onActivated:hqTools.placing=false}
    ColumnLayout {
        anchors.left:parent.left;anchors.bottom:dock.top;anchors.margins:22*desktop.uiScale
        width:Math.max(200,Math.min(520*desktop.uiScale,desktop.width-(desktop.compact?44:320*desktop.uiScale)))
        visible:desktop.place!=="map" && !desktop.shortLayout && !desktop.physicalHQ
        spacing:7
        Label {Layout.fillWidth:true;text:({workspace:qsTr("An idea takes shape."),lab:qsTr("Make the next discovery."),meeting:qsTr("Every choice has a company behind it."),compute:qsTr("Keep the work running."),research_annex:qsTr("Space for the next idea."),evaluation_suite:qsTr("Put your evidence to work."),collaboration:qsTr("Bring the team together.")})[desktop.place] || "";font.pixelSize:desktop.textSize*1.7;font.bold:true;color:"#f4ead6";wrapMode:Text.Wrap}
        Label {Layout.fillWidth:true;text:campaign.world.setting;font.pixelSize:desktop.textSize;color:"#c7d7cf";wrapMode:Text.Wrap}
        WorldButton {objectName:"openPlaceApp";text:desktop.placeApp()==="lab"?qsTr("Open research notebook"):desktop.placeApp()==="inbox"?qsTr("Open decisions"):desktop.placeApp()==="operations"?qsTr("Open infrastructure"):qsTr("Open company desktop");textSize:desktop.textSize;primary:true;onClicked:desktop.openPlace()}
    }
    RowLayout {
        visible:desktop.shortLayout && desktop.place!=="map" && !desktop.physicalHQ
        anchors.top:hud.bottom;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:10
        GameSelect {objectName:"compactRoomPicker";Layout.fillWidth:true;model:campaign.world.rooms;textRole:"name";valueRole:"id";font.pixelSize:desktop.textSize;currentIndex:Math.max(0,model.findIndex(row=>row.id===desktop.place));Accessible.name:qsTr("Current room");onActivated:desktop.visit(currentValue)}
        WorldButton {text:qsTr("Open");textSize:desktop.textSize;onClicked:desktop.openPlace()}
    }
    Rectangle {
        id: agenda
        objectName:"worldAgenda"
        anchors.top:places.bottom;anchors.bottom:dock.top;anchors.right:parent.right
        anchors.margins:16*desktop.uiScale;width:244*desktop.uiScale;color:"#17283a";radius:14;border.color:"#4f687c"
        visible:!desktop.compact && desktop.place!=="map" && !desktop.physicalHQ
        Rectangle { width:parent.width-24; height:3; radius:1.5; anchors.top:parent.top; anchors.topMargin:8; anchors.horizontalCenter:parent.horizontalCenter; color:campaign.requiredKind!=="none"?"#efc389":"#5e888a" }
        ScrollView {
            id: agendaScroll
            anchors.fill:parent;anchors.margins:16*desktop.uiScale;anchors.topMargin:20*desktop.uiScale;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:agendaScroll.availableWidth;spacing:14
                Label {text:qsTr("TODAY");font.pixelSize:desktop.textSize*0.85;font.letterSpacing:2;color:"#e5bc7e"}
                Label {Layout.fillWidth:true;text:campaign.requiredKind!=="none"?campaign.requiredTitle:qsTr("Your next move");textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:desktop.textSize*1.2;font.bold:true;color:"#eee6d5"}
                Label {Layout.fillWidth:true;text:campaign.requiredKind!=="none"?qsTr("A decision needs your attention. Time waits for you."):qsTr("Review the team, check your research, or let the next day begin.");wrapMode:Text.Wrap;font.pixelSize:desktop.textSize;color:"#b7cac2"}
                WorldButton {objectName:"agendaDecisionButton";Layout.fillWidth:true;text:campaign.requiredKind!=="none"?qsTr("Review decision"):qsTr("Open journal");textSize:desktop.textSize;primary:campaign.requiredKind!=="none";onClicked:desktop.journalRequested()}
                Rectangle {Layout.fillWidth:true;height:1;color:"#4e6175"}
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
        height:desktop.shortLayout?desktop.textSize*3.2+16:76*desktop.uiScale;color:"#101c29"
        Rectangle {width:parent.width;height:1;color:"#425263"}
        WorldButton {
            objectName:"dockPrevious";anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;anchors.leftMargin:4
            visible:desktop.dockOverflow;enabled:dockScroll.contentItem.contentX>1
            text:"‹";hint:qsTr("More apps to the left");Accessible.name:hint;textSize:desktop.textSize;padding:8
            onClicked:desktop.scrollDock(-1)
        }
        WorldButton {
            objectName:"dockNext";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.rightMargin:4
            visible:desktop.dockOverflow;enabled:dockScroll.contentItem.contentX+dockScroll.availableWidth<dockRow.width-1
            text:"›";hint:qsTr("More apps to the right");Accessible.name:hint;textSize:desktop.textSize;padding:8
            onClicked:desktop.scrollDock(1)
        }
        ScrollView {
            id:dockScroll
            anchors.fill:parent;anchors.margins:desktop.shortLayout?8:10*desktop.uiScale;anchors.leftMargin:desktop.dockOverflow?46*desktop.uiScale:10*desktop.uiScale;anchors.rightMargin:anchors.leftMargin;contentHeight:availableHeight;clip:true
            ScrollBar.vertical.policy:ScrollBar.AlwaysOff
            Row {
                id:dockRow
                spacing:4
                WorldButton {objectName:"officeTab";iconApp:"office";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Company");textSize:desktop.textSize;onClicked:desktop.openApp("office","","laptop")}
                WorldButton {objectName:"peopleTab";iconApp:"people";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("People");textSize:desktop.textSize;onClicked:desktop.openApp("people","","laptop")}
                WorldButton {objectName:"messagesTab";iconApp:"inbox";visible:campaign.world.contactsAvailable;quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Messages");textSize:desktop.textSize;onClicked:desktop.openApp("people","messages","tablet")}
                WorldButton {objectName:"personalLifeTab";iconApp:"leadership";visible:!!campaign.world.personalLife.cash;quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("My life");textSize:desktop.textSize;onClicked:desktop.openApp("leadership","personal","tablet")}
                WorldButton {objectName:"headquartersTab";iconApp:"facilities";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Headquarters");textSize:desktop.textSize;onClicked:desktop.openApp("facilities","","laptop")}
                WorldButton {objectName:"labTab";iconApp:"lab";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Lab");textSize:desktop.textSize;onClicked:desktop.openApp("lab","research","laptop")}
                WorldButton {objectName:"inboxTab";iconApp:"inbox";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("News & decisions");textSize:desktop.textSize;onClicked:desktop.journalRequested()}
                WorldButton {objectName:"mapTab";iconApp:"society";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Map");textSize:desktop.textSize;checked:desktop.place==="map";onClicked:desktop.showMap(desktop.nav.scale || "hq")}
                WorldButton {objectName:"devicesButton";iconApp:"devices";quiet:true;padding:desktop.textSize*0.65;onActiveFocusChanged:if(activeFocus)desktop.revealDock(this);text:qsTr("Devices & apps");textSize:desktop.textSize;onClicked:desktop.launcherRequested()}
            }
        }
    }
    Popup {
        id:compactTime;anchors.centerIn:Overlay.overlay;width:Math.min(520,desktop.width-32);height:Math.min(500,desktop.height-32);modal:true;focus:true; padding:16
        background:Rectangle {radius:12;color:"#1a2b3d";border.color:"#8d9d7e"}
        contentItem:ScrollView {
            id:timeScroll;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:timeScroll.availableWidth;spacing:12
                Label {Layout.fillWidth:true;text:qsTr("Cash %1\nAvailable %2").arg(campaign.cashText,campaign.availableText);font.pixelSize:desktop.textSize;wrapMode:Text.Wrap}
                WorldButton {text:campaign.paused?qsTr("Play"):qsTr("Pause");textSize:desktop.textSize;enabled:!campaign.busy && !campaign.storageBlocked && (!campaign.paused || campaign.canPlay);onClicked:campaign.togglePaused()}
                WorldButton {text:qsTr("Next day");textSize:desktop.textSize;enabled:campaign.canAdvance && !campaign.storageBlocked;onClicked:campaign.step()}
                GameSelect {Layout.fillWidth:true;model:[1,10,30,60];currentIndex:model.indexOf(campaign.daysPerSecond);displayText:qsTr("%1 days/s").arg(campaign.daysPerSecond);font.pixelSize:desktop.textSize;Accessible.name:qsTr("Days per second");onActivated:campaign.daysPerSecond=model[currentIndex]}
                WorldButton {text:qsTr("Advance until…");textSize:desktop.textSize;onClicked:{compactTime.close();advanceMenu.open()}}
                WorldButton {text:qsTr("Close");textSize:desktop.textSize;onClicked:compactTime.close()}
            }
        }
    }
    Popup {
        id:advanceMenu;anchors.centerIn:Overlay.overlay;width:Math.min(410,desktop.width-32);modal:true;focus:true;padding:20
        background:Rectangle {radius:12;color:"#1a2b3d";border.color:"#8d9d7e"}
        contentItem:ColumnLayout {
            Label {Layout.fillWidth:true;text:qsTr("Advance until…");font.pixelSize:desktop.textSize;color:"#f2e8d3"}
            Repeater {model:[{id:"project",name:qsTr("Next project")},{id:"evaluation",name:qsTr("Next evaluation")},{id:"deadline",name:qsTr("Next deadline")},{id:"leadership",name:qsTr("Next leadership date")}];WorldButton {required property var modelData;Layout.fillWidth:true;text:modelData.name;textSize:desktop.textSize;enabled:campaign.paused && campaign.canPlay && !campaign.storageBlocked;onClicked:{advanceMenu.close();campaign.advanceTo(modelData.id)}}}
            WorldButton {Layout.fillWidth:true;text:qsTr("Cancel");textSize:desktop.textSize;onClicked:advanceMenu.close()}
        }
    }
}
