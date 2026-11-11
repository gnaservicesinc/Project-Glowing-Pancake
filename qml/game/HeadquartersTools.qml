import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tools
    GameInk { id: ink }
    required property int textSize
    required property bool editable
    property var headquarters:campaign.world.headquarters
    property int selectedItem:Number(campaign.world.draft("hq/selected-item","0"))
    property bool compactOverlay:false
    property bool arranging:false
    property bool placing:false
    property bool pending:false
    property bool rotated:false
    readonly property var item:(headquarters.items || []).find(row=>row.id===selectedItem) || null
    readonly property var desks:(headquarters.items || []).filter(row=>row.kind==="desk" && row.placed && row.delivered)
    property int mount:0
    property var preview:({})
    property int hoverX:-1
    property int hoverY:-1
    property int hoverTarget:0
    readonly property var traffic: arranging && headquarters.trafficEnabled ? campaign.world.hqTraffic() : ({})
    readonly property var coordinates: arranging && item && !placing && !item.deskDevice
        ? campaign.world.hqPlacement(item.id, Number(column.text)-1, Number(row.text)-1, rotated, 0) : ({})
    readonly property var deskWalk: ((placing ? preview.traffic : coordinates.traffic)?.desks || traffic.desks || [])
        .find(desk => desk.id === selectedItem) || ({})
    readonly property var changedDesks: (coordinates.traffic?.desks || []).filter(desk => {
        if(!desk.assigned || desk.id === selectedItem) return false
        const before = (traffic.desks || []).find(row => row.id === desk.id)
        return !before || before.detail !== desk.detail || before.amenities !== desk.amenities || before.mood !== desk.mood
    })
    function describeWalk(desk) {
        return (desk.detail || "")+(desk.mood<0?qsTr(" · %1 mood").arg(desk.mood):"")
            +(desk.amenities?"\n"+desk.amenities:"")
    }
    function refreshPreview() {
        preview=placing && item?campaign.world.hqPlacement(item.id,hoverX,hoverY,rotated,hoverTarget):({})
    }
    function hover(x,y,target) {
        if(hoverX===x && hoverY===y && hoverTarget===target) return
        hoverX=x;hoverY=y;hoverTarget=target
        refreshPreview()
    }
    onRotatedChanged:refreshPreview()
    onPlacingChanged:refreshPreview()
    onHeadquartersChanged:refreshPreview()
    function pickup(id) {select(id);if(item && item.delivered && editable){placing=true;hover(hoverX,hoverY,hoverTarget)}}
    signal doneRequested()
    signal shopRequested()
    signal propertiesRequested()
    signal walksRequested()
    color:ink.panel;radius:11;border.color:ink.border
    function refreshSelection() {if(item){rotated=item.rotated;column.text=String(item.x+1);row.text=String(item.y+1);mount=item.mount || (desks[0] || {}).id || 0}}
    onItemChanged: refreshSelection()
    function select(id) {selectedItem=id;placing=false;campaign.world.setDraft("hq/selected-item",String(id));refreshSelection()}
    function tile(x,y,target) {
        if(!placing || !item) {if(target) select(target);return}
        hover(x,y,target)
        if(!preview.valid) return
        place(item.id,preview.x,preview.y,rotated,preview.mount)
    }
    function drag(id,x,y,target) {
        select(id)
        if(!item) return
        if(item.deskDevice) {placing=true;tile(x,y,target)}
        else place(id,x,y,item.rotated,0)
    }
    function place(id,x,y,turn,desk) {
        if(!editable || pending) return
        pending=true
        campaign.world.placeHQEquipment(id,x,y,turn,desk,campaign.world.reviewToken)
    }
    Connections {target:campaign;function onCommandFinished(accepted,explanation){if(tools.pending){tools.pending=false;if(accepted)tools.placing=false}}}
    Connections {target:campaign.world;function onDraftsReset(){tools.selectedItem=Number(campaign.world.draft("hq/selected-item","0"));tools.placing=false}}
    implicitHeight:Math.min(content.implicitHeight+24,parent?parent.height:600)
    ScrollView {
        id:scroll;anchors.fill:parent;anchors.margins:12;contentWidth:availableWidth;clip:true
        ScrollBar.vertical:ScrollBar {
            policy:ScrollBar.AlwaysOn;minimumSize:0.1
            contentItem:Rectangle {implicitWidth:8;radius:4;color:"#91aabd"}
            background:Rectangle {implicitWidth:8;radius:4;color:ink.chrome}
        }
        ColumnLayout {
            id:content;width:scroll.availableWidth;spacing:10
            component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:tools.textSize;color:"#c9dacb"}
            Copy {text:qsTr("Arrange headquarters");font.bold:true;font.pixelSize:tools.textSize*1.2;color:"#eee4c9"}
            WorldButton {objectName:"hqDoneArranging";visible:tools.compactOverlay;text:qsTr("Done arranging");textSize:tools.textSize;onClicked:tools.doneRequested()}
            Copy {objectName:"hqTrafficSummary";visible:!!tools.traffic.summary;text:tools.traffic.summary || ""}
            Copy {visible:!!tools.traffic.crowded?.length;text:qsTr("%1 crowded floor tiles").arg(tools.traffic.crowded?.length || 0);color:"#edc088"}
            WorldButton {objectName:"hqViewWalks";visible:tools.compactOverlay && !!tools.traffic.summary;text:qsTr("View walking routes");textSize:tools.textSize;onClicked:tools.walksRequested()}
            Flow {
                Layout.fillWidth:true;spacing:6
                WorldButton {objectName:"hqOpenShop";text:qsTr("Suppliers");textSize:tools.textSize;onClicked:tools.shopRequested()}
                WorldButton {objectName:"hqOpenRealtor";text:qsTr("Realtor");textSize:tools.textSize;onClicked:tools.propertiesRequested()}
            }
            Flow {
                Layout.fillWidth:true;spacing:8
                Repeater {
                    model:tools.headquarters.items || []
                    Rectangle {
                        required property var modelData
                        objectName:"hqPick_"+modelData.id
                        width:Math.max(88,tools.textSize*6);height:Math.max(112,tools.textSize*8)
                        radius:10;color:tools.selectedItem===modelData.id?"#384c5c":ink.control
                        border.color:tools.selectedItem===modelData.id?ink.accent:ink.border
                        opacity:modelData.delivered?1:0.5
                        EquipmentGlyph {anchors.top:parent.top;anchors.horizontalCenter:parent.horizontalCenter;width:parent.width-20;height:parent.height*0.57;kind:modelData.kind;powered:modelData.powered}
                        Label {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:6;height:parent.height*0.35;text:modelData.name;font.pixelSize:tools.textSize*0.8;color:"#e7e5cf";wrapMode:Text.Wrap;elide:Text.ElideRight;textFormat:Text.PlainText}
                        activeFocusOnTab:true
                        Accessible.role:Accessible.Button;Accessible.name:qsTr("Pick up %1 · %2").arg(modelData.name).arg(modelData.detail)
                        Accessible.onPressAction:tools.pickup(modelData.id)
                        Keys.onReturnPressed:tools.pickup(modelData.id)
                        Keys.onSpacePressed:tools.pickup(modelData.id)
                        MouseArea {anchors.fill:parent;cursorShape:Qt.OpenHandCursor;onClicked:tools.pickup(modelData.id)}
                    }
                }
            }
            GameSelect {
                objectName:"hqOwnedItemPicker";Layout.fillWidth:true;textSize:tools.textSize
                model:[{id:0,name:qsTr("Choose owned equipment")}].concat(tools.headquarters.items || [])
                textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(entry=>entry.id===tools.selectedItem))
                Accessible.name:qsTr("Owned equipment to arrange")
                onActivated:tools.select(currentValue)
            }
            Copy {visible:!tools.item;text:qsTr("Select equipment from this list or click it in the room. Unplaced deliveries are kept in storage.")}
            ColumnLayout {
                visible:!!tools.item;Layout.fillWidth:true;spacing:9
                Copy {text:tools.item?tools.item.name+"\n"+tools.item.detail+"\n"+tools.item.ownership:""}
                Copy {visible:tools.item && tools.item.compute>0;text:tools.item?qsTr("%1 CU/day · %2 W when powered. Output requires a delivered, placed and powered machine.").arg(tools.item.compute).arg(tools.item.watts):""}
                WorldButton {objectName:"hqStartPlacement";text:tools.placing?qsTr("Cancel placement"):tools.item && tools.item.placed?qsTr("Choose a new position"):qsTr("Place in the room");textSize:tools.textSize;enabled:tools.editable && !!tools.item && tools.item.delivered;onClicked:tools.placing=!tools.placing}
                Copy {visible:tools.placing;text:tools.item && tools.item.deskDevice?qsTr("Click a free desk to mount this computer."):qsTr("Move the translucent object over the floor. Green fits; red needs another position. R rotates; right-click or Escape cancels.");color:"#f1cf8e"}
                GameSelect {
                    visible:tools.item && tools.item.deskDevice;Layout.fillWidth:true;textSize:tools.textSize
                    model:tools.desks.map(desk=>({id:desk.id,name:qsTr("Desk %1 · %2").arg(desk.id).arg(desk.personName || qsTr("unassigned"))}))
                    textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(desk=>desk.id===tools.mount))
                    Accessible.name:qsTr("Desk for this computer");onActivated:tools.mount=currentValue
                }
                RowLayout {
                    visible:tools.item && !tools.item.deskDevice;Layout.fillWidth:true
                    GameField {id:column;objectName:"hqItemColumn";Layout.fillWidth:true;text:"1";textSize:tools.textSize;maximumLength:2;Accessible.name:qsTr("Floor column, starting at one")}
                    GameField {id:row;objectName:"hqItemRow";Layout.fillWidth:true;text:"1";textSize:tools.textSize;maximumLength:2;Accessible.name:qsTr("Floor row, starting at one")}
                }
                WorldButton {visible:tools.item && !tools.item.deskDevice;text:tools.rotated?qsTr("Orientation · rotated"):qsTr("Orientation · normal");textSize:tools.textSize;enabled:tools.editable;onClicked:tools.rotated=!tools.rotated}
                Copy {
                    visible:!!tools.coordinates.traffic?.summary
                    text:qsTr("After placement · %1").arg(tools.coordinates.traffic?.summary || "")
                }
                Copy {
                    objectName:"hqDeskWalk"
                    visible:!!tools.deskWalk.detail
                    text:tools.describeWalk(tools.deskWalk)
                }
                WorldButton {objectName:"hqPlaceAtCoordinates";text:tools.item && tools.item.deskDevice?qsTr("Mount on selected desk"):qsTr("Place at column / row");textSize:tools.textSize;enabled:tools.editable && !!tools.item && tools.item.delivered;onClicked:tools.place(tools.item.id,Number(column.text)-1,Number(row.text)-1,tools.rotated,tools.item.deskDevice?tools.mount:0)}
                GameSelect {
                    objectName:"hqDeskPerson";visible:tools.item && tools.item.kind==="desk" && tools.item.placed
                    Layout.fillWidth:true;textSize:tools.textSize;model:campaign.world.hqEmployees;textRole:"name";valueRole:"id"
                    currentIndex:Math.max(0,model.findIndex(person=>tools.item && person.id===tools.item.person))
                    Accessible.name:qsTr("Person assigned to this desk");onActivated:campaign.world.assignHQDesk(tools.item.id,currentValue,campaign.world.reviewToken)
                }
                GameSelect {
                    objectName:"hqLaptopPerson";visible:tools.item && tools.item.portable
                    Layout.fillWidth:true;textSize:tools.textSize;model:campaign.world.hqEmployees;textRole:"name";valueRole:"id"
                    currentIndex:Math.max(0,model.findIndex(person=>tools.item && person.id===tools.item.person))
                    Accessible.name:qsTr("Issue laptop to employee for desk or home working")
                    onActivated:campaign.world.assignHQLaptop(tools.item.id,currentValue,campaign.world.reviewToken)
                }
                WorldButton {objectName:"hqEquipmentPower";visible:tools.item && tools.item.watts>0;text:tools.item && tools.item.powered?qsTr("Turn power off"):qsTr("Turn power on");textSize:tools.textSize;enabled:tools.editable;onClicked:campaign.world.powerHQEquipment(tools.item.id,!tools.item.powered,campaign.world.reviewToken)}
                WorldButton {objectName:"hqStoreItem";visible:tools.item && tools.item.placed;text:qsTr("Move to storage");textSize:tools.textSize;enabled:tools.editable;onClicked:campaign.world.storeHQEquipment(tools.item.id,campaign.world.reviewToken)}
            }
            Repeater {
                model:tools.changedDesks
                Copy {
                    required property var modelData
                    readonly property var desk:tools.desks.find(row => row.id === modelData.id) || ({})
                    text:(desk.personName || qsTr("Desk %1").arg(modelData.id))+" · "+tools.describeWalk(modelData)
                }
            }
            Repeater {model:(tools.headquarters.rooms || []).filter(room=>!room.passage);Copy {required property var modelData;text:modelData.name+"\n"+modelData.detail}}
        }
    }
}
