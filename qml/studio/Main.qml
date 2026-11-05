import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("Glowing Pancake Content Studio")
    visible: false
    property bool closeWindowAfterDiscard: false
    property bool showLibrary: true
    property bool outlineMode: false
    property bool inspectorOnly: false
    property bool wasEditing: false
    onClosing: event => {
        if (studio.draftDirty || studio.canvasController.hasLayoutEdits) {
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
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: window.showLibrary || !studio.editing ? qsTr("Content library") : qsTr("Content Studio")
                font.pixelSize: 25
                font.bold: true
            }
            Button {
                objectName: "openAssetRegister"
                text: qsTr("Assets and credits")
                enabled: studio.checkout.length > 0
                onClicked: { if (studio.openAssets()) assetEditor.open() }
            }
            Button {
                objectName: "openContentRevision"
                text: qsTr("Review content revision")
                enabled: studio.checkout.length > 0
                onClicked: revisionReview.open()
            }
        }
        RowLayout {
            visible: window.showLibrary || !studio.editing
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
            visible: window.showLibrary || !studio.editing
            Layout.fillWidth: true
            text: studio.message || qsTr("Choose a local development checkout to browse its definitions.")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
        RowLayout {
            visible: window.showLibrary || !studio.editing
            Layout.fillWidth: true
            TextField {
                id: query
                objectName: "librarySearch"
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
            visible: studio.editing
            Layout.fillWidth: true
            Button { text: qsTr("Browse library"); checkable: true; checked: window.showLibrary; onClicked: window.showLibrary = true }
            Button { text: qsTr("Build flow"); enabled: !studio.worldController.hasStaged && !studio.worldController.hasUnapplied; checkable: true; checked: !window.showLibrary && !studio.worldMode; onClicked: { studio.selectWorkspace(false); window.showLibrary = false } }
            Button { objectName: "showStudioWorld"; text: qsTr("Scenario and world"); checkable: true; checked: !window.showLibrary && studio.worldMode && !studio.worldController.catalogMode; onClicked: { studio.selectWorkspace(true,false); window.showLibrary = false } }
            Button { objectName: "showStudioCatalog"; text: qsTr("Catalogs"); checkable: true; checked: !window.showLibrary && studio.worldMode && studio.worldController.catalogMode; onClicked: { studio.selectWorkspace(true,true); window.showLibrary = false } }
            Button { objectName: "showStudioCanvas"; text: qsTr("Canvas"); visible: !window.showLibrary && !studio.worldMode; checkable: true; checked: !window.outlineMode && !window.inspectorOnly; onClicked: { window.outlineMode = false; window.inspectorOnly = false } }
            Button { text: qsTr("Outline"); visible: !window.showLibrary && !studio.worldMode; checkable: true; checked: window.outlineMode && !window.inspectorOnly; onClicked: { window.outlineMode = true; window.inspectorOnly = false } }
            Button { objectName: "showStudioSettings"; text: qsTr("Settings and test"); visible: !window.showLibrary && !studio.worldMode && window.width < 1100; onClicked: window.inspectorOnly = true }
            Label { Layout.fillWidth: true; text: (studio.worldMode ? studio.worldController.scenario : studio.workingFlow) + qsTr(" · content %1").arg(studio.workingVersion) + (studio.canvasController.hasLayoutEdits ? qsTr(" · unsaved layout moves") : ""); wrapMode: Text.Wrap }
        }
        WorldForms {
            visible: studio.editing && studio.worldMode && !window.showLibrary
            Layout.fillWidth: true; Layout.fillHeight: true
            author: studio.worldController
            session: studio
            onCloseRequested: {
                if (studio.draftDirty || studio.canvasController.hasLayoutEdits) {
                    window.closeWindowAfterDiscard = false
                    discardDraft.open()
                } else studio.closeDraft()
            }
        }
        GridLayout {
            visible: !studio.editing || !studio.worldMode || window.showLibrary
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: window.width < 1100 ? 1 : 2
            columnSpacing: 16
            rowSpacing: 16
            ScrollView {
                visible: window.showLibrary || !studio.editing
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
            ColumnLayout {
                visible: studio.editing && !window.showLibrary && (window.width >= 1100 || !window.inspectorOnly)
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: Math.max(480, window.width - 480)
                FlowCanvas {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !window.outlineMode
                    author: studio.canvasController
                    inspector: studio.probeController
                }
                FlowForms {
                    id: centerOutline
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.outlineMode
                    author: studio
                    showFields: false
                    fillOutline: true
                }
            }
            ScrollView {
                id: inspector
                objectName: "studioInspector"
                visible: window.showLibrary || !studio.editing || window.width >= 1100 || window.inspectorOnly
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: studio.editing && !window.showLibrary ? 400 : window.width * 0.45
                Layout.minimumWidth: studio.editing && !window.showLibrary ? 330 : 0
                contentWidth: availableWidth
                readonly property var entry: library.currentIndex >= 0 ? studio.rows[library.currentIndex] || {} : {}
                ColumnLayout {
                    width: inspector.availableWidth
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: window.showLibrary || !studio.editing
                    Label {
                        Layout.fillWidth: true
                        text: inspector.entry.id || qsTr("Select a library row")
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        font.bold: true
                    }
                    Label {
                        visible: !!inspector.entry.id
                        text: (inspector.entry.origin || "") + " · " + (inspector.entry.validation || "") +
                            (inspector.entry.state ? " · " + inspector.entry.state : "")
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
                        objectName: "editSelectedDefinition"
                        visible: !!inspector.entry.revision && inspector.entry.validation === "Validated"
                        text: inspector.entry.kind === "flow" ? qsTr("Edit flow") : inspector.entry.kind === "scenario" ? qsTr("Edit scenario and world") : qsTr("Open working collection")
                        onClicked: studio.beginDraft(inspector.entry.file, inspector.entry.id)
                    }
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
                            RowLayout {
                                Button { objectName: "undoStudioContent"; text: qsTr("Undo content"); enabled: studio.canUndo; onClicked: studio.undo() }
                                Button { objectName: "redoStudioContent"; text: qsTr("Redo content"); enabled: studio.canRedo; onClicked: studio.redo() }
                                Button { objectName: "saveExistingDraft"; text: qsTr("Save draft"); enabled: studio.existingDraft && studio.draftDirty; onClicked: studio.saveDraft("") }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: draftName
                                    objectName: "draftFilename"
                                    Layout.fillWidth: true
                                    maximumLength: 133
                                    placeholderText: qsTr("new-draft.json")
                                    Accessible.name: qsTr("New draft filename")
                                }
                                Button {
                                    objectName: "saveDraftAs"
                                    text: qsTr("Save as draft")
                                    enabled: draftName.text.length > 0
                                    onClicked: { if (studio.saveDraft(draftName.text.trim())) draftName.clear() }
                                }
                            }
                            ComboBox {
                                objectName: "workingFlow"
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
                                    objectName: "newFlowId"
                                    Layout.fillWidth: true
                                    maximumLength: 128
                                    placeholderText: qsTr("New flow ID")
                                    Accessible.name: qsTr("New flow ID")
                                }
                                ComboBox {
                                    id: newFlowScope
                                    objectName: "newFlowScope"
                                    Accessible.name: qsTr("New flow scope")
                                    model: studio.flowScopes
                                    textRole: "label"
                                    valueRole: "value"
                                }
                                Button {
                                    objectName: "createFlow"
                                    text: qsTr("Create flow")
                                    enabled: newFlowId.text.length > 0 && newFlowScope.currentIndex >= 0
                                    onClicked: { if (studio.createFlow(newFlowId.text, newFlowScope.currentValue)) newFlowId.clear() }
                                }
                            }
                            FlowForms {
                                Layout.fillWidth: true
                                visible: studio.workingFlow.length > 0
                                author: studio
                                showOutline: false
                                outlineObjectName: "hiddenFormOutline"
                                outlineFocusTarget: centerOutline.outlineView
                            }
                            Button {
                                objectName: "openGroupTools"
                                text: qsTr("Reusable flow groups")
                                visible: studio.workingFlow.length > 0
                                onClicked: groupTools.open()
                            }
                            FlowInspection {
                                Layout.fillWidth: true
                                inspector: studio.probeController
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
                            Button {
                                objectName: "closeWorkingDraft"
                                text: qsTr("Close draft")
                                onClicked: {
                                    if (studio.draftDirty || studio.canvasController.hasLayoutEdits) {
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
        title: qsTr("Discard unsaved content or layout changes?")
        standardButtons: Dialog.Discard | Dialog.Cancel
        onDiscarded: {
            studio.closeDraft()
            if (window.closeWindowAfterDiscard) window.close()
            close()
        }
    }
    SnapshotReview {
        id: revisionReview
        finalizer: studio.snapshotController
    }
    AssetEditor { id: assetEditor; author: studio.assetController; session: studio }
    GroupTools { id: groupTools; author: studio }
    Connections {
        target: studio
        function onDraftChanged() {
            if (studio.editing && !window.wasEditing) window.showLibrary = false
            if (!studio.editing) window.showLibrary = true
            window.wasEditing = studio.editing
        }
    }
}
