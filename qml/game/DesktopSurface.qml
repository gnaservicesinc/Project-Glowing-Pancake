import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: desktop
    required property var applications
    property int textSize: 16
    property string owner: ""
    property string date: ""
    property string status: ""
    property bool showClose: true
    signal launched(string app, string section)
    signal closeRequested()
    readonly property bool compact: width < 760 || height < 550
    color: "#263c51"
    Image { anchors.fill: parent; source: "qrc:/pgp/assets/ui/desktop-wallpaper.svg"; fillMode: Image.PreserveAspectCrop }
    Rectangle { anchors.fill: parent; color: "#18152336" }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillWidth: true; implicitHeight: bar.implicitHeight+14
            color: "#ec182738"
            RowLayout {
                id: bar; anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12; anchors.topMargin: 7; anchors.bottomMargin: 7
                Label { text: "✦"; color: "#f4cc8d"; font.pixelSize: desktop.textSize*1.2; Accessible.ignored: true }
                Label { Layout.fillWidth: true; text: qsTr("Pancake Desktop"); font.pixelSize: desktop.textSize; font.bold: true; color: "#f5e9d5"; elide: Text.ElideRight }
                Label { visible: !desktop.compact; text: desktop.date; color: "#ccd4dc"; font.pixelSize: desktop.textSize*0.8 }
                Label { text: "▰"; color: "#bdd2ba"; font.pixelSize: desktop.textSize; Accessible.name: qsTr("Battery charged") }
                WorldButton { objectName: "desktopClose"; visible: desktop.showClose; text: qsTr("Close"); textSize: desktop.textSize*0.8; padding: 6; onClicked: desktop.closeRequested() }
            }
        }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            Column {
                visible: !desktop.compact
                anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 38
                width: parent.width*0.3; spacing: 10
                Label { width: parent.width; text: desktop.owner; color: "#f7ead3"; font.family: "Georgia"; font.pixelSize: desktop.textSize*2; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignRight }
                Label { width: parent.width; text: desktop.status; color: "#e0d6c2"; font.pixelSize: desktop.textSize; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignRight }
            }
            ScrollView {
                id: appView
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.margins: desktop.compact ? 10 : 26
                width: desktop.compact ? parent.width-20 : parent.width*0.65
                contentWidth: availableWidth; clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                GridLayout {
                    id: apps
                    width: appView.availableWidth
                    height: Math.max(appView.availableHeight,Math.ceil(desktop.applications.length/columns)*(desktop.compact ? desktop.textSize*5.4 : desktop.textSize*6.5))
                    columns: desktop.compact ? (desktop.width<500 ? 3 : 4) : 4
                    columnSpacing: desktop.compact ? 5 : 16; rowSpacing: desktop.compact ? 4 : 14
                Repeater {
                    model: desktop.applications
                    Button {
                        id: icon
                        required property var modelData
                        objectName: modelData.objectName || modelData.id+"App"
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; Layout.preferredHeight: 1
                        Layout.minimumWidth: 0; Layout.minimumHeight: 0
                        enabled: modelData.available !== false
                        padding: desktop.compact ? 5 : 10
                        Accessible.name: modelData.name
                        ToolTip.visible: hovered; ToolTip.text: modelData.detail || modelData.name
                        background: Rectangle { radius: 8; color: icon.hovered || icon.activeFocus ? "#70445d72" : "transparent"; border.color: icon.activeFocus ? "#f8d299" : "transparent"; border.width: 2 }
                        contentItem: Item {
                            Rectangle {
                                id: symbol
                                opacity: icon.enabled ? 1 : 0.4
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: Math.max(0,(parent.height-height-title.implicitHeight-7)/2)
                                width: Math.max(38,Math.min(desktop.compact ? 45 : 70,parent.height-title.implicitHeight-8,parent.width*0.65)); height: width
                                radius: width*0.22; color: icon.modelData.tint || "#5d8497"
                                border.color: "#a3b4b9"; border.width: 1
                                gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(icon.modelData.tint || "#5d8497",1.25) } GradientStop { position: 1; color: icon.modelData.tint || "#5d8497" } }
                                DeviceGlyph { anchors.fill: parent; anchors.margins: parent.width*0.22; app: icon.modelData.glyph || icon.modelData.id }
                            }
                            Text {
                                id: title
                                anchors.top: symbol.bottom; anchors.topMargin: 6; width: parent.width
                                text: desktop.compact ? ({facilities:qsTr("HQ"),inbox:qsTr("Decisions"),objectives:qsTr("Mission")})[icon.modelData.id] || icon.modelData.name : icon.modelData.name; textFormat: Text.PlainText
                                color: icon.enabled ? "#fff3db" : "#b3b9bb"
                                font.pixelSize: desktop.textSize*(desktop.compact ? 0.65 : 0.9)
                                font.weight: Font.Medium; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                                style: Text.Outline; styleColor: "#4833444f"
                            }
                        }
                        onActiveFocusChanged: if(activeFocus) {
                            const flick=appView.contentItem
                            const y=icon.mapToItem(apps,0,0).y
                            if(y<flick.contentY) flick.contentY=y
                            else if(y+height>flick.contentY+appView.availableHeight) flick.contentY=y+height-appView.availableHeight
                        }
                        onClicked: desktop.launched(modelData.id,modelData.section || "")
                    }
                }
            }
        }
        }

        Rectangle {
            Layout.fillWidth: true; implicitHeight: taskbar.implicitHeight+14; color: "#ed172638"
            RowLayout {
                id: taskbar; anchors.fill: parent; anchors.margins: 7; spacing: 12
                Label { text: "✦"; color: "#f4cc8d"; font.pixelSize: desktop.textSize*1.3; leftPadding: 12; Accessible.ignored: true }
                Label { Layout.fillWidth: true; text: desktop.compact ? desktop.date : qsTr("%1 · Personal workspace").arg(desktop.owner); font.pixelSize: desktop.textSize*0.8; color: "#d4dce2"; elide: Text.ElideRight }
                Label { text: qsTr("Local files"); font.pixelSize: desktop.textSize*0.8; color: "#a7c5c1"; rightPadding: 12 }
            }
        }
    }
}
