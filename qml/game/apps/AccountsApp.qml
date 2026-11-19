import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: root
    objectName: "accountsAppRoot"
    property int textSize: 14
    property var manager: campaign.management
    property string selectedCategory: ""
    DraftState {
        id: spendingDraft
        draftKey: "accounts/category"
        onRestored: value => root.selectedCategory = value.category || ""
        onReset: root.selectedCategory = ""
    }
    function chooseCategory(category) {
        selectedCategory = selectedCategory === category ? "" : category
        spendingDraft.remember({category: selectedCategory})
    }
    readonly property var stage: manager.industryStage || ({})
    readonly property var buckets: manager.expenseBreakdown || []
    readonly property var categoryPostings: selectedCategory.length > 0 ? manager.ledgerPostingsByCategory(selectedCategory) : []
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: root.textSize
        color: "#d9e6df"
    }

    // Observed Industry Stage Banner
    Frame {
        visible: !!(root.stage.title)
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#43637a" }
        contentItem: ColumnLayout {
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Copy {
                    objectName: "industryStageTitle"
                    Layout.fillWidth: true
                    text: root.stage.title || ""
                    font.bold: true
                    color: "#ffd897"
                }
                Copy {
                    Layout.fillWidth: false
                    text: qsTr("%1 publications · %2 deployments")
                        .arg(root.stage.publicBreakthroughs || 0)
                        .arg(root.stage.deployedServices || 0)
                    font.pixelSize: root.textSize * 0.84
                    color: "#9ec8ba"
                }
            }
            Copy {
                text: root.stage.summary || ""
                font.pixelSize: root.textSize * 0.88
                color: "#c5d9d1"
            }
        }
    }

    // Interactive Clickable Segmented Expense Breakdown Bar
    Frame {
        Layout.fillWidth: true
        padding: 10
        background: Rectangle { radius: 8; color: "#152635"; border.color: "#3d5a70" }
        contentItem: ColumnLayout {
            spacing: 8
            Copy {
                text: qsTr("Expense Breakdown by Category (click a segment or category to inspect ledger postings)")
                font.bold: true
                color: "#ffd897"
            }

            // Segmented horizontal bar
            Rectangle {
                id: segmentedBar
                objectName: "expenseBreakdownBar"
                Layout.fillWidth: true
                height: 26
                radius: 6
                color: "#0e1a24"
                border.color: "#344e61"
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 2
                    Repeater {
                        model: root.buckets
                        AbstractButton {
                            id: segBtn
                            required property var modelData
                            objectName: "expenseSegment_" + modelData.id
                            visible: (modelData.share || 0) > 0
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.preferredWidth: Math.max(18, Math.round((modelData.share || 0) * 1000))
                            hoverEnabled: true
                            Accessible.name: modelData.label + " · " + modelData.amount + " (" + modelData.percentText + ")"
                            onClicked: root.chooseCategory(modelData.id)
                            background: Rectangle {
                                radius: 4
                                color: segBtn.modelData.color || "#4a90d9"
                                opacity: root.selectedCategory === "" || root.selectedCategory === segBtn.modelData.id ? 1.0 : 0.45
                                border.width: root.selectedCategory === segBtn.modelData.id ? 2 : 0
                                border.color: "#ffffff"
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.label + ": " + modelData.amount + " (" + modelData.percentText + ")"
                        }
                    }
                }
            }

            // Category legend chips
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: root.buckets
                    WorldButton {
                        required property var modelData
                        objectName: "expenseCategoryChip_" + modelData.id
                        text: modelData.label + " · " + modelData.amount + " (" + modelData.percentText + ")"
                        textSize: root.textSize * 0.82
                        padding: 5
                        checked: root.selectedCategory === modelData.id
                        onClicked: root.chooseCategory(modelData.id)
                    }
                }
            }

            // Filtered postings drill-down when a category is selected
            ColumnLayout {
                visible: root.selectedCategory.length > 0
                Layout.fillWidth: true
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    Copy {
                        Layout.fillWidth: true
                        text: qsTr("Settled ledger postings for category: %1").arg(root.selectedCategory)
                        font.bold: true
                        color: "#98c9b8"
                    }
                    WorldButton {
                        text: qsTr("Clear filter")
                        textSize: root.textSize * 0.8
                        padding: 4
                        onClicked: root.chooseCategory("")
                    }
                }
                Copy {
                    visible: root.categoryPostings.length === 0
                    text: qsTr("No settled cash outlays in this category yet.")
                    font.pixelSize: root.textSize * 0.86
                    color: "#9aaeb8"
                }
                Repeater {
                    model: root.categoryPostings
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        Copy {
                            Layout.fillWidth: true
                            text: modelData.date + " · " + modelData.cause + " (" + modelData.reference + ")"
                            font.pixelSize: root.textSize * 0.85
                        }
                        Label {
                            text: modelData.amount
                            textFormat: Text.PlainText
                            font.pixelSize: root.textSize * 0.86
                            font.bold: true
                            color: modelData.incoming ? "#afdcc3" : "#f5c6a7"
                        }
                    }
                }
            }
        }
    }
}
