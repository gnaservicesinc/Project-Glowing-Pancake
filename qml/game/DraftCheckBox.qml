import QtQuick
import QtQuick.Controls

CheckBox {
    id: check
    property string draftScope: ""
    property bool draftAllowed: true
    property bool draftDefault: false
    DraftState {
        id: saved
        draftKey: check.objectName.length ? "check/" + check.objectName + "/" + check.draftScope : ""
        accepts: value => typeof value === "boolean"
        onRestored: value => { if (check.draftAllowed) check.checked = value }
        onReset: check.checked = check.draftDefault
    }
    Connections { target: check; function onClicked() { saved.remember(check.checked) } }
}
