import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ColumnLayout {
    id: panel
    objectName:"personalLifePanel"
    required property int textSize
    readonly property var life: campaign.world.personalLife
    readonly property var family: campaign.world.familyHome
    signal openApp(string app, string section, string device)
    readonly property bool editable: campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    property string page: campaign.world.draft("personal/page", "homes")
    property string pendingPage: page
    readonly property bool transitioning: pageFade.running
    enabled: !transitioning
    property int propertyId: Number(campaign.world.draft("personal/property", "0"))
    property int selectedItem: 0
    property bool placing: false
    property bool rotated: false
    property int px: 0
    property int py: 0
    readonly property var properties: (life.properties || []).filter(property => !property.sold)
    readonly property var floor: { campaign.world.reviewToken; return campaign.world.personalFloor(propertyId) }
    readonly property var preview: {
        campaign.world.reviewToken
        return placing ? campaign.world.personalPlacement(selectedItem, propertyId, px, py, rotated) : ({})
    }
    spacing: 12

    function commitPage(id) {
        page=id;campaign.world.setDraft("personal/page",id)
        if(id==="rooms" && !floor.width && properties.length)selectProperty(properties[0].id)
    }
    function changePage(id) {
        if(id===page)return
        placing=false
        if(campaign.reducedMotion)commitPage(id)
        else {pendingPage=id;pageFade.restart()}
    }
    function selectProperty(id) {propertyId=id;placing=false;campaign.world.setDraft("personal/property",String(id))}
    function pick(id) {
        const item = (life.items || []).find(row => row.id === id)
        if (!item || !item.delivered) return
        selectedItem=id;rotated=item.rotated;placing=true
        floorPlan.forceActiveFocus(Qt.OtherFocusReason)
    }
    function place() {
        if (!editable || !preview.valid) return
        campaign.world.placePersonalDecor(selectedItem,propertyId,px,py,rotated,campaign.world.reviewToken)
        placing=false
    }
    function review(kind, row) {
        placing=false
        purchase.kind=kind;purchase.subject=row.id;purchase.heading=row.name
        purchase.copy=kind==="home"?row.description+"\n\n"+row.detail
            :kind==="decor"?qsTr("%1 · %2\n%3").arg(row.supplier).arg(row.price).arg(row.detail)
            :kind==="sell_decor"?qsTr("Sell %1 for %2. The proceeds return to your personal savings.").arg(row.name).arg(row.resale)
            :qsTr("Sell this home for %1. Its contents return to personal storage. Paid upkeep for the current month stays paid, and outstanding bills remain yours.").arg(row.resale)
        purchase.revision=campaign.world.reviewToken
        purchase.open()
    }
    SequentialAnimation {
        id:pageFade
        NumberAnimation {target:panel;property:"opacity";to:0;duration:90}
        ScriptAction {script:panel.commitPage(panel.pendingPage)}
        NumberAnimation {target:panel;property:"opacity";to:1;duration:140}
    }
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
        color: "#d0e0d4"
    }
    Copy {text:qsTr("A life of your own");font.pixelSize:panel.textSize*1.4;font.bold:true;color:"#f0e3c2"}
    Copy {
        objectName:"personalBalances"
        text:qsTr("Personal savings %1 · Private assets %2 · Unpaid upkeep %3").arg(life.cash || "").arg(life.assets || "").arg(life.unpaid || "")
        color:"#b9dba7"
    }
    Copy {text:qsTr("Homes, art and interiors use your savings and paid salary. Asset values use the disclosed resale prices.")}
    WorldButton {
        objectName:"personalPayUpkeep"
        visible:!!panel.life.hasUnpaid
        text:qsTr("Pay personal upkeep · %1").arg(panel.life.unpaid || "")
        textSize:panel.textSize
        Layout.maximumWidth:panel.width
        enabled:panel.editable
        onClicked:campaign.world.payPersonalUpkeep(campaign.world.reviewToken)
    }
    Flow {
        Layout.fillWidth:true;spacing:8
        Repeater {
            model:[{id:"homes",name:qsTr("Juniper Estates")},{id:"rooms",name:qsTr("My rooms")},{id:"interiors",name:qsTr("Interiors & gallery")}]
                .concat(panel.family.name ? [{id:"family",name:qsTr("Family home")}] : [])
                .concat(campaign.world.personalEvents.packages ? [{id:"events",name:qsTr("Gatherings")}] : [])
            WorldButton {
                required property var modelData
                objectName:"personalPage_"+modelData.id
                text:modelData.name;textSize:panel.textSize;checked:panel.page===modelData.id
                onClicked:panel.changePage(modelData.id)
            }
        }
    }
    FamilyHomePanel {
        Layout.fillWidth: true
        visible: panel.page === "family" && !!panel.family.name
        textSize: panel.textSize
        home: panel.family
        onMessagesRequested: panel.openApp("people", "messages", "laptop")
        onRealtorRequested: panel.openApp("facilities", "property", "laptop")
    }
    Repeater {
        model:panel.page==="homes"?panel.life.offers || []:[]
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:offerCopy.implicitHeight+24;color:"#234247";radius:10;border.color:"#56766b"
            ColumnLayout {
                id:offerCopy
                anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                EquipmentGlyph {kind:"home";Layout.preferredWidth:96;Layout.preferredHeight:72;Accessible.name:modelData.name}
                Copy {text:modelData.name;font.pixelSize:panel.textSize*1.2;font.bold:true}
                Copy {text:qsTr("%1 × %2 m · %3 rooms · %4").arg(modelData.width).arg(modelData.depth).arg(modelData.rooms).arg(modelData.agent)}
                Copy {text:modelData.description+"\n"+modelData.detail}
                WorldButton {
                    objectName:"personalBuyHome_"+modelData.id
                    text:modelData.owned?qsTr("Already owned"):qsTr("Review purchase · %1").arg(modelData.cost)
                    textSize:panel.textSize;Layout.maximumWidth:offerCopy.width
                    enabled:panel.editable && !modelData.owned
                    onClicked:panel.review("home",modelData)
                }
            }
        }
    }
    ColumnLayout {
        visible:panel.page==="rooms";Layout.fillWidth:true;spacing:12
        Copy {visible:panel.properties.length===0;text:qsTr("Your owned homes will appear here. Juniper Estates has listings to review.")}
        GameSelect {
            objectName:"personalPropertyPicker";visible:panel.properties.length>0;Layout.fillWidth:true;textSize:panel.textSize
            model:panel.properties;textRole:"name";valueRole:"id"
            currentIndex:model.findIndex(row=>row.id===panel.propertyId)
            Accessible.name:qsTr("Personal property")
            onActivated:panel.selectProperty(currentValue)
        }
        Flow {
            Layout.fillWidth:true;spacing:8
            Repeater {
                model:panel.properties
                WorldButton {
                    required property var modelData
                    objectName:"personalOpenHome_"+modelData.id
                    text:modelData.name;textSize:panel.textSize;checked:panel.propertyId===modelData.id
                    onClicked:panel.selectProperty(modelData.id)
                }
            }
        }
        Copy {visible:!!panel.floor.width;text:(panel.properties.find(property=>property.id===panel.propertyId) || {}).detail || ""}
        HeadquartersPlan {
            id:floorPlan;objectName:"personalFloorPlan"
            Layout.fillWidth:true;Layout.preferredHeight:Math.max(180,Math.min(320,panel.width*0.72,panel.Window.window.height-panel.textSize*12))
            visible:!!panel.floor.width
            headquarters:panel.floor
            selectedItem:panel.selectedItem;editing:panel.editable;placing:panel.placing;preview:panel.preview
            Accessible.name:qsTr("%1, personal floor plan").arg(panel.floor.name || "")
            onHovered:(x,y,target)=>{if(panel.placing){panel.px=x;panel.py=y}}
            onTilePicked:(x,y,item)=>{floorPlan.forceActiveFocus();if(panel.placing){panel.px=x;panel.py=y;panel.place()}else if(item)panel.pick(item)}
            onItemDragged:(item,x,y,target)=>{panel.pick(item);panel.px=x;panel.py=y;panel.place()}
            onRotateRequested:panel.rotated=!panel.rotated
            onCancelRequested:panel.placing=false
            Keys.onPressed:event=>{
                if(event.key===Qt.Key_Escape){panel.placing=false;event.accepted=true}
                else if(event.key===Qt.Key_R && panel.placing){panel.rotated=!panel.rotated;event.accepted=true}
            }
        }
        Copy {visible:!!panel.floor.width;text:panel.placing?panel.preview.reason || "":qsTr("Click a furnishing below to pick it up, then click a block. Rotate with R or the button. Escape or right-click cancels.")}
        Flow {
            visible:panel.placing;Layout.fillWidth:true;spacing:8
            WorldButton {objectName:"personalRotate";text:qsTr("Rotate");textSize:panel.textSize;onClicked:panel.rotated=!panel.rotated}
            WorldButton {text:qsTr("Cancel placement");textSize:panel.textSize;onClicked:panel.placing=false}
            SpinBox {objectName:"personalX";from:0;to:31;value:Math.max(0,panel.px);editable:true;font.pixelSize:panel.textSize;Accessible.name:qsTr("Personal furnishing X coordinate");onValueModified:panel.px=value}
            SpinBox {objectName:"personalY";from:0;to:31;value:Math.max(0,panel.py);editable:true;font.pixelSize:panel.textSize;Accessible.name:qsTr("Personal furnishing Y coordinate");onValueModified:panel.py=value}
            WorldButton {objectName:"personalPlace";text:qsTr("Place at coordinates");textSize:panel.textSize;enabled:panel.editable && !!panel.preview.valid;onClicked:panel.place()}
            WorldButton {text:qsTr("Return to storage");textSize:panel.textSize;enabled:panel.editable;onClicked:{campaign.world.storePersonalDecor(panel.selectedItem,campaign.world.reviewToken);panel.placing=false}}
        }
        Flow {
            Layout.fillWidth:true;spacing:8
            Repeater {
                model:panel.life.items || []
                ColumnLayout {
                    required property var modelData
                    width:Math.min(panel.width,Math.max(180,panel.textSize*12))
                WorldButton {
                    objectName:"personalPick_"+modelData.id
                    Layout.fillWidth:true
                    height:inventoryCopy.implicitHeight+16;padding:8;text:modelData.name;textSize:panel.textSize
                    enabled:panel.editable && modelData.delivered && !!panel.floor.width
                    contentItem:ColumnLayout {
                        id:inventoryCopy
                        EquipmentGlyph {kind:modelData.kind;Layout.preferredWidth:80;Layout.preferredHeight:72;Layout.alignment:Qt.AlignHCenter}
                        Copy {text:modelData.name;font.bold:true}
                        Copy {text:modelData.detail;font.pixelSize:panel.textSize*0.85}
                    }
                    onClicked:panel.pick(modelData.id)
                }
                WorldButton {
                    objectName:"personalSellDecor_"+modelData.id
                    text:qsTr("Sell · %1").arg(modelData.resale)
                    textSize:panel.textSize
                    Layout.fillWidth:true
                    enabled:panel.editable && modelData.delivered
                    onClicked:panel.review("sell_decor",modelData)
                }
                }
            }
        }
        WorldButton {
            objectName:"personalSellHome";visible:!!panel.floor.width;text:qsTr("Review selling this home");textSize:panel.textSize;enabled:panel.editable
            onClicked:panel.review("sell",panel.properties.find(property=>property.id===panel.propertyId))
        }
    }
    Repeater {
        model:panel.page==="interiors"?panel.life.decor || []:[]
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:interiorCopy.implicitHeight+24;color:"#234247";radius:10;border.color:"#56766b"
            ColumnLayout {
                id:interiorCopy
                anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                EquipmentGlyph {kind:modelData.kind;Layout.preferredWidth:96;Layout.preferredHeight:80;Accessible.name:modelData.name}
                Copy {text:modelData.name;font.bold:true}
                Copy {text:modelData.supplier+" · "+modelData.price+"\n"+modelData.detail}
                WorldButton {objectName:"personalBuyDecor_"+modelData.id;text:qsTr("Review purchase");textSize:panel.textSize;enabled:panel.editable;onClicked:panel.review("decor",modelData)}
            }
        }
    }
    PersonalEventsPanel {Layout.fillWidth:true;visible:panel.page==="events";textSize:panel.textSize}
    GameDialog {
        id:purchase
        property string kind:""
        property var subject:""
        property string heading:""
        property string copy:""
        property string revision:""
        title:kind.startsWith("sell")?qsTr("Review sale"):qsTr("Review purchase")
        modal:true;focus:true
        anchors.centerIn:Overlay.overlay
        width:Math.min(Overlay.overlay.width-32,panel.textSize*38)
        implicitHeight:Math.min(panel.textSize*25,Overlay.overlay.height-32)
        height:implicitHeight
        onOpened:if(purchaseScroll.contentItem)purchaseScroll.contentItem.contentY=0
        enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:campaign.reducedMotion?0:150}}
        exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:campaign.reducedMotion?0:100}}
        header:Item {visible:false;implicitHeight:0}
        contentItem:ScrollView {
            id:purchaseScroll
            contentWidth:availableWidth;clip:true
            ScrollBar.vertical.policy:ScrollBar.AlwaysOn
            ColumnLayout {
                width:purchaseScroll.availableWidth;spacing:12
                Copy {text:purchase.title;font.pixelSize:panel.textSize*1.15;font.bold:true;color:"#f1e3bc"}
                Copy {text:purchase.heading;font.bold:true}
                Copy {text:purchase.copy}
            }
        }
        footer:Flow {
            width:purchase.availableWidth;spacing:8;padding:8
            WorldButton {text:qsTr("Back");textSize:panel.textSize;onClicked:purchase.close()}
            WorldButton {
                objectName:"personalConfirm";text:purchase.kind.startsWith("sell")?qsTr("Confirm sale"):qsTr("Pay from personal savings")
                width:Math.min(implicitWidth,parent.width-16);textSize:panel.textSize;primary:true;enabled:panel.editable
                onClicked:{
                    if(purchase.kind==="home")campaign.world.buyPersonalProperty(purchase.subject,purchase.revision)
                    else if(purchase.kind==="decor")campaign.world.buyPersonalDecor(purchase.subject,purchase.revision)
                    else if(purchase.kind==="sell_decor")campaign.world.sellPersonalDecor(Number(purchase.subject),purchase.revision)
                    else campaign.world.sellPersonalProperty(Number(purchase.subject),purchase.revision)
                    purchase.close()
                }
            }
        }
    }
}
