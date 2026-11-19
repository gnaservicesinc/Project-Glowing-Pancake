import QtQuick

QtObject {
    id: state
    required property string draftKey
    property var accepts: value => true
    property bool ready: false
    signal restored(var value)
    signal reset()
    readonly property var workspace: typeof deviceSession === "undefined" ? null : deviceSession
    function readValue(key, fallback) {
        if (!workspace) return campaign.world.draft(key, fallback)
        return campaign.world.deviceAppState(workspace.deviceId, workspace.application)[key] ?? fallback
    }
    function writeValue(key, value) {
        if (workspace) campaign.world.setDeviceAppState(workspace.deviceId, workspace.application, key, value)
        else campaign.world.setDraft(key, value)
    }
    function restoreDraft() {
        if (!ready || !draftKey.length || !campaign.currentCampaign) return
        const raw = readValue("form/" + draftKey, "")
        if (!raw.length) return
        try {
            const value = JSON.parse(raw)
            if (accepts(value)) restored(value)
        } catch (error) { }
    }
    function remember(value) {
        if (ready && draftKey.length && accepts(value))
            writeValue("form/" + draftKey, JSON.stringify(value))
    }
    function scheduleRestore() { if (ready) restoreTimer.restart() }
    property Timer restoreTimer: Timer { interval: 0; onTriggered: state.restoreDraft() }
    Component.onCompleted: { ready = true; restoreTimer.restart() }
    onDraftKeyChanged: if (ready) restoreTimer.restart()
    property Connections savedCompany: Connections {
        target: campaign.world
        function onDraftsReset() { state.reset(); state.restoreTimer.restart() }
    }
}
