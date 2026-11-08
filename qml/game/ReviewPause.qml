import QtQuick
Item {
    id:reviewPause
    visible:false
    property var pending:null
    function begin(callback) {
        if(campaign.paused && !campaign.busy) callback()
        else {pending=callback;if(!campaign.paused && !campaign.busy)campaign.togglePaused()}
    }
    Connections {
        target:campaign
        function onStateChanged() {
            if(!reviewPause.pending || campaign.busy || campaign.storageBlocked) return
            if(!campaign.paused) {campaign.togglePaused();return}
            const callback=reviewPause.pending
            reviewPause.pending=null
            callback()
        }
    }
}
