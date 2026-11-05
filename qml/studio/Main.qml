import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1100
    height: 700
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("Glowing Pancake Content Studio")
    visible: false
    property bool closeWindowAfterDiscard: false
    onClosing: event => {
        if (studio.draftDirty) {
            event.accepted = false
            closeWindowAfterDiscard = true
            discardDraft.open()
        }
    }

    function updateFilter() {
        studio.filter(query.text, kinds.currentValue, origins.currentValue)
    }
    function follow(file, definition) {
        query.text = definition
        kinds.currentIndex = 0
        origins.currentIndex = 0
        updateFilter()
        const index = studio.locate(file, definition)
        if (index >= 0) {
            library.currentIndex = index
            library.positionViewAtIndex(index, ListView.Contain)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        Label {
            text: qsTr("Content library")
            font.pixelSize: 25
            font.bold: true
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: folder
                Layout.fillWidth: true
                maximumLength: 4096
                text: studio.checkout
                placeholderText: qsTr("Absolute checkout folder")
                Accessible.name: qsTr("Checkout folder")
                onAccepted: studio.openCheckout(text)
            }
            Button {
                text: qsTr("Open checkout")
                enabled: folder.text.length > 0
                onClicked: studio.openCheckout(folder.text)
            }
        }
        Label {
            Layout.fillWidth: true
            text: studio.message || qsTr("Choose a local development checkout to browse its definitions.")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: query
                Layout.fillWidth: true
                maximumLength: 256
                placeholderText: qsTr("Search ID, tag, file or validation")
                Accessible.name: qsTr("Search content library")
                onTextEdited: window.updateFilter()
            }
            ComboBox {
                id: kinds
                Accessible.name: qsTr("Definition type")
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("All types"), value: "" }, { label: qsTr("Flows"), value: "flow" },
                    { label: qsTr("Scenarios"), value: "scenario" }, { label: qsTr("Research"), value: "research" },
                    { label: qsTr("Deals"), value: "deal" }, { label: qsTr("Documents"), value: "document" }
                ]
                onActivated: window.updateFilter()
            }
            ComboBox {
                id: origins
                Accessible.name: qsTr("Content source")
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("All sources"), value: "" }, { label: qsTr("Reviewed"), value: "Reviewed" },
                    { label: qsTr("Drafts"), value: "Draft" }, { label: qsTr("Engineering fixtures"), value: "Engineering fixture" }
                ]
                onActivated: window.updateFilter()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: window.width * 0.45
                ListView {
                    id: library
                    objectName: "studioLibrary"
                    clip: true
                    model: studio.rows
                    Accessible.name: qsTr("Definitions and documents")
                    keyNavigationEnabled: true
                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        text: modelData.id + "\n" + modelData.file
                        onClicked: library.currentIndex = index
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: library.count === 0
                        text: qsTr("No matching definitions or documents")
                    }
                }
            }
            ScrollView {
                id: inspector
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: window.width * 0.45
                contentWidth: availableWidth
                readonly property var entry: library.currentIndex >= 0 ? studio.rows[library.currentIndex] || {} : {}
                ColumnLayout {
                    width: inspector.availableWidth
                    Label {
                        Layout.fillWidth: true
                        text: inspector.entry.id || qsTr("Select a library row")
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        font.bold: true
                    }
                    Label {
                        visible: !!inspector.entry.id
                        text: (inspector.entry.origin || "") + " · " + (inspector.entry.validation || "")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!inspector.entry.revision
                        text: qsTr("%1 · revision %2 · content %3").arg(inspector.entry.kind || "")
                              .arg(inspector.entry.revision || "").arg(inspector.entry.version || "")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!inspector.entry.diagnostic
                        text: inspector.entry.diagnostic || ""
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        visible: !!inspector.entry.tags && inspector.entry.tags.length > 0
                        Layout.fillWidth: true
                        text: (inspector.entry.tags || []).join(" · ")
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                    }
                    Label { visible: !!inspector.entry.revision; text: qsTr("Uses"); font.bold: true }
                    Repeater {
                        model: inspector.entry.uses || []
                        delegate: Button {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            onClicked: window.follow(inspector.entry.file, modelData)
                        }
                    }
                    Label { visible: !!inspector.entry.revision; text: qsTr("Used by"); font.bold: true }
                    Repeater {
                        model: inspector.entry.usedBy || []
                        delegate: Button {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            onClicked: window.follow(inspector.entry.file, modelData)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!inspector.entry.fingerprint
                        text: qsTr("Source SHA-256 %1").arg(inspector.entry.fingerprint || "")
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                    }
                    Button {
                        visible: !!inspector.entry.revision && inspector.entry.validation === "Validated"
                        text: inspector.entry.kind === "flow" ? qsTr("Edit flow") : qsTr("Open working collection")
                        onClicked: studio.beginDraft(inspector.entry.file, inspector.entry.id)
                    }
                    GroupBox {
                        Layout.fillWidth: true
                        visible: studio.editing
                        title: qsTr("Working draft")
                        ColumnLayout {
                            width: parent.width
                            Label {
                                Layout.fillWidth: true
                                text: studio.workingFlow + "\n" + studio.workingFile
                                textFormat: Text.PlainText
                                wrapMode: Text.WrapAnywhere
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                visible: studio.workingFlows.length > 0
                                Accessible.name: qsTr("Working flow")
                                model: studio.workingFlows
                                currentIndex: find(studio.workingFlow)
                                onActivated: studio.selectWorkingFlow(currentText)
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: newFlowId
                                    Layout.fillWidth: true
                                    maximumLength: 128
                                    placeholderText: qsTr("New flow ID")
                                    Accessible.name: qsTr("New flow ID")
                                }
                                ComboBox {
                                    id: newFlowScope
                                    Accessible.name: qsTr("New flow scope")
                                    model: studio.flowScopes
                                    textRole: "label"
                                    valueRole: "value"
                                }
                                Button {
                                    text: qsTr("Create flow")
                                    enabled: newFlowId.text.length > 0 && newFlowScope.currentIndex >= 0
                                    onClicked: { if (studio.createFlow(newFlowId.text, newFlowScope.currentValue)) newFlowId.clear() }
                                }
                            }
                            FlowForms {
                                Layout.fillWidth: true
                                visible: studio.workingFlow.length > 0
                                author: studio
                            }
                            Label { visible: studio.workingFlow.length > 0; text: qsTr("Flow tags"); font.bold: true }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: studio.workingTags
                                    delegate: Button {
                                        required property string modelData
                                        text: qsTr("Remove %1").arg(modelData)
                                        onClicked: studio.changeTag(modelData, true)
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: studio.workingFlow.length > 0
                                TextField {
                                    id: tag
                                    Layout.fillWidth: true
                                    maximumLength: 128
                                    placeholderText: qsTr("New lowercase tag")
                                    Accessible.name: qsTr("New tag")
                                    onAccepted: { if (studio.changeTag(text, false)) clear() }
                                }
                                Button {
                                    text: qsTr("Add")
                                    enabled: tag.text.length > 0
                                    onClicked: { if (studio.changeTag(tag.text, false)) tag.clear() }
                                }
                            }
                            RowLayout {
                                Button { text: qsTr("Undo"); enabled: studio.canUndo; onClicked: studio.undo() }
                                Button { text: qsTr("Redo"); enabled: studio.canRedo; onClicked: studio.redo() }
                                Button {
                                    text: qsTr("Save draft")
                                    enabled: studio.existingDraft && studio.draftDirty
                                    onClicked: studio.saveDraft("")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: draftName
                                    Layout.fillWidth: true
                                    maximumLength: 133
                                    placeholderText: qsTr("new-draft.json")
                                    Accessible.name: qsTr("New draft filename")
                                }
                                Button {
                                    text: qsTr("Save as draft")
                                    enabled: draftName.text.length > 0
                                    onClicked: { if (studio.saveDraft(draftName.text.trimmed())) draftName.clear() }
                                }
                            }
                            Button {
                                text: qsTr("Close draft")
                                onClicked: {
                                    if (studio.draftDirty) {
                                        window.closeWindowAfterDiscard = false
                                        discardDraft.open()
                                    } else studio.closeDraft()
                                }
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: studio.draftMessage.length > 0
                        text: studio.draftMessage
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }

    Dialog {
        id: discardDraft
        anchors.centerIn: parent
        modal: true
        title: qsTr("Discard unsaved draft changes?")
        standardButtons: Dialog.Discard | Dialog.Cancel
        onDiscarded: {
            studio.closeDraft()
            if (window.closeWindowAfterDiscard) window.close()
            close()
        }
    }
}
