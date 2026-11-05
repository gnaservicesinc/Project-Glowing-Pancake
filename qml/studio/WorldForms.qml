import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: forms
    required property QtObject author
    required property QtObject session
    signal closeRequested()
    property var pending: ({})
    property string observedTicket: ""
    property bool mapMode: false
    property bool catalogRecords: true
    property bool showFields: false
    readonly property bool hasPending: Object.keys(pending).length > 0
    onPendingChanged: { if (author) author.setUnapplied(pending) }
    function stage(key, value, original) {
        const next = Object.assign({}, pending)
        if (value === original) delete next[key]
        else next[key] = value
        pending = next
    }
    function value(field) {
        return Object.prototype.hasOwnProperty.call(pending, field.key) ? pending[field.key] : field.value
    }
    function applySection() {
        return author.apply(author.ticket, pending)
    }
    Connections {
        target: forms.author
        function onChanged() {
            if (forms.observedTicket !== forms.author.ticket) {
                forms.observedTicket = forms.author.ticket
                forms.pending = ({})
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Scenario"); font.bold: true }
        ComboBox {
            objectName: "worldScenario"
            Layout.fillWidth: true
            enabled: !forms.hasPending && !author.adding && !author.hasStaged
            model: author.scenarios
            textRole: "label"; valueRole: "value"
            currentIndex: indexOfValue(author.scenario)
            Accessible.name: qsTr("Working scenario")
            onActivated: author.selectScenario(currentValue)
        }
        Button { text: qsTr("Undo"); enabled: session.canUndo && !forms.hasPending; onClicked: session.undo() }
        Button { text: qsTr("Redo"); enabled: session.canRedo && !forms.hasPending; onClicked: session.redo() }
        Button { text: qsTr("Save draft"); enabled: session.existingDraft && session.draftDirty && !forms.hasPending && !author.hasStaged; onClicked: session.saveDraft("") }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: filename
            objectName: "worldDraftFilename"
            Layout.fillWidth: true
            maximumLength: 133
            placeholderText: qsTr("new-world-draft.json")
            Accessible.name: qsTr("New world draft filename")
        }
        Button { objectName: "saveWorldDraft"; text: qsTr("Save as draft"); enabled: filename.text.length > 0 && !forms.hasPending && !author.hasStaged; onClicked: { if (session.saveDraft(filename.text.trim())) filename.clear() } }
        Button { text: qsTr("Close draft"); onClicked: forms.closeRequested() }
    }
    Label { Layout.fillWidth: true; text: session.workingFile + (session.draftDirty ? qsTr(" · unsaved content") : ""); wrapMode: Text.WrapAnywhere }
    Flow {
        Layout.fillWidth: true; spacing: 8
        visible: author.hasStaged
        Button { objectName: "applyWorldBatch"; text: qsTr("Apply staged sections together"); enabled: !forms.hasPending; onClicked: author.applyStaged() }
        Button { text: qsTr("Discard staged sections"); enabled: !forms.hasPending; onClicked: author.discardStaged() }
        Label { text: qsTr("Staged fields remain outside the draft until the whole batch validates.") }
    }
    RowLayout {
        visible: forms.width < 1000
        Button { objectName: "worldSectionsPane"; text: qsTr("Sections"); checkable: true; checked: !forms.showFields; onClicked: forms.showFields = false }
        Button { objectName: "worldFieldsPane"; text: qsTr("Fields and preview"); checkable: true; checked: forms.showFields; onClicked: forms.showFields = true }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 16
        ColumnLayout {
            visible: forms.width >= 1000 || !forms.showFields
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.preferredWidth: forms.width * 0.38
            RowLayout {
                Button { text: qsTr("Sections"); checkable: true; checked: !forms.mapMode && (!author.catalogMode || !forms.catalogRecords); enabled: !forms.hasPending; onClicked: { forms.mapMode = false; forms.catalogRecords = false } }
                Button { visible: author.catalogMode; text: qsTr("Catalog records"); checkable: true; checked: forms.catalogRecords; enabled: !forms.hasPending; onClicked: forms.catalogRecords = true }
                Button { objectName: "worldMapTab"; visible: !author.catalogMode; text: qsTr("States map and list"); checkable: true; checked: forms.mapMode; enabled: !forms.hasPending && author.states.length > 0; onClicked: forms.mapMode = true }
            }
            TextField {
                objectName: "worldSectionSearch"
                visible: author.catalogMode || !forms.mapMode
                Layout.fillWidth: true
                maximumLength: 256
                enabled: !forms.hasPending && !author.adding
                placeholderText: author.catalogMode ? qsTr("Find catalog family, title or ID") : qsTr("Find scoring, policy, rival, region or field")
                Accessible.name: qsTr("Search authoring records and sections")
                onTextEdited: author.filter(text)
            }
            DomesticMap {
                objectName: "worldDomesticMap"
                visible: forms.mapMode && !author.catalogMode
                Layout.fillWidth: true; Layout.preferredHeight: Math.max(70, Math.min(180, forms.height * 0.25))
                geometry: author.mapGeometry
                selectedId: author.selectedState
                onStatePicked: id => { if (!forms.hasPending && !author.adding) { author.selectState(id); forms.showFields = true } }
            }
            Button { visible: forms.mapMode && !author.catalogMode; text: qsTr("Map credits"); onClicked: mapCredits.open() }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 60
                ListView {
                    id: sections
                    objectName: "worldSectionList"
                    clip: true
                    activeFocusOnTab: true
                    enabled: !forms.hasPending && !author.adding
                    model: author.catalogMode ? (forms.catalogRecords ? author.catalogEntries : author.sections) : (forms.mapMode ? author.states : author.sections)
                    keyNavigationEnabled: true
                    Accessible.name: forms.mapMode ? qsTr("Domestic state profiles") : qsTr("Scenario and world sections")
                    Keys.onReturnPressed: {
                        const row = model[currentIndex]
                        if (!row) return
                        if (author.catalogMode && forms.catalogRecords) author.selectCatalogEntry(row.key)
                        else if (!author.catalogMode && forms.mapMode) author.selectState(row.id)
                        else author.selectSection(row.key)
                        forms.showFields = true
                    }
                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        text: modelData.label
                        highlighted: !author.catalogMode && forms.mapMode ? modelData.id === author.selectedState : ListView.isCurrentItem
                        onClicked: {
                            sections.currentIndex = index
                            if (author.catalogMode && forms.catalogRecords) author.selectCatalogEntry(modelData.key)
                            else if (!author.catalogMode && forms.mapMode) author.selectState(modelData.id)
                            else author.selectSection(modelData.key)
                            forms.showFields = true
                        }
                    }
                }
            }
        }
        ScrollView {
            id: inspector
            objectName: "worldFieldScroll"
            visible: forms.width >= 1000 || forms.showFields
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.preferredWidth: forms.width * 0.62
            contentWidth: availableWidth
            ColumnLayout {
                width: inspector.availableWidth
                Label { Layout.fillWidth: true; text: author.path; wrapMode: Text.WrapAnywhere; font.bold: true }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: author.adding ? qsTr("New entry from a validated template. Give it a distinct identity and adjust its terms, then apply the whole entry.")
                        : qsTr("Edit this section, then apply its fields together. Nested sections appear in the section list. Units and references use the game's typed content contract.")
                }
                CatalogActions {
                    visible: author.catalogMode && !author.adding
                    Layout.fillWidth: true
                    author: forms.author
                    ready: !forms.hasPending && !author.hasStaged
                }
                Repeater {
                    model: author.fields
                    delegate: ColumnLayout {
                        id: fieldRow
                        required property var modelData
                        readonly property var field: modelData
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: fieldRow.field.label; wrapMode: Text.Wrap; font.bold: true }
                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: fieldRow.field.control === "choice" ? choice : fieldRow.field.control === "note" ? note : input
                        }
                        Label { Layout.fillWidth: true; text: fieldRow.field.help; wrapMode: Text.Wrap }
                        Label {
                            visible: forms.value(fieldRow.field) !== fieldRow.field.before
                            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                            text: qsTr("Before: %1\nProposed: %2").arg(fieldRow.field.before).arg(forms.value(fieldRow.field))
                        }
                        Component {
                            id: note
                            Label { text: fieldRow.field.value; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
                        }
                        Component {
                            id: input
                            TextField {
                                objectName: "worldInput/" + fieldRow.field.key
                                text: forms.value(fieldRow.field)
                                maximumLength: fieldRow.field.control === "integer" ? 21 : 4096
                                inputMethodHints: fieldRow.field.control === "integer" ? Qt.ImhPreferNumbers : Qt.ImhNone
                                Accessible.name: fieldRow.field.label
                                Accessible.description: fieldRow.field.help
                                onTextEdited: forms.stage(fieldRow.field.key, text, fieldRow.field.value)
                                onEditingFinished: forms.stage(fieldRow.field.key, text, fieldRow.field.value)
                            }
                        }
                        Component {
                            id: choice
                            ComboBox {
                                objectName: "worldChoice/" + fieldRow.field.key
                                model: fieldRow.field.options
                                textRole: "label"; valueRole: "value"
                                currentIndex: indexOfValue(forms.value(fieldRow.field))
                                Accessible.name: fieldRow.field.label
                                Accessible.description: fieldRow.field.help
                                onActivated: forms.stage(fieldRow.field.key, currentValue, fieldRow.field.value)
                            }
                        }
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Button { objectName: "applyWorldSection"; text: author.adding ? qsTr("Add reviewed entry") : author.hasStaged ? qsTr("Apply section with staged changes") : qsTr("Apply section"); enabled: forms.hasPending; onClicked: forms.applySection() }
                    Button { objectName: "stageWorldSection"; text: qsTr("Stage section"); visible: !author.adding; enabled: forms.hasPending; onClicked: author.stage(author.ticket, forms.pending) }
                    Button { text: qsTr("Reset unapplied fields"); enabled: forms.hasPending; onClicked: forms.pending = ({}) }
                    Button { text: qsTr("Cancel new entry"); visible: author.adding; onClicked: author.cancelAppend() }
                    Button { objectName: "removeWorldEntry"; text: qsTr("Remove selected entry"); visible: author.canRemove; enabled: !forms.hasPending && !author.hasStaged; onClicked: author.removeEntry() }
                }
                RowLayout {
                    visible: author.canAppend && !author.adding
                    Layout.fillWidth: true
                    ComboBox { id: entryTemplate; objectName: "worldEntryTemplate"; Layout.fillWidth: true; model: author.templates; textRole: "label"; valueRole: "value"; Accessible.name: qsTr("New entry template") }
                    Button { objectName: "appendWorldEntry"; text: qsTr("Add from template"); enabled: !forms.hasPending && !author.hasStaged; onClicked: author.beginAppend(entryTemplate.currentValue) }
                }
                Label { Layout.fillWidth: true; text: author.message || session.draftMessage; visible: text.length > 0; wrapMode: Text.WrapAnywhere; font.bold: true }
                Label { Layout.fillWidth: true; text: author.stagedSummary; visible: text.length > 0; wrapMode: Text.WrapAnywhere }
                GroupBox {
                    visible: !author.catalogMode
                    title: qsTr("Scenario templates")
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        TextField { id: newScenario; objectName: "newWorldScenarioId"; Layout.fillWidth: true; maximumLength: 128; placeholderText: qsTr("scenario.new_campaign"); Accessible.name: qsTr("New scenario ID") }
                        Button { objectName: "duplicateWorldScenario"; text: qsTr("Duplicate current scenario"); enabled: !forms.hasPending && newScenario.text.length > 0; onClicked: { if (author.duplicate(newScenario.text.trim())) newScenario.clear() } }
                        ComboBox { id: systemsTemplate; Layout.fillWidth: true; model: author.scenarios; textRole: "label"; valueRole: "value"; Accessible.name: qsTr("Missing systems template") }
                        Button { text: qsTr("Add missing systems from template"); enabled: !forms.hasPending; onClicked: author.addMissing(systemsTemplate.currentValue) }
                        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Copies only absent sections as one validated change. Existing settings and stable identities are retained. Shared world definitions affect every scenario that references them.") }
                    }
                }
                GroupBox {
                    visible: !author.catalogMode
                    title: qsTr("Test the actual opening")
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        ComboBox { id: headquarters; objectName: "worldPreviewHeadquarters"; Layout.fillWidth: true; model: author.headquarters; Accessible.name: qsTr("Preview headquarters") }
                        Button { objectName: "previewWorldOpening"; text: qsTr("Inspect opening and exact save"); enabled: !forms.hasPending && headquarters.currentIndex >= 0; onClicked: author.preview(headquarters.currentText) }
                        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Runs the selected scenario on isolated state. Registered facts show actual opening values and targets. Applied edits invalidate this report.") }
                        Label { objectName: "worldOpeningReport"; Layout.fillWidth: true; text: author.report; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
                    }
                }
            }
        }
    }
    Dialog {
        id: mapCredits
        anchors.centerIn: parent
        modal: true
        title: qsTr("Prepared domestic map")
        width: Math.min(forms.width - 20, 700)
        height: Math.min(forms.height, 450)
        implicitHeight: 450
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth
            Label { width: mapCredits.availableWidth; text: author.mapCredit; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        }
    }
}
