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
    property bool ruleMode: false
    readonly property string base: "/definitions/" + author.scenario
    readonly property var rulePages: [
        {key: base + "/founding/generation", label: qsTr("New campaign rules")},
        {key: base + "/management/people/first_names", label: qsTr("Given names")},
        {key: base + "/management/people/last_names", label: qsTr("Surnames")},
        {key: base + "/founding/generation/company_prefixes", label: qsTr("Company name beginnings")},
        {key: base + "/founding/generation/company_suffixes", label: qsTr("Company name endings")}
    ].filter(row => author.sections.some(section => section.key === row.key)).concat(sourceRules())
    function sourceRules() {
        const sections = author.sections
        const sources = new Set(sections.filter(row => row.key.startsWith(base + "/lab/acquisition/") && row.key.endsWith("/source"))
                                        .map(row => row.key.slice(0, -7)))
        return sections.filter(row => sources.has(row.key)).map(row => ({key: row.key, label: qsTr("Data source · %1").arg(row.label.trim())}))
    }
    function openRules() {
        if (!author.catalogMode && !author.setCatalogMode(true)) return
        author.filter("")
        if (!rulePages.length) return
        ruleMode = true
        mapMode = false
        catalogRecords = false
        author.selectSection(rulePages[0].key)
    }
    property bool mapMode: false
    property bool catalogRecords: true
    property bool wasCatalogMode: false
    property bool showFields: false
    property bool showAdvancedTemplates: false
    readonly property bool hasPending: Object.keys(pending).length > 0
    readonly property bool hasEditableFields: author ? (author.adding || author.fields.some(f => f.control !== "note")) : false
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
    function optionIndex(options, val) {
        if (!options || !options.length) return -1
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === val) return i
        }
        return val === "" && options[0].value === "" ? 0 : -1
    }
    function formatUnitPreview(label, help, textValue) {
        if (typeof textValue !== "string" || !/^-?[0-9]+$/.test(textValue.trim())) return ""
        const n = Number(textValue.trim())
        if (!Number.isSafeInteger(n)) return ""
        const info = (label + " " + help).toLowerCase()
        if (info.indexOf("thousandths of a constant-2014 cent") >= 0 || info.indexOf("millicents") >= 0) {
            return "= " + (n / 1000).toFixed(3) + " ¢/kWh"
        }
        if (info.indexOf("constant-2014 cents") >= 0 || info.indexOf("(cents)") >= 0) {
            const sign = n < 0 ? "-" : ""
            const abs = Math.abs(n)
            const dollars = Math.floor(abs / 100).toLocaleString("en-US")
            const cents = String(abs % 100).padStart(2, "0")
            return "= " + sign + "$" + dollars + "." + cents
        }
        if (info.indexOf("basis points") >= 0 || info.indexOf("(bp)") >= 0) {
            return "= " + (n / 100).toFixed(2) + "%"
        }
        if (info.indexOf("milli-cu/day") >= 0 || info.indexOf("milli-cu per day") >= 0) {
            return "= " + (n / 1000).toFixed(2) + " CU/day"
        }
        if (info.indexOf("milli-cu") >= 0) {
            return "= " + (n / 1000).toFixed(2) + " CU"
        }
        if (info.indexOf("pue × 1,000") >= 0 || info.indexOf("pue × 1000") >= 0) {
            return "= " + (n / 1000).toFixed(2) + " PUE"
        }
        if (info.indexOf("days since 2014-01-01") >= 0 && n >= 0 && n <= 100000) {
            const d = new Date(Date.UTC(2014, 0, 1) + n * 86400000)
            return "= " + d.toISOString().slice(0, 10)
        }
        return ""
    }
    function syncSectionSelection() {
        const list = sections.model
        if (!list || !list.length) {
            sections.currentIndex = -1
            return
        }
        let found = -1
        if (!author.catalogMode && forms.mapMode) {
            found = list.findIndex(r => r.id === author.selectedState)
        } else {
            found = list.findIndex(r => r.key === author.path)
            if (found < 0 && author.catalogMode && forms.catalogRecords) {
                found = list.findIndex(r => author.path.startsWith(r.key + "/"))
            }
        }
        if (found >= 0) sections.currentIndex = found
        else if (sections.currentIndex < 0 && list.length > 0) sections.currentIndex = 0
    }
    Connections {
        target: forms.author
        function onDocumentReplaced() {
            forms.mapMode = false
            forms.ruleMode = false
            forms.wasCatalogMode = false
            forms.catalogRecords = true
            forms.showFields = false
        }
        function onChanged() {
            if (forms.author.catalogMode) forms.wasCatalogMode = true
            if (forms.observedTicket !== forms.author.ticket) {
                forms.observedTicket = forms.author.ticket
                forms.pending = ({})
                if (inspector.contentItem) inspector.contentItem.contentY = 0
            }
            Qt.callLater(forms.syncSectionSelection)
        }
    }
    onCatalogRecordsChanged: Qt.callLater(forms.syncSectionSelection)
    onMapModeChanged: Qt.callLater(forms.syncSectionSelection)
    RowLayout {
        visible: !author.catalogMode && !forms.mapMode && !forms.wasCatalogMode
        Layout.fillWidth: true
        Label { text: qsTr("Test preset"); font.bold: true }
        ComboBox {
            objectName: "worldScenario"
            Layout.fillWidth: true
            enabled: !forms.hasPending && !author.adding && !author.hasStaged
            model: author.scenarios
            textRole: "label"; valueRole: "value"
            currentIndex: forms.optionIndex(author.scenarios, author.scenario)
            popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
            Accessible.name: qsTr("Working scenario")
            onActivated: author.selectScenario(currentValue)
        }
    }
    Label {
        visible: session.existingDraft
        Layout.fillWidth: true
        text: qsTr("Draft: %1").arg(session.workingFile) + (session.draftDirty ? qsTr(" · unsaved changes") : "")
        wrapMode: Text.WrapAnywhere
        color: "#a8c1d8"
    }
    Flow {
        Layout.fillWidth: true; spacing: 8
        visible: author.hasStaged
        Button { objectName: "applyWorldBatch"; text: qsTr("Apply staged sections together"); enabled: !forms.hasPending; onClicked: author.applyStaged() }
        Button { text: qsTr("Discard staged sections"); enabled: !forms.hasPending; onClicked: author.discardStaged() }
        Label { text: qsTr("Apply staged changes together before saving to the project.") }
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
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Button {
                    text: qsTr("Campaign rules")
                    visible: author.catalogMode || forms.wasCatalogMode
                    checked: forms.ruleMode
                    enabled: !forms.hasPending && !author.adding && !author.hasStaged
                    onClicked: forms.openRules()
                }
                Button {
                    visible: author.catalogMode || forms.wasCatalogMode
                    text: qsTr("Templates")
                    checkable: true
                    checked: author.catalogMode && forms.catalogRecords && !forms.mapMode
                    enabled: !forms.hasPending && !author.adding && !author.hasStaged
                    onClicked: {
                        if (!author.catalogMode) author.setCatalogMode(true)
                        forms.mapMode = false
                        forms.ruleMode = false
                        forms.catalogRecords = true
                        if (author.catalogEntries.length && (!author.selectedIdentity || author.selectedFamily === "flow"))
                            author.selectCatalogEntry(author.catalogEntries[0].key)
                    }
                }
                Button {
                    text: qsTr("All settings")
                    checkable: true
                    checked: !forms.ruleMode && !forms.mapMode && (!author.catalogMode || !forms.catalogRecords)
                    enabled: !forms.hasPending && !author.adding && !author.hasStaged
                    onClicked: {
                        if (forms.wasCatalogMode && !author.catalogMode) author.setCatalogMode(true)
                        forms.mapMode = false
                        forms.ruleMode = false
                        forms.catalogRecords = false
                        if (author.sections.length && !author.sections.some(r => r.key === author.path))
                            author.selectSection(author.sections[0].key)
                    }
                }
                Button {
                    objectName: "worldMapTab"
                    visible: author.states.length > 0
                    text: qsTr("US states map")
                    checkable: true
                    checked: !author.catalogMode && forms.mapMode
                    enabled: !forms.hasPending && !author.adding && !author.hasStaged && author.states.length > 0
                    onClicked: {
                        if (author.catalogMode) author.setCatalogMode(false)
                        forms.ruleMode = false
                        forms.mapMode = true
                        const selected = author.selectedState || author.states[0].id
                        author.selectState(selected)
                    }
                }
            }
            ComboBox {
                id: categoryFilter
                objectName: "catalogCategoryFilter"
                visible: author.catalogMode && forms.catalogRecords && !forms.mapMode
                Layout.fillWidth: true
                enabled: !forms.hasPending && !author.adding
                textRole: "label"
                valueRole: "value"
                model: [
                    { label: qsTr("All game records"), value: "" },
                    { label: qsTr("Research topics"), value: "research" },
                    { label: qsTr("AI models & evaluations"), value: "models" },
                    { label: qsTr("Datasets"), value: "datasets" },
                    { label: qsTr("Hardware & infrastructure"), value: "infrastructure" },
                    { label: qsTr("Products & markets"), value: "markets" },
                    { label: qsTr("Rival companies"), value: "rivals" },
                    { label: qsTr("People, board & roles"), value: "people" },
                    { label: qsTr("Business & funding offers"), value: "offers" },
                    { label: qsTr("Story variables & events"), value: "events" },
                    { label: qsTr("Localized messages"), value: "text" }
                ]
                currentIndex: forms.optionIndex(model, author.familyFilter)
                popup.height: Math.min(320, popup.contentItem.implicitHeight + 16)
                Accessible.name: qsTr("Filter game records by category")
                onActivated: {
                    author.setFamilyFilter(currentValue)
                    if (author.catalogEntries.length) author.selectCatalogEntry(author.catalogEntries[0].key)
                }
            }
            TextField {
                objectName: "worldSectionSearch"
                visible: !forms.ruleMode && (author.catalogMode || !forms.mapMode)
                Layout.fillWidth: true
                maximumLength: 256
                enabled: !forms.hasPending && !author.adding
                placeholderText: author.catalogMode && forms.catalogRecords ? qsTr("Search records by name or ID") : qsTr("Find a setting or section")
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
                    model: forms.ruleMode ? forms.rulePages : author.catalogMode ? (forms.catalogRecords ? author.catalogEntries : author.sections) : (forms.mapMode ? author.states : author.sections)
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
                        id: sectionDelegate
                        required property var modelData
                        required property int index
                        readonly property bool isRecord: author.catalogMode && forms.catalogRecords && !forms.mapMode
                        width: ListView.view.width
                        text: modelData.label
                        highlighted: !author.catalogMode && forms.mapMode ? modelData.id === author.selectedState : ListView.isCurrentItem
                        contentItem: RowLayout {
                            spacing: 8
                            Label {
                                Layout.fillWidth: true
                                text: sectionDelegate.isRecord ? (sectionDelegate.modelData.title || sectionDelegate.modelData.label) : sectionDelegate.modelData.label
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                            }
                            Label {
                                visible: sectionDelegate.isRecord && !!(sectionDelegate.modelData.category)
                                text: sectionDelegate.modelData.category || ""
                                textFormat: Text.PlainText
                                font.pixelSize: 11
                                color: "#a8c1d8"
                            }
                        }
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
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: forms.ruleMode ? (forms.rulePages.find(row => row.key === author.path) || {}).label || author.sectionTitle : author.sectionTitle || author.path
                        wrapMode: Text.Wrap
                        font.bold: true
                        font.pixelSize: 18
                    }
                    Button {
                        visible: author.catalogMode && !author.adding && author.selectedFamily === "flow"
                        text: qsTr("Open in Event Editor")
                        enabled: !forms.hasPending && !author.hasStaged
                        onClicked: author.openSelectedFlow()
                    }
                }
                Label {
                    visible: author.adding
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    color: "#a8c1d8"
                    text: qsTr("New entry from a validated template. Adjust its fields below, then click Add reviewed entry.")
                }
                Repeater {
                    model: author.fields
                    delegate: ColumnLayout {
                        id: fieldRow
                        required property var modelData
                        readonly property var field: modelData
                        visible: author.adding || fieldRow.field.control !== "note" || !["revision", "deprecated", "kind"].includes(fieldRow.field.key)
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: fieldRow.field.label; wrapMode: Text.Wrap; font.bold: true }
                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: fieldRow.field.control === "choice" ? choice : fieldRow.field.control === "note" ? note : input
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: fieldRow.field.help.length > 0
                            text: fieldRow.field.help
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            opacity: 0.85
                        }
                        Label {
                            visible: forms.value(fieldRow.field) !== fieldRow.field.before
                            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                            color: "#ffd38a"
                            text: qsTr("Before: %1 → Proposed: %2").arg(fieldRow.field.before).arg(forms.value(fieldRow.field))
                        }
                        Component {
                            id: note
                            Label { text: fieldRow.field.value; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
                        }
                        Component {
                            id: input
                            RowLayout {
                                readonly property string unitPreview: fieldRow.field.control === "integer"
                                    ? forms.formatUnitPreview(fieldRow.field.label, fieldRow.field.help, fieldInput.text) : ""
                                TextField {
                                    id: fieldInput
                                    objectName: "worldInput/" + fieldRow.field.key
                                    Layout.fillWidth: true
                                    text: forms.value(fieldRow.field)
                                    maximumLength: fieldRow.field.control === "integer" ? 21 : 4096
                                    inputMethodHints: fieldRow.field.control === "integer" ? Qt.ImhPreferNumbers : Qt.ImhNone
                                    Accessible.name: fieldRow.field.label
                                    Accessible.description: fieldRow.field.help
                                    onTextEdited: forms.stage(fieldRow.field.key, text, fieldRow.field.value)
                                    onEditingFinished: forms.stage(fieldRow.field.key, text, fieldRow.field.value)
                                }
                                Label {
                                    visible: unitPreview.length > 0
                                    text: unitPreview
                                    color: "#8fe2b4"
                                    font.bold: true
                                    font.pixelSize: 12
                                }
                            }
                        }
                        Component {
                            id: choice
                            ComboBox {
                                objectName: "worldChoice/" + fieldRow.field.key
                                model: fieldRow.field.options
                                textRole: "label"; valueRole: "value"
                                currentIndex: forms.optionIndex(fieldRow.field.options, forms.value(fieldRow.field))
                                Component.onCompleted: currentIndex = forms.optionIndex(fieldRow.field.options, forms.value(fieldRow.field))
                                onModelChanged: currentIndex = forms.optionIndex(fieldRow.field.options, forms.value(fieldRow.field))
                                popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
                                Accessible.name: fieldRow.field.label
                                Accessible.description: fieldRow.field.help
                                onActivated: forms.stage(fieldRow.field.key, currentValue, fieldRow.field.value)
                            }
                        }
                    }
                }
                Flow {
                    visible: forms.hasEditableFields || author.canRemove || author.adding
                    Layout.fillWidth: true
                    spacing: 8
                    Button { objectName: "applyWorldSection"; visible: forms.hasEditableFields; text: author.adding ? qsTr("Add reviewed entry") : author.hasStaged ? qsTr("Apply section with staged changes") : qsTr("Apply section"); enabled: forms.hasPending; onClicked: forms.applySection() }
                    Button { objectName: "stageWorldSection"; visible: forms.hasEditableFields && !author.adding && !forms.ruleMode; text: qsTr("Stage section"); enabled: forms.hasPending; onClicked: author.stage(author.ticket, forms.pending) }
                    Button { visible: forms.hasEditableFields; text: qsTr("Reset unapplied fields"); enabled: forms.hasPending; onClicked: forms.pending = ({}) }
                    Button { text: qsTr("Cancel new entry"); visible: author.adding; onClicked: author.cancelAppend() }
                    Button { objectName: "removeWorldEntry"; text: qsTr("Remove selected entry"); visible: author.canRemove; enabled: !forms.hasPending && !author.hasStaged; onClicked: author.removeEntry() }
                }
                RowLayout {
                    visible: author.canAppend && !author.adding
                    Layout.fillWidth: true
                    ComboBox {
                        id: entryTemplate
                        objectName: "worldEntryTemplate"
                        Layout.fillWidth: true
                        model: author.templates
                        textRole: "label"; valueRole: "value"
                        popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
                        Accessible.name: qsTr("New entry template")
                    }
                    Button { objectName: "appendWorldEntry"; text: qsTr("Add from template"); enabled: !forms.hasPending && !author.hasStaged; onClicked: author.beginAppend(entryTemplate.currentValue) }
                }
                Label { Layout.fillWidth: true; text: author.message || session.draftMessage; visible: text.length > 0; wrapMode: Text.WrapAnywhere; font.bold: true }
                Label { Layout.fillWidth: true; text: author.stagedSummary; visible: text.length > 0; wrapMode: Text.WrapAnywhere }
                CatalogActions {
                    visible: author.catalogMode && !forms.ruleMode && !author.adding
                    Layout.fillWidth: true
                    author: forms.author
                    ready: !forms.hasPending && !author.hasStaged
                }
                GroupBox {
                    visible: !author.catalogMode
                    title: qsTr("Test the actual opening")
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        ComboBox {
                            id: headquarters
                            objectName: "worldPreviewHeadquarters"
                            Layout.fillWidth: true
                            model: author.headquarters
                            popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
                            Accessible.name: qsTr("Preview headquarters")
                        }
                        Button { objectName: "previewWorldOpening"; text: qsTr("Inspect opening and exact save"); enabled: !forms.hasPending && headquarters.currentIndex >= 0; onClicked: author.preview(headquarters.currentText) }
                        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Runs the selected scenario on isolated state. Registered facts show actual opening values and targets. Applied edits invalidate this report.") }
                        Label { objectName: "worldOpeningReport"; Layout.fillWidth: true; text: author.report; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
                    }
                }
                Button {
                    visible: !author.catalogMode
                    text: forms.showAdvancedTemplates ? qsTr("Hide advanced preset tools ▴") : qsTr("Advanced preset tools ▾")
                    onClicked: forms.showAdvancedTemplates = !forms.showAdvancedTemplates
                }
                GroupBox {
                    visible: !author.catalogMode && forms.showAdvancedTemplates
                    title: qsTr("Scenario templates")
                    Layout.fillWidth: true
                    ColumnLayout {
                        width: parent.width
                        TextField { id: newScenario; objectName: "newWorldScenarioId"; Layout.fillWidth: true; maximumLength: 128; placeholderText: qsTr("scenario.new_campaign"); Accessible.name: qsTr("New scenario ID") }
                        Button { objectName: "duplicateWorldScenario"; text: qsTr("Duplicate current scenario"); enabled: !forms.hasPending && newScenario.text.length > 0; onClicked: { if (author.duplicate(newScenario.text.trim())) newScenario.clear() } }
                        ComboBox {
                            id: systemsTemplate
                            Layout.fillWidth: true
                            model: author.scenarios
                            textRole: "label"; valueRole: "value"
                            popup.height: Math.min(300, popup.contentItem.implicitHeight + 16)
                            Accessible.name: qsTr("Missing systems template")
                        }
                        Button { text: qsTr("Add missing systems from template"); enabled: !forms.hasPending; onClicked: author.addMissing(systemsTemplate.currentValue) }
                        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Copies only absent sections as one validated change. Existing settings and stable identities are retained. Shared world definitions affect every scenario that references them.") }
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
