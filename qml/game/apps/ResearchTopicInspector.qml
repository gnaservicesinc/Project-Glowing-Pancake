import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: insp
    objectName: "researchTopicInspector"
    property int textSize: 14
    property bool editable: false
    property string selectedId: ""
    property var selected: ({})
    property var workers: []
    property var assigned: []
    property var visibleSources: []
    property string selectedSource: ""
    property string computeRateText: ""
    signal datasetToggled(string id)
    property var chosenData: []
    property var teamPreview: ({})
    property bool quoteChanged: false
    property string blocker: ""
    property var rentals: []
    property bool showTechnical: false

    signal topicJumpRequested(string id)
    signal workerToggled(string id)
    signal sourceChosen(string sourceId, string defaultRate)
    signal rateEdited(string rateText)
    signal startRequested()
    signal suppliersRequested()
    signal rentalRequested(string offerId)

    spacing: 10

    component SectionCard: Frame {
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#385063" }
    }
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: insp.textSize
        color: "#d4e3dc"
    }

    // Step 1: What the team will learn
    SectionCard {
        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: insp.selected.name || qsTr("Select a research topic")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: insp.textSize * 1.25
                    font.bold: true
                    color: "#f4e6c4"
                }
                Rectangle {
                    visible: !!(insp.selected.domainLabel)
                    radius: 8
                    color: "#223b4e"
                    border.color: "#4f738b"
                    implicitWidth: domLabel.implicitWidth + 12
                    implicitHeight: domLabel.implicitHeight + 6
                    Label {
                        id: domLabel
                        anchors.centerIn: parent
                        text: insp.selected.domainLabel || ""
                        textFormat: Text.PlainText
                        font.pixelSize: insp.textSize * 0.78
                        color: "#c9e0ee"
                    }
                }
            }
            Copy {
                objectName: "innovationLevel"
                visible: !!insp.selected.innovation
                text: qsTr("Innovation · level %1").arg(insp.selected.innovationLevel || 0)
                color: "#ebc887"
                font.bold: true
            }
            Copy {
                visible: !!(insp.selected.pocSpec && insp.selected.pocSpec.producesPoc)
                text: qsTr("Proof-of-Concept Prototype · produces %1 (%2)").arg(insp.selected.pocSpec ? insp.selected.pocSpec.methodLabel : "").arg(insp.selected.pocSpec ? insp.selected.pocSpec.taskScope : "")
                color: "#9ae3be"
                font.bold: true
            }
            Copy {
                visible: !!(insp.selected.licensed)
                text: qsTr("Commercial technology license active · satisfies prerequisites without internal discovery credit")
                color: "#8cd6f5"
                font.bold: true
            }
            Copy {
                text: qsTr("1. What the team will learn")
                font.pixelSize: insp.textSize * 0.85
                font.bold: true
                color: "#98c2b7"
            }
            Copy {
                visible: !!(insp.selected.description)
                text: insp.selected.description || ""
            }
            Copy {
                visible: !!(insp.selected.effects)
                text: insp.selected.effects || ""
                color: "#b7dfc7"
            }
        }
    }

    // Step 2: Concrete examples (clickable!)
    SectionCard {
        visible: (insp.selected.examples || []).length > 0 || (insp.selected.opportunities || []).length > 0
        contentItem: ColumnLayout {
            spacing: 6
            Copy {
                text: qsTr("2. Concrete examples & what this unlocks")
                font.pixelSize: insp.textSize * 0.85
                font.bold: true
                color: "#98c2b7"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                Repeater {
                    model: insp.selected.examples || []
                    AbstractButton {
                        id: exCard
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: exLabel.implicitHeight + 12
                        enabled: modelData.actionApp === "research" && !!modelData.targetId
                        hoverEnabled: true
                        onClicked: if (modelData.actionApp === "research" && modelData.targetId) insp.topicJumpRequested(modelData.targetId)
                        background: Rectangle {
                            radius: 6
                            color: exCard.enabled && exCard.hovered ? "#254052" : "#1a3040"
                            border.color: exCard.enabled ? "#577e94" : "#344c5d"
                        }
                        contentItem: Label {
                            id: exLabel
                            anchors.fill: parent
                            anchors.margins: 6
                            text: exCard.modelData.label + (exCard.enabled ? " →" : "")
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            font.pixelSize: insp.textSize * 0.9
                            color: exCard.enabled ? "#d6ecdf" : "#c0d3cc"
                        }
                    }
                }
            }
        }
    }

    // Step 3: What is needed or missing
    SectionCard {
        visible: (insp.selected.missingChecklist || []).length > 0 || !!(insp.selected.blocker)
        contentItem: ColumnLayout {
            spacing: 6
            Copy {
                text: qsTr("3. What is needed or missing")
                font.pixelSize: insp.textSize * 0.85
                font.bold: true
                color: "#98c2b7"
            }
            Copy {
                visible: !!(insp.selected.blocker)
                text: insp.selected.blocker || ""
                color: "#f0c68c"
                font.bold: true
            }
            Repeater {
                model: insp.selected.missingChecklist || []
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: modelData.met ? "✓" : "○"
                        font.pixelSize: insp.textSize
                        font.bold: true
                        color: modelData.met ? "#88d4a2" : "#f0b97e"
                    }
                    Copy {
                        Layout.fillWidth: true
                        text: modelData.label || ""
                        color: modelData.met ? "#bfe0cc" : "#f4d7b2"
                    }
                    WorldButton {
                        visible: !modelData.met && (modelData.kind === "research" && !!modelData.targetId || modelData.kind === "equipment")
                        text: modelData.kind === "equipment" ? qsTr("Suppliers") : qsTr("Inspect")
                        textSize: insp.textSize * 0.8
                        padding: 5
                        onClicked: {
                            if (modelData.kind === "equipment") insp.suppliersRequested()
                            else if (modelData.targetId) insp.topicJumpRequested(modelData.targetId)
                        }
                    }
                }
            }
        }
    }

    // Step 4: Cost, effort & active project actions
    SectionCard {
        visible: !!(insp.selected.name)
        contentItem: ColumnLayout {
            spacing: 6
            Copy {
                text: qsTr("4. Cost, effort & status")
                font.pixelSize: insp.textSize * 0.85
                font.bold: true
                color: "#98c2b7"
            }
            Copy {
                text: qsTr("%1 staff-days · %2 CU of experiments").arg(insp.selected.staffDays || 0).arg(insp.selected.experiment || "0") +
                    (insp.selected.fee && !insp.selected.completed && !insp.selected.active && !insp.selected.paused
                        ? qsTr(" · Project fee %1").arg(insp.selected.fee) : "")
            }
            Copy { visible: !!(insp.selected.progress); text: insp.selected.progress || ""; color: "#ebc887" }
            Copy { visible: !!(insp.selected.hold); text: insp.selected.hold || ""; color: "#f0b882" }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                WorldButton { objectName: "reviewNextInnovation"; visible: !!insp.selected.completed && !!insp.selected.nextInnovation; text: qsTr("Review next innovation"); textSize: insp.textSize; onClicked: insp.topicJumpRequested(insp.selected.nextInnovation) }
                WorldButton { visible: !!insp.selected.active; text: qsTr("Pause project"); textSize: insp.textSize; enabled: insp.editable; onClicked: campaign.projectPause(insp.selected.project) }
                WorldButton {
                    objectName: "completePlaytestResearch"
                    visible: typeof godMode !== "undefined" && godMode && (!!insp.selected.active || !!insp.selected.paused)
                    text: qsTr("Complete research now")
                    textSize: insp.textSize
                    enabled: insp.editable && !campaign.busy
                    onClicked: campaign.usePlaytestTool("research", insp.selectedId)
                }
                WorldButton { visible: !!insp.selected.paused; text: qsTr("Resume project"); textSize: insp.textSize; enabled: insp.editable && !insp.selected.hold; onClicked: campaign.projectResume(insp.selected.project) }
                WorldButton { visible: !!insp.selected.active || !!insp.selected.paused; text: qsTr("Cancel remaining work"); textSize: insp.textSize; enabled: insp.editable; onClicked: campaign.projectCancel(insp.selected.project) }
                WorldButton { visible: !!insp.selected.knowledge && !insp.selected.published; text: qsTr("Publish result"); textSize: insp.textSize; enabled: insp.editable; onClicked: campaign.management.publishResearch(insp.selectedId) }
                WorldButton { visible: !!insp.selected.canAdopt; text: qsTr("Adopt published findings"); textSize: insp.textSize; enabled: insp.editable; onClicked: campaign.management.acquirePublication(insp.selectedId) }
            }
            Rectangle {
                Layout.fillWidth: true
                visible: !!(insp.selected.licenseOffer && insp.selected.licenseOffer.available)
                implicitHeight: licCol.implicitHeight + 14
                radius: 6
                color: "#132332"
                border.color: "#4d7894"
                ColumnLayout {
                    id: licCol
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 4
                    Copy {
                        text: qsTr("Commercial technology license · %1").arg(insp.selected.licenseOffer ? (insp.selected.licenseOffer.licensor || "") : "")
                        font.bold: true
                        color: "#a7dbf2"
                    }
                    Copy {
                        text: insp.selected.licenseOffer
                            ? qsTr("%1 upfront · %2/month royalty · %3-day term · %4-day integration window.\n%5\nGrants technology access for dependent research & recipes without internal discovery or publication credit.")
                                  .arg(insp.selected.licenseOffer.upfrontFee || "$0")
                                  .arg(insp.selected.licenseOffer.monthlyRoyalty || "$0")
                                  .arg(insp.selected.licenseOffer.termDays || 365)
                                  .arg(insp.selected.licenseOffer.integrationDays || 0)
                                  .arg(insp.selected.licenseOffer.scopeLabel || "")
                            : ""
                        font.pixelSize: insp.textSize * 0.86
                    }
                    Copy {
                        visible: !!(insp.selected.licenseOffer && (insp.selected.licenseOffer.active || insp.selected.licenseOffer.integrating || insp.selected.licenseOffer.expired))
                        text: insp.selected.licenseOffer && insp.selected.licenseOffer.integrating
                            ? qsTr("Integrating · ready on %1 · expires before %2").arg(insp.selected.licenseOffer.readyOn).arg(insp.selected.licenseOffer.expiresBefore)
                            : insp.selected.licenseOffer && insp.selected.licenseOffer.active
                            ? qsTr("Active license · ready since %1 · expires before %2").arg(insp.selected.licenseOffer.readyOn).arg(insp.selected.licenseOffer.expiresBefore)
                            : qsTr("License expired on %1 · acquire a new term or complete internal R&D").arg(insp.selected.licenseOffer ? insp.selected.licenseOffer.expiresBefore : "")
                        color: "#ffd897"
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 8
                        WorldButton {
                            objectName: "licenseTechButton"
                            visible: !!(insp.selected.licenseOffer && !insp.selected.licenseOffer.active && !insp.selected.licenseOffer.integrating && !insp.selected.completed)
                            text: qsTr("License technology (%1)").arg(insp.selected.licenseOffer ? (insp.selected.licenseOffer.upfrontFee || "") : "")
                            textSize: insp.textSize * 0.86
                            enabled: insp.editable && !!(insp.selected.licenseOffer && insp.selected.licenseOffer.canLicense)
                            onClicked: campaign.management.licenseTechnology(insp.selected.licenseOffer.offerId)
                        }
                        WorldButton {
                            objectName: "renewTechLicenseButton"
                            visible: !!(insp.selected.licenseOffer && insp.selected.licenseOffer.active)
                            text: qsTr("Renew technology license")
                            textSize: insp.textSize * 0.86
                            enabled: insp.editable && !!(insp.selected.licenseOffer && insp.selected.licenseOffer.canRenew)
                            onClicked: campaign.management.renewTechLicense(insp.selected.licenseOffer.offerId)
                        }
                    }
                }
            }
        }
    }

    // Step 5: Team & Start (full-width dynamically sized non-overlapping worker cards!)
    SectionCard {
        visible: !!(insp.selected.name) && !insp.selected.active && !insp.selected.paused && !insp.selected.completed
        contentItem: ColumnLayout {
            spacing: 8
            Copy {
                text: qsTr("5. Assign team, compute & start")
                font.pixelSize: insp.textSize * 0.85
                font.bold: true
                color: "#98c2b7"
            }
            Copy {
                visible: !insp.workers.length
                text: qsTr("There are no research-capable people in the company yet. Recruit someone in People.")
            }

            // Full-width dynamically sized worker cards in a strict ColumnLayout — zero overlap at any text scale!
            ColumnLayout {
                id: workerCardsList
                objectName: "researchWorkerCards"
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: insp.workers
                    AbstractButton {
                        id: workerCard
                        required property var modelData
                        objectName: "researcher_" + modelData.id
                        Layout.fillWidth: true
                        implicitHeight: Math.max(44, workerRow.implicitHeight + 16)
                        checked: insp.assigned.includes(modelData.id)
                        enabled: !modelData.blocker || checked
                        hoverEnabled: true
                        activeFocusOnTab: true
                        Accessible.name: qsTr("%1 · Research %2").arg(modelData.name).arg(modelData.skill) +
                            (modelData.blocker ? " · " + modelData.blocker : "")
                        onClicked: insp.workerToggled(modelData.id)
                        background: Rectangle {
                            radius: 7
                            color: workerCard.checked ? "#2d5161" : !workerCard.enabled ? "#13202b" : workerCard.hovered ? "#22394a" : "#1a2f3f"
                            border.width: workerCard.activeFocus || workerCard.checked ? 2 : 1
                            border.color: workerCard.activeFocus ? "#ffffff" : workerCard.checked ? "#efc389" : workerCard.enabled ? "#4a6b7f" : "#304352"
                        }
                        contentItem: ColumnLayout {
                            id: workerRow
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    Layout.fillWidth: true
                                    text: (workerCard.checked ? "☑ " : "☐ ") + (workerCard.modelData.name || "")
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    font.pixelSize: insp.textSize
                                    font.bold: true
                                    color: workerCard.enabled ? "#f4ecdc" : "#8ea2af"
                                }
                                Label {
                                    text: qsTr("Research %1").arg(workerCard.modelData.skill || 0)
                                    textFormat: Text.PlainText
                                    font.pixelSize: insp.textSize * 0.88
                                    font.bold: true
                                    color: workerCard.checked ? "#ffd897" : "#9bd1be"
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: workerCard.modelData.blocker
                                    ? workerCard.modelData.blocker
                                    : qsTr("%1 · Ready for assignment").arg(workerCard.modelData.role || qsTr("Researcher"))
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                font.pixelSize: insp.textSize * 0.84
                                color: workerCard.modelData.blocker ? "#e9b987" : "#a9c8bd"
                            }
                        }
                    }
                }
            }

            // Collapsible Technical rules & creativity math (automatically shown when team selected)
            WorldButton {
                visible: !!(insp.teamPreview && insp.teamPreview.explanation)
                text: (insp.showTechnical || insp.assigned.length > 0) ? qsTr("Hide technical rules & creativity math") : qsTr("Show technical rules & creativity math")
                textSize: insp.textSize * 0.82
                quiet: true
                padding: 5
                onClicked: insp.showTechnical = !insp.showTechnical
            }
            Rectangle {
                Layout.fillWidth: true
                visible: !!(insp.teamPreview && insp.teamPreview.explanation) && (insp.showTechnical || insp.assigned.length > 0)
                implicitHeight: techCol.implicitHeight + 14
                radius: 6
                color: "#11202c"
                border.color: "#3d5669"
                ColumnLayout {
                    id: techCol
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 4
                    Copy {
                        objectName: "researchPacingExplanation"
                        text: insp.teamPreview.explanation || ""
                        color: "#dfd7b2"
                        font.pixelSize: insp.textSize * 0.88
                    }
                    Copy {
                        visible: !!(insp.teamPreview.traits)
                        text: qsTr("Distinct team traits: %1").arg(insp.teamPreview.traits || "")
                        font.pixelSize: insp.textSize * 0.82
                        color: "#abc2cc"
                    }
                }
            }

            Copy { text: qsTr("Compute source"); font.bold: true }
            GameSelect {
                objectName: "researchSource"
                Layout.fillWidth: true
                textSize: insp.textSize
                model: insp.visibleSources
                textRole: "name"
                valueRole: "id"
                currentIndex: model.findIndex(row => row.id === insp.selectedSource)
                Accessible.name: qsTr("Working research compute source")
                onActivated: if (currentIndex >= 0) insp.sourceChosen(currentValue, model[currentIndex].free)
            }
            Copy { text: qsTr("CU/day to reserve") }
            GameField {
                id: rateField
                objectName: "researchComputeRate"
                Layout.fillWidth: true
                textSize: insp.textSize
                text: insp.computeRateText
                maximumLength: 13
                Accessible.name: qsTr("Research compute reservation in CU per day")
                onTextEdited: insp.rateEdited(text)
            }
            Copy { visible: insp.quoteChanged; text: qsTr("The project terms have changed. Review the updated team and compute costs, then start again to confirm."); color: "#ebc887" }
            Copy { visible: !!(insp.blocker); text: insp.blocker; color: "#ebc887" }
            Copy {
                visible: !!insp.selected.dataRequirement?.required
                text: qsTr("Research inputs · %1").arg(insp.selected.dataRequirement?.domainHint || qsTr("any domain"))
                font.bold: true
            }
            Repeater {
                model: insp.selected.dataRequirement?.required ? insp.selected.dataRequirement.eligibleDatasets : []
                GameCheck {
                    required property var modelData
                    Layout.fillWidth: true
                    text: qsTr("%1 · %2 usable records").arg(modelData.name).arg(modelData.uniqueUnits)
                    checked: insp.chosenData.includes(modelData.id)
                    onToggled: insp.datasetToggled(modelData.id)
                }
            }
            WorldButton {
                objectName: "startGroundedResearch"
                text: insp.selected.innovation ? qsTr("Start innovation") : qsTr("Start research")
                textSize: insp.textSize
                primary: true
                enabled: insp.editable && !insp.blocker
                onClicked: insp.startRequested()
            }
        }
    }

    // Compute rentals & equipment suppliers shortcuts
    SectionCard {
        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Copy { text: qsTr("Equipment & compute rentals"); font.bold: true; color: "#f0e3c3" }
                WorldButton { text: qsTr("Equipment suppliers"); textSize: insp.textSize * 0.86; padding: 6; onClicked: insp.suppliersRequested() }
            }
            Repeater {
                model: insp.rentals || []
                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 3
                    Copy { text: modelData.name || ""; font.bold: true }
                    Copy { text: modelData.terms || ""; font.pixelSize: insp.textSize * 0.88 }
                    WorldButton {
                        objectName: "rentCompute_" + modelData.id
                        text: qsTr("Sign server rental")
                        textSize: insp.textSize * 0.88
                        enabled: insp.editable && !!modelData.eligible
                        onClicked: insp.rentalRequested(modelData.id)
                    }
                }
            }
        }
    }
}
