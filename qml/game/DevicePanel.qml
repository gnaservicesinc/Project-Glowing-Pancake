import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    property int textSize:14
    readonly property bool editable:campaign.companyEditable && !campaign.storageBlocked && !campaign.busy
    spacing:16
    Label {text:qsTr("Your equipment");font.pixelSize:panel.textSize*1.5;font.bold:true}
    Label {Layout.fillWidth:true;text:campaign.world.deviceSummary;wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
    Repeater {
        model:campaign.world.deviceInventory
        Frame {
            required property var modelData
            Layout.fillWidth:true
            contentItem:ColumnLayout {
                Label {Layout.fillWidth:true;text:modelData.name+" #"+modelData.id;font.pixelSize:panel.textSize*1.15;font.bold:true;wrapMode:Text.Wrap}
                Label {Layout.fillWidth:true;text:modelData.detail;wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
                Label {Layout.fillWidth:true;text:qsTr("Applications: %1").arg(modelData.apps);wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
                Flow {
                    Layout.fillWidth:true;spacing:8
                    WorldButton {objectName:"devicePower"+modelData.id;text:modelData.powered?qsTr("Power off"):qsTr("Power on");textSize:panel.textSize;enabled:modelData.canPower && !campaign.storageBlocked && !campaign.busy;onClicked:campaign.world.powerDevice(modelData.id,!modelData.powered)}
                    WorldButton {text:qsTr("Service…");textSize:panel.textSize;enabled:modelData.canService && panel.editable;onClicked:panel.review("service",modelData,modelData.serviceDetail)}
                    WorldButton {text:qsTr("Retire…");textSize:panel.textSize;enabled:modelData.canRetire && panel.editable;onClicked:panel.review("retire",modelData,qsTr("Retire this device permanently, without a refund. Keep another working portable device available for company decisions."))}
                }
            }
        }
    }
    Label {text:qsTr("Add a device");font.pixelSize:panel.textSize*1.3;font.bold:true;visible:campaign.world.deviceOffers.length>0}
    Repeater {
        model:campaign.world.deviceOffers
        Frame {
            required property var modelData
            Layout.fillWidth:true
            contentItem:ColumnLayout {
                Label {Layout.fillWidth:true;text:modelData.name;font.pixelSize:panel.textSize*1.15;font.bold:true;wrapMode:Text.Wrap}
                Label {Layout.fillWidth:true;text:modelData.detail;wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
                WorldButton {objectName:"buyDevice"+modelData.id;text:qsTr("Review purchase…");textSize:panel.textSize;enabled:panel.editable;onClicked:panel.review("buy",modelData,modelData.detail)}
            }
        }
    }
    function review(action,row,detail) {if(!campaign.paused)campaign.togglePaused();confirmation.action=action;confirmation.reviewed=row;confirmation.detail=detail;confirmation.open()}
    Dialog {
        id:confirmation
        objectName:"deviceQuote"
        property string action:""
        property var reviewed:({})
        property string detail:""
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*38,Overlay.overlay.width-32);modal:true;focus:true
        title:reviewed.name || qsTr("Equipment")
        standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label {text:confirmation.detail;wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
        onAccepted:{if(action==="buy")campaign.world.buyDevice(reviewed.id,reviewed.revision);else if(action==="service")campaign.world.serviceDevice(reviewed.id,reviewed.revision);else campaign.world.retireDevice(reviewed.id,reviewed.revision)}
    }
}
