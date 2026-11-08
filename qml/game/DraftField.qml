import QtQuick
import QtQuick.Controls

TextField {
    id:field
    property string draftScope:""
    property string defaultText:""
    text:defaultText
    readonly property string draftKey:objectName.length?"field/"+objectName+"/"+draftScope:""
    property bool draftReady:false
    function setDraftText(value) {
        text=value
        if(draftReady && draftKey.length) campaign.world.setDraft(draftKey,text)
    }
    function resetDraft() {text=Qt.binding(()=>defaultText);Qt.callLater(restoreDraft)}
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
        function onTextEdited(){field.setDraftText(field.text)}
    }
    Connections {target:campaign.world;function onDraftsReset(){field.resetDraft()}}
}
