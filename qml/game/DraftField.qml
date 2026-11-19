import QtQuick
import QtQuick.Controls

GameField {
    id:field
    property string draftScope:""
    property string defaultText:""
    text:defaultText
    readonly property string draftKey:objectName.length?"field/"+objectName+"/"+draftScope:""
    property bool draftReady:false
    readonly property var workspace: typeof deviceSession === "undefined" ? null : deviceSession
    function readValue(key, fallback) {
        if (!workspace) return campaign.world.draft(key, fallback)
        return campaign.world.deviceAppState(workspace.deviceId, workspace.application)[key] ?? fallback
    }
    function writeValue(key, value) {
        if (workspace) campaign.world.setDeviceAppState(workspace.deviceId, workspace.application, key, value)
        else campaign.world.setDraft(key, value)
    }
    function setDraftText(value) {
        text=value
        if(draftReady && draftKey.length) writeValue(draftKey,text)
    }
    function resetDraft() {text=Qt.binding(()=>defaultText);restoreTimer.restart()}
    function restoreDraft() {
        if(draftKey.length && campaign.currentCampaign) {
            let value=readValue(draftKey,text)
            if(value!==text) text=value
        }
    }
    Timer { id: restoreTimer; interval: 0; onTriggered: field.restoreDraft() }
    Component.onCompleted:{draftReady=true;restoreTimer.restart()}
    onDraftKeyChanged:if(draftReady) restoreTimer.restart()
    Connections {
        target:field
        function onTextEdited(){field.setDraftText(field.text)}
    }
    Connections {target:campaign.world;function onDraftsReset(){field.resetDraft()}}
}
