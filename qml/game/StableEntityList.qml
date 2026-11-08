import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ListView {
    id: list
    required property var rows
    required property int textSize
    required property bool reducedMotion
    required property string emptyText
    signal selected(string entityId)

    property bool restoreFocus: false
    property string focusedId: ""

    model: rows
    clip: true
    spacing: 4
    reuseItems: true
    boundsBehavior: reducedMotion ? Flickable.StopAtBounds : Flickable.DragOverBounds
    activeFocusOnTab: count > 0
    Accessible.name: emptyText
    ScrollBar.vertical: ScrollBar { }
    currentIndex: rows.indexOf(rows.selectedId)
    DraftState {
        id: selectionDraft
        draftKey: list.rows.objectName.length ? "list/" + list.rows.objectName : ""
        accepts: value => typeof value === "string" && list.rows.indexOf(value) >= 0
        onRestored: value => list.rows.selectedId = value
    }
    Connections {
        target: list
        function onSelected(entityId) { selectionDraft.remember(entityId) }
        function onCountChanged() { Qt.callLater(selectionDraft.restoreDraft) }
    }

    function revealFocus() {
        if (currentIndex >= 0) {
            positionViewAtIndex(currentIndex, ListView.Contain)
            if (currentItem)
                currentItem.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    Keys.onUpPressed: {
        if (currentIndex > 0) {
            selected(rows.idAt(currentIndex - 1))
            revealFocus()
        }
    }
    Keys.onDownPressed: {
        if (currentIndex < count - 1) {
            selected(rows.idAt(currentIndex + 1))
            revealFocus()
        }
    }
    Keys.onSpacePressed: revealFocus()

    delegate: Button {
        id: entry
        required property string entityId
        required property string title
        required property string subtitle
        required property string status
        width: list.width - 14
        padding: 10
        hoverEnabled: true
        focusPolicy: Qt.ClickFocus
        Accessible.name: title + ", " + subtitle + ", " + status
        onClicked: list.selected(entityId)
        contentItem: ColumnLayout {
            spacing: 4
            Label {
                Layout.fillWidth: true
                text: entry.title + " · " + entry.status
                wrapMode: Text.Wrap
                font.pixelSize: list.textSize
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: entry.subtitle
                wrapMode: Text.Wrap
                font.pixelSize: list.textSize
            }
        }
        background: Rectangle {
            radius: 5
            color: entry.entityId === list.rows.selectedId ? "#25445a" : entry.hovered ? "#263442" : "#1c2835"
            border.width: entry.activeFocus ? 2 : 1
            border.color: entry.activeFocus ? "#ffd897" : "#3a4b5c"
        }
    }

    Label {
        anchors.fill: parent
        anchors.margins: 12
        visible: list.count === 0
        text: list.emptyText
        wrapMode: Text.Wrap
        font.pixelSize: list.textSize
    }

    Connections {
        target: list.rows
        function onAboutToReconcile() {
            if (!list.visible || list.count === 0)
                return
            let top = list.indexAt(4, list.contentY + 1)
            if (top < 0)
                top = list.indexAt(4, list.contentY + list.spacing + 1)
            const item = list.itemAtIndex(top)
            if (item) {
                list.rows.anchorId = list.rows.idAt(top)
                list.rows.anchorOffset = list.contentY - item.y
            }
            list.restoreFocus = list.activeFocus && list.currentItem && list.currentItem.activeFocus
            list.focusedId = list.restoreFocus ? list.currentItem.entityId : ""
        }
        function onReconciled() {
            if (!list.visible || list.count === 0)
                return
            const anchor = list.rows.indexOf(list.rows.anchorId)
            if (anchor >= 0) {
                list.forceLayout()
                const offset = list.rows.anchorOffset
                const top = list.indexAt(4, list.contentY + 1)
                const previous = list.itemAtIndex(top)
                if (top !== anchor || (previous && Math.abs(list.contentY - previous.y - offset) > 1)) {
                    list.positionViewAtIndex(anchor, ListView.Beginning)
                    const item = list.itemAtIndex(anchor)
                    if (item)
                        list.contentY = item.y + offset
                    list.returnToBounds()
                }
            }
            const focused = list.rows.indexOf(list.focusedId)
            if (list.restoreFocus && focused >= 0 && list.activeFocus) {
                const item = list.itemAtIndex(focused)
                if (item)
                    item.forceActiveFocus(Qt.OtherFocusReason)
            }
            list.restoreFocus = false
        }
    }
}
