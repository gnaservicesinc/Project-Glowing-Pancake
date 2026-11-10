import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    property int textSize:14
    signal visitRoom(string place)
    signal openInfrastructure()
    readonly property var headquarters:campaign.world.headquarters
    readonly property bool editable:campaign.companyEditable && !campaign.storageBlocked && !campaign.busy
    property string tab:campaign.world.navigation.section==="property"?"property":"equipment"
    property string market:headquarters.market || "los_angeles"
    property string landId:""
    property var planRooms:[{x:0,y:0,width:6,depth:4}]
    property int planWidth:6
    property int planDepth:4
    property int selectedRoom:0
    property var bids:[]
    property string error:""
    property string supplier:"All shops"
    readonly property var suppliers:["All shops"].concat(Array.from(new Set(campaign.world.hqEquipmentOffers.map(row=>row.supplier))))
    spacing:14
    ReviewPause {id:reviewPause}
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#c5d8cb"}
    component Heading:Copy {font.pixelSize:panel.textSize*1.3;font.bold:true;color:"#f0e6cd"}
    function restore() {
        landId="";bids=[];error=""
        market=campaign.world.draft("hq/market",headquarters.market || "los_angeles")
        try {
            const saved=JSON.parse(campaign.world.draft("hq/build-plan","{}"))
            if(campaign.world.hqProperties.some(lot=>lot.id===saved.land) && Number.isInteger(saved.width) && Number.isInteger(saved.depth) && Array.isArray(saved.rooms)) {
                landId=saved.land;planWidth=saved.width;planDepth=saved.depth;planRooms=saved.rooms;selectedRoom=0
                buildWidth.text=String(planWidth);buildDepth.text=String(planDepth)
            }
        } catch(e) {}
    }
    Component.onCompleted:Qt.callLater(restore)
    Connections {
        target:campaign.world
        function onDraftsReset(){Qt.callLater(panel.restore)}
        function onNavigationChanged(){const n=campaign.world.navigation;if(n.app==="facilities" && ["equipment","property","build"].includes(n.section))panel.tab=n.section}
    }
    function preservePlan() {campaign.world.setDraft("hq/build-plan",JSON.stringify({land:landId,width:planWidth,depth:planDepth,rooms:planRooms}));bids=[]}
    function resetPlan() {
        const lot=campaign.world.hqProperties.find(row=>row.id===landId)
        if(!lot) return
        planWidth=lot.width;planDepth=lot.depth;planRooms=[{x:0,y:0,width:planWidth,depth:planDepth}];selectedRoom=0
        buildWidth.text=String(planWidth);buildDepth.text=String(planDepth);preservePlan()
    }
    function resizePlan() {
        const lot=campaign.world.hqProperties.find(row=>row.id===landId),w=Number(buildWidth.text),d=Number(buildDepth.text)
        if(!lot || !Number.isInteger(w) || !Number.isInteger(d) || w<3 || d<3 || w>lot.width || d>lot.depth) {error=qsTr("Enter whole-metre dimensions within your plot.");return}
        planWidth=w;planDepth=d;planRooms=[{x:0,y:0,width:w,depth:d}];selectedRoom=0;error="";preservePlan()
    }
    function splitRoom(vertical) {
        const r=planRooms[selectedRoom],cut=Number(splitAt.text)
        if(!r || !Number.isInteger(cut) || cut<2 || cut>(vertical?r.width:r.depth)-2) {error=qsTr("Each new room needs at least two metres on either side of the split.");return}
        const next=planRooms.slice()
        if(vertical) next.splice(selectedRoom,1,{x:r.x,y:r.y,width:cut,depth:r.depth},{x:r.x+cut,y:r.y,width:r.width-cut,depth:r.depth})
        else next.splice(selectedRoom,1,{x:r.x,y:r.y,width:r.width,depth:cut},{x:r.x,y:r.y+cut,width:r.width,depth:r.depth-cut})
        planRooms=next;error="";preservePlan()
    }
    function reviewProperty(plan,land) {
        reviewPause.begin(()=>{
            const quote=campaign.world.hqPropertyQuote(panel.market,plan,land)
            purchase.review=quote;purchase.kind=land?"land":"lease";purchase.title=land?qsTr("Purchase this land?"):qsTr("Lease and move headquarters?");purchase.open()
        })
    }
    function reviewEquipment(id) {
        reviewPause.begin(()=>{
            const row=campaign.world.hqEquipmentOffers.find(value=>value.id===id)
            if(!row) return
            purchase.review={offer:row.id,revision:row.revision,detail:row.name+"\n"+row.detail};purchase.kind="equipment";purchase.title=qsTr("Place this supplier order?");purchase.open()
        })
    }
    function reviewBuild(bid) {
        reviewPause.begin(()=>{
            const current=campaign.world.hqBids(landId,planWidth,planDepth,planRooms).find(row=>row.id===bid.id)
            if(!current || current.error) return
            purchase.review={land:landId,contractor:current.id,width:planWidth,depth:planDepth,rooms:JSON.parse(JSON.stringify(planRooms)),revision:current.revision,detail:current.name+"\n"+current.detail};purchase.kind="build";purchase.title=qsTr("Hire this contractor?");purchase.open()
        })
    }
    Heading {text:qsTr("A place for the company")}
    Copy {text:headquarters.summary || ""}
    Flow {
        Layout.fillWidth:true;spacing:8
        Repeater {model:[{id:"equipment",name:qsTr("Equipment & suppliers")},{id:"property",name:qsTr("Realtor")},{id:"build",name:qsTr("Land & construction")}];WorldButton {required property var modelData;objectName:"hqTab_"+modelData.id;text:modelData.name;textSize:panel.textSize;checked:panel.tab===modelData.id;onClicked:{panel.tab=modelData.id;const n=campaign.world.navigation;campaign.world.navigate(n.place,n.scale,n.device,"facilities",modelData.id)}}}
        WorldButton {text:qsTr("Return to the room");textSize:panel.textSize;onClicked:panel.visitRoom("workspace")}
    }
    ColumnLayout {
        visible:panel.tab==="equipment";Layout.fillWidth:true;spacing:12
        Copy {text:qsTr("Buy furnishings and hardware from a supplier. Deliveries go into storage. Return to the room to place and move them, mount a computer on a desk, and assign people. Power and actual hardware determine compute; an empty room supplies none.")}
        Rectangle {
            Layout.fillWidth:true;implicitHeight:storeHeader.implicitHeight+24;radius:12;color:"#e7dfc8"
            ColumnLayout {
                id:storeHeader;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                Label {text:qsTr("←  →   ◉   supplies.local / storefronts");font.pixelSize:panel.textSize;color:"#39585a"}
                Label {text:panel.supplier==="All shops"?qsTr("Make room for your next idea."):panel.supplier;font.pixelSize:panel.textSize*1.55;font.bold:true;color:"#233f46";Layout.fillWidth:true;wrapMode:Text.Wrap}
                Flow {Layout.fillWidth:true;spacing:6;Repeater {model:panel.suppliers;WorldButton {required property string modelData;text:modelData;textSize:panel.textSize;checked:panel.supplier===modelData;onClicked:panel.supplier=modelData}}}
            }
        }
        Repeater {
            model:campaign.world.hqEquipmentOffers.filter(row=>panel.supplier==="All shops" || row.supplier===panel.supplier)
            Rectangle {
                required property var modelData
                Layout.fillWidth:true;implicitHeight:equipmentCard.implicitHeight+28;color:"#213d40";radius:10;border.color:"#48675c"
                ColumnLayout {
                    id:equipmentCard;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14
                    RowLayout {
                        Layout.fillWidth:true
                        EquipmentGlyph {Layout.preferredWidth:panel.textSize*6;Layout.preferredHeight:panel.textSize*6;kind:modelData.kind}
                        ColumnLayout {Layout.fillWidth:true;Heading {text:modelData.name} Copy {text:modelData.supplier;color:"#b2ccaa"}}
                    }
                    Copy {text:modelData.detail}
                    WorldButton {objectName:"hqBuy_"+modelData.id;text:qsTr("Order · %1").arg(modelData.price);textSize:panel.textSize;enabled:panel.editable;onClicked:panel.reviewEquipment(modelData.id)}
                }
            }
        }
        Heading {text:qsTr("Your equipment")}
        Repeater {model:panel.headquarters.items || [];Copy {required property var modelData;text:modelData.name+" · "+modelData.detail+"\n"+modelData.ownership}}
    }
    ColumnLayout {
        visible:panel.tab==="property";Layout.fillWidth:true;spacing:12
        Heading {text:qsTr("Available commercial premises")}
        Copy {text:qsTr("A realtor offers actual spaces in the city you select. Compare floor plans, landlords, rent and deposits. Moving ends your old lease under its terms and puts equipment into storage so you can arrange the new rooms.")}
        GameSelect {
            id:marketPicker;objectName:"hqMarketPicker";Layout.fillWidth:true;textSize:panel.textSize
            model:campaign.world.hqMarkets;textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(row=>row.id===panel.market))
            Accessible.name:qsTr("Property market city");onActivated:{panel.market=currentValue;campaign.world.setDraft("hq/market",currentValue)}
        }
        Repeater {
            model:campaign.world.hqListings(panel.market)
            Rectangle {
                required property var modelData
                Layout.fillWidth:true;implicitHeight:propertyCard.implicitHeight+28;color:"#213d40";radius:10;border.color:"#48675c"
                ColumnLayout {
                    id:propertyCard;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14
                    Heading {text:modelData.name || modelData.error || ""}
                    HeadquartersPlan {Layout.fillWidth:true;Layout.preferredHeight:panel.textSize*13;headquarters:({width:modelData.width,depth:modelData.depth,rooms:modelData.rooms,items:[]})}
                    Copy {text:modelData.detail || ""}
                    Flow {
                        Layout.fillWidth:true;spacing:8
                        WorldButton {objectName:"hqLease_"+modelData.plan;text:qsTr("Review lease & move");textSize:panel.textSize;enabled:panel.editable && !modelData.error;onClicked:panel.reviewProperty(modelData.plan,false)}
                        WorldButton {objectName:"hqLand_"+modelData.plan;text:qsTr("Buy land of this size instead");textSize:panel.textSize;enabled:panel.editable && !modelData.error;onClicked:panel.reviewProperty(modelData.plan,true)}
                    }
                }
            }
        }
    }
    ColumnLayout {
        visible:panel.tab==="build";Layout.fillWidth:true;spacing:12
        Heading {text:qsTr("Build on your land")}
        Copy {text:campaign.world.hqProperties.length?qsTr("Choose an owned plot, design the rooms, and compare three contractor bids. You prepay the quoted cost and maximum authorized contingency; unused contingency comes back. Delays and overruns have real consequences. Owned land and buildings pay property tax, with no rent."):qsTr("Buy a plot through the realtor first. A land purchase includes no building, furniture or compute.")}
        Repeater {
            model:campaign.world.hqProperties
            ColumnLayout {
                required property var modelData
                Layout.fillWidth:true
                Heading {text:modelData.name}
                Copy {text:modelData.detail}
                WorldButton {objectName:"hqDesign_"+modelData.id;visible:modelData.canBuild;text:qsTr("Design a building here");textSize:panel.textSize;enabled:panel.editable;onClicked:{panel.landId=modelData.id;panel.resetPlan()}}
                WorldButton {objectName:"hqMoveOwned_"+modelData.construction;visible:modelData.ready || false;text:qsTr("Move into this building");textSize:panel.textSize;enabled:panel.editable;onClicked:reviewPause.begin(()=>{purchase.review=campaign.world.hqMoveQuote(modelData.construction);purchase.kind="move";purchase.title=qsTr("Move headquarters?");purchase.open()})}
            }
        }
        ColumnLayout {
            visible:panel.landId.length>0;Layout.fillWidth:true;spacing:10
            Heading {text:qsTr("Your floor plan")}
            Flow {
                Layout.fillWidth:true;spacing:8
                GameField {id:buildWidth;objectName:"hqBuildingWidth";width:panel.textSize*6;text:"6";textSize:panel.textSize;maximumLength:2;Accessible.name:qsTr("Building width in metres")}
                GameField {id:buildDepth;objectName:"hqBuildingDepth";width:panel.textSize*6;text:"4";textSize:panel.textSize;maximumLength:2;Accessible.name:qsTr("Building depth in metres")}
                WorldButton {text:qsTr("Set dimensions");textSize:panel.textSize;onClicked:panel.resizePlan()}
                WorldButton {text:qsTr("Use whole plot / remove splits");textSize:panel.textSize;onClicked:panel.resetPlan()}
            }
            HeadquartersPlan {
                objectName:"hqConstructionPlan";Layout.fillWidth:true;Layout.preferredHeight:panel.textSize*19;editing:true
                headquarters:({width:panel.planWidth,depth:panel.planDepth,rooms:panel.planRooms,items:[]})
                onTilePicked:(x,y,item)=>{const n=panel.planRooms.findIndex(room=>x>=room.x && y>=room.y && x<room.x+room.width && y<room.y+room.depth);if(n>=0)panel.selectedRoom=n}
            }
            Copy {text:qsTr("Selected room %1. Click a room in the plan, then split it at a chosen metre mark. Equipment will define what each room is used for.").arg(panel.selectedRoom+1)}
            Flow {
                Layout.fillWidth:true;spacing:8
                GameSelect {width:panel.textSize*12;textSize:panel.textSize;model:panel.planRooms.map((room,index)=>({id:index,name:qsTr("Room %1").arg(index+1)}));textRole:"name";valueRole:"id";currentIndex:panel.selectedRoom;Accessible.name:qsTr("Room to split");onActivated:panel.selectedRoom=currentValue}
                GameField {id:splitAt;objectName:"hqRoomSplit";width:panel.textSize*6;text:"3";textSize:panel.textSize;maximumLength:2;Accessible.name:qsTr("Split position in metres")}
                WorldButton {objectName:"hqSplitVertical";text:qsTr("Split left / right");textSize:panel.textSize;enabled:panel.planRooms.length<16;onClicked:panel.splitRoom(true)}
                WorldButton {text:qsTr("Split top / bottom");textSize:panel.textSize;enabled:panel.planRooms.length<16;onClicked:panel.splitRoom(false)}
                WorldButton {objectName:"hqGetBids";text:qsTr("Get three contractor bids");textSize:panel.textSize;enabled:panel.editable;onClicked:{panel.bids=campaign.world.hqBids(panel.landId,panel.planWidth,panel.planDepth,panel.planRooms);panel.error=""}}
            }
            Copy {visible:panel.error.length>0;text:panel.error;color:"#edbd90"}
            Repeater {
                model:panel.bids
                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth:true
                    Heading {text:modelData.name || qsTr("Revise the plan")}
                    Copy {text:modelData.detail || modelData.error || ""}
                    WorldButton {objectName:"hqBid_"+modelData.id;visible:!modelData.error;text:qsTr("Review this contractor");textSize:panel.textSize;enabled:panel.editable;onClicked:panel.reviewBuild(modelData)}
                }
            }
        }
        Repeater {model:panel.headquarters.notices || [];Copy {required property var modelData;text:modelData.date+" · "+modelData.text}}
    }
    WorldButton {text:qsTr("Regional facilities & travel");textSize:panel.textSize;onClicked:panel.openInfrastructure()}
    GameDialog {
        id:purchase;objectName:"hqPurchaseReview"
        property var review:({})
        property string kind:""
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*46,Overlay.overlay.width-32);height:Math.min(panel.textSize*34,Overlay.overlay.height-32)
        modal:true;focus:true;padding:20
        background:Rectangle {radius:12;color:"#142d32";border.color:"#bba775"}
        header:Label {padding:20;text:purchase.title;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*1.4;font.bold:true;color:"#eee5cc"}
        contentItem:ScrollView {id:purchaseScroll;contentWidth:availableWidth;clip:true;Label {width:purchaseScroll.availableWidth;text:purchase.review.error || purchase.review.detail || "";font.pixelSize:panel.textSize;color:"#c6d9ca";textFormat:Text.PlainText;wrapMode:Text.Wrap}}
        footer:RowLayout {
            spacing:10
            Item {Layout.fillWidth:true}
            WorldButton {text:qsTr("Cancel");textSize:panel.textSize;onClicked:purchase.reject()}
            WorldButton {objectName:"hqConfirmPurchase";text:purchase.kind==="lease" || purchase.kind==="move"?qsTr("Move headquarters"):purchase.kind==="build"?qsTr("Hire contractor"):qsTr("Confirm purchase");textSize:panel.textSize;primary:true;enabled:!purchase.review.error && panel.editable;onClicked:purchase.accept()}
        }
        onAccepted:{
            const r=review
            if(kind==="equipment") campaign.world.buyHQEquipment(r.offer,r.revision)
            else if(kind==="lease") campaign.world.leaseHQ(r.market,r.plan,r.revision)
            else if(kind==="land") campaign.world.buyHQLand(r.market,r.plan,r.revision)
            else if(kind==="build") campaign.world.buildHQ(r.land,r.contractor,r.width,r.depth,r.rooms,r.revision)
            else if(kind==="move") campaign.world.moveToOwnedHQ(r.construction,r.revision)
        }
    }
}
