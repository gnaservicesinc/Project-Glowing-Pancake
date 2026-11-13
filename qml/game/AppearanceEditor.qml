import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: editor
    required property var story
    required property real textScale
    readonly property int typeSize: Math.round(15*textScale)
    readonly property bool narrow: width < 800
    property string previewStage: "adult"
    Connections {
        target: editor.Window.window
        function onActiveFocusItemChanged() {
            const item=editor.Window.window.activeFocusItem
            let ancestor=item
            while(ancestor && ancestor!==features) ancestor=ancestor.parent
            if(!ancestor || !features.contentItem) return
            const p=item.mapToItem(features.contentItem.contentItem,0,0)
            const flick=features.contentItem
            if(p.y<flick.contentY) flick.contentY=p.y
            else if(p.y+item.height>flick.contentY+flick.height) flick.contentY=Math.max(0,Math.min(flick.contentHeight-flick.height,p.y+item.height-flick.height))
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 12
        Label { text: qsTr("Your reflection"); font.family: "Georgia"; font.pixelSize: editor.typeSize*1.6; color: "#f3e5cc" }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: editor.narrow ? 12 : 28
            Rectangle {
                Layout.fillHeight: true; Layout.preferredWidth: editor.width*(editor.narrow ? 0.25 : 0.30)
                color: "#253b48"; radius: 10; border.color: "#6e7e83"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 8
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Image {
                            id: preview; objectName: "foundingPortraitPreview"
                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height)
                            height: width
                            source: editor.story.portrait && editor.previewStage
                                ? editor.story.portraitForStage(editor.previewStage) : editor.story.portrait
                            fillMode: Image.PreserveAspectFit
                            sourceSize: Qt.size(Math.ceil(width*Screen.devicePixelRatio),Math.ceil(height*Screen.devicePixelRatio))
                            Accessible.name: qsTr("Your customized portrait"); Accessible.role: Accessible.Graphic
                        }
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 4
                        Repeater {
                            model: [
                                {id: "baby", label: qsTr("Baby")},
                                {id: "toddler", label: qsTr("Toddler")},
                                {id: "kid", label: qsTr("Kid")},
                                {id: "teen", label: qsTr("Teen")},
                                {id: "adult", label: qsTr("Adult")},
                                {id: "mature", label: qsTr("Midlife")},
                                {id: "senior", label: qsTr("Senior")},
                                {id: "elder", label: qsTr("Elder")}
                            ]
                            AbstractButton {
                                required property var modelData
                                objectName: "appearanceStage_" + modelData.id
                                implicitWidth: stageLbl.implicitWidth + 12
                                implicitHeight: Math.max(24, editor.typeSize * 1.45)
                                hoverEnabled: true
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Preview %1 life stage").arg(modelData.label)
                                onClicked: editor.previewStage = modelData.id
                                contentItem: Text {
                                    id: stageLbl
                                    text: parent.modelData.label
                                    textFormat: Text.PlainText
                                    font.pixelSize: Math.max(10, editor.typeSize * 0.72)
                                    font.weight: editor.previewStage === parent.modelData.id ? Font.DemiBold : Font.Normal
                                    color: editor.previewStage === parent.modelData.id ? "#18242e" : "#d8e3e6"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 5
                                    color: editor.previewStage === parent.modelData.id ? "#ffda91" : (parent.hovered ? "#355365" : "#1c2d37")
                                    border.color: parent.activeFocus ? "#ffda91" : (editor.previewStage === parent.modelData.id ? "#ffda91" : "#556a73")
                                }
                            }
                        }
                    }
                }
            }
            ScrollView {
                id: features; objectName: "appearanceFeatures"
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: availableWidth; clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                GridLayout {
                    width: features.availableWidth-10
                    columns: width > 700 ? 4 : width > 560 ? 3 : 2
                    rowSpacing: 8; columnSpacing: 8
                    Repeater {
                        model: editor.story.appearanceParts
                        Rectangle {
                            required property var modelData
                            required property int index
                            readonly property int value: Number(index<15 ? editor.story.form.appearance[index] : editor.story.form.style[index-15])
                            Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.minimumWidth: 0
                            implicitHeight: contents.implicitHeight+18
                            color: "#e5233444"; radius: 5; border.color: "#526373"
                            ColumnLayout {
                                id: contents
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 9; spacing: 6
                                Label { Layout.fillWidth: true; text: modelData.name; color: "#e9d8b8"; font.pixelSize: editor.typeSize*0.85; wrapMode: Text.Wrap }
                                Flow {
                                    visible: !!modelData.palette && modelData.palette.length > 0
                                    Layout.fillWidth: true; spacing: 4
                                    Repeater {
                                        model: modelData.palette || []
                                        AbstractButton {
                                            required property string modelData
                                            required property int index
                                            width: Math.max(20,editor.typeSize); height: width
                                            hoverEnabled: true
                                            activeFocusOnTab: true
                                            Accessible.role: Accessible.Button
                                            Accessible.name: qsTr("%1, color %2").arg(contents.parent.modelData.name).arg(index+1)
                                            contentItem: Item {}
                                            background: Rectangle { radius: width/2; color: parent.modelData; border.width: contents.parent.value === parent.index || parent.activeFocus || parent.hovered ? 3 : 1; border.color: contents.parent.value === parent.index || parent.activeFocus ? "#ffda91" : parent.hovered ? "#d8e3e6" : "#82919b" }
                                            onClicked: editor.story.setAppearance(contents.parent.index,index)
                                        }
                                    }
                                }
                                RowLayout {
                                    visible: !modelData.palette || !modelData.palette.length
                                    Layout.fillWidth: true; spacing: 4
                                    WorldButton { objectName: "appearancePrev_"+index; text: "‹"; textSize: editor.typeSize*0.8; padding: 3; implicitHeight: Math.max(26,editor.typeSize*1.4); Accessible.name: qsTr("Previous %1").arg(modelData.name); onClicked: editor.story.cycleAppearance(index,-1) }
                                    Label { Layout.fillWidth: true; text: qsTr("%1 / %2").arg(contents.parent.value+1).arg(modelData.count); horizontalAlignment: Text.AlignHCenter; font.pixelSize: editor.typeSize*0.8; color: "#c5d1d7" }
                                    WorldButton { objectName: "appearanceNext_"+index; text: "›"; textSize: editor.typeSize*0.8; padding: 3; implicitHeight: Math.max(26,editor.typeSize*1.4); Accessible.name: qsTr("Next %1").arg(modelData.name); onClicked: editor.story.cycleAppearance(index,1) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
