import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    readonly property bool editable:campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    visible:campaign.world.overseasOfficeOffers.length>0
    spacing:12
    Label {Layout.fillWidth:true;text:qsTr("Your company's reach");font.pixelSize:panel.textSize*1.35;font.bold:true;wrapMode:Text.Wrap}
    Label {Layout.fillWidth:true;text:campaign.world.accessSummary;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
    Repeater {
        model:campaign.world.overseasOffices
        Frame {
            property bool focusContext:true
            required property var modelData
            Layout.fillWidth:true
            contentItem:ColumnLayout {
                Label {Layout.fillWidth:true;text:modelData.name;font.bold:true;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
                Label {Layout.fillWidth:true;text:modelData.detail;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
                WorldButton {objectName:"closeOffice_"+modelData.id;Accessible.name:qsTr("Close %1").arg(modelData.name);text:qsTr("Close this office…");textSize:panel.textSize;enabled:modelData.canClose && panel.editable;onClicked:panel.review(modelData,true)}
            }
        }
    }
    DraftCheckBox {id:planning;objectName:"overseasOfficePlanning";text:qsTr("Plan an overseas liaison office");font.pixelSize:panel.textSize}
    Repeater {
        model:planning.checked?campaign.world.overseasOfficeOffers:[]
        Frame {
            property bool focusContext:true
            required property var modelData
            Layout.fillWidth:true
            contentItem:ColumnLayout {
                Label {Layout.fillWidth:true;text:modelData.name;font.bold:true;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
                Label {Layout.fillWidth:true;text:modelData.detail;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
                WorldButton {objectName:"reviewOffice_"+modelData.id;text:qsTr("Review office commitment…");textSize:panel.textSize;enabled:panel.editable;onClicked:panel.review(modelData,false)}
            }
        }
    }
    ReviewPause {id:reviewPause}
    function review(row,close) {reviewPause.begin(()=>{
        const rows=close?campaign.world.overseasOffices:campaign.world.overseasOfficeOffers
        const current=rows.find(value=>value.id===row.id)
        if(!current) return
        quote.reviewed=current;quote.closing=close;quote.open()
    })}
    Dialog {
        id:quote;objectName:"overseasOfficeQuote"
        property var reviewed:({})
        property bool closing:false
        title:reviewed.name || qsTr("Overseas office")
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*40,Overlay.overlay.width-32);height:Math.min(panel.textSize*24,Overlay.overlay.height-32)
        modal:true;focus:true;standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:ScrollView {id:quoteScroll;contentWidth:availableWidth;clip:true;Label {width:quoteScroll.availableWidth;text:quote.closing?qsTr("Close the office and stop future payroll and rent. Setup costs are not refunded. Contacts already established remain available."):quote.reviewed.detail || "";font.pixelSize:panel.textSize;wrapMode:Text.Wrap}}
        onAccepted:if(closing)campaign.world.closeOverseasOffice(reviewed.id,reviewed.revision);else campaign.world.openOverseasOffice(reviewed.id,reviewed.revision)
    }
}
