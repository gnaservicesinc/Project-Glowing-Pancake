import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id:panel
    property int textSize:14
    readonly property bool editable:campaign.companyEditable && !campaign.storageBlocked && !campaign.busy
    signal visitRoom(string place)
    signal openInfrastructure()
    ReviewPause {id:reviewPause}
    function reviewRoom(id) {reviewPause.begin(()=>{const current=campaign.world.roomOffers.find(row=>row.id===id);if(current){roomDialog.review={id:current.id,detail:current.name+"\n"+current.detail,revision:campaign.world.reviewToken};roomDialog.open()}})}
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
    FacilityBlueprints {Layout.fillWidth:true;textSize:panel.textSize}
    AccessPanel {Layout.fillWidth:true;textSize:panel.textSize}
    WorldButton {text:qsTr("Manage facilities, inspections & travel");textSize:panel.textSize;onClicked:panel.openInfrastructure()}
    GameDialog {
        id:roomDialog;objectName:"headquartersRoomDialog";property var review:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*42,Overlay.overlay.width-32);height:Math.min(panel.textSize*31,Overlay.overlay.height-32)
        modal:true;focus:true;title:qsTr("Authorize room construction?");standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ScrollView {id:roomReviewScroll;contentWidth:availableWidth;clip:true;Label {width:roomReviewScroll.availableWidth;text:roomDialog.review.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}}
        onAccepted:campaign.world.orderHeadquartersRoom(review.id,review.revision)
    }
}
