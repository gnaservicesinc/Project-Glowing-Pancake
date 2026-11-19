import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: canvasRoot
    property int textSize: 14
    property var nodes: []
    property string selectedId: ""
    property bool fullTree: false
    property real zoom: 1.0
    property string viewMode: "graph" // "graph" | "list"
    property string domainFilter: "all"
    property string statusFilter: "all"
    property string searchQuery: ""
    property string deviceId: ""
    property bool showPopOut: true
    property real graphHeight: textSize * 26

    signal searchEdited(string query)
    signal topicSelected(string id, bool reveal)
    signal fullTreeToggled()
    signal zoomUpdated(real newZoom)
    signal viewportChanged(real panX, real panY, real zoom)
    signal viewModeUpdated(string mode)
    signal domainFilterUpdated(string domain)
    signal statusFilterUpdated(string status)
    signal popOutRequested()

    readonly property var selectedNode: nodes.find(n => n.id === selectedId) || ({})
    readonly property var connectedNodes: nodes.filter(n =>
        (selectedNode.all || []).includes(n.id) ||
        (selectedNode.any || []).includes(n.id) ||
        (n.all || []).includes(selectedId) ||
        (n.any || []).includes(selectedId))

    readonly property var availableDomains: {
        const seen = new Map()
        for (const n of nodes) {
            if (n.domain && !seen.has(n.domain)) seen.set(n.domain, n.domainLabel || n.domain)
        }
        const out = [{id: "all", label: qsTr("All domains")}]
        for (const [id, label] of seen.entries()) out.push({id, label})
        return out
    }

    readonly property var filteredNodes: {
        const q = searchQuery.trim().toLocaleLowerCase()
        return nodes.filter(n => {
            if (domainFilter !== "all" && n.domain !== domainFilter) return false
            if (statusFilter !== "all") {
                if (statusFilter === "open" && n.status !== "ready" && n.status !== "creative") return false
                else if (statusFilter === "active" && n.status !== "active" && n.status !== "paused") return false
                else if (statusFilter !== "open" && statusFilter !== "active" && n.status !== statusFilter) return false
            }
            if (q && !(n.name || "").toLocaleLowerCase().includes(q) && !(n.summary || "").toLocaleLowerCase().includes(q)) return false
            return true
        })
    }

    readonly property var graphNodes: {
        let base = filteredNodes
        const hasFilters = domainFilter !== "all" || statusFilter !== "all" || searchQuery.trim().length > 0
        if (!fullTree && !hasFilters && selectedNode.id) {
            base = [selectedNode, ...connectedNodes]
        }
        const levels = [...new Set(base.map(n => n.level))].sort((a, b) => a - b)
        const slots = new Map()
        return base.map(n => {
            const lvl = hasFilters || !fullTree ? levels.indexOf(n.level) : n.level
            const slot = slots.get(lvl) || 0
            slots.set(lvl, slot + 1)
            return Object.assign({}, n, {displayLevel: lvl, displaySlot: slot})
        })
    }

    readonly property real baseColumnStep: Math.max(210, textSize * 16.5)
    readonly property real baseRowStep: Math.max(82, textSize * 5.8)
    readonly property real baseNodeWidth: Math.max(176, textSize * 13.8)
    readonly property real baseNodeHeight: Math.max(66, textSize * 4.7)
    readonly property real columnStep: baseColumnStep * zoom
    readonly property real rowStep: baseRowStep * zoom
    readonly property real nodeWidth: baseNodeWidth * zoom
    readonly property real nodeHeight: baseNodeHeight * zoom
    readonly property real worldW: Math.max(graph.width, graphNodes.reduce((hi, n) => Math.max(hi, (n.displayLevel + 1) * columnStep + textSize * 2), 0))
    readonly property real worldH: Math.max(graph.height, graphNodes.reduce((hi, n) => Math.max(hi, (n.displaySlot + 1) * rowStep + textSize * 2), 0))

    function statusName(status) {
        return ({
            ready: qsTr("Open"),
            creative: qsTr("Creative leap"),
            locked: qsTr("Locked"),
            active: qsTr("Researching"),
            paused: qsTr("Paused"),
            complete: qsTr("Complete")
        })[status] || ""
    }

    function statusColor(status) {
        return ({
            ready: "#78c8a2",
            creative: "#e7be79",
            locked: "#8295a3",
            active: "#73bce6",
            paused: "#e59c6c",
            complete: "#a8d98b"
        })[status] || "#9eb4c2"
    }

    function revealSelected() {
        if (viewMode !== "graph") return
        const node = graphNodes.find(n => n.id === selectedId)
        if (!node) return
        graph.contentX = Math.max(0, Math.min(Math.max(0, graph.contentWidth - graph.width), node.displayLevel * columnStep - graph.width * 0.28))
        graph.contentY = Math.max(0, Math.min(Math.max(0, graph.contentHeight - graph.height), node.displaySlot * rowStep - graph.height * 0.35))
        links.requestPaint()
    }

    function restoreViewport(panX, panY) {
        if (viewMode !== "graph") return
        if (panX >= 0) graph.contentX = Math.max(0, Math.min(Math.max(0, graph.contentWidth - graph.width), panX))
        if (panY >= 0) graph.contentY = Math.max(0, Math.min(Math.max(0, graph.contentHeight - graph.height), panY))
        links.requestPaint()
    }

    function fitToGraph() {
        const maxLevel = graphNodes.reduce((hi, n) => Math.max(hi, n.displayLevel + 1), 1)
        const maxSlot = graphNodes.reduce((hi, n) => Math.max(hi, n.displaySlot + 1), 1)
        const fitX = (graph.width - 24) / Math.max(1, maxLevel * baseColumnStep)
        const fitY = (graph.height - 24) / Math.max(1, maxSlot * baseRowStep)
        const next = Math.max(0.55, Math.min(1.2, Math.min(fitX, fitY)))
        zoomUpdated(Math.round(next * 20) / 20)
        graph.contentX = 0
        graph.contentY = 0
        links.requestPaint()
    }

    spacing: 8

    // Search + keyboard selector + view & zoom controls
    Flow {
        Layout.fillWidth: true
        spacing: 6
        GameField {
            id: searchField
            objectName: "technologySearch"
            width: Math.min(parent.width, Math.max(180, canvasRoot.textSize * 15))
            textSize: canvasRoot.textSize
            text: canvasRoot.searchQuery
            placeholderText: qsTr("Search topics or capabilities")
            Accessible.name: placeholderText
            onTextEdited: { canvasRoot.searchQuery = text; canvasRoot.searchEdited(text) }
        }
        GameSelect {
            objectName: "technologyPicker"
            width: Math.min(parent.width, Math.max(190, canvasRoot.textSize * 16))
            textSize: canvasRoot.textSize
            model: canvasRoot.filteredNodes
            textRole: "name"
            valueRole: "id"
            currentIndex: model.findIndex(n => n.id === canvasRoot.selectedId)
            Accessible.name: qsTr("Select a technology from the tree")
            onActivated: if (currentValue) canvasRoot.topicSelected(currentValue, true)
        }
        WorldButton {
            objectName: "technologyScope"
            text: canvasRoot.fullTree ? qsTr("Show connected") : qsTr("Show all (%1)").arg(canvasRoot.nodes.length)
            textSize: canvasRoot.textSize * 0.88
            padding: 8
            onClicked: canvasRoot.fullTreeToggled()
        }
        WorldButton {
            objectName: "technologyViewMode"
            text: canvasRoot.viewMode === "graph" ? qsTr("List view") : qsTr("Map view")
            textSize: canvasRoot.textSize * 0.88
            padding: 8
            onClicked: canvasRoot.viewModeUpdated(canvasRoot.viewMode === "graph" ? "list" : "graph")
        }
        WorldButton {
            visible: canvasRoot.viewMode === "graph"
            text: "−"
            Accessible.name: qsTr("Zoom out")
            textSize: canvasRoot.textSize * 0.88
            padding: 7
            enabled: canvasRoot.zoom > 0.56
            onClicked: canvasRoot.zoomUpdated(Math.max(0.55, Math.round((canvasRoot.zoom - 0.15) * 20) / 20))
        }
        WorldButton {
            visible: canvasRoot.viewMode === "graph"
            text: "+"
            Accessible.name: qsTr("Zoom in")
            textSize: canvasRoot.textSize * 0.88
            padding: 7
            enabled: canvasRoot.zoom < 1.59
            onClicked: canvasRoot.zoomUpdated(Math.min(1.6, Math.round((canvasRoot.zoom + 0.15) * 20) / 20))
        }
        WorldButton {
            visible: canvasRoot.viewMode === "graph"
            text: qsTr("Fit")
            textSize: canvasRoot.textSize * 0.85
            padding: 7
            onClicked: canvasRoot.fitToGraph()
        }
        WorldButton {
            visible: canvasRoot.viewMode === "graph" && Math.abs(canvasRoot.zoom - 1.0) > 0.02
            text: qsTr("100%")
            textSize: canvasRoot.textSize * 0.85
            padding: 7
            onClicked: { canvasRoot.zoomUpdated(1.0); Qt.callLater(canvasRoot.revealSelected) }
        }
        WorldButton {
            objectName: "researchPopOutTree"
            visible: canvasRoot.showPopOut && !!canvasRoot.deviceId
            text: qsTr("Pop out tree")
            textSize: canvasRoot.textSize * 0.85
            padding: 7
            onClicked: canvasRoot.popOutRequested()
        }
    }

    // Domain & Status filters
    Flow {
        Layout.fillWidth: true
        spacing: 5
        Repeater {
            model: canvasRoot.availableDomains
            WorldButton {
                required property var modelData
                text: modelData.label
                textSize: canvasRoot.textSize * 0.78
                padding: 5
                quiet: canvasRoot.domainFilter !== modelData.id
                checked: canvasRoot.domainFilter === modelData.id
                onClicked: canvasRoot.domainFilterUpdated(modelData.id)
            }
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 5
        Repeater {
            model: [
                {id: "all", label: qsTr("All states")},
                {id: "open", label: qsTr("Available")},
                {id: "active", label: qsTr("In progress")},
                {id: "complete", label: qsTr("Completed")},
                {id: "locked", label: qsTr("Locked")}
            ]
            WorldButton {
                required property var modelData
                text: modelData.label
                textSize: canvasRoot.textSize * 0.78
                padding: 5
                quiet: canvasRoot.statusFilter !== modelData.id
                checked: canvasRoot.statusFilter === modelData.id
                onClicked: canvasRoot.statusFilterUpdated(modelData.id)
            }
        }
    }

    // Graph View
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: canvasRoot.graphHeight
        visible: canvasRoot.viewMode === "graph"
        color: "#0f222a"
        border.color: "#4a6672"
        radius: 8

        Flickable {
            id: graph
            objectName: "technologyGraph"
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: canvasRoot.worldW
            contentHeight: canvasRoot.worldH
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            onContentXChanged: { links.requestPaint(); viewportTimer.restart() }
            onContentYChanged: { links.requestPaint(); viewportTimer.restart() }

            Timer {
                id: viewportTimer
                interval: 220
                repeat: false
                onTriggered: canvasRoot.viewportChanged(graph.contentX, graph.contentY, canvasRoot.zoom)
            }

            // Mouse wheel zoom + background drag pan
            WheelHandler {
                onWheel: event => {
                    if (event.modifiers & Qt.ControlModifier || Math.abs(event.angleDelta.y) > 0) {
                        const step = event.angleDelta.y > 0 ? 0.1 : -0.1
                        const next = Math.max(0.55, Math.min(1.6, Math.round((canvasRoot.zoom + step) * 20) / 20))
                        if (Math.abs(next - canvasRoot.zoom) > 0.01) {
                            canvasRoot.zoomUpdated(next)
                            event.accepted = true
                        }
                    }
                }
            }

            Canvas {
                id: links
                x: graph.contentX
                y: graph.contentY
                width: graph.width
                height: graph.height
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: canvasRoot
                    function onGraphNodesChanged() { links.requestPaint() }
                    function onSelectedIdChanged() { links.requestPaint() }
                    function onZoomChanged() { links.requestPaint() }
                }
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const byId = new Map(canvasRoot.graphNodes.map(n => [n.id, n]))
                    function drawEdge(parentId, node, optional) {
                        const src = byId.get(parentId)
                        if (!src) return
                        const pad = canvasRoot.textSize
                        const x1 = src.displayLevel * canvasRoot.columnStep + pad + canvasRoot.nodeWidth - graph.contentX
                        const x2 = node.displayLevel * canvasRoot.columnStep + pad - graph.contentX
                        const y1 = src.displaySlot * canvasRoot.rowStep + pad + canvasRoot.nodeHeight / 2 - graph.contentY
                        const y2 = node.displaySlot * canvasRoot.rowStep + pad + canvasRoot.nodeHeight / 2 - graph.contentY
                        if (x2 < -40 || x1 > width + 40) return
                        const highlighted = canvasRoot.selectedId === node.id || canvasRoot.selectedId === parentId
                        ctx.beginPath()
                        ctx.strokeStyle = highlighted ? "#f0c97e" : "#5e838c"
                        ctx.lineWidth = highlighted ? 2.6 : 1.4
                        ctx.setLineDash(optional ? [6, 5] : [])
                        ctx.moveTo(x1, y1)
                        const mid = (x1 + x2) / 2
                        ctx.bezierCurveTo(mid, y1, mid, y2, x2, y2)
                        ctx.stroke()
                    }
                    for (const node of canvasRoot.graphNodes) {
                        for (const id of (node.all || [])) drawEdge(id, node, false)
                        for (const id of (node.any || [])) drawEdge(id, node, true)
                    }
                }
            }

            Repeater {
                model: canvasRoot.graphNodes
                AbstractButton {
                    id: nodeCard
                    required property var modelData
                    objectName: "technology_" + modelData.id
                    x: modelData.displayLevel * canvasRoot.columnStep + canvasRoot.textSize
                    y: modelData.displaySlot * canvasRoot.rowStep + canvasRoot.textSize
                    width: canvasRoot.nodeWidth
                    height: canvasRoot.nodeHeight
                    checked: canvasRoot.selectedId === modelData.id
                    hoverEnabled: true
                    activeFocusOnTab: true
                    Accessible.name: modelData.name + " · " + canvasRoot.statusName(modelData.status)
                    onClicked: canvasRoot.topicSelected(modelData.id, false)
                    onActiveFocusChanged: if (activeFocus) {
                        graph.contentX = Math.max(0, Math.min(Math.max(0, graph.contentWidth - graph.width), Math.min(x, Math.max(graph.contentX, x + width - graph.width))))
                        graph.contentY = Math.max(0, Math.min(Math.max(0, graph.contentHeight - graph.height), Math.min(y, Math.max(graph.contentY, y + height - graph.height))))
                    }
                    background: Rectangle {
                        radius: 8
                        color: nodeCard.checked ? "#2d4c5c" : nodeCard.hovered ? "#203846" : "#162a36"
                        border.width: nodeCard.activeFocus || nodeCard.checked ? 2 : 1
                        border.color: nodeCard.activeFocus ? "#ffffff" : nodeCard.checked ? "#f0c97e" : canvasRoot.statusColor(nodeCard.modelData.status)
                        Rectangle {
                            width: 5
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            radius: 3
                            color: canvasRoot.statusColor(nodeCard.modelData.status)
                        }
                    }
                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 7
                        anchors.topMargin: 5
                        anchors.bottomMargin: 5
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: nodeCard.modelData.name || ""
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            font.pixelSize: Math.max(10, Math.round(canvasRoot.textSize * 0.88 * Math.min(1.15, canvasRoot.zoom)))
                            font.bold: true
                            color: "#f4ecdc"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                text: canvasRoot.statusName(nodeCard.modelData.status) + (nodeCard.modelData.activeProgress ? " · " + nodeCard.modelData.activeProgress : "")
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                font.pixelSize: Math.max(9, Math.round(canvasRoot.textSize * 0.72 * Math.min(1.1, canvasRoot.zoom)))
                                color: canvasRoot.statusColor(nodeCard.modelData.status)
                            }
                        }
                    }
                }
            }
        }

        // Compact minimap location indicator (bottom-right)
        Rectangle {
            visible: graph.contentWidth > graph.width + 20 || graph.contentHeight > graph.height + 20
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            width: 112
            height: 64
            radius: 5
            color: "#d9101d26"
            border.color: "#557180"
            Rectangle {
                x: Math.max(2, (graph.contentX / Math.max(1, graph.contentWidth)) * (parent.width - 4))
                y: Math.max(2, (graph.contentY / Math.max(1, graph.contentHeight)) * (parent.height - 4))
                width: Math.max(14, Math.min(parent.width - 4, (graph.width / Math.max(1, graph.contentWidth)) * (parent.width - 4)))
                height: Math.max(10, Math.min(parent.height - 4, (graph.height / Math.max(1, graph.contentHeight)) * (parent.height - 4)))
                color: "#44efc389"
                border.color: "#efc389"
                radius: 2
            }
        }
    }

    // List View Alternative (always keyboard accessible and clean for large catalogs)
    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: canvasRoot.graphHeight
        visible: canvasRoot.viewMode === "list"
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: parent.width
            spacing: 6
            Repeater {
                model: canvasRoot.filteredNodes
                AbstractButton {
                    id: listRow
                    required property var modelData
                    objectName: "technologyList_" + modelData.id
                    Layout.fillWidth: true
                    implicitHeight: listCol.implicitHeight + 16
                    checked: canvasRoot.selectedId === modelData.id
                    hoverEnabled: true
                    activeFocusOnTab: true
                    onClicked: canvasRoot.topicSelected(modelData.id, false)
                    background: Rectangle {
                        radius: 7
                        color: listRow.checked ? "#2b4959" : listRow.hovered ? "#1e3442" : "#152631"
                        border.color: listRow.checked ? "#efc389" : "#3a5463"
                    }
                    contentItem: ColumnLayout {
                        id: listCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 3
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                text: listRow.modelData.name || ""
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                font.pixelSize: canvasRoot.textSize
                                font.bold: true
                                color: "#f5ecda"
                            }
                            Label {
                                text: (listRow.modelData.domainLabel || "") + " · " + canvasRoot.statusName(listRow.modelData.status)
                                textFormat: Text.PlainText
                                font.pixelSize: canvasRoot.textSize * 0.82
                                color: canvasRoot.statusColor(listRow.modelData.status)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: !!(listRow.modelData.summary)
                            text: listRow.modelData.summary || ""
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            font.pixelSize: canvasRoot.textSize * 0.86
                            color: "#bed0da"
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: canvasRoot.filteredNodes.length === 0
                text: qsTr("No research topics match the current search and filters.")
                wrapMode: Text.Wrap
                font.pixelSize: canvasRoot.textSize
                color: "#b4c5d0"
            }
        }
    }
}
