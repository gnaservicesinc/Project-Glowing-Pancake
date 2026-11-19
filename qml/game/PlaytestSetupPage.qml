import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GameDialog {
    id: setupDialog
    objectName: "playtestSetupDialog"
    title: qsTr("Playtest Environment · Custom Setup & Start")
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(960, (Overlay.overlay ? Overlay.overlay.width : 960) - 24)
    height: Math.min(760, (Overlay.overlay ? Overlay.overlay.height : 760) - 24)

    property int textSize: 16
    property var catalog: ({})
    property var draft: ({})
    property var errors: []
    property bool valid: false
    property bool launching: false
    signal started()

    function loadDraft() {
        catalog = campaign.playtestSetupCatalog()
        const state = campaign.playtestSetupDraft()
        draft = Object.assign({}, state.draft || ({}))
        errors = state.errors || []
        valid = !!state.valid
    }

    function updateField(key, value) {
        const next = Object.assign({}, draft)
        next[key] = value
        const res = campaign.savePlaytestSetupDraft(next)
        draft = Object.assign({}, res.draft || next)
        errors = res.errors || []
        valid = !!res.valid
    }

    function toggleListValue(key, item) {
        const cur = (draft[key] || []).slice()
        const idx = cur.indexOf(item)
        if (idx >= 0) cur.splice(idx, 1)
        else cur.push(item)
        updateField(key, cur)
    }

    onOpened: loadDraft()

    component SectionCard: Rectangle {
        id: sec
        property string heading: ""
        default property alias bodyData: bodyCol.data
        Layout.fillWidth: true
        implicitHeight: bodyCol.implicitHeight + 28
        radius: 10
        color: "#1b2c3b"
        border.color: "#4a6678"
        ColumnLayout {
            id: bodyCol
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 14
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: sec.heading
                font.bold: true
                font.pixelSize: setupDialog.textSize * 1.15
                color: "#f1e3bc"
                wrapMode: Text.Wrap
            }
        }
    }

    component FieldRow: RowLayout {
        property string label: ""
        property string key: ""
        property bool numeric: false
        Layout.fillWidth: true
        spacing: 8
        Label {
            Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34)
            text: parent.label
            font.pixelSize: setupDialog.textSize * 0.9
            color: "#d4e6e2"
            wrapMode: Text.Wrap
        }
        GameField {
            objectName: "playtestField_" + parent.key
            Layout.fillWidth: true
            textSize: setupDialog.textSize * 0.92
            text: setupDialog.draft[parent.key] !== undefined ? String(setupDialog.draft[parent.key]) : ""
            Accessible.name: parent.label
            onEditingFinished: {
                if (parent.numeric) {
                    setupDialog.updateField(parent.key, text.trim())
                } else {
                    setupDialog.updateField(parent.key, text)
                }
            }
        }
    }

    component SetupChoice: RowLayout {
        required property string label
        required property string key
        required property var options
        property bool namedOptions: false
        Layout.fillWidth: true
        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: parent.label; color: "#d4e6e2"; wrapMode: Text.Wrap }
        GameSelect {
            Layout.fillWidth: true
            model: parent.options
            textRole: parent.namedOptions ? "name" : ""
            currentIndex: parent.namedOptions ? model.findIndex(row => row.id === setupDialog.draft[parent.key]) : model.indexOf(setupDialog.draft[parent.key])
            Accessible.name: parent.label
            onActivated: setupDialog.updateField(parent.key, parent.namedOptions ? model[currentIndex].id : currentText)
        }
    }

    contentItem: ColumnLayout {
        spacing: 10
        Rectangle {
            objectName: "playtestValidationBanner"
            visible: setupDialog.errors.length > 0
            Layout.fillWidth: true
            implicitHeight: errCol.implicitHeight + 16
            radius: 8
            color: "#4a2424"
            border.color: "#d47d6a"
            ColumnLayout {
                id: errCol
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Please resolve the following before starting (your edits are preserved):")
                    font.bold: true
                    font.pixelSize: setupDialog.textSize * 0.88
                    color: "#ffd6be"
                    wrapMode: Text.Wrap
                }
                Repeater {
                    model: setupDialog.errors
                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        text: "• " + modelData
                        font.pixelSize: setupDialog.textSize * 0.84
                        color: "#ffece0"
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        ScrollView {
            id: formScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

            ColumnLayout {
                width: formScroll.availableWidth
                spacing: 12

                SectionCard {
                    heading: qsTr("1. Protagonist Biography, Stats & Personal Finances")
                    FieldRow { label: qsTr("First name"); key: "firstName" }
                    FieldRow { label: qsTr("Last name"); key: "lastName" }
                    FieldRow { label: qsTr("Pronouns"); key: "pronouns" }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Gender"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_gender"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            model: ["nonbinary", "woman", "man"]
                            currentIndex: Math.max(0, model.indexOf(setupDialog.draft.gender || "nonbinary"))
                            onActivated: setupDialog.updateField("gender", currentText)
                        }
                    }
                    FieldRow { label: qsTr("Birth year"); key: "birthYear"; numeric: true }
                    FieldRow { label: qsTr("Campaign start year"); key: "launchYear"; numeric: true }
                    SetupChoice { label: qsTr("Orientation"); key: "orientation"; options: setupDialog.catalog.orientations || [] }
                    SetupChoice { label: qsTr("Family background"); key: "family"; options: setupDialog.catalog.families || []; namedOptions: true }
                    SetupChoice { label: qsTr("Sociability"); key: "sociability"; options: ["introverted", "outgoing"] }
                    SetupChoice { label: qsTr("Activity"); key: "activity"; options: ["sedentary", "athletic"] }
                    SetupChoice { label: qsTr("Decision making"); key: "disposition"; options: ["deliberate", "impulsive"] }
                    Label { text: qsTr("Personal traits"); color: "#d4e6e2" }
                    Flow {
                        Layout.fillWidth: true
                        Repeater {
                            model: setupDialog.catalog.traits || []
                            GameCheck {
                                required property string modelData
                                text: modelData
                                checked: (setupDialog.draft.traits || []).includes(modelData)
                                onToggled: setupDialog.toggleListValue("traits", modelData)
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Image {
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 96
                            fillMode: Image.PreserveAspectFit
                            source: setupDialog.draft.appearance ? "image://portraits/1/" + setupDialog.draft.appearance.concat(setupDialog.draft.style).join("/") : ""
                        }
                        GameSelect {
                            id: part
                            Layout.fillWidth: true
                            model: campaign.founding.appearanceParts
                            textRole: "name"
                            Accessible.name: qsTr("Portrait feature")
                        }
                        GameSelect {
                            readonly property string field: part.currentIndex < 15 ? "appearance" : "style"
                            readonly property int offset: part.currentIndex < 15 ? part.currentIndex : part.currentIndex - 15
                            model: part.currentIndex >= 0 ? part.model[part.currentIndex].count : 0
                            currentIndex: (setupDialog.draft[field] || [])[offset] ?? 0
                            displayText: qsTr("Variant %1").arg(currentIndex + 1)
                            Accessible.name: qsTr("Portrait variant")
                            onActivated: index => {
                                const values = setupDialog.draft[field].slice()
                                values[offset] = index
                                setupDialog.updateField(field, values)
                            }
                        }
                    }
                    FieldRow { label: qsTr("Education label"); key: "education" }
                    FieldRow { label: qsTr("Experience (years)"); key: "experience"; numeric: true }
                    FieldRow { label: qsTr("Research stat (0-100)"); key: "research"; numeric: true }
                    FieldRow { label: qsTr("Engineering stat (0-100)"); key: "engineering"; numeric: true }
                    FieldRow { label: qsTr("Evaluation stat (0-100)"); key: "evaluation"; numeric: true }
                    FieldRow { label: qsTr("Mission fit (0-100)"); key: "missionFit"; numeric: true }
                    FieldRow { label: qsTr("Prestige (0-100)"); key: "prestige"; numeric: true }
                    FieldRow { label: qsTr("Buzz (0-100)"); key: "buzz"; numeric: true }
                    FieldRow { label: qsTr("Personal cash ($)"); key: "personalCashDollars"; numeric: true }
                    FieldRow { label: qsTr("Personal debt ($)"); key: "personalDebtDollars"; numeric: true }
                    Label { text: qsTr("Protagonist skills"); font.bold: true; font.pixelSize: setupDialog.textSize * 0.88; color: "#bcd8d2" }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: setupDialog.catalog.skills || []
                            GameCheck {
                                required property var modelData
                                text: modelData.name
                                font.pixelSize: setupDialog.textSize * 0.85
                                checked: (setupDialog.draft.skills || []).includes(modelData.id)
                                onToggled: setupDialog.toggleListValue("skills", modelData.id)
                            }
                        }
                    }
                    Label { text: qsTr("Background advantages"); font.bold: true; font.pixelSize: setupDialog.textSize * 0.88; color: "#bcd8d2" }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: [{id:"genius",name:qsTr("Actual Genius")},{id:"charisma",name:qsTr("Charismatic")},{id:"political",name:qsTr("Political network")}]
                            GameCheck {
                                required property var modelData
                                text: modelData.name
                                font.pixelSize: setupDialog.textSize * 0.85
                                checked: (setupDialog.draft.advantages || []).includes(modelData.id)
                                onToggled: setupDialog.toggleListValue("advantages", modelData.id)
                            }
                        }
                    }
                }

                SectionCard {
                    heading: qsTr("2. Company, Governance, Ownership & Mission")
                    FieldRow { label: qsTr("Company name"); key: "companyName" }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Headquarters"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_headquarters"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: setupDialog.catalog.headquarters || []
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.headquarters))
                            onActivated: setupDialog.updateField("headquarters", currentValue)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Legal form"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_legalForm"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: [{id:"for_profit",name:qsTr("Corporation (for-profit)")},{id:"nonprofit",name:qsTr("Nonprofit")}]
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.legalForm))
                            onActivated: setupDialog.updateField("legalForm", currentValue)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Starting route"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_route"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: [{id:"self_funded",name:qsTr("Self-funded founder")},{id:"hired_ceo",name:qsTr("Appointed CEO (existing company)")},{id:"partnership",name:qsTr("Cofounder partnership")},{id:"backed",name:qsTr("Investor-backed")}]
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.route))
                            onActivated: setupDialog.updateField("route", currentValue)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Company mission"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_mission"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: [{id:"research",name:qsTr("Research")},{id:"public benefit",name:qsTr("Public benefit")},{id:"commercial",name:qsTr("Commercial")}]
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.mission))
                            onActivated: setupDialog.updateField("mission", currentValue)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Primary objective"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_objective"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: [{id:"survival",name:qsTr("Build a lab that lasts (survival)")},{id:"utopia",name:qsTr("Public benefit (utopia)")},{id:"profit",name:qsTr("Founder wealth (profit)")},{id:"monopoly",name:qsTr("Market leadership (monopoly)")},{id:"domination",name:qsTr("Steer the AI age (domination)")},{id:"asi",name:qsTr("Demonstrate ASI first (asi)")}]
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.objective))
                            onActivated: setupDialog.updateField("objective", currentValue)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Bylaws / charter"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_charter"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: setupDialog.catalog.charters || []
                            currentIndex: Math.max(0, model.findIndex(r => r.id === (setupDialog.draft.charter || "")))
                            onActivated: setupDialog.updateField("charter", currentValue)
                        }
                    }
                    FieldRow { label: qsTr("Company cash ($)"); key: "companyCashDollars"; numeric: true }
                    FieldRow { label: qsTr("Company debt ($)"); key: "companyDebtDollars"; numeric: true }
                    FieldRow { label: qsTr("Founder equity (%)"); key: "founderEquityPct"; numeric: true }
                    FieldRow { label: qsTr("Founder board seats (1-3)"); key: "founderBoardSeats"; numeric: true }
                    FieldRow { label: qsTr("Outside board seats (0-6)"); key: "outsideBoardSeats"; numeric: true }
                    GameCheck {
                        objectName: "playtestField_inheritedPurpose"
                        text: qsTr("Inherited company purpose (reversing mission triggers public scandal)")
                        font.pixelSize: setupDialog.textSize * 0.88
                        checked: !!setupDialog.draft.inheritedPurpose
                        onToggled: setupDialog.updateField("inheritedPurpose", checked)
                    }
                }

                SectionCard {
                    heading: qsTr("3. Starting Staff, Relationships, Premises & Compute")
                    FieldRow { label: qsTr("Additional starting staff (0-8)"); key: "extraStaffCount"; numeric: true }
                    FieldRow { label: qsTr("Staff baseline skill (1-100)"); key: "staffSkillLevel"; numeric: true }
                    FieldRow { label: qsTr("Starting close industry contacts (0-6)"); key: "bestFriendsCount"; numeric: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label { Layout.preferredWidth: Math.min(200, setupDialog.width * 0.34); text: qsTr("Office premises"); font.pixelSize: setupDialog.textSize * 0.9; color: "#d4e6e2" }
                        GameSelect {
                            objectName: "playtestField_office"
                            Layout.fillWidth: true; textSize: setupDialog.textSize * 0.92
                            textRole: "name"; valueRole: "id"
                            model: [{id:"leased",name:qsTr("Leased HQ office")},{id:"home",name:qsTr("Family home rooms")}]
                            currentIndex: Math.max(0, model.findIndex(r => r.id === setupDialog.draft.office))
                            onActivated: setupDialog.updateField("office", currentValue)
                        }
                    }
                    FieldRow { label: qsTr("Extra personal laptops (0-3)"); key: "extraLaptops"; numeric: true }
                    FieldRow { label: qsTr("Extra HQ desktops (0-4)"); key: "extraDesktops"; numeric: true }
                    FieldRow { label: qsTr("Starting compute capacity (CU/day)"); key: "startingComputeCu"; numeric: true }
                }

                SectionCard {
                    heading: qsTr("4. Starting Research, Data, Models & World Seed")
                    FieldRow { label: qsTr("Starting industry stage (0 early, 1 rising, 2 boom, 3 mature)"); key: "industryStage"; numeric: true }
                    FieldRow { label: qsTr("World seed"); key: "seed" }
                    GameCheck {
                        objectName: "playtestField_includeStarterModel"
                        text: qsTr("Start with a trained baseline model (requires an owned dataset)")
                        font.pixelSize: setupDialog.textSize * 0.88
                        checked: !!setupDialog.draft.includeStarterModel
                        onToggled: setupDialog.updateField("includeStarterModel", checked)
                    }
                    GameCheck {
                        objectName: "playtestField_includeStarterProduct"
                        text: qsTr("Deploy a document service with the starter model and an independent evaluation")
                        checked: !!setupDialog.draft.includeStarterProduct
                        onToggled: setupDialog.updateField("includeStarterProduct", checked)
                    }
                    GameCheck {
                        objectName: "playtestField_playtestUnlimited"
                        text: qsTr("Enable unlimited playtest resources flag")
                        font.pixelSize: setupDialog.textSize * 0.88
                        checked: !!setupDialog.draft.playtestUnlimited
                        onToggled: setupDialog.updateField("playtestUnlimited", checked)
                    }
                    Label { text: qsTr("Owned datasets"); font.bold: true; font.pixelSize: setupDialog.textSize * 0.88; color: "#bcd8d2" }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: setupDialog.catalog.datasets || []
                            GameCheck {
                                required property var modelData
                                text: modelData.name
                                font.pixelSize: setupDialog.textSize * 0.84
                                checked: (setupDialog.draft.ownedDatasetIds || []).includes(modelData.id)
                                onToggled: setupDialog.toggleListValue("ownedDatasetIds", modelData.id)
                            }
                        }
                    }
                    Label { text: qsTr("Pre-completed research topics (include prerequisites)"); font.bold: true; font.pixelSize: setupDialog.textSize * 0.88; color: "#bcd8d2" }
                    GameField {
                        id: researchSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Find a research topic")
                        Accessible.name: placeholderText
                    }
                    Flow {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: (setupDialog.catalog.research || []).filter(row => row.name.toLowerCase().includes(researchSearch.text.trim().toLowerCase()))
                            GameCheck {
                                required property var modelData
                                text: modelData.name
                                font.pixelSize: setupDialog.textSize * 0.82
                                checked: (setupDialog.draft.completedResearchIds || []).includes(modelData.id)
                                onToggled: setupDialog.toggleListValue("completedResearchIds", modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }

    footer: RowLayout {
        spacing: 8
        WorldButton {
            objectName: "playtestSetupResetButton"
            text: qsTr("Reset defaults")
            textSize: setupDialog.textSize
            enabled: !campaign.busy
            onClicked: {
                const res = campaign.resetPlaytestSetupDraft()
                setupDialog.draft = Object.assign({}, res.draft || ({}))
                setupDialog.errors = res.errors || []
                setupDialog.valid = !!res.valid
            }
        }
        Item { Layout.fillWidth: true }
        WorldButton {
            text: qsTr("Cancel")
            textSize: setupDialog.textSize
            enabled: !campaign.busy
            onClicked: setupDialog.close()
        }
        WorldButton {
            objectName: "playtestSetupStartButton"
            text: qsTr("Start playtest")
            primary: true
            textSize: setupDialog.textSize
            enabled: setupDialog.valid && !campaign.busy
            onClicked: {
                setupDialog.launching = true
                campaign.startPlaytestCustom(setupDialog.draft)
            }
        }
    }

    Connections {
        target: campaign
        function onCommandFinished(accepted, explanation) {
            if (!setupDialog.launching) return
            setupDialog.launching = false
            if (accepted) { setupDialog.close(); setupDialog.started() }
            else setupDialog.errors = [explanation || campaign.notice]
        }
    }
}
