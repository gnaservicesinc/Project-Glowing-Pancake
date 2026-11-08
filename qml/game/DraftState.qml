import QtQuick

QtObject {
    id: state
    required property string draftKey
    property var accepts: value => true
    property bool ready: false
    signal restored(var value)
    signal reset()
    function restoreDraft() {
        if (!ready || !draftKey.length || !campaign.currentCampaign) return
        const raw = campaign.world.draft("form/" + draftKey, "")
        if (!raw.length) return
        try {
            const value = JSON.parse(raw)
            if (accepts(value)) restored(value)
        } catch (error) { }
    }
    function remember(value) {
        if (ready && draftKey.length && accepts(value))
            campaign.world.setDraft("form/" + draftKey, JSON.stringify(value))
    }
    Component.onCompleted: { ready = true; Qt.callLater(restoreDraft) }
    onDraftKeyChanged: if (ready) Qt.callLater(restoreDraft)
    property Connections savedCompany: Connections {
        target: campaign.world
        function onDraftsReset() { state.reset(); Qt.callLater(state.restoreDraft) }
    }
}
