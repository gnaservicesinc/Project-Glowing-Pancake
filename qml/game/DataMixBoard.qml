import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: board
    required property var mix
    required property var weights
    required property bool editable
    required property bool checkpoint
    required property int textSize
    // Rebuild controls only when the selected source IDs change.
    readonly property string sourceKeys: JSON.stringify((mix.sources || []).map(source => source.id))
    signal weightEdited(string source, int weight)
    signal weightsSettled()
    signal resetWeights()
    implicitHeight: content.implicitHeight + 28
    color: "#183640"
    border.color: "#557c7d"
    radius: 12

    component Copy: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: "#dbeae4"
        font.pixelSize: board.textSize
    }
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10
        Copy {
            text: board.checkpoint ? qsTr("Saved dataset mix") : qsTr("Dataset mix")
            font.bold: true
            color: "#e9d7a2"
        }
        Copy {
            objectName: "dataMixSamples"
            visible: !!board.mix.used
            text: board.mix.weighted ? qsTr("%1 / %2 samples").arg(board.mix.used || "").arg(board.mix.budget || "")
                                     : qsTr("%1 samples").arg(board.mix.used || "")
        }
        Copy {
            visible: !!board.mix.missing
            text: qsTr("%1 slots unfilled. Shift weight to sources with more records.").arg(board.mix.missing || "")
            color: "#e7bb92"
        }
        Copy {
            visible: !!board.mix.error || !board.mix.sources
            text: board.mix.error || qsTr("Choose datasets above.")
        }
        Repeater {
            model: JSON.parse(board.sourceKeys)
            ColumnLayout {
                id: source
                required property string modelData
                readonly property var sample: (board.mix.sources || []).find(row => row.id === modelData) || ({})
                readonly property color tint: sample.limited ? "#dfa879" : "#8bc9b4"
                Layout.fillWidth: true
                spacing: 6
                Copy { text: source.sample.name || ""; font.bold: true }
                Copy {
                    objectName: "mixShare_" + source.modelData
                    text: board.mix.weighted ? qsTr("%1% of budget · %2 of %3 samples")
                            .arg(Math.round((source.sample.share || 0) * 1000) / 10).arg(source.sample.used || "").arg(source.sample.planned || "")
                        : qsTr("%1 samples").arg(source.sample.used || "")
                    color: source.tint
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    visible: board.mix.weighted
                    color: "#122830"
                    radius: 4
                    Rectangle {
                        height: parent.height
                        width: parent.width * (source.sample.share || 0)
                        color: "#475966"
                        radius: 4
                        Rectangle {
                            height: parent.height
                            width: parent.width * (source.sample.filled || 0)
                            color: source.tint
                            radius: 4
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !!board.mix.weighted && !board.checkpoint
                    Copy {
                        Layout.fillWidth: false
                        text: qsTr("Weight %1").arg(Math.round(weight.value))
                    }
                    GameSlider {
                        id: weight
                        objectName: "mixWeight_" + source.modelData
                        Layout.fillWidth: true
                        textSize: board.textSize
                        from: 1
                        to: 100
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        enabled: board.editable
                        value: board.weights[source.modelData] || 10
                        Accessible.name: qsTr("Sampling weight for %1").arg(source.sample.name || "")
                        onMoved: {
                            board.weightEdited(source.modelData,Math.round(value))
                            if (!pressed) board.weightsSettled()
                        }
                        onPressedChanged: if (!pressed) board.weightsSettled()
                    }
                }
            }
        }
        WorldButton {
            objectName: "evenDatasetMix"
            visible: !!board.mix.weighted && !board.checkpoint && (board.mix.sources || []).length > 1
            text: qsTr("Even mix")
            textSize: board.textSize
            enabled: board.editable
            onClicked: board.resetWeights()
        }
        Copy {
            visible: (board.mix.domains || []).length > 0
            text: qsTr("Useful coverage")
            font.bold: true
            color: "#e9d7a2"
        }
        Repeater {
            model: board.mix.domains || []
            ColumnLayout {
                id: domain
                required property var modelData
                Layout.fillWidth: true
                spacing: 5
                Copy { text: qsTr("%1 · %2 effective samples").arg(domain.modelData.name).arg(domain.modelData.units) }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    color: "#122830"
                    radius: 4
                    Accessible.name: qsTr("%1 · %2 of the recipe’s data potential").arg(domain.modelData.name).arg(domain.modelData.coverage)
                    Accessible.role: Accessible.ProgressBar
                    Rectangle {
                        height: parent.height
                        width: parent.width * domain.modelData.ratio
                        color: "#9eb8de"
                        radius: 4
                    }
                }
            }
        }
    }
}
