import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: editor
    required property QtObject author
    objectName: "reusableGroupDialog"
    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 770)
    height: Math.min(parent.height - 32, 620)
    title: qsTr("Reusable flow groups")
    modal: true
    standardButtons: Dialog.Close
    property var bindings: ({})
    onOpened: bindings = ({})
    contentItem: ScrollView {
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                text: qsTr("Copy the working flow as a reusable group. In its outline, name the inputs and outputs that its body reads or writes.")
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: newId
                    objectName: "newReusableGroupId"
                    Layout.fillWidth: true
                    maximumLength: 128
                    placeholderText: qsTr("Name for the group")
                    Accessible.name: qsTr("Name for the group")
                }
                Button {
                    objectName: "createReusableGroup"
                    text: qsTr("Copy as group")
                    enabled: newId.text.length > 0
                    onClicked: {
                        if (editor.author.createGroup(newId.text.trim())) {
                            newId.clear()
                            editor.close()
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Insert a call before a node. Choose every binding, including the actual variable that receives each output.")
                wrapMode: Text.Wrap
            }
            ComboBox {
                id: group
                objectName: "selectReusableGroup"
                Layout.fillWidth: true
                model: editor.author.groupOptions
                textRole: "label"
                valueRole: "value"
                Accessible.name: qsTr("Reusable group to call")
                onCurrentIndexChanged: editor.bindings = ({})
            }
            ComboBox {
                id: node
                objectName: "selectGroupContinuation"
                Layout.fillWidth: true
                model: editor.author.groupContinuations
                textRole: "label"
                valueRole: "value"
                Accessible.name: qsTr("Continue at this node after the group returns")
            }
            Repeater {
                id: ports
                model: group.currentIndex >= 0 ? editor.author.groupOptions[group.currentIndex].ports : []
                delegate: ColumnLayout {
                    id: port
                    required property var modelData
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: port.modelData.label
                        wrapMode: Text.WrapAnywhere
                    }
                    ComboBox {
                        objectName: "groupBinding/" + port.modelData.name
                        Layout.fillWidth: true
                        model: port.modelData.options
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: indexOfValue(editor.bindings[port.modelData.name] || "")
                        Accessible.name: port.modelData.label
                        onActivated: {
                            const next = Object.assign({}, editor.bindings)
                            next[port.modelData.name] = currentValue
                            editor.bindings = next
                        }
                    }
                }
            }
            Button {
                objectName: "insertReusableGroupCall"
                text: qsTr("Insert reviewed call")
                enabled: group.currentIndex >= 0 && node.currentIndex >= 0 && Object.keys(editor.bindings).length === ports.count
                onClicked: { if (editor.author.insertGroup(group.currentValue,node.currentValue,editor.bindings)) editor.close() }
            }
            Label {
                Layout.fillWidth: true
                text: editor.author.draftMessage
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
