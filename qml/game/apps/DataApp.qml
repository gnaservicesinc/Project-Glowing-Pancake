import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: root
    objectName: "dataAppRoot"
    property int textSize: 14
    property var manager: campaign.management
    readonly property bool editable: campaign.paused && !campaign.busy && campaign.companyEditable
    property string sourceFilter: "all"
    property string activeTab: "catalog"
    property var selectedCreativeWorkers: []
    readonly property var selectedFields: (root.manager.dataOffers.selectedRow && root.manager.dataOffers.selectedRow.fields) ? root.manager.dataOffers.selectedRow.fields : ({})
    spacing: 10

    function form() {
        return {tab: activeTab, filter: sourceFilter, operation: procPanel.operation, processingSource: procPanel.sourceId,
            processingRate: procPanel.rateText, staff: procPanel.staffText, domain: creativePanel.domain, units: creativePanel.unitsText,
            creativeSource: creativePanel.sourceId, creativeRate: creativePanel.rateText, workers: selectedCreativeWorkers}
    }
    onSourceFilterChanged: manager.dataOffers.filterField(sourceFilter === "all" ? "" : "sourcePath", sourceFilter)
    DraftState {
        id: settings
        draftKey: "data/planning"
        onRestored: value => {
            root.activeTab = value.tab
            root.sourceFilter = value.filter
            procPanel.operation = value.operation
            procPanel.sourceId = value.processingSource
            procPanel.rateText = value.processingRate
            procPanel.staffText = value.staff
            creativePanel.domain = value.domain
            creativePanel.unitsText = value.units
            creativePanel.sourceId = value.creativeSource
            creativePanel.rateText = value.creativeRate
            root.selectedCreativeWorkers = value.workers
        }
    }

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: root.textSize
        color: "#d9e6df"
    }

    // Sub-tab bar: Catalog & Provenance | Process & Clean | In-House Creative Studio
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: [
                {id: "catalog", label: qsTr("Catalog & licenses")},
                {id: "collect", label: qsTr("Build a dataset")},
                {id: "process", label: qsTr("Process & Clean Data")},
                {id: "creative", label: qsTr("In-House Creative Studio")}
            ]
            WorldButton {
                required property var modelData
                visible: modelData.id !== "collect" || root.manager.datasetSources.length > 0
                objectName: "dataAppTab_" + modelData.id
                text: modelData.label
                textSize: root.textSize * 0.9
                checked: root.activeTab === modelData.id
                onClicked: { root.activeTab = modelData.id; settings.remember(root.form()) }
            }
        }
    }

    // Source-path filter pills (visible on Catalog tab)
    Flow {
        visible: root.activeTab === "catalog"
        Layout.fillWidth: true
        spacing: 5
        Repeater {
            model: [
                {id: "all", label: qsTr("All sources")},
                {id: "licensed", label: qsTr("Licensed")},
                {id: "public_dataset", label: qsTr("Public")},
                {id: "direct_scraping", label: qsTr("Direct scraping")},
                {id: "torrent_distributed", label: qsTr("Distributed collection")},
                {id: "company_created", label: qsTr("Company-created")},
                {id: "in_house_creative", label: qsTr("In-house creative")},
                {id: "user_feedback", label: qsTr("User feedback")},
                {id: "synthetic_teacher", label: qsTr("Synthetic")}
            ]
            WorldButton {
                required property var modelData
                objectName: "dataSourceFilter_" + modelData.id
                text: modelData.label
                textSize: root.textSize * 0.8
                padding: 5
                checked: root.sourceFilter === modelData.id
                onClicked: {
                    root.sourceFilter = modelData.id
                    settings.remember(root.form())
                }
            }
        }
    }

    WorldButton {
        visible: root.activeTab === "catalog" && !!root.selectedFields.parentDatasetId
        text: qsTr("Inspect source corpus · %1").arg(root.selectedFields.parentDatasetId || "")
        textSize: root.textSize
        onClicked: {
            const parentId = root.selectedFields.parentDatasetId
            root.sourceFilter = "all"
            root.manager.dataOffers.filterText = ""
            root.manager.dataOffers.selectedId = parentId
        }
    }

    // Selected Dataset Provenance & Quality Inspector Card
    Frame {
        visible: root.activeTab === "catalog" && !!(root.manager.dataOffers.selectedRow && root.manager.dataOffers.selectedRow.title)
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#3d5a70" }
        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Copy {
                    Layout.fillWidth: true
                    text: (root.manager.dataOffers.selectedRow ? root.manager.dataOffers.selectedRow.title : "") +
                        (root.selectedFields.sourcePath ? "  [" + root.selectedFields.sourcePath + "]" : "")
                    font.bold: true
                    color: "#f5e7c6"
                }
                Rectangle {
                    radius: 6
                    color: root.selectedFields.controversyRisk ? "#5a2323" : "#1f4734"
                    border.color: root.selectedFields.controversyRisk ? "#d66b6b" : "#63b88c"
                    implicitWidth: riskBadge.implicitWidth + 12
                    implicitHeight: riskBadge.implicitHeight + 6
                    Label {
                        id: riskBadge
                        anchors.centerIn: parent
                        text: root.selectedFields.controversyRisk ? qsTr("High Controversy Risk") : qsTr("Clean Provenance")
                        textFormat: Text.PlainText
                        font.pixelSize: root.textSize * 0.78
                        font.bold: true
                        color: root.selectedFields.controversyRisk ? "#ffd0d0" : "#c8f2da"
                    }
                }
            }
            Copy {
                text: qsTr("Quality %1 · Duplication %2 · Contamination %3 · Provenance confidence %4\nCommercial deployment: %5 · Open-weight release: %6")
                    .arg(root.selectedFields.qualityPct || "—")
                    .arg(root.selectedFields.duplicationPct || "0%")
                    .arg(root.selectedFields.contaminationPct || "0%")
                    .arg(root.selectedFields.provenanceConfidencePct || "100%")
                    .arg(root.selectedFields.commercialUse ? qsTr("Permitted") : qsTr("Prohibited"))
                    .arg(root.selectedFields.openWeightRelease ? qsTr("Permitted") : qsTr("Restricted"))
                font.pixelSize: root.textSize * 0.88
                color: "#b8d6ca"
            }
            Copy {
                visible: !!(root.selectedFields.supplier || root.selectedFields.history)
                text: (root.selectedFields.supplier ? qsTr("Supplier / Publisher: %1").arg(root.selectedFields.supplier) : "") +
                    (root.selectedFields.history ? ("\n" + root.selectedFields.history) : "")
                font.pixelSize: root.textSize * 0.84
                color: "#9db8c4"
            }
        }
    }

    // Data Processing Pipeline Tab
    Frame {
        visible: root.activeTab === "process"
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#3d5a70" }
        contentItem: ColumnLayout {
            id: procPanel
            spacing: 8
            property string operation: "deduplicate"
            property string sourceId: root.manager.sources.length > 0 ? root.manager.sources[0].id : ""
            property string rateText: "15"
            property string staffText: "1"
            readonly property var quote: root.manager.dataProcessingQuote(
                root.manager.dataOffers.selectedId || "",
                procPanel.operation,
                procPanel.sourceId,
                procPanel.rateText,
                procPanel.staffText
            )

            Copy {
                text: qsTr("Data Processing & Curation Pipeline")
                font.bold: true
                color: "#ffd897"
            }
            Copy {
                text: qsTr("Trade compute, staff time, cash, and raw record volume for higher effective quality, lower duplication, and lower benchmark contamination. Processing never launders missing commercial rights or unconsented provenance.")
                font.pixelSize: root.textSize * 0.88
            }
            Copy {
                text: qsTr("Selected source dataset: %1").arg(root.manager.dataOffers.selectedRow.title || qsTr("Select an owned dataset in the list below"))
                font.bold: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [
                        {id: "deduplicate", label: qsTr("Deduplicate")},
                        {id: "filter", label: qsTr("Quality Filter")},
                        {id: "correct", label: qsTr("Clean & Correct")},
                        {id: "label", label: qsTr("Annotate & Label")},
                        {id: "balance", label: qsTr("Domain Balance")},
                        {id: "review", label: qsTr("Provenance Audit")}
                    ]
                    WorldButton {
                        required property var modelData
                        objectName: "processOp_" + modelData.id
                        text: modelData.label
                        textSize: root.textSize * 0.84
                        padding: 6
                        checked: procPanel.operation === modelData.id
                        onClicked: { procPanel.operation = modelData.id; settings.remember(root.form()) }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    Copy { text: qsTr("Compute source"); font.pixelSize: root.textSize * 0.85 }
                    GameSelect {
                        objectName: "processComputeSource"
                        Layout.fillWidth: true
                        textSize: root.textSize * 0.88
                        model: root.manager.sources
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: Math.max(0, model.findIndex(r => r.id === procPanel.sourceId))
                        onActivated: { if (currentIndex >= 0) procPanel.sourceId = model[currentIndex].id; settings.remember(root.form()) }
                    }
                }
                ColumnLayout {
                    Copy { text: qsTr("CU/day"); font.pixelSize: root.textSize * 0.85 }
                    GameField {
                        objectName: "processRateField"
                        textSize: root.textSize * 0.88
                        text: procPanel.rateText
                        onTextEdited: { procPanel.rateText = text; settings.remember(root.form()) }
                    }
                }
                ColumnLayout {
                    Copy { text: qsTr("Staff"); font.pixelSize: root.textSize * 0.85 }
                    GameField {
                        objectName: "processStaffField"
                        textSize: root.textSize * 0.88
                        text: procPanel.staffText
                        onTextEdited: { procPanel.staffText = text; settings.remember(root.form()) }
                    }
                }
            }
            Copy {
                objectName: "dataProcessingPreviewText"
                text: procPanel.quote.detail || qsTr("Select an owned dataset and compute source to preview processing.")
                font.pixelSize: root.textSize * 0.86
                color: "#dfd7b2"
            }
            WorldButton {
                objectName: "startDataProcessingButton"
                text: qsTr("Start data processing job")
                textSize: root.textSize
                primary: true
                enabled: root.editable && procPanel.quote.ready === true
                onClicked: root.manager.processDataset(
                    root.manager.dataOffers.selectedId,
                    procPanel.operation,
                    procPanel.sourceId,
                    procPanel.rateText,
                    procPanel.staffText
                )
            }
            Repeater {
                model: root.manager.processedDatasets
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8
                    Copy {
                        Layout.fillWidth: true
                        text: modelData.id + " (" + modelData.operation + ") · " + modelData.status + "\n" + modelData.detail
                        font.pixelSize: root.textSize * 0.85
                    }
                    WorldButton {
                        visible: !!modelData.active
                        text: qsTr("Pause")
                        textSize: root.textSize * 0.82
                        enabled: root.editable
                        onClicked: campaign.projectPause(modelData.projectId)
                    }
                    WorldButton {
                        visible: !!modelData.paused
                        text: qsTr("Resume")
                        textSize: root.textSize * 0.82
                        enabled: root.editable
                        onClicked: campaign.projectResume(modelData.projectId)
                    }
                }
            }
        }
    }

    // In-House Creative Studio Tab
    Frame {
        visible: root.activeTab === "creative"
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#3d5a70" }
        contentItem: ColumnLayout {
            id: creativePanel
            spacing: 8
            property string domain: "documents"
            property string unitsText: "25000"
            property string sourceId: root.manager.sources.length > 0 ? root.manager.sources[0].id : ""
            property string rateText: "10"
            readonly property var quote: root.manager.creativeProductionQuote(
                creativePanel.domain,
                creativePanel.unitsText,
                creativePanel.sourceId,
                creativePanel.rateText,
                root.selectedCreativeWorkers
            )

            Copy {
                text: qsTr("In-House Creative Team & Work-for-Hire Data Studio")
                font.bold: true
                color: "#ffd897"
            }
            Copy {
                text: qsTr("Assign Data Creative specialists and researchers to author original work-for-hire datasets with 100% verified provenance, full commercial & open-weight rights, and zero third-party controversy. Quality scales with team creative skill (never automatic 100%).")
                font.pixelSize: root.textSize * 0.88
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [
                        {id: "documents", label: qsTr("Documents & Prose")},
                        {id: "structured_tables", label: qsTr("Structured Tables")},
                        {id: "code_verification", label: qsTr("Code & Verification")}
                    ]
                    WorldButton {
                        required property var modelData
                        objectName: "creativeDomain_" + modelData.id
                        text: modelData.label
                        textSize: root.textSize * 0.84
                        padding: 6
                        checked: creativePanel.domain === modelData.id
                        onClicked: { creativePanel.domain = modelData.id; settings.remember(root.form()) }
                    }
                }
            }
            Copy { text: qsTr("Select team members (Creative skill scales dataset quality)"); font.bold: true; font.pixelSize: root.textSize * 0.88 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Repeater {
                    model: root.manager.creativeWorkers
                    AbstractButton {
                        id: cwCard
                        required property var modelData
                        objectName: "creativeWorker_" + modelData.id
                        Layout.fillWidth: true
                        implicitHeight: cwRow.implicitHeight + 12
                        checked: root.selectedCreativeWorkers.includes(modelData.id)
                        enabled: modelData.available || checked
                        onClicked: {
                            if (root.selectedCreativeWorkers.includes(modelData.id))
                                root.selectedCreativeWorkers = root.selectedCreativeWorkers.filter(x => x !== modelData.id)
                            else
                                root.selectedCreativeWorkers = root.selectedCreativeWorkers.concat([modelData.id])
                        }
                        background: Rectangle {
                            radius: 6
                            color: cwCard.checked ? "#2a4e5e" : "#192d3b"
                            border.color: cwCard.checked ? "#efc389" : "#3a566a"
                        }
                        contentItem: RowLayout {
                            id: cwRow
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                text: (cwCard.checked ? "☑ " : "☐ ") + cwCard.modelData.name + " (" + cwCard.modelData.role + ")"
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                font.pixelSize: root.textSize * 0.88
                                font.bold: true
                                color: "#f3e9d2"
                            }
                            Label {
                                text: qsTr("Creative %1").arg(cwCard.modelData.creativeSkill || 0)
                                textFormat: Text.PlainText
                                font.pixelSize: root.textSize * 0.85
                                font.bold: true
                                color: "#9ae3be"
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout {
                    Copy { text: qsTr("Target records"); font.pixelSize: root.textSize * 0.85 }
                    GameField {
                        objectName: "creativeUnitsField"
                        textSize: root.textSize * 0.88
                        text: creativePanel.unitsText
                        onTextEdited: { creativePanel.unitsText = text; settings.remember(root.form()) }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Copy { text: qsTr("Compute source"); font.pixelSize: root.textSize * 0.85 }
                    GameSelect {
                        objectName: "creativeComputeSource"
                        Layout.fillWidth: true
                        textSize: root.textSize * 0.88
                        model: root.manager.sources
                        textRole: "label"
                        valueRole: "id"
                        currentIndex: Math.max(0, model.findIndex(r => r.id === creativePanel.sourceId))
                        onActivated: { if (currentIndex >= 0) creativePanel.sourceId = model[currentIndex].id; settings.remember(root.form()) }
                    }
                }
                ColumnLayout {
                    Copy { text: qsTr("CU/day"); font.pixelSize: root.textSize * 0.85 }
                    GameField {
                        objectName: "creativeRateField"
                        textSize: root.textSize * 0.88
                        text: creativePanel.rateText
                        onTextEdited: { creativePanel.rateText = text; settings.remember(root.form()) }
                    }
                }
            }
            Copy {
                objectName: "creativeProductionPreviewText"
                text: creativePanel.quote.detail || qsTr("Assign one or more team members and a compute source to preview in-house creative production.")
                font.pixelSize: root.textSize * 0.86
                color: "#dfd7b2"
            }
            WorldButton {
                objectName: "startCreativeProductionButton"
                text: qsTr("Start in-house creative dataset")
                textSize: root.textSize
                primary: true
                enabled: root.editable && creativePanel.quote.ready === true
                onClicked: root.manager.startCreativeProduction(
                    creativePanel.domain,
                    creativePanel.unitsText,
                    creativePanel.sourceId,
                    creativePanel.rateText,
                    root.selectedCreativeWorkers
                )
            }
            Repeater {
                model: root.manager.creativeJobs
                Copy {
                    required property var modelData
                    text: modelData.id + " · " + modelData.status + " — " + modelData.detail
                    font.pixelSize: root.textSize * 0.85
                }
            }
        }
    }
}
