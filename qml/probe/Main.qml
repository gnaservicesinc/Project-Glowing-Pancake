import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1180
    height: 760
    minimumWidth: 720
    minimumHeight: 540
    visible: false
    title: qsTr("Project Glowing Pancake · Interface preview")
    color: "#171d29"
    font.pixelSize: 17 * probeTextScale
    readonly property real textScale: probeTextScale
    palette.window: "#171d29"
    palette.windowText: "#f4ebdc"
    palette.button: "#354456"
    palette.buttonText: "#f4ebdc"
    palette.base: "#1c2532"
    palette.text: "#f4ebdc"
    palette.highlight: "#a7c9bf"
    palette.highlightedText: "#152127"
    palette.mid: "#637087"
    palette.dark: "#0f1520"
    palette.light: "#9eaabd"
    palette.brightText: "#f4ebdc"

    // Text scaling can put controls below the fold. Keep keyboard focus visible
    // without requiring pointer scrolling or changing the selected control.
    function revealFocus() {
        const focusItem = window.activeFocusItem
        const flick = scroll.contentItem
        if (!focusItem || !flick || flick.contentY === undefined)
            return
        const point = focusItem.mapToItem(flick.contentItem, 0, 0)
        let position = flick.contentY
        if (point.y < position + 12)
            position = point.y - 12
        else if (point.y + focusItem.height > position + flick.height - 12)
            position = point.y + focusItem.height - flick.height + 12
        flick.contentY = Math.max(0, Math.min(position, flick.contentHeight - flick.height))
    }
    onActiveFocusItemChanged: Qt.callLater(revealFocus)

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 26
            Item { Layout.preferredHeight: 10 }
            Label {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.fillWidth: true
                text: qsTr("PROJECT GLOWING PANCAKE")
                color: "#a7c9bf"
                font.pixelSize: 13 * window.textScale
                font.letterSpacing: 2
                wrapMode: Text.WordWrap
            }
            GridLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.fillWidth: true
                columns: window.width > 1050 ? 2 : 1
                columnSpacing: 34
                rowSpacing: 24
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 580
                    Layout.preferredHeight: window.width > 1050 ? 385 : 290
                    radius: 14
                    color: "#293244"
                    clip: true
                    Image {
                        objectName: "localPng"
                        anchors.fill: parent
                        source: "qrc:/pgp/assets/probe/images/office.png"
                        fillMode: Image.PreserveAspectCrop
                        Accessible.ignored: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 460
                    spacing: 18
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("A quiet office.\nA long horizon.")
                        color: "#f4ebdc"
                        font.pixelSize: 40 * window.textScale
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("A company begins with a room and a possibility. The next century is still unwritten.")
                        color: "#b6bfce"
                        wrapMode: Text.WordWrap
                        lineHeight: 1.25
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("INTERFACE PREVIEW")
                        color: "#e2bd86"
                        font.pixelSize: 12 * window.textScale
                        font.letterSpacing: 1.4
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Explore the controls below. Company simulation will arrive in a later build.")
                        color: "#b6bfce"
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.fillWidth: true
                implicitHeight: controls.implicitHeight + 48
                radius: 14
                color: "#242d3c"
                ColumnLayout {
                    id: controls
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Make yourself at home")
                        font.pixelSize: 23 * window.textScale
                        font.weight: Font.DemiBold
                        color: "#f4ebdc"
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 16
                        Button {
                            objectName: "controlsButton"
                            text: qsTr("Check controls")
                            highlighted: true
                            palette.dark: "#a7c9bf"
                            palette.brightText: "#152127"
                            Accessible.name: qsTr("Check interface controls")
                            Accessible.description: qsTr("Confirms that click and keyboard input work.")
                            onClicked: probe.checkControls()
                        }
                        Button {
                            objectName: "soundButton"
                            text: qsTr("Check sound")
                            Accessible.name: qsTr("Play a short sound check")
                            onClicked: probe.checkSound()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: probe.controlsMessage
                        color: "#a7c9bf"
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: probe.audioMessage
                        color: "#b6bfce"
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 20
                        CheckBox {
                            text: qsTr("Mute sound")
                            palette.windowText: "#f4ebdc"
                            checked: probe.muted
                            onToggled: probe.muted = checked
                        }
                        Slider {
                            width: 230
                            from: 0
                            to: 1
                            value: probe.volume
                            Accessible.name: qsTr("Sound volume")
                            onMoved: probe.volume = value
                        }
                    }
                }
            }
            RowLayout {
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.bottomMargin: 28
                Layout.fillWidth: true
                spacing: 14
                Image {
                    objectName: "localJpeg"
                    source: "qrc:/pgp/assets/probe/images/paper.jpg"
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    fillMode: Image.PreserveAspectCrop
                    Accessible.ignored: true
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Built for offline play. Sound is always optional.")
                    color: "#929eaf"
                    font.pixelSize: 14 * window.textScale
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
