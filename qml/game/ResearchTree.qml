import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: tree
    columns: width >= textSize * 62 ? 2 : 1
    columnSpacing: 16
    rowSpacing: 12
    required property int textSize
    required property bool editable
    signal suppliersRequested()
    readonly property var nodes: campaign.management.researchGraph
    readonly property var workers: campaign.management.researchWorkers
    readonly property var sources: campaign.management.researchSources
    property int updates: 0
    property string selectedId: campaign.world.draft("research/tree", "")
    property string selectedSource: campaign.world.draft("research/source", "")
    property var assigned: []
    readonly property var selected: {
        const current = updates
        return campaign.management.researchTopic(selectedId)
    }
    readonly property var selectedNode: nodes.find(node => node.id === selectedId) || ({})
    readonly property var connected: nodes.filter(node => (selectedNode.all || []).includes(node.id)
        || (selectedNode.any || []).includes(node.id) || node.all.includes(selectedId) || node.any.includes(selectedId))
    readonly property var visibleSources: sources.filter(source => source.canAllocate)
    readonly property var teamPreview: {
        campaign.management.researchGraph
        return visible?campaign.management.researchTeam(selectedId,assigned):({})
    }
    readonly property string blocker: {
        const current = updates
        return visible ? campaign.management.researchBlocker(selectedId, selectedSource, rate.text, assigned) : ""
    }
    readonly property int columnStep: textSize * 19
    readonly property int rowStep: textSize * 6
    readonly property int nodeWidth: textSize * 15
    readonly property int nodeHeight: textSize * 5

    function select(id, reveal) {
        selectedId = id
        campaign.world.setDraft("research/tree", id)
        if (!reveal) return
        const node = nodes.find(row => row.id === id)
        if (node) {
            graph.contentX = Math.max(0, Math.min(graph.contentWidth - graph.width, node.level * columnStep - columnStep / 3))
            graph.contentY = Math.max(0, Math.min(graph.contentHeight - graph.height, node.slot * rowStep - rowStep))
        }
    }
    function restore() {
        const preferred = campaign.world.draft("research/tree", "research.reproducible_tooling")
        selectedId = nodes.some(node => node.id === preferred) ? preferred : (nodes[0] || {}).id || ""
        selectedSource = campaign.world.draft("research/source", "")
        rate.text = campaign.world.draft("research/rate", "")
        try { assigned = JSON.parse(campaign.world.draft("research/team", "[]")) } catch (error) { assigned = [] }
        if (!Array.isArray(assigned)) assigned = []
        chooseSource()
    }
    function chooseSource() {
        if (visibleSources.some(source => source.id === selectedSource)) return
        selectedSource = ""
        if (visibleSources.length) {
            selectedSource = visibleSources[0].id
            rate.text = visibleSources[0].free
        }
    }
    function toggleWorker(id) {
        assigned = assigned.includes(id) ? assigned.filter(value => value !== id) : assigned.concat([id])
        campaign.world.setDraft("research/team", JSON.stringify(assigned))
    }
    function statusName(status) {
        return ({ready:qsTr("Open"), creative:qsTr("Needs creativity"), locked:qsTr("Locked"), active:qsTr("Researching"), paused:qsTr("Paused"), complete:qsTr("Complete")})[status] || ""
    }
    onVisibleChanged: if (visible) Qt.callLater(restore)
    onVisibleSourcesChanged: if (visible) Qt.callLater(chooseSource)
    Connections {
        target: campaign.management
        function onChanged() { tree.updates++ }
    }
    Connections {
        target: campaign.world
        function onDraftsReset() { if (tree.visible) Qt.callLater(tree.restore) }
    }

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: tree.textSize
        color: "#c5dbce"
    }
    Copy { Layout.columnSpan: tree.columns; text: qsTr("Technology tree"); font.pixelSize: tree.textSize * 1.5; font.bold: true; color: "#f1e5c5" }
    Copy { Layout.columnSpan: tree.columns; text: qsTr("Follow the links from earlier discoveries. Solid links count individually, and one result satisfies a dashed group. Teams with enough distinct traits can start some topics before finishing those links.") }
    GameField {
        id: search
        Layout.columnSpan: tree.columns
        objectName: "technologySearch"
        Layout.fillWidth: true
        textSize: tree.textSize
        placeholderText: qsTr("Find a technology")
        Accessible.name: placeholderText
    }
    GameSelect {
        objectName: "technologyPicker"
        Layout.columnSpan: tree.columns
        Layout.fillWidth: true
        textSize: tree.textSize
        model: tree.nodes.filter(node => node.name.toLocaleLowerCase().includes(search.text.toLocaleLowerCase()))
        textRole: "name"
        valueRole: "id"
        currentIndex: model.findIndex(node => node.id === tree.selectedId)
        Accessible.name: qsTr("Select a technology from the tree")
        onActivated: tree.select(currentValue, true)
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: tree.textSize * 34
        Layout.preferredHeight: tree.textSize * (tree.columns === 2 ? 23 : 15)
        Layout.alignment: Qt.AlignTop
        color: "#11282d"
        border.color: "#536e65"
        radius: 8
        Flickable {
            id: graph
            objectName: "technologyGraph"
            anchors.fill: parent
            anchors.margins: 5
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: Math.max(width, tree.nodes.reduce((high,node) => Math.max(high, (node.level + 1) * tree.columnStep), 0))
            contentHeight: Math.max(height, tree.nodes.reduce((high,node) => Math.max(high, (node.slot + 1) * tree.rowStep), 0))
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
            onContentXChanged: links.requestPaint()
            onContentYChanged: links.requestPaint()
            Canvas {
                id: links
                x: graph.contentX
                y: graph.contentY
                width: graph.width
                height: graph.height
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: tree
                    function onNodesChanged() { links.requestPaint() }
                    function onSelectedIdChanged() { links.requestPaint() }
                }
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const nodes = new Map(tree.nodes.map(node => [node.id,node]))
                    function edge(parentId, node, optional) {
                        const source = nodes.get(parentId)
                        if (!source) return
                        const x1 = source.level * tree.columnStep + tree.textSize + tree.nodeWidth - graph.contentX
                        const x2 = node.level * tree.columnStep + tree.textSize - graph.contentX
                        const y1 = source.slot * tree.rowStep + tree.textSize + tree.nodeHeight / 2 - graph.contentY
                        const y2 = node.slot * tree.rowStep + tree.textSize + tree.nodeHeight / 2 - graph.contentY
                        if (x2 < 0 || x1 > width) return
                        const highlighted = tree.selectedId === node.id || tree.selectedId === parentId
                        ctx.beginPath()
                        ctx.strokeStyle = highlighted ? "#e7be79" : "#698b7f"
                        ctx.lineWidth = highlighted ? 2.5 : 1.4
                        ctx.setLineDash(optional ? [6,5] : [])
                        ctx.moveTo(x1,y1)
                        const bend = (x1+x2)/2
                        ctx.bezierCurveTo(bend,y1,bend,y2,x2,y2)
                        ctx.stroke()
                    }
                    for (const node of tree.nodes) {
                        for (const id of node.all) edge(id,node,false)
                        for (const id of node.any) edge(id,node,true)
                    }
                }
            }
            Repeater {
                model: tree.nodes
                WorldButton {
                    required property var modelData
                    objectName: "technology_" + modelData.id
                    x: modelData.level * tree.columnStep + tree.textSize
                    y: modelData.slot * tree.rowStep + tree.textSize
                    width: tree.nodeWidth
                    height: tree.nodeHeight
                    textSize: tree.textSize
                    text: modelData.name + "\n" + tree.statusName(modelData.status)
                    checked: tree.selectedId === modelData.id
                    primary: checked
                    onClicked: tree.select(modelData.id, false)
                    onActiveFocusChanged: if (activeFocus) {
                        graph.contentX = Math.max(0, Math.min(graph.contentWidth-graph.width, Math.min(x, Math.max(graph.contentX, x+width-graph.width))))
                        graph.contentY = Math.max(0, Math.min(graph.contentHeight-graph.height, Math.min(y, Math.max(graph.contentY, y+height-graph.height))))
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: tree.textSize * 27
        Layout.alignment: Qt.AlignTop
        spacing: 10
        Copy { text: tree.selected.name || ""; font.bold: true; font.pixelSize: tree.textSize * 1.25; color: "#f1e5c5" }
        Copy { text: qsTr("%1 staff-days and %2 CU of experiments. Staff work and compute settle separately.").arg(tree.selected.staffDays || 0).arg(tree.selected.experiment || "0") }
        Copy {
            visible: (tree.selected.equipment || []).length > 0
            text: qsTr("One equipped room needs: %1.").arg((tree.selected.equipment || []).join(", "))
        }
        Copy { visible: !!tree.selected.progress; text: tree.selected.progress || ""; color: "#ebc887" }
        Copy { visible: !!tree.selected.hold; text: tree.selected.hold || ""; color: "#f0c28a" }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            WorldButton { visible: !!tree.selected.active; text: qsTr("Pause project"); textSize: tree.textSize; enabled: tree.editable; onClicked: campaign.projectPause(tree.selected.project) }
            WorldButton { visible: !!tree.selected.paused; text: qsTr("Resume project"); textSize: tree.textSize; enabled: tree.editable && !tree.selected.hold; onClicked: campaign.projectResume(tree.selected.project) }
            WorldButton { visible: !!tree.selected.active || !!tree.selected.paused; text: qsTr("Cancel remaining work"); textSize: tree.textSize; enabled: tree.editable; onClicked: campaign.projectCancel(tree.selected.project) }
            WorldButton { visible: !!tree.selected.knowledge && !tree.selected.published; text: qsTr("Publish result"); textSize: tree.textSize; enabled: tree.editable; onClicked: campaign.management.publishResearch(tree.selectedId) }
            WorldButton { visible: !!tree.selected.canAdopt; text: qsTr("Adopt published findings"); textSize: tree.textSize; enabled: tree.editable; onClicked: campaign.management.acquirePublication(tree.selectedId) }
        }
        ColumnLayout {
            visible: !!tree.selected.name && !tree.selected.active && !tree.selected.paused && !tree.selected.completed
            Layout.fillWidth: true
            Copy { text: qsTr("Who will do the work? Use a powered desk computer or an issued laptop. Research laptops need a powered HQ rack."); font.bold: true }
            Copy {objectName:"researchPacingExplanation";visible:!!tree.teamPreview.explanation;text:tree.teamPreview.explanation || "";color:"#d9d3ae"}
            Copy {visible:!!tree.teamPreview.traits;text:tree.teamPreview.traits || "";font.pixelSize:tree.textSize*0.85}
            Copy { visible: !tree.workers.length; text: qsTr("There are no research-capable people in the company. Recruit someone in People.") }
            Flow {
                Layout.fillWidth: true
                spacing: 7
                Repeater {
                    model: tree.workers
                    WorldButton {
                        required property var modelData
                        objectName: "researcher_" + modelData.id
                        width: Math.min(implicitWidth, parent.width)
                        textSize: tree.textSize
                        text: qsTr("%1 · Research %2").arg(modelData.name).arg(modelData.skill)
                            + (modelData.blocker ? "\n" + modelData.blocker : "")
                        checked: tree.assigned.includes(modelData.id)
                        enabled: !modelData.blocker || checked
                        onClicked: tree.toggleWorker(modelData.id)
                    }
                }
            }
            Copy { text: qsTr("Compute source"); font.bold: true }
            GameSelect {
                objectName: "researchSource"
                Layout.fillWidth: true
                textSize: tree.textSize
                model: tree.visibleSources
                textRole: "name"
                valueRole: "id"
                currentIndex: model.findIndex(row => row.id === tree.selectedSource)
                Accessible.name: qsTr("Working research compute source")
                onActivated: {
                    tree.selectedSource = currentValue
                    rate.text = model[currentIndex].free
                    campaign.world.setDraft("research/source", tree.selectedSource)
                    campaign.world.setDraft("research/rate", rate.text)
                }
            }
            Copy { text: qsTr("CU/day to reserve") }
            GameField {
                id: rate
                objectName: "researchComputeRate"
                Layout.fillWidth: true
                textSize: tree.textSize
                maximumLength: 13
                Accessible.name: qsTr("Research compute reservation in CU per day")
                onTextEdited: campaign.world.setDraft("research/rate", text)
            }
            Copy { visible: !!tree.blocker; text: tree.blocker; color: "#ebc887" }
            WorldButton {
                objectName: "startGroundedResearch"
                text: qsTr("Start research")
                textSize: tree.textSize
                primary: true
                enabled: tree.editable && !tree.blocker
                onClicked: campaign.management.beginGroundedResearch(tree.selectedId, tree.selectedSource, rate.text, tree.assigned)
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: tree.connected
                WorldButton {
                    required property var modelData
                    textSize: tree.textSize
                    text: (tree.selectedNode.all || []).includes(modelData.id) ? qsTr("Requires · %1").arg(modelData.name)
                        : (tree.selectedNode.any || []).includes(modelData.id) ? qsTr("Alternative · %1").arg(modelData.name)
                        : qsTr("Leads to · %1").arg(modelData.name)
                    onClicked: tree.select(modelData.id, true)
                }
            }
        }
        Copy { text: tree.selected.description || "" }
        WorldButton { text: qsTr("Equipment suppliers"); textSize: tree.textSize; onClicked: tree.suppliersRequested() }
        Repeater {
            model: campaign.management.researchRentals
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Copy { text: modelData.name; font.bold: true }
                Copy { text: modelData.terms }
                WorldButton {
                    objectName: "rentCompute_" + modelData.id
                    text: qsTr("Sign server rental")
                    textSize: tree.textSize
                    enabled: tree.editable && modelData.eligible
                    onClicked: campaign.management.acceptOffer(modelData.id)
                }
            }
        }
    }
}
