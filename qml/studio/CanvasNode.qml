import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: card
    required property var modelData
    required property QtObject author
    required property QtObject inspector
    required property QtObject workspace
    readonly property var trace: inspector.graphTrace[modelData.key] || ({})
    readonly property bool selected: author.selectedKey === modelData.key
    x: modelData.x
    y: modelData.y
    width: modelData.width
    height: modelData.height
    radius: 8
    color: selected ? "#253d50" : "#172936"
    border.width: selected ? 3 : 1
    border.color: selected ? "#bee7ff" : workspace.traceColor(trace.state)
    visible: (x + width) * workspace.zoom + workspace.panX >= 0 && x * workspace.zoom + workspace.panX <= workspace.viewportWidth &&
             (y + height) * workspace.zoom + workspace.panY >= 0 && y * workspace.zoom + workspace.panY <= workspace.viewportHeight

    Button {
        id: heading
        objectName: "canvasNode_" + card.modelData.key
        x: 10; y: 8; width: card.width - 20; height: 34
        text: card.modelData.title
        Accessible.name: card.modelData.title + ", " + card.modelData.scope
        Accessible.description: card.modelData.detail
        onClicked: card.workspace.chooseNode(card.modelData.key)
        contentItem: Text { text: heading.text; textFormat: Text.PlainText; color: "#f0f7fc"; font.bold: true; font.pixelSize: 14; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 4; color: heading.activeFocus ? "#3b607a" : "#233b4d" }
        ToolTip.visible: dragHeader.containsMouse || activeFocus
        ToolTip.text: card.workspace.tooltipText(card.modelData.title + "\n" + card.modelData.detail + (card.trace.detail ? "\n" + card.trace.detail : ""))
        MouseArea {
            id: dragHeader
            anchors.fill: parent
            hoverEnabled: true
            drag.target: card
            drag.minimumX: 0; drag.minimumY: 0
            drag.maximumX: 1048576; drag.maximumY: 1048576
            onPressed: card.workspace.chooseNode(card.modelData.key)
            onPositionChanged: {
                if (drag.active) card.workspace.previewMove(card.modelData.key, card.x, card.y)
            }
            onReleased: {
                const px = Math.max(0, Math.round(card.x / 16) * 16)
                const py = Math.max(0, Math.round(card.y / 16) * 16)
                card.x = px; card.y = py
                if (!card.workspace.finishMove(card.modelData.ticket, card.modelData.key, px, py)) {
                    card.x = card.modelData.x; card.y = card.modelData.y
                }
            }
            onCanceled: { card.x = card.modelData.x; card.y = card.modelData.y; card.workspace.cancelMove() }
        }
    }
    Text {
        x: 14; y: 49; width: parent.width - 28; height: 38
        text: card.modelData.detail
        textFormat: Text.PlainText
        color: "#bfd0dc"
        font.pixelSize: 12
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }
    Text {
        objectName: "canvasState_" + card.modelData.key
        x: 14; y: 88; width: parent.width - 28
        text: card.trace.state ? card.trace.state.toUpperCase() : card.modelData.scope
        color: card.workspace.traceColor(card.trace.state)
        font.pixelSize: 11
        font.bold: true
    }
    Repeater {
        model: card.modelData.ports
        Button {
            id: portButton
            required property var modelData
            readonly property var observation: card.inspector.graphTrace[modelData.id] ||
                ((modelData.kind === 1 || modelData.kind === 2) ? card.inspector.graphTrace[modelData.row] :
                 modelData.kind === 3 && modelData.output ? card.inspector.graphTrace[card.modelData.key] :
                 modelData.kind === 3 ? card.inspector.graphTrace[modelData.row] : undefined) || ({})
            objectName: "canvasPort_" + modelData.id
            x: 12; y: modelData.offset - 18; width: card.width - 24; height: 36
            Accessible.name: (modelData.output ? qsTr("Output ") : qsTr("Input ")) + modelData.label + ", " + modelData.type + ", " + (observation.state || "")
            Accessible.description: modelData.help + "\n" + (observation.detail || "")
            onClicked: {
                card.workspace.pickPort(card.modelData.ticket, modelData.id)
            }
            background: Rectangle {
                radius: 4
                color: card.author.pendingOutput === portButton.modelData.id ? "#68552a" : portButton.activeFocus ? "#365268" : "#233846"
                border.width: card.author.selectedPort === portButton.modelData.id ? 2 : 1
                border.color: card.workspace.portColor(portButton.modelData.kind)
            }
            contentItem: RowLayout {
                spacing: 6
                Text { text: portButton.modelData.output ? "○" : "▶"; color: card.workspace.portColor(portButton.modelData.kind); font.pixelSize: 17 }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text { Layout.fillWidth: true; text: portButton.modelData.label; textFormat: Text.PlainText; color: "#eff6fa"; font.pixelSize: 12; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: portButton.modelData.type; textFormat: Text.PlainText; color: "#becfda"; font.pixelSize: 10; elide: Text.ElideRight }
                }
                Text {
                    objectName: "canvasPortState_" + portButton.modelData.id
                    text: portButton.observation.state ? portButton.observation.state.toUpperCase() : portButton.modelData.output ? "OUT" : "IN"
                    color: card.workspace.traceColor(portButton.observation.state)
                    font.pixelSize: 10
                    font.bold: true
                }
            }
            ToolTip.visible: hovered || activeFocus
            ToolTip.text: card.workspace.tooltipText((modelData.output ? qsTr("Output · ") : qsTr("Input · ")) + modelData.label + "\n" + modelData.type + "\n" + modelData.help + (observation.detail ? "\n" + observation.detail : ""))
        }
    }
}
