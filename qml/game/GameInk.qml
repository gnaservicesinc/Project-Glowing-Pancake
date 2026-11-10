import QtQuick

// Shared color and type roles for the game's illustrations and framing.
QtObject {
    property string tone: "amber"

    readonly property string displayFace: "Georgia"
    readonly property color ink: "#0b111a"
    readonly property color panel: "#192634"
    readonly property color chrome: "#111e2c"
    readonly property color control: "#223342"
    readonly property color hover: "#304456"
    readonly property color border: "#455967"
    readonly property color focus: "#f5ead8"
    readonly property color positive: "#aad8c9"
    readonly property color paper: "#f5ead8"
    readonly property color secondary: "#c9cac4"
    readonly property color muted: "#aab2b7"
    readonly property color accent: tones[tone] ? tones[tone][0] : tones.amber[0]
    readonly property color stage: tones[tone] ? tones[tone][1] : tones.amber[1]

    readonly property var tones: ({
        amber: ["#efc389", "#413847"],
        slate: ["#a8cbe3", "#2c4155"],
        olive: ["#cad79b", "#394a3f"],
        jade: ["#9ad6bc", "#294b48"],
        plum: ["#d7b9dd", "#40394f"],
        clay: ["#efb1a0", "#4b3b43"]
    })
}
