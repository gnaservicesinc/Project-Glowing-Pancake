import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    property bool focusContext:true
    property string chosenPerson:""
    property string chosenSource:""
    readonly property var department:campaign.world.operationsManagement
    readonly property var candidates:campaign.world.managerCandidates
    readonly property string personId:candidates.some(row=>row.id===chosenPerson)?chosenPerson:candidates.some(row=>row.id===department.person)?department.person:(candidates[0] || {}).id || ""
    readonly property string sourceId:campaign.world.managementSources.some(row=>row.id===chosenSource)?chosenSource:department.source || (campaign.world.managementSources[0] || {}).id || ""
    readonly property var selectedPerson:candidates.find(row=>row.id===personId) || ({})
    readonly property bool editable:campaign.companyEditable && !campaign.busy && !campaign.storageBlocked && campaign.requiredKind==="none"
    signal profileRequested(var person)
    visible:Object.keys(department).length>0
    spacing:12
    function restore() {chosenPerson=campaign.world.draft("departments/person","");chosenSource=campaign.world.draft("departments/source","")}
    Component.onCompleted:restore()
    Connections {target:campaign.world;function onDraftsReset(){panel.restore()}}
    Label {Layout.fillWidth:true;text:qsTr("HQ operations management");font.pixelSize:panel.textSize*1.4;font.bold:true;wrapMode:Text.Wrap}
    Label {objectName:"managerStatus";textFormat:Text.PlainText;Layout.fillWidth:true;text:(panel.department.name || "")+qsTr(" · Branch availability %1").arg(panel.department.availability || "");font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
    Label {Layout.fillWidth:true;text:panel.department.detail || "";textFormat:Text.PlainText;font.pixelSize:panel.textSize;wrapMode:Text.Wrap}
    GameSelect {objectName:"operationsManagerPicker";Layout.fillWidth:true;model:panel.candidates;textRole:"label";valueRole:"id";currentIndex:Math.max(0,model.findIndex(row=>row.id===panel.personId));font.pixelSize:panel.textSize;Accessible.name:qsTr("HQ operations manager");onActivated:{panel.chosenPerson=currentValue;campaign.world.setDraft("departments/person",currentValue)}}
    EmployeePortrait {objectName:"managerPortrait";visible:panel.personId.length>0;Layout.preferredWidth:panel.textSize*6;Layout.preferredHeight:Layout.preferredWidth;portraitSource:panel.selectedPerson.portrait || "";personName:panel.selectedPerson.name || "";reducedMotion:campaign.reducedMotion;onClicked:panel.profileRequested(panel.selectedPerson)}
    GameSelect {objectName:"operationsSourcePicker";Layout.fillWidth:true;model:campaign.world.managementSources;textRole:"name";valueRole:"id";currentIndex:Math.max(0,model.findIndex(row=>row.id===panel.sourceId));font.pixelSize:panel.textSize;Accessible.name:qsTr("Compute for management work");onActivated:{panel.chosenSource=currentValue;campaign.world.setDraft("departments/source",currentValue)}}
    Flow {Layout.fillWidth:true;spacing:8
        WorldButton {objectName:"reviewOperationsManager";text:qsTr("Review manager assignment…");textSize:panel.textSize;enabled:panel.editable && panel.personId.length>0 && panel.sourceId.length>0;onClicked:panel.review(false)}
        WorldButton {objectName:"releaseOperationsManager";text:qsTr("Release manager to other work…");textSize:panel.textSize;enabled:panel.editable && !!panel.department.person;onClicked:panel.review(true)}
    }
    ReviewPause {id:reviewPause}
    function review(release) {reviewPause.begin(()=>{quote.reviewed=campaign.world.managerQuote(release?"":panel.personId,release?"":panel.sourceId);quote.open()})}
    GameDialog {
        id:quote;objectName:"operationsManagerQuote";property var reviewed:({})
        title:reviewed.error?qsTr("This assignment needs a change"):qsTr("HQ operations assignment")
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*43,Overlay.overlay.width-32);height:Math.min(panel.textSize*34,Overlay.overlay.height-32);modal:true;focus:true
        standardButtons:reviewed.error?Dialog.Close:Dialog.Ok|Dialog.Cancel
        contentItem:ScrollView {id:quoteScroll;contentWidth:availableWidth;clip:true;Label {width:quoteScroll.availableWidth;text:quote.reviewed.error || quote.reviewed.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}}
        onAccepted:campaign.world.assignOperationsManager(reviewed)
    }
}
