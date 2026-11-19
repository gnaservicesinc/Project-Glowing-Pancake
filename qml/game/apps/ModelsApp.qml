import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: root
    objectName: "modelsAppRoot"
    property int textSize: 14
    property var manager: campaign.management
    readonly property bool editable: campaign.paused && !campaign.busy && campaign.companyEditable
    signal continueFromPocRequested(string pocModelId, string recipeId)
    spacing: 10

    readonly property var selectedRow: manager.models.selectedRow || ({})
    readonly property var fields: selectedRow.fields || ({})

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: root.textSize
        color: "#d9e6df"
    }

    // Selected Model / PoC Lineage & Technical Traits Card
    Frame {
        visible: !!(root.selectedRow && root.selectedRow.title)
        Layout.fillWidth: true
        padding: 10
        background: Rectangle {
            radius: 8
            color: root.fields.isProofOfConcept ? "#192d26" : "#152635"
            border.color: root.fields.isProofOfConcept ? "#58a882" : "#3d5a70"
        }
        contentItem: ColumnLayout {
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Copy {
                    Layout.fillWidth: true
                    text: root.selectedRow.title || ""
                    font.bold: true
                    color: "#f5e7c6"
                }
                Rectangle {
                    radius: 6
                    color: root.fields.isProofOfConcept ? "#27543e" : "#223b4e"
                    border.color: root.fields.isProofOfConcept ? "#6ac99b" : "#4f738b"
                    implicitWidth: kindBadge.implicitWidth + 12
                    implicitHeight: kindBadge.implicitHeight + 6
                    Label {
                        id: kindBadge
                        anchors.centerIn: parent
                        text: root.fields.isProofOfConcept ? qsTr("Proof-of-Concept Prototype") : qsTr("Production Model / Checkpoint")
                        textFormat: Text.PlainText
                        font.pixelSize: root.textSize * 0.78
                        font.bold: true
                        color: root.fields.isProofOfConcept ? "#d2f8e4" : "#c9e0ee"
                    }
                }
            }
            Copy {
                visible: !!root.fields.isProofOfConcept
                text: qsTr("Origin research: %1 · Method: %2 · Task scope: %3\nNon-deployable prototype — continue into a full production training run with production data and an independent evaluation before customer deployment.")
                    .arg(root.fields.originResearchId || "—")
                    .arg(root.fields.methodLabel || "—")
                    .arg(root.fields.taskScope || "—")
                font.pixelSize: root.textSize * 0.88
                color: "#b7e8cd"
            }
            Copy {
                visible: !root.fields.isProofOfConcept
                text: qsTr("Recipe: %1 · Parent: %2 · Datasets: %3\nData efficiency +%4% · Compute efficiency +%5% · Latency bonus +%6% · Robustness +%7%\nProvenance status: %8")
                    .arg(root.fields.recipeId || "—")
                    .arg(root.fields.parentId || qsTr("none (fresh run)"))
                    .arg(root.fields.dataNames || "—")
                    .arg(root.fields.dataEfficiencyPct || 0)
                    .arg(root.fields.computeEfficiencyPct || 0)
                    .arg(root.fields.latencyBonusPct || 0)
                    .arg(root.fields.robustnessBonusPct || 0)
                    .arg(root.fields.cleanProvenance ? qsTr("Clean & verified") : qsTr("Contains unconsented/disputed sources"))
                font.pixelSize: root.textSize * 0.88
                color: "#b8d6ca"
            }
            WorldButton {
                objectName: "continueFromPocButton"
                visible: !!root.fields.isProofOfConcept
                text: qsTr("Continue PoC into production training →")
                textSize: root.textSize * 0.9
                primary: true
                enabled: root.editable
                onClicked: root.continueFromPocRequested(root.manager.models.selectedId, root.fields.recipeId || "")
            }
        }
    }
}
