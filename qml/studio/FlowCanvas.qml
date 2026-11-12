import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: graph
    objectName: "flowCanvas"
    required property QtObject author
    required property QtObject inspector
    property real zoom: 0.8
    property real panX: 0
    property real panY: 0
    property bool initializedView: false
    property bool restoreCanvasFocus: false
    property var dragging: ({})
    readonly property real viewportWidth: viewport.width
    readonly property real viewportHeight: viewport.height
    readonly property var filteredNodes: author.nodes.filter(node => !nodeSearch.text.length || (node.title + " " + node.key).toLowerCase().indexOf(nodeSearch.text.toLowerCase()) >= 0)
    readonly property var filteredPalette: author.palette.filter(node => !paletteSearch.text.length || node.label.toLowerCase().indexOf(paletteSearch.text.toLowerCase()) >= 0)
    spacing: 6
    focus: true

    function portColor(kind) { return ["#d5aeff", "#ffcc7c", "#8ad8ff", "#91e0b7"][kind] || "#b5c8d6" }
    function tooltipText(value) { return "<p>" + value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/\n/g, "<br>") + "</p>" }
    function traceColor(state) {
        if (state === "waiting" || state === "queued") return "#ffd38a"
        if (state === "committed" || state === "true" || state === "active") return "#8fe2b4"
        if (state === "false" || state === "candidate") return "#ffb0a4"
        if (state === "synthetic") return "#d5aeff"
        return "#9bb4c7"
    }
    function remember() {
        author.rememberViewport(Math.round(panX), Math.round(panY), Math.round(zoom * 1000))
    }
    function pickPort(ticket, port) {
        const before = author.ticket
        restoreCanvasFocus = true
        const ok = author.pickPort(ticket, port)
        if (author.ticket === before) restoreCanvasFocus = false
        remember()
        return ok
    }
    function createSelected() {
        const before = author.ticket
        restoreCanvasFocus = true
        author.insertNode(author.ticket, palette.currentValue)
        if (author.ticket === before) restoreCanvasFocus = false
    }
    function changedView(persist) {
        initializedView = true
        author.previewViewport(Math.round(panX), Math.round(panY), Math.round(zoom * 1000))
        wires.requestPaint()
        if (persist) remember()
    }
    function setZoom(value, px, py) {
        const old = zoom
        zoom = Math.max(0.1, Math.min(2, value))
        panX = px - (px - panX) * zoom / old
        panY = py - (py - panY) * zoom / old
        changedView(true)
    }
    function chooseNode(key) {
        author.selectNode(key)
        graph.forceActiveFocus(Qt.OtherFocusReason)
        remember()
    }
    function focusNode(key, persist) {
        const node = author.nodeLookup[key]
        if (!node) return
        zoom = Math.max(0.1, Math.min(1, (viewport.width - 32) / node.width, (viewport.height - 32) / node.height))
        panX = viewport.width / 2 - (node.x + node.width / 2) * zoom
        panY = viewport.height / 2 - (node.y + node.height / 2) * zoom
        chooseNode(key)
        changedView(persist)
    }
    function fitAll(persist) {
        if (!author.nodes.length || viewport.width < 1 || viewport.height < 1) return
        let left = 1048576, top = 1048576, right = 0, bottom = 0
        for (const node of author.nodes) { left = Math.min(left, node.x); top = Math.min(top, node.y); right = Math.max(right, node.x + node.width); bottom = Math.max(bottom, node.y + node.height) }
        zoom = Math.max(0.1, Math.min(1, (viewport.width - 32) / (right - left), (viewport.height - 32) / (bottom - top)))
        panX = viewport.width / 2 - (left + right) * zoom / 2
        panY = viewport.height / 2 - (top + bottom) * zoom / 2
        changedView(persist)
    }
    function previewMove(key, x, y) { const next = {}; next[key] = {x: x, y: y}; dragging = next; wires.requestPaint() }
    function finishMove(ticket, key, x, y) { const ok = author.moveNode(ticket, key, x, y); dragging = ({}); wires.requestPaint(); return ok }
    function cancelMove() { dragging = ({}); wires.requestPaint() }
    Keys.onPressed: event => {
        const node = author.nodeLookup[author.selectedKey]
        if (event.key === Qt.Key_Escape) { author.cancelConnection(); event.accepted = true; return }
        if (event.key === Qt.Key_Home) { fitAll(true); event.accepted = true; return }
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) { setZoom(zoom * 1.2, viewport.width / 2, viewport.height / 2); event.accepted = true; return }
        if (event.key === Qt.Key_Minus) { setZoom(zoom / 1.2, viewport.width / 2, viewport.height / 2); event.accepted = true; return }
        let dx = 0, dy = 0
        if (event.key === Qt.Key_Left) dx = -1
        else if (event.key === Qt.Key_Right) dx = 1
        else if (event.key === Qt.Key_Up) dy = -1
        else if (event.key === Qt.Key_Down) dy = 1
        else return
        if (event.modifiers & Qt.ControlModifier) { panX += dx * 64; panY += dy * 64; changedView(true) }
        else if (node) author.moveNode(author.ticket, node.key, Math.max(0, node.x + dx * 16), Math.max(0, node.y + dy * 16))
        event.accepted = true
    }

    RowLayout {
        Layout.fillWidth: true
        TextField { id: nodeSearch; objectName: "canvasSearch"; Layout.fillWidth: true; placeholderText: qsTr("Find a box by name"); Accessible.name: qsTr("Find canvas box") }
        ComboBox { id: navigator; objectName: "canvasNavigator"; Layout.preferredWidth: 180; model: graph.filteredNodes; textRole: "title"; valueRole: "key"; Accessible.name: qsTr("Canvas box navigator") }
        Button { objectName: "focusCanvasNode"; text: qsTr("Focus"); enabled: navigator.currentIndex >= 0; onClicked: graph.focusNode(navigator.currentValue, true) }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 4
        Button { objectName: "canvasZoomOut"; text: "−"; Accessible.name: qsTr("Zoom out"); onClicked: graph.setZoom(graph.zoom / 1.2, viewport.width / 2, viewport.height / 2) }
        Label { width: 42; height: 34; text: Math.round(graph.zoom * 100) + "%"; verticalAlignment: Text.AlignVCenter }
        Button { objectName: "canvasZoomIn"; text: "+"; Accessible.name: qsTr("Zoom in"); onClicked: graph.setZoom(graph.zoom * 1.2, viewport.width / 2, viewport.height / 2) }
        Button { text: qsTr("Fit all"); onClicked: graph.fitAll(true) }
        Button { text: qsTr("Fit selected"); enabled: !!graph.author.selectedKey; onClicked: graph.focusNode(graph.author.selectedKey, true) }
        Button { objectName: "autoLayoutCanvas"; text: qsTr("Arrange"); onClicked: graph.author.autoLayout() }
        Button { objectName: "saveCanvasLayout"; text: qsTr("Save layout"); enabled: graph.author.layoutDirty; onClicked: graph.author.saveLayout() }
        Button { text: qsTr("Undo move"); enabled: graph.author.canUndoLayout; onClicked: graph.author.undoLayout() }
        Button { text: qsTr("Redo move"); enabled: graph.author.canRedoLayout; onClicked: graph.author.redoLayout() }
        Button { text: qsTr("Reload saved layout"); onClicked: reloadDialog.open() }
        Button { objectName: "showCanvasNotices"; visible: graph.author.warnings.length > 0; text: qsTr("Compiler notices (%1)").arg(graph.author.warnings.length); onClicked: noticesDialog.open() }
    }
    RowLayout {
        Layout.fillWidth: true
        Button {
            text: qsTr("Add a box…")
            onClicked: graph.author.prepareInsert()
        }
        TextField { id: paletteSearch;
            visible: graph.width >= 560; Layout.fillWidth: true; placeholderText: qsTr("Search this port's palette"); Accessible.name: qsTr("Search node palette") }
        ComboBox { id: palette; objectName: "canvasPalette"; Layout.fillWidth: true; Layout.maximumWidth: 230; model: graph.filteredPalette; textRole: "label"; valueRole: "value"; Accessible.name: qsTr("Node palette") }
        Button { objectName: "insertCanvasNode"; text: qsTr("Add"); enabled: palette.currentIndex >= 0; onClicked: graph.createSelected() }
        Button { objectName: "cancelCanvasLink"; text: qsTr("Cancel link"); enabled: !!graph.author.pendingOutput; onClicked: graph.author.cancelConnection() }
    }
    Label {
        Layout.fillWidth: true
        text: author.portLabel || qsTr("Select a box to edit its settings. Add a box to the current route, or select a connection point to choose a different route.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    Label {
        objectName: "canvasStatus"
        Layout.fillWidth: true
        visible: author.message.length > 0
        text: author.message
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
    }
    Label {
        objectName: "canvasWarnings"
        Layout.fillWidth: true
        visible: author.warnings.length > 0
        text: qsTr("%1 compiler notice(s) · %2").arg(author.warnings.length).arg(author.warnings[0] || "")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }
    Rectangle {
        id: viewport
        objectName: "canvasViewport"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 180
        clip: true
        color: "#0d1b25"
        border.color: "#506676"
        MouseArea {
            anchors.fill: parent
            property real previousX
            property real previousY
            onPressed: mouse => { previousX = mouse.x; previousY = mouse.y; graph.forceActiveFocus(Qt.MouseFocusReason) }
            onPositionChanged: mouse => {
                if (!pressed) return
                graph.panX = Math.max(-4194304, Math.min(4194304, graph.panX + mouse.x - previousX))
                graph.panY = Math.max(-4194304, Math.min(4194304, graph.panY + mouse.y - previousY))
                previousX = mouse.x; previousY = mouse.y
                graph.changedView(false)
            }
            onReleased: graph.remember()
            onWheel: wheel => { graph.setZoom(graph.zoom * (wheel.angleDelta.y > 0 ? 1.12 : 1 / 1.12), wheel.x, wheel.y); wheel.accepted = true }
        }
        Canvas {
            id: wires
            anchors.fill: parent
            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.translate(graph.panX, graph.panY)
                context.scale(graph.zoom, graph.zoom)
                context.lineWidth = 1.5 / graph.zoom
                for (const edge of graph.author.edges) {
                    const source = graph.author.nodeLookup[edge.fromNode], target = graph.author.nodeLookup[edge.toNode]
                    if (!source || !target) continue
                    const from = graph.dragging[edge.fromNode] || source, to = graph.dragging[edge.toNode] || target
                    const sx = from.x + source.width - 8, sy = from.y + edge.fromY, tx = to.x + 8, ty = to.y + edge.toY
                    const reach = Math.max(48, Math.abs(tx - sx) * 0.5)
                    const trace = graph.inspector.graphTrace[edge.from] || graph.inspector.graphTrace[edge.to] || graph.inspector.graphTrace[edge.row] || ({})
                    context.strokeStyle = trace.state ? graph.traceColor(trace.state) : edge.kind === "scope" ? "#9c85b6" : "#7293aa"
                    context.setLineDash(edge.kind === "owned" || trace.state === "queued" ? [6 / graph.zoom, 4 / graph.zoom] : [])
                    context.beginPath(); context.moveTo(sx, sy); context.bezierCurveTo(sx + reach, sy, tx - reach, ty, tx, ty); context.stroke()
                    context.setLineDash([])
                    context.fillStyle = context.strokeStyle
                    context.beginPath(); context.moveTo(tx, ty); context.lineTo(tx - 7 / graph.zoom, ty - 4 / graph.zoom); context.lineTo(tx - 7 / graph.zoom, ty + 4 / graph.zoom); context.closePath(); context.fill()
                    context.font = "11px sans-serif"
                    context.fillText(edge.label, (sx + tx) / 2, (sy + ty) / 2 - 5)
                }
            }
        }
        Item {
            x: graph.panX; y: graph.panY
            scale: graph.zoom
            transformOrigin: Item.TopLeft
            Repeater {
                model: graph.author.nodes
                CanvasNode {
                    author: graph.author
                    inspector: graph.inspector
                    workspace: graph
                }
            }
        }
        Label { anchors.centerIn: parent; visible: graph.author.nodes.length === 0; text: qsTr("Choose a flow. Its shared outline remains available."); color: "#cedee9" }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Trace: green = committed/true; amber = waiting/queued; red = false/rejected candidate; purple = separate synthetic probe. State labels and full details are also in settings.")
        wrapMode: Text.Wrap
        font.pixelSize: 11
    }
    Connections {
        target: graph.author
        function onGraphChanged() {
            wires.requestPaint()
            if (!graph.initializedView) Qt.callLater(() => graph.fitAll(false))
            if (graph.restoreCanvasFocus) { graph.restoreCanvasFocus = false; Qt.callLater(() => graph.forceActiveFocus(Qt.OtherFocusReason)) }
        }
        function onViewChanged() {
            const view = graph.author.view
            graph.panX = view.x; graph.panY = view.y; graph.zoom = view.zoom / 1000
            graph.initializedView = view.restored
            if (!view.restored) Qt.callLater(() => graph.fitAll(false))
            wires.requestPaint()
        }
    }
    Connections { target: graph.inspector; function onChanged() { wires.requestPaint() } }
    Dialog {
        id: reloadDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Reload saved layout and discard local moves?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: graph.author.reloadLayout()
    }
    Dialog {
        id: noticesDialog
        objectName: "canvasNotices"
        anchors.centerIn: parent
        width: Math.min(680, graph.width - 24)
        height: Math.min(400, graph.height - 24)
        title: qsTr("Shared compiler notices")
        modal: true
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            TextArea {
                objectName: "canvasNoticesText"
                text: graph.author.warnings.join("\n\n")
                textFormat: Text.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                Accessible.name: qsTr("Compiler timing and graph notices")
            }
        }
    }
}
