import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: strip
    property var participants: []
    property int textSize: 14
    property bool reducedMotion: true
    readonly property real windowHeight: ApplicationWindow.window ? ApplicationWindow.window.height : 800
    readonly property bool compact: windowHeight < textSize * 24
    readonly property real portraitEdge: compact ? Math.max(48, Math.min(textSize * 4, windowHeight * 0.15)) : textSize * 6
    visible: participants.length > 0
    onVisibleChanged: if (!visible) profile.close()
    Label { text: qsTr("In this decision"); font.pixelSize: strip.textSize; font.bold: true }
    Flow {
        Layout.fillWidth: true
        spacing: strip.textSize
        Repeater {
            model: strip.participants
            Item {
                id: entry
                required property var modelData
                width: strip.compact ? strip.width : Math.min(strip.width, strip.textSize * 11)
                height: strip.compact ? Math.max(strip.portraitEdge, labels.implicitHeight) : strip.portraitEdge + 4 + labels.implicitHeight
                function inspect() { profile.person = modelData; profile.open() }
                EmployeePortrait {
                    objectName: "decisionParticipant_" + entry.modelData.id
                    x: strip.compact ? 0 : (parent.width - width) / 2
                    width: strip.portraitEdge
                    height: width
                    visible: !!entry.modelData.portrait
                    portraitSource: entry.modelData.portrait || ""
                    personName: entry.modelData.name || ""
                    reducedMotion: strip.reducedMotion
                    onClicked: entry.inspect()
                }
                AbstractButton {
                    id: monogram
                    objectName: "decisionParticipant_" + entry.modelData.id
                    x: strip.compact ? 0 : (parent.width - width) / 2
                    width: strip.portraitEdge
                    height: width
                    hoverEnabled: true
                    visible: !entry.modelData.portrait
                    Accessible.name: qsTr("View %1").arg(entry.modelData.name)
                    contentItem: Label {
                        text: (entry.modelData.name || "").split(" ").filter(word => word.length).slice(0, 2).map(word => word[0]).join("")
                        font.pixelSize: strip.textSize * 2
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#f0e6d3"
                    }
                    background: Rectangle { radius: 10; color: monogram.hovered ? "#3b5e58" : "#294841"; border.color: monogram.visualFocus ? "#f1d196" : "#819c8e"; border.width: monogram.visualFocus ? 2 : 1 }
                    onClicked: entry.inspect()
                }
                Column {
                    id: labels
                    x: strip.compact ? strip.portraitEdge + strip.textSize * 0.5 : 0
                    y: strip.compact ? 0 : strip.portraitEdge + 4
                    width: parent.width - x
                    spacing: 4
                    Label { width: parent.width; text: entry.modelData.name || ""; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: strip.textSize; horizontalAlignment: strip.compact ? Text.AlignLeft : Text.AlignHCenter }
                    Label { width: parent.width; text: entry.modelData.role || ""; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: strip.textSize * 0.9; horizontalAlignment: strip.compact ? Text.AlignLeft : Text.AlignHCenter; visible: text.length > 0 }
                }
            }
        }
    }
    PersonProfileDialog { id: profile; objectName: "decisionProfileDialog"; textSize: strip.textSize }
}
