import QtQuick
import QtQuick.Controls

TextField {
    id:field
    property string draftScope:""
    readonly property string draftKey:objectName.length?"field/"+objectName+"/"+draftScope:""
    property bool draftReady:false
    function restoreDraft() {
        if(draftKey.length && campaign.currentCampaign) {
            let value=campaign.world.draft(draftKey,text)
            if(value!==text) text=value
        }
    }
    Component.onCompleted:{draftReady=true;Qt.callLater(restoreDraft)}
    onDraftKeyChanged:if(draftReady) Qt.callLater(restoreDraft)
    Connections {
        target:field
        function onTextEdited(){if(field.draftReady && field.draftKey.length) campaign.world.setDraft(field.draftKey,field.text)}
    }
    Connections {target:campaign.world;function onDraftsReset(){Qt.callLater(field.restoreDraft)}}
}
