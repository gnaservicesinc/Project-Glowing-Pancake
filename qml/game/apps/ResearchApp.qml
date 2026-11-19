import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../"

ColumnLayout {
    id: app
    required property int textSize
    required property bool editable
    property string deviceId: ""
    property bool canDetach: true
    property bool detachedTree: false
    signal suppliersRequested()

    readonly property var nodes: campaign.management.researchGraph
    readonly property var workers: campaign.management.researchWorkers
    readonly property var sources: campaign.management.researchSources
    readonly property var visibleSources: sources.filter(s => s.canAllocate)
    readonly property var activeProjects: nodes.filter(n => n.status === "active" || n.status === "paused")

    property int updates: 0
    property string selectedId: ""
    property string selectedSource: ""
    property string computeRateText: ""
    property var assigned: []
    property var datasetChoices: ({})
    readonly property var chosenData: datasetChoices[selectedId] || []
    property bool fullTree: false
    property real zoom: 1.0
    property string viewMode: "graph"
    property string domainFilter: "all"
    property string statusFilter: "all"
    property bool compactDetails: false
    property bool inspectorOpen: true
    readonly property bool wideLayout: width >= Math.max(760, textSize * 54)

    readonly property var selected: {
        const current = updates
        return campaign.management.researchTopic(selectedId)
    }
    readonly property var teamPreview: {
        campaign.management.researchGraph
        return visible ? campaign.management.researchTeam(selectedId, assigned) : ({})
    }
    readonly property string blocker: {
        const current = updates
        return visible ? campaign.management.researchBlocker(selectedId, selectedSource, computeRateText, assigned, chosenData) : ""
    }

    property string reviewedRevision: ""
    property bool quoteChanged: false
    function reviewTerms() {
        reviewedRevision = campaign.world.reviewToken
        persistSession("quoteRevision", reviewedRevision)
        quoteChanged = false
    }
    function submit() {
        if (reviewedRevision !== campaign.world.reviewToken) {
            reviewTerms()
            quoteChanged = true
            return
        }
        quoteChanged = false
        campaign.management.beginGroundedResearch(selectedId, selectedSource, computeRateText, assigned, chosenData)
    }
    function persistSession(key, value) {
        if (deviceId) campaign.world.setDeviceAppState(deviceId, "lab", key, value)
    }

    function selectTopic(id, reveal) {
        if (!id) return
        selectedId = id
        if (!wideLayout) compactDetails = true
        if (!app.deviceId) campaign.world.setDraft("research/tree", id)
        persistSession("selectedTopic", id)
        reviewTerms()
        if (reveal || !fullTree) Qt.callLater(() => graphCanvas.revealSelected())
    }

    function chooseSource() {
        if (visibleSources.some(s => s.id === selectedSource)) return
        selectedSource = ""
        if (visibleSources.length) {
            selectedSource = visibleSources[0].id
            computeRateText = visibleSources[0].free
        }
    }

    function toggleWorker(id) {
        assigned = assigned.includes(id) ? assigned.filter(v => v !== id) : assigned.concat([id])
        if (!app.deviceId) campaign.world.setDraft("research/team", JSON.stringify(assigned))
        persistSession("assignedTeam", assigned)
        reviewTerms()
    }

    function restore() {
        const session = deviceId ? campaign.world.deviceAppState(deviceId, "lab") : ({})
        const preferred = session.selectedTopic || (deviceId ? "research.reproducible_tooling" : campaign.world.draft("research/tree", "research.reproducible_tooling"))
        selectedId = nodes.some(n => n.id === preferred) ? preferred : (nodes[0] || {}).id || ""
        selectedSource = session.selectedSource ?? (deviceId ? "" : campaign.world.draft("research/source", ""))
        computeRateText = session.computeRate ?? (deviceId ? "" : campaign.world.draft("research/rate", ""))
        if (Array.isArray(session.assignedTeam)) assigned = session.assignedTeam
        else if (deviceId) assigned = []
        else {
            try { assigned = JSON.parse(campaign.world.draft("research/team", "[]")) } catch (e) { assigned = [] }
            if (!Array.isArray(assigned)) assigned = []
        }
        if (typeof session.fullTree === "boolean") fullTree = session.fullTree
        if (typeof session.zoom === "number" && session.zoom >= 0.5 && session.zoom <= 1.7) zoom = session.zoom
        if (session.viewMode === "graph" || session.viewMode === "list") viewMode = session.viewMode
        if (typeof session.domainFilter === "string" && session.domainFilter.length) domainFilter = session.domainFilter
        if (typeof session.statusFilter === "string" && session.statusFilter.length) statusFilter = session.statusFilter
        if (typeof session.inspectorOpen === "boolean") inspectorOpen = session.inspectorOpen
        reviewedRevision = session.quoteRevision || campaign.world.reviewToken
        datasetChoices = session.datasetChoices || ({})
        graphCanvas.searchQuery = session.search || ""
        chooseSource()
        if (typeof session.panX === "number" && typeof session.panY === "number")
            Qt.callLater(() => graphCanvas.restoreViewport(session.panX, session.panY))
        else Qt.callLater(() => graphCanvas.revealSelected())
    }

    Component.onCompleted: Qt.callLater(restore)
    onVisibleChanged: if (visible) Qt.callLater(restore)
    onVisibleSourcesChanged: if (visible) Qt.callLater(chooseSource)
    Connections {
        target: campaign.management
        function onChanged() { app.updates++ }
    }
    Connections {
        target: campaign.world
        function onDraftsReset() { if (app.visible) Qt.callLater(app.restore) }
        function onWorkspacesChanged() {
            if (!app.visible || !app.deviceId) return
            const session = campaign.world.deviceAppState(app.deviceId, "lab")
            if (session.selectedTopic && session.selectedTopic !== app.selectedId && app.nodes.some(n => n.id === session.selectedTopic))
                app.selectedId = session.selectedTopic
            if (session.domainFilter) app.domainFilter = session.domainFilter
            if (session.statusFilter) app.statusFilter = session.statusFilter
        }
    }

    spacing: 10

    // Header + Active/Paused research strip (NO completion %!)
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: headerCol.implicitHeight + 16
        radius: 8
        color: "#172939"
        border.color: "#3f596d"
        ColumnLayout {
            id: headerCol
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Research Frontier")
                        font.pixelSize: app.textSize * 1.35
                        font.bold: true
                        color: "#f4e6c4"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Explore ideas across domains. Solid links are required prerequisites; dashed links show alternative paths.")
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: app.textSize * 0.86
                        color: "#b7ced3"
                    }
                }
                WorldButton {
                    objectName: "researchViewToggle"
                    text: !app.wideLayout ? (app.compactDetails ? qsTr("Back to tree") : qsTr("Topic details")) : app.inspectorOpen ? qsTr("Hide inspector") : qsTr("Show inspector")
                    textSize: app.textSize * 0.85
                    padding: 7
                    onClicked: {
                        if (!app.wideLayout) app.compactDetails = !app.compactDetails
                        else {
                            app.inspectorOpen = !app.inspectorOpen
                            app.persistSession("inspectorOpen", app.inspectorOpen)
                        }
                    }
                }
            }

            // Active & paused research strip
            Flow {
                Layout.fillWidth: true
                visible: app.activeProjects.length > 0
                spacing: 6
                Label {
                    text: qsTr("Live projects:")
                    font.pixelSize: app.textSize * 0.84
                    font.bold: true
                    color: "#efc389"
                    height: 30
                    verticalAlignment: Text.AlignVCenter
                }
                Repeater {
                    model: app.activeProjects
                    WorldButton {
                        required property var modelData
                        text: modelData.name + (modelData.activeProgress ? " (" + modelData.activeProgress + ")" : "") +
                            (modelData.status === "paused" ? qsTr(" · Paused") : "")
                        textSize: app.textSize * 0.82
                        padding: 6
                        checked: app.selectedId === modelData.id
                        onClicked: app.selectTopic(modelData.id, true)
                    }
                }
            }
        }
    }

    // Main split (wide) or stacked (compact) workspace
    GridLayout {
        Layout.fillWidth: true
        columns: app.wideLayout && app.inspectorOpen ? 2 : 1
        columnSpacing: 14
        rowSpacing: 12

        ResearchGraphCanvas {
            id: graphCanvas
            visible: app.wideLayout || !app.compactDetails
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: app.wideLayout ? (app.inspectorOpen ? app.width * 0.56 : app.width) : app.width
            Layout.alignment: Qt.AlignTop
            textSize: app.textSize
            nodes: app.nodes
            selectedId: app.selectedId
            fullTree: app.fullTree
            zoom: app.zoom
            viewMode: app.viewMode
            domainFilter: app.domainFilter
            statusFilter: app.statusFilter
            deviceId: app.deviceId
            showPopOut: app.canDetach && !app.detachedTree && !!app.deviceId
            graphHeight: app.detachedTree ? Math.max(260, app.Window.window.height - 270) : app.wideLayout ? Math.max(420, app.textSize * 27) : Math.max(240, app.textSize * 15)
            onTopicSelected: (id, reveal) => app.selectTopic(id, reveal)
            onSearchEdited: query => app.persistSession("search", query)
            onFullTreeToggled: {
                app.fullTree = !app.fullTree
                app.persistSession("fullTree", app.fullTree)
                Qt.callLater(() => graphCanvas.revealSelected())
            }
            onZoomUpdated: next => { app.zoom = next; app.persistSession("zoom", next) }
            onViewportChanged: (px, py, z) => { app.persistSession("panX", px); app.persistSession("panY", py); app.persistSession("zoom", z) }
            onViewModeUpdated: mode => { app.viewMode = mode; app.persistSession("viewMode", mode) }
            onDomainFilterUpdated: dom => { app.domainFilter = dom; app.persistSession("domainFilter", dom) }
            onStatusFilterUpdated: st => { app.statusFilter = st; app.persistSession("statusFilter", st) }
            onPopOutRequested: if (app.deviceId) campaign.world.openDetachedResearchTree(app.deviceId)
        }

        ResearchTopicInspector {
            id: inspector
            visible: app.wideLayout ? app.inspectorOpen : app.compactDetails
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: app.wideLayout ? app.width * 0.42 : app.width
            Layout.alignment: Qt.AlignTop
            textSize: app.textSize
            editable: app.editable
            selectedId: app.selectedId
            chosenData: app.chosenData
            onDatasetToggled: id => {
                const choices = Object.assign({}, app.datasetChoices)
                choices[app.selectedId] = app.chosenData.includes(id) ? app.chosenData.filter(value => value !== id) : app.chosenData.concat([id])
                app.datasetChoices = choices
                app.persistSession("datasetChoices", choices)
                app.reviewTerms()
            }
            selected: app.selected
            workers: app.workers
            assigned: app.assigned
            visibleSources: app.visibleSources
            selectedSource: app.selectedSource
            computeRateText: app.computeRateText
            teamPreview: app.teamPreview
            blocker: app.blocker
            rentals: campaign.management.researchRentals
            onTopicJumpRequested: id => app.selectTopic(id, true)
            onWorkerToggled: id => app.toggleWorker(id)
            onSourceChosen: (src, defRate) => {
                app.selectedSource = src
                app.computeRateText = defRate
                if (!app.deviceId) campaign.world.setDraft("research/source", src)
                if (!app.deviceId) campaign.world.setDraft("research/rate", defRate)
                app.persistSession("selectedSource", src)
                app.persistSession("computeRate", defRate)
                app.reviewTerms()
            }
            onRateEdited: txt => {
                app.computeRateText = txt
                if (!app.deviceId) campaign.world.setDraft("research/rate", txt)
                app.persistSession("computeRate", txt)
                app.reviewTerms()
            }
            quoteChanged: app.quoteChanged
            onStartRequested: app.submit()
            onSuppliersRequested: app.suppliersRequested()
            onRentalRequested: offerId => campaign.management.acceptOffer(offerId)
        }
    }
}
