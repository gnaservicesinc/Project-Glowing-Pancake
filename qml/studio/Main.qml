import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 720
    minimumHeight: 480
    title: qsTr("Glowing Pancake Content Studio")
    visible: false
    color: "#151c25"
    palette.window: "#151c25"
    palette.windowText: "#eff2f7"
    palette.base: "#101720"
    palette.text: "#eff2f7"
    palette.button: "#2a3748"
    palette.buttonText: "#eff2f7"
    palette.highlight: "#4375ad"
    palette.highlightedText: "#ffffff"

    property bool closeWindowAfterDiscard: false
    property bool showLibrary: true
    property int page: 0
    property bool wasEditing: false
    readonly property bool editingChapter: !!studio.chapter.title
    readonly property bool unsaved: studio.draftDirty || studio.canvasController.hasLayoutEdits || eventStory.changed || childhoodScene.changed
    readonly property string statusMessage: studio.saving ? qsTr("Validating and saving the project…") : studio.draftMessage || studio.message

    function updateFilter() {
        studio.filter(query.text, kinds.currentValue, "Reviewed")
        library.currentIndex = library.count ? 0 : -1
    }
    function editSelected() {
        const entry = library.currentItem ? library.currentItem.modelData : null
        if (eventStory.changed && !eventStory.applyEdits()) return
        if (childhoodScene.changed && !childhoodScene.applyEdits()) return
        if (!entry || entry.validation !== "Validated" || entry.kind === "document") return
        const opened = entry.kind === "chapter" ? studio.openChapter(entry.file, entry.owner, entry.id)
            : studio.beginDraft(entry.file, entry.id)
        if (opened) {
            window.showLibrary = false
            window.page = 0
        }
    }
    function save() {
        if (studio.saving) return
        if (eventStory.changed && !eventStory.applyEdits()) return
        if (childhoodScene.changed && !childhoodScene.applyEdits()) return
        if (studio.draftDirty && !studio.saveEvent()) return
        if (studio.canvasController.layoutDirty) studio.canvasController.saveLayout()
    }
    function browse() {
        window.showLibrary = true
        Qt.callLater(() => query.forceActiveFocus())
    }
    onClosing: event => {
        if (studio.saving) { event.accepted = false; return }
        if (!unsaved) return
        event.accepted = false
        closeWindowAfterDiscard = true
        discardDraft.open()
    }
    Shortcut { sequences: [StandardKey.Save]; enabled: studio.editing && window.unsaved && !studio.saving; onActivated: window.save() }

    header: ToolBar {
        enabled: !studio.saving
        background: Rectangle { color: "#202b38" }
        contentItem: RowLayout {
            spacing: 8
            Item { implicitWidth: 8 }
            Button {
                text: qsTr("Events")
                checkable: true
                checked: window.showLibrary
                onClicked: window.browse()
            }
            Button {
                text: qsTr("New event")
                enabled: studio.checkout.length > 0
                onClicked: newEvent.open()
            }
            Button {
                visible: studio.editing && window.showLibrary
                text: qsTr("Back to editing")
                onClicked: window.showLibrary = false
            }
            Item { Layout.fillWidth: true }
            Button {
                id: tools
                objectName: "studioProjectMenu"
                text: qsTr("Project ▾")
                onClicked: projectMenu.open()
                Menu {
                    id: projectMenu
                    MenuItem { text: qsTr("Open project folder…"); onTriggered: projectPicker.open() }
                    MenuItem {
                        text: qsTr("Game data")
                        enabled: studio.editing && !window.editingChapter
                        onTriggered: { studio.selectWorkspace(true, true); window.showLibrary = false }
                    }
                    MenuItem {
                        objectName: "openAssetRegister"
                        text: qsTr("Assets and credits")
                        enabled: studio.checkout.length > 0
                        onTriggered: { if (studio.openAssets()) assetEditor.open() }
                    }
                    MenuItem {
                        objectName: "openContentRevision"
                        text: qsTr("Review content revision")
                        enabled: studio.checkout.length > 0
                        onTriggered: revisionReview.open()
                    }
                    MenuItem {
                        objectName: "openRevisionComparison"
                        text: qsTr("Compare revisions")
                        enabled: studio.checkout.length > 0
                        onTriggered: revisionComparison.open()
                    }
                }
            }
            Item { implicitWidth: 8 }
        }
    }
    FolderDialog {
        id: projectPicker
        title: qsTr("Open the game project folder")
        onAccepted: {
            if (studio.openCheckout(decodeURIComponent(selectedFolder.toString().replace(/^file:\/\/(?:localhost)?/, "")))) {
                window.browse()
                window.updateFilter()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        ColumnLayout {
            visible: window.showLibrary
            enabled: !studio.saving
            Layout.fillWidth: true
            Layout.fillHeight: true
            Label { text: qsTr("Events"); font.pixelSize: 28; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: query
                    objectName: "librarySearch"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search event titles or story text")
                    Accessible.name: qsTr("Search events")
                    onTextEdited: window.updateFilter()
                    Keys.onDownPressed: { library.forceActiveFocus(); if (library.currentIndex < 0) library.currentIndex = 0 }
                }
                ComboBox {
                    id: kinds
                    Accessible.name: qsTr("What to browse")
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("All events"), value: "events" },
                        { label: qsTr("Company events"), value: "flow" },
                        { label: qsTr("Childhood scenes"), value: "chapter" },
                        { label: qsTr("Scenarios"), value: "scenario" },
                        { label: qsTr("All game content"), value: "" }
                    ]
                    onActivated: window.updateFilter()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16
                ListView {
                    id: library
                    objectName: "studioLibrary"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: window.width * 0.6
                    model: studio.rows
                    clip: true
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    Accessible.name: qsTr("Event list")
                    Keys.onReturnPressed: window.editSelected()
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width - 12
                        highlighted: ListView.isCurrentItem
                        text: modelData.title || qsTr("Content file")
                        onClicked: { library.currentIndex = index; library.forceActiveFocus() }
                        onDoubleClicked: window.editSelected()
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: library.count === 0
                        text: studio.checkout.length ? qsTr("No events match this search.") : qsTr("Open a project folder to begin.")
                    }
                }
                Rectangle {
                    Layout.preferredWidth: window.width * 0.32
                    Layout.fillHeight: true
                    color: "#202b38"
                    radius: 8
                    ScrollView {
                        id: overview
                        anchors.fill: parent
                        anchors.margins: 16
                        contentWidth: availableWidth
                        readonly property var entry: library.currentItem ? library.currentItem.modelData : ({})
                        ColumnLayout {
                            width: overview.availableWidth
                            spacing: 12
                            Label {
                                Layout.fillWidth: true
                                text: overview.entry.title || qsTr("Choose an event")
                                wrapMode: Text.Wrap
                                font.bold: true
                                font.pixelSize: 20
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!overview.entry.state
                                text: overview.entry.state || ""
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!overview.entry.synopsis
                                text: overview.entry.synopsis || ""
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !!overview.entry.diagnostic
                                text: overview.entry.diagnostic || ""
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                            }
                            Button {
                                objectName: "editSelectedDefinition"
                                visible: !!overview.entry.title && overview.entry.validation === "Validated"
                                text: ["flow", "chapter"].includes(overview.entry.kind) ? qsTr("Edit event") : qsTr("Open editor")
                                onClicked: window.editSelected()
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: studio.editing && !window.showLibrary
            enabled: !studio.saving
            Layout.fillWidth: true
            Layout.fillHeight: true
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: window.editingChapter ? studio.chapter.title : studio.worldMode ? qsTr("Game content") : studio.workingTitle
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    font.pixelSize: 24
                    font.bold: true
                }
                Button { objectName: "undoStudioContent"; text: qsTr("Undo"); enabled: studio.canUndo; onClicked: studio.undo() }
                Button { objectName: "redoStudioContent"; text: qsTr("Redo"); enabled: studio.canRedo; onClicked: studio.redo() }
                Button { objectName: "saveExistingDraft"; text: qsTr("Save to project"); enabled: window.unsaved; onClicked: window.save() }
                Button {
                    objectName: "closeWorkingDraft"
                    text: qsTr("Close")
                    onClicked: {
                        if (window.unsaved) {
                            window.closeWindowAfterDiscard = false
                            discardDraft.open()
                        } else { studio.closeDraft(); window.browse() }
                    }
                }
            }
            RowLayout {
                visible: !studio.worldMode && !window.editingChapter
                Layout.fillWidth: true
                TabBar {
                    currentIndex: window.page
                    onCurrentIndexChanged: {
                        if (currentIndex !== 0 && eventStory.changed && !eventStory.applyEdits()) {
                            window.page = 0
                            setCurrentIndex(0)
                        } else window.page = currentIndex
                    }
                    TabButton { text: qsTr("Story"); width: implicitWidth }
                    TabButton { text: qsTr("Steps"); width: implicitWidth }
                    TabButton { objectName: "showStudioCanvas"; text: qsTr("Graph"); width: implicitWidth }
                    TabButton { objectName: "showStudioSettings"; text: qsTr("Preview in game"); width: implicitWidth }
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "openGroupTools"
                    text: qsTr("Reusable steps…")
                    onClicked: groupTools.open()
                }
            }
            ChildhoodScene {
                id: childhoodScene
                visible: window.editingChapter
                Layout.fillWidth: true
                Layout.fillHeight: true
                author: studio
            }
            WorldForms {
                visible: studio.worldMode
                Layout.fillWidth: true
                Layout.fillHeight: true
                author: studio.worldController
                session: studio
                onCloseRequested: window.browse()
            }
            EventStory {
                id: eventStory
                visible: !studio.worldMode && !window.editingChapter && window.page === 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                author: studio
                onStepRequested: (step, choice) => {
                    studio.selectEventStep(step, choice)
                    window.page = 1
                }
            }
            RowLayout {
                visible: !studio.worldMode && !window.editingChapter && (window.page === 1 || window.page === 2)
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                FlowForms {
                    id: outline
                    visible: window.page === 1
                    Layout.preferredWidth: Math.min(window.width * 0.33, 340)
                    Layout.fillHeight: true
                    author: studio
                    showFields: false
                    fillOutline: true
                }
                FlowCanvas {
                    visible: window.page === 2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    author: studio.canvasController
                    inspector: studio.probeController
                }
                Rectangle {
                    Layout.fillWidth: window.page === 1
                    Layout.preferredWidth: window.page === 2 ? Math.min(window.width * 0.37, 410) : 420
                    Layout.fillHeight: true
                    radius: 8
                    color: "#202b38"
                    ScrollView {
                        id: inspector
                        objectName: "studioInspector"
                        anchors.fill: parent
                        anchors.margins: 14
                        contentWidth: availableWidth
                        FlowForms {
                            width: inspector.availableWidth
                            author: studio
                            showOutline: false
                            outlineObjectName: "hiddenFormOutline"
                            outlineFocusTarget: outline.outlineView
                        }
                    }
                }
            }
            ScrollView {
                id: preview
                objectName: "studioPreview"
                visible: !studio.worldMode && !window.editingChapter && window.page === 3
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                FlowInspection { width: Math.min(900, preview.availableWidth); inspector: studio.probeController }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: window.statusMessage
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
        }
    }

    Dialog {
        id: newEvent
        anchors.centerIn: parent
        modal: true
        title: qsTr("Create an event")
        width: Math.min(520, window.width - 36)
        closePolicy: Popup.CloseOnEscape
        onOpened: eventTitle.forceActiveFocus()
        height: Math.min(490, window.height - 36)
        contentItem: ScrollView {
            id: newEventScroll
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: newEventScroll.availableWidth
                spacing: 10
                Label { text: qsTr("Name this event") }
                TextField { id: eventTitle; objectName: "newEventTitle"; Layout.fillWidth: true; maximumLength: 120; placeholderText: qsTr("Example: A letter from the mayor") }
                Label { text: qsTr("What should the player see?") }
                ComboBox {
                    id: presentation
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("Dialogue with a choice"), value: "dialogue" },
                        { label: qsTr("News or a notification"), value: "news" },
                        { label: qsTr("Nothing on screen"), value: "logic" },
                        { label: qsTr("A childhood scene"), value: "chapter" }
                    ]
                }
                Label { visible: presentation.currentValue !== "chapter"; text: qsTr("Who can this happen to?") }
                ComboBox {
                    id: scope
                    visible: presentation.currentValue !== "chapter"
                    Layout.fillWidth: true
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("A company"), value: "company" },
                        { label: qsTr("The whole game world"), value: "world" },
                        { label: qsTr("An individual state"), value: "state" }
                    ]
                }
                Label { visible: presentation.currentValue !== "logic"; text: qsTr("Write the first message") }
                ScrollView {
                    visible: presentation.currentValue !== "logic"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 100
                    TextArea { id: eventBody; objectName: "newEventBody"; wrapMode: TextEdit.Wrap; placeholderText: qsTr("What happens?") }
                }
                Label {
                    Layout.fillWidth: true
                    text: presentation.currentValue === "chapter"
                        ? qsTr("After creating it, choose a date and write the player’s choices.")
                        : qsTr("After creating it, choose when it happens and which scenarios use it.")
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: studio.draftMessage.length > 0
                    text: studio.draftMessage
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    Button { text: qsTr("Cancel"); onClicked: newEvent.close() }
                    Button {
                        objectName: "createFlow"
                        text: qsTr("Create event")
                        enabled: eventTitle.text.trim().length > 0 && (presentation.currentValue === "logic" || eventBody.text.trim().length > 0)
                        onClicked: {
                            const created = presentation.currentValue === "chapter"
                                ? studio.createChapter(eventTitle.text, eventBody.text)
                                : studio.createEvent(eventTitle.text, presentation.currentValue, eventBody.text, scope.currentValue)
                            if (created) {
                                newEvent.close()
                                eventTitle.clear()
                                eventBody.clear()
                                window.page = 0
                                window.showLibrary = false
                            }
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: discardDraft
        anchors.centerIn: parent
        modal: true
        title: qsTr("Discard unsaved changes?")
        standardButtons: Dialog.Discard | Dialog.Cancel
        onDiscarded: {
            studio.closeDraft()
            if (window.closeWindowAfterDiscard) window.close()
            else window.browse()
            close()
        }
    }
    SnapshotReview { id: revisionReview; finalizer: studio.snapshotController }
    RevisionComparison { id: revisionComparison; reviewer: studio.revisionController; checkout: studio.checkout }
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
    Component.onCompleted: window.updateFilter()
}
