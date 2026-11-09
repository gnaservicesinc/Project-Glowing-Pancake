import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tools
    required property int textSize
    required property bool editable
    property var headquarters:campaign.world.headquarters
    property int selectedItem:Number(campaign.world.draft("hq/selected-item","0"))
    property bool compactOverlay:false
    property bool placing:false
    property bool pending:false
    property bool rotated:false
    readonly property var item:(headquarters.items || []).find(row=>row.id===selectedItem) || null
    readonly property var desks:(headquarters.items || []).filter(row=>row.kind==="desk" && row.placed && row.delivered)
    property int mount:0
    signal doneRequested()
    signal shopRequested()
    signal propertiesRequested()
    color:"#173338";radius:11;border.color:"#658475"
    function refreshSelection() {if(item){rotated=item.rotated;column.text=String(item.x+1);row.text=String(item.y+1);mount=item.mount || (desks[0] || {}).id || 0}}
    onItemChanged: refreshSelection()
    function select(id) {selectedItem=id;placing=false;campaign.world.setDraft("hq/selected-item",String(id));refreshSelection()}
    function tile(x,y,target) {
        if(!placing || !item) {if(target) select(target);return}
        let desk=0
        if(item.kind==="desktop") {
            const hit=(headquarters.items || []).find(entry=>entry.id===target)
            desk=hit?(hit.kind==="desk"?hit.id:hit.mount || 0):0
        }
        place(item.id,x,y,rotated,desk)
    }
    function drag(id,x,y,target) {
        select(id)
        if(!item) return
        if(item.kind==="desktop") {placing=true;tile(x,y,target)}
        else place(id,x,y,item.rotated,0)
    }
    function place(id,x,y,turn,desk) {
        if(!editable) return
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
            contentItem:Rectangle {implicitWidth:8;radius:4;color:"#92bca6"}
            background:Rectangle {implicitWidth:8;radius:4;color:"#10292d"}
        }
        ColumnLayout {
            id:content;width:scroll.availableWidth;spacing:10
            component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:tools.textSize;color:"#c9dacb"}
            Copy {text:qsTr("Arrange headquarters");font.bold:true;font.pixelSize:tools.textSize*1.2;color:"#eee4c9"}
            WorldButton {objectName:"hqDoneArranging";visible:tools.compactOverlay;text:qsTr("Done arranging");textSize:tools.textSize;onClicked:tools.doneRequested()}
            Copy {text:tools.headquarters.kind==="home"?qsTr("A small garage, with room for one desk. Nothing is furnished until you buy and place it."):qsTr("Arrange your actual furnishings. Drag placed items, or use the coordinates below. Rooms take their purpose from their equipment.")}
            Flow {
                Layout.fillWidth:true;spacing:6
                WorldButton {objectName:"hqOpenShop";text:qsTr("Suppliers");textSize:tools.textSize;onClicked:tools.shopRequested()}
                WorldButton {objectName:"hqOpenRealtor";text:qsTr("Realtor");textSize:tools.textSize;onClicked:tools.propertiesRequested()}
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
                Copy {visible:tools.placing;text:tools.item && tools.item.kind==="desktop"?qsTr("Click a free desk to mount this computer."):qsTr("Click a floor cell for the top-left corner. Items cannot overlap or cross a wall.");color:"#f1cf8e"}
                GameSelect {
                    visible:tools.item && tools.item.kind==="desktop";Layout.fillWidth:true;textSize:tools.textSize
                    model:tools.desks.map(desk=>({id:desk.id,name:qsTr("Desk %1 · %2").arg(desk.id).arg(desk.personName || qsTr("unassigned"))}))
                    textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(desk=>desk.id===tools.mount))
                    Accessible.name:qsTr("Desk for this computer");onActivated:tools.mount=currentValue
                }
                RowLayout {
                    visible:tools.item && tools.item.kind!=="desktop";Layout.fillWidth:true
                    GameField {id:column;objectName:"hqItemColumn";Layout.fillWidth:true;text:"1";textSize:tools.textSize;maximumLength:2;Accessible.name:qsTr("Floor column, starting at one")}
                    GameField {id:row;objectName:"hqItemRow";Layout.fillWidth:true;text:"1";textSize:tools.textSize;maximumLength:2;Accessible.name:qsTr("Floor row, starting at one")}
                }
                WorldButton {visible:tools.item && tools.item.kind!=="desktop";text:tools.rotated?qsTr("Orientation · rotated"):qsTr("Orientation · normal");textSize:tools.textSize;enabled:tools.editable;onClicked:tools.rotated=!tools.rotated}
                WorldButton {objectName:"hqPlaceAtCoordinates";text:tools.item && tools.item.kind==="desktop"?qsTr("Mount on selected desk"):qsTr("Place at column / row");textSize:tools.textSize;enabled:tools.editable && !!tools.item && tools.item.delivered;onClicked:tools.place(tools.item.id,Number(column.text)-1,Number(row.text)-1,tools.rotated,tools.item.kind==="desktop"?tools.mount:0)}
                GameSelect {
                    objectName:"hqDeskPerson";visible:tools.item && tools.item.kind==="desk" && tools.item.placed
                    Layout.fillWidth:true;textSize:tools.textSize;model:campaign.world.hqEmployees;textRole:"name";valueRole:"id"
                    currentIndex:Math.max(0,model.findIndex(person=>tools.item && person.id===tools.item.person))
                    Accessible.name:qsTr("Person assigned to this desk");onActivated:campaign.world.assignHQDesk(tools.item.id,currentValue,campaign.world.reviewToken)
                }
                WorldButton {objectName:"hqEquipmentPower";visible:tools.item && tools.item.watts>0;text:tools.item && tools.item.powered?qsTr("Turn power off"):qsTr("Turn power on");textSize:tools.textSize;enabled:tools.editable;onClicked:campaign.world.powerHQEquipment(tools.item.id,!tools.item.powered,campaign.world.reviewToken)}
                WorldButton {objectName:"hqStoreItem";visible:tools.item && tools.item.placed;text:qsTr("Move to storage");textSize:tools.textSize;enabled:tools.editable;onClicked:campaign.world.storeHQEquipment(tools.item.id,campaign.world.reviewToken)}
            }
            Repeater {model:tools.headquarters.rooms || [];Copy {required property var modelData;text:modelData.name+"\n"+modelData.detail}}
        }
    }
}
