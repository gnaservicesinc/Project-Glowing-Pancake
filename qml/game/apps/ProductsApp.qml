import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../"

ColumnLayout {
    id: root
    objectName: "productsAppRoot"
    property int textSize: 14
    property var manager: campaign.management
    readonly property string selectedModelId: manager.models.selectedId || ""
    readonly property var techProfile: selectedModelId.length > 0 ? manager.modelTechnicalProfile(selectedModelId) : ({})
    signal openSoftwareStudioRequested()
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: root.textSize
        color: "#d9e6df"
    }

    Frame {
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
                    text: qsTr("Product & Service Technical Fit")
                    font.bold: true
                    color: "#ffd897"
                }
                WorldButton {
                    visible: !!campaign.world.talent.briefs
                    text: qsTr("Open Software Studio →")
                    textSize: root.textSize * 0.84
                    padding: 5
                    onClicked: root.openSoftwareStudioRequested()
                }
            }
            Copy {
                visible: !!root.techProfile.modelId
                text: qsTr("Selected model (%1) technical traits:\n• Serving latency bonus: +%2% · Operational robustness: +%3%\n• Data efficiency: +%4% · Compute efficiency: +%5%\n• Provenance status: %6\n• Unlocked modalities: %7 · Deployment settings: %8")
                    .arg(root.techProfile.modelId || "")
                    .arg(root.techProfile.latencyBonusPct || 0)
                    .arg(root.techProfile.robustnessBonusPct || 0)
                    .arg(root.techProfile.dataEfficiencyPct || 0)
                    .arg(root.techProfile.computeEfficiencyPct || 0)
                    .arg(root.techProfile.cleanProvenance ? qsTr("Clean & enterprise-verified") : qsTr("Contains unconsented/disputed sources (triggers scrutiny)"))
                    .arg((root.techProfile.modalities || []).length ? (root.techProfile.modalities || []).join(", ") : qsTr("standard text"))
                    .arg((root.techProfile.deploymentSettings || []).length ? (root.techProfile.deploymentSettings || []).join(", ") : qsTr("cloud hosted"))
                font.pixelSize: root.textSize * 0.86
                color: "#b8d6ca"
            }
            Copy {
                visible: !root.techProfile.modelId
                text: qsTr("Select a trained model checkpoint in Lab → Checkpoints to inspect how its researched technical traits and data provenance affect customer service fit.")
                font.pixelSize: root.textSize * 0.86
                color: "#9db8c4"
            }
        }
    }
}
