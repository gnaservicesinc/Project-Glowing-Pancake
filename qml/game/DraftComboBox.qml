import QtQuick
import QtQuick.Controls

ComboBox {
    id: picker
    property string draftScope: ""
    property var draftIds: []
    property var draftOwner: null
    property string draftProperty: ""
    property var draftResetValue: undefined
    property int draftDefaultIndex: 0
    function stableId(index) {
        if (index < 0 || index >= count) return ""
        return draftIds.length ? draftIds[index] : valueRole.length ? String(valueAt(index)) : textAt(index)
    }
    function stableIndex(value) {
        if (draftIds.length) return draftIds.indexOf(value)
        return valueRole.length ? indexOfValue(value) : find(value, Qt.MatchExactly)
    }
    function restoreDraft() {
        if (draftOwner && draftProperty.length && draftResetValue !== undefined && stableIndex(draftOwner[draftProperty]) < 0)
            draftOwner[draftProperty] = draftResetValue
        saved.restoreDraft()
    }
    DraftState {
        id: saved
        draftKey: picker.draftOwner && picker.draftProperty === "selectedId" && typeof picker.draftOwner.containsSourceId === "function"
            ? "list/" + picker.draftOwner.objectName : picker.objectName.length ? "choice/" + picker.objectName + "/" + picker.draftScope : ""
        accepts: value => typeof value === "string" && picker.stableIndex(value) >= 0
        onRestored: value => {
            if (picker.draftOwner && picker.draftProperty.length) picker.draftOwner[picker.draftProperty] = value
            else picker.currentIndex = picker.stableIndex(value)
        }
        onReset: {
            if (picker.draftOwner && picker.draftProperty.length) {
                if (picker.draftResetValue !== undefined) picker.draftOwner[picker.draftProperty] = picker.draftResetValue
            } else picker.currentIndex = picker.count > 0 ? picker.draftDefaultIndex : -1
        }
    }
    Connections {
        target: picker
        function onActivated(index) { saved.remember(picker.stableId(index)) }
        function onModelChanged() { Qt.callLater(picker.restoreDraft) }
        function onCountChanged() { Qt.callLater(picker.restoreDraft) }
    }
}
