import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id:panel
    property int textSize:14
    property string hardwareId:"rack.reference"
    property string blueprintId:""
    property string buildState:campaign.infrastructure.states.selectedId
    readonly property bool editable:campaign.companyEditable && !campaign.storageBlocked && !campaign.busy
    signal visitRoom(string place)
    signal openInfrastructure()
    ReviewPause {id:reviewPause}
    function reviewRoom(id) {reviewPause.begin(()=>{const current=campaign.world.roomOffers.find(row=>row.id===id);if(current){roomDialog.review={id:current.id,detail:current.name+"\n"+current.detail,revision:campaign.world.reviewToken};roomDialog.open()}})}
    function reviewBuild() {reviewPause.begin(()=>{buildDialog.review=campaign.world.blueprintQuote(panel.blueprintId || campaign.world.blueprints[0].id,panel.buildState);buildDialog.open()})}
    function restoreForm() {
        hardwareId=campaign.world.draft("workplace/hardware","rack.reference")
        blueprintId=campaign.world.draft("workplace/blueprint","")
        buildState=campaign.world.draft("workplace/state",campaign.infrastructure.states.selectedId)
        cooling.checked=campaign.world.draft("workplace/cooling","air")==="evaporative"
    }
    Component.onCompleted:Qt.callLater(restoreForm)
    Connections {target:campaign.world;function onDraftsReset(){Qt.callLater(panel.restoreForm)}}
    spacing:16
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#c6d8cd"}
    component Heading:Copy {font.pixelSize:panel.textSize*1.35;font.bold:true;color:"#eee5d2"}
    Heading {text:qsTr("Make room for the company")}
    Copy {text:campaign.world.headquartersSummary}
    Repeater {
        model:campaign.world.headquartersRooms
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            Copy {text:modelData.name+" · "+modelData.detail}
            WorldButton {visible:modelData.ready;text:qsTr("Visit %1").arg(modelData.name);textSize:panel.textSize;onClicked:panel.visitRoom(modelData.place)}
        }
    }
    Heading {visible:campaign.world.roomOffers.length>0;text:qsTr("Add a headquarters room")}
    Repeater {
        model:campaign.world.roomOffers
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:offer.implicitHeight+28;color:"#213b40";radius:10
            ColumnLayout {
                id:offer;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:8
                Heading {text:modelData.name}
                Copy {text:modelData.detail}
                WorldButton {objectName:"reviewRoom_"+modelData.id;text:qsTr("Review construction");textSize:panel.textSize;enabled:panel.editable;onClicked:panel.reviewRoom(modelData.id)}
            }
        }
    }
    Heading {visible:campaign.world.roomOffers.length>0;text:qsTr("A repeatable branch blueprint")}
    Copy {visible:campaign.world.roomOffers.length>0;text:qsTr("Choose a hardware bundle, its grid reservation and dedicated operators. Save the plan, then review a price for each state. The plan supplies no free resources. Each build follows construction, delivery, power, inspection and payroll.")}
    ColumnLayout {
        visible:campaign.world.roomOffers.length>0;Layout.fillWidth:true;spacing:8
        Copy {text:qsTr("Blueprint name")}
        DraftField {id:blueprintName;objectName:"blueprintName";Layout.fillWidth:true;font.pixelSize:panel.textSize;maximumLength:72;text:qsTr("Compact research branch");Accessible.name:qsTr("Blueprint name")}
        Copy {text:qsTr("Hardware bundle")}
        ComboBox {
            id:hardware;objectName:"blueprintHardware";Layout.fillWidth:true;font.pixelSize:panel.textSize
            model:campaign.world.blueprintHardware;textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(row=>row.id===panel.hardwareId))
            Accessible.name:qsTr("Blueprint hardware")
            onActivated:{panel.hardwareId=currentValue;campaign.world.setDraft("workplace/hardware",currentValue)}
        }
        Copy {text:(campaign.world.blueprintHardware.find(row=>row.id===panel.hardwareId) || {}).detail || ""}
        Copy {text:qsTr("Grid reservation · whole kW")}
        DraftField {id:grid;objectName:"blueprintGridKw";Layout.fillWidth:true;font.pixelSize:panel.textSize;maximumLength:4;text:"20";Accessible.name:qsTr("Blueprint grid reservation in whole kW")}
        Copy {text:qsTr("Dedicated operators")}
        DraftField {id:operators;objectName:"blueprintOperators";Layout.fillWidth:true;font.pixelSize:panel.textSize;maximumLength:2;text:"1";Accessible.name:qsTr("Blueprint operator headcount")}
        CheckBox {id:cooling;objectName:"blueprintWaterCooling";text:qsTr("Evaporative cooling · uses water");font.pixelSize:panel.textSize;onToggled:campaign.world.setDraft("workplace/cooling",checked?"evaporative":"air")}
        WorldButton {objectName:"saveFacilityBlueprint";text:qsTr("Save this blueprint");textSize:panel.textSize;enabled:panel.editable;onClicked:campaign.world.createBlueprint(blueprintName.text,panel.hardwareId,grid.text,operators.text,cooling.checked)}
    }
    Heading {visible:campaign.world.blueprints.length>0;text:qsTr("Build from a saved plan")}
    ColumnLayout {
        visible:campaign.world.blueprints.length>0;Layout.fillWidth:true
        ComboBox {
            objectName:"savedBlueprint";Layout.fillWidth:true;font.pixelSize:panel.textSize
            model:campaign.world.blueprints;textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(row=>row.id===panel.blueprintId))
            Accessible.name:qsTr("Saved blueprint")
            onActivated:{panel.blueprintId=currentValue;campaign.world.setDraft("workplace/blueprint",currentValue)}
        }
        Copy {text:(campaign.world.blueprints.find(row=>row.id===panel.blueprintId) || campaign.world.blueprints[0] || {}).detail || ""}
        ComboBox {
            objectName:"blueprintBuildState";Layout.fillWidth:true;font.pixelSize:panel.textSize
            model:campaign.infrastructure.states;textRole:"title";valueRole:"entityId";currentIndex:model.indexOf(panel.buildState)
            Accessible.name:qsTr("State for blueprint construction")
            onActivated:{panel.buildState=currentValue;campaign.world.setDraft("workplace/state",currentValue)}
        }
        WorldButton {
            objectName:"reviewBlueprintBuild";text:qsTr("Review the complete state quote");textSize:panel.textSize;enabled:panel.editable
            onClicked:panel.reviewBuild()
        }
    }
    AccessPanel {Layout.fillWidth:true;textSize:panel.textSize}
    WorldButton {text:qsTr("Manage facilities, inspections & travel");textSize:panel.textSize;onClicked:panel.openInfrastructure()}
    Dialog {
        id:roomDialog;objectName:"headquartersRoomDialog";property var review:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*42,Overlay.overlay.width-32);height:Math.min(panel.textSize*31,Overlay.overlay.height-32)
        modal:true;focus:true;title:qsTr("Authorize room construction?");standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ScrollView {id:roomReviewScroll;contentWidth:availableWidth;clip:true;Label {width:roomReviewScroll.availableWidth;text:roomDialog.review.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}}
        onAccepted:campaign.world.orderHeadquartersRoom(review.id,review.revision)
    }
    Dialog {
        id:buildDialog;objectName:"blueprintBuildDialog";property var review:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*48,Overlay.overlay.width-32);height:Math.min(panel.textSize*43,Overlay.overlay.height-32)
        modal:true;focus:true;title:review.error?qsTr("This plan needs a change"):qsTr("Authorize the complete facility?")
        standardButtons:review.error?Dialog.Close:Dialog.Ok|Dialog.Cancel
        contentItem:ScrollView {id:buildReviewScroll;contentWidth:availableWidth;clip:true;Label {width:buildReviewScroll.availableWidth;text:buildDialog.review.error || buildDialog.review.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}}
        onAccepted:campaign.world.buildBlueprint(review)
    }
}
