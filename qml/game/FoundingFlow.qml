import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: opening
    objectName: "foundingScreen"
    required property int textSize
    signal menuRequested()
    readonly property var story: campaign.founding
    readonly property var form: story.form
    readonly property int count: story.chapters.length
    readonly property int entryPage: count + 3
    readonly property var chapter: story.page >= 3 && story.page < entryPage ? story.chapters[story.page - 3] : ({})
    readonly property url sceneSource: "qrc:/pgp/assets/founding/" + (story.page <= 3 ? "california-1997.png" : story.page <= 6 ? "curiosity-2003.png" : "threshold-2015.png")
    readonly property bool settling: sceneFade.running || textReveal.running
    property int lastPage: -1
    padding: 0
    background: Item {
        Rectangle { anchors.fill: parent; color: "#273f42" }
        Image { id: previousScene; anchors.fill: parent; fillMode: Image.PreserveAspectCrop; source: "qrc:/pgp/assets/founding/california-1997.png" }
        Image {
            id: currentScene
            anchors.fill: parent; fillMode: Image.PreserveAspectCrop; source: opening.sceneSource
            onSourceChanged: { opacity = campaign.reducedMotion ? 1 : 0; sceneFade.restart() }
        }
        Rectangle { anchors.fill: parent; color: "#3522393e" }
    }
    NumberAnimation {
        id: sceneFade; target: currentScene; property: "opacity"; to: 1
        duration: campaign.reducedMotion ? 0 : 2200; easing.type: Easing.InOutSine
        onFinished: previousScene.source = currentScene.source
    }
    SequentialAnimation {
        id: textReveal
        PauseAnimation { duration: campaign.reducedMotion ? 0 : 120 }
        NumberAnimation { target: readingPanel; property: "opacity"; from: 0; to: 1; duration: campaign.reducedMotion ? 0 : 900; easing.type: Easing.OutSine }
    }
    palette.text: "#263b42"
    palette.windowText: "#263b42"
    palette.buttonText: "#263b42"
    palette.base: "#fffaf2"
    palette.button: "#fffaf2"
    palette.highlight: "#2d6666"
    palette.highlightedText: "white"

    function selectedName(rows, id) {
        for (const row of rows) if (row.id === id) return row.name
        return id
    }

    function reveal(item) {
        if (!item || !body.contentItem) return
        let ancestor = item
        while (ancestor && ancestor !== body) ancestor = ancestor.parent
        if (!ancestor) return
        const p = item.mapToItem(body.contentItem, 0, 0)
        if (p.y < body.contentItem.contentY + 12) body.contentItem.contentY = Math.max(0, p.y - 12)
        else if (p.y + item.height > body.contentItem.contentY + body.height - 12)
            body.contentItem.contentY = Math.max(0, Math.min(body.contentHeight - body.height, p.y + item.height - body.height + 12))
    }
    Connections {
        target: opening.story
        function onChanged() {
            if (opening.lastPage !== opening.story.page) {
                opening.lastPage = opening.story.page
                if (campaign.reducedMotion) { textReveal.stop(); readingPanel.opacity = 1 }
                else { readingPanel.opacity = 0; textReveal.restart() }
            }
            Qt.callLater(function() { body.contentItem.contentY = Math.min(body.contentItem.contentY, Math.max(0, body.contentHeight-body.height)) })
        }
    }
    Connections {
        target: campaign
        function onSettingsChanged() { if (campaign.reducedMotion) { sceneFade.complete(); textReveal.complete(); readingPanel.opacity = 1 } }
    }
    component Copy: Label {
        Layout.fillWidth: true
        color: "#344e54"
        font.pixelSize: opening.textSize
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }
    component Title: Copy { color: "#183f43"; font.pixelSize: opening.textSize * 1.7; font.bold: true }
    component Field: TextField {
        required property string field
        Layout.fillWidth: true
        font.pixelSize: opening.textSize
        color: "#213f44"
        maximumLength: field === "first_name" || field === "last_name" ? 63 : 64
        selectByMouse: true
        text: opening.form[field] || ""
        Accessible.name: placeholderText
        onTextEdited: opening.story.setField(field, text)
    }
    component Choices: ColumnLayout {
        required property var rows
        property string field: ""
        property string scene: ""
        property bool multiple: false
        Layout.fillWidth: true
        spacing: 9
        Repeater {
            model: rows
            delegate: Button {
                id: choice
                required property var modelData
                Layout.fillWidth: true
                objectName: "foundingChoice_" + modelData.id
                readonly property bool chosen: multiple ? (opening.form.backers || []).indexOf(modelData.id) >= 0
                    : scene.length > 0 ? opening.form.choices[scene] === modelData.id : opening.form[field] === modelData.id
                enabled: modelData.available !== false || (multiple && chosen)
                Accessible.name: modelData.name + ". " + modelData.detail + (chosen ? ". Selected." : "")
                padding: 16
                contentItem: ColumnLayout {
                    spacing: 4
                    Label { Layout.fillWidth: true; text: (choice.chosen ? "●  " : "○  ") + choice.modelData.name; color: "#183f43"; font.bold: true; font.pixelSize: opening.textSize; wrapMode: Text.Wrap }
                    Label { Layout.fillWidth: true; text: choice.modelData.detail + (choice.modelData.reason ? "\n" + choice.modelData.reason : ""); color: "#425c60"; font.pixelSize: opening.textSize * 0.94; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                }
                background: Rectangle { color: choice.chosen ? "#d6e6db" : "#fff9ee"; border.color: choice.activeFocus ? "#ad6b2e" : choice.chosen ? "#467569" : "#c7c1af"; border.width: choice.activeFocus ? 3 : 1; radius: 7; opacity: choice.enabled ? 1 : 0.55 }
                onClicked: {
                    if (multiple) opening.story.back(modelData.id, !chosen)
                    else if (scene.length > 0) opening.story.choose(scene, modelData.id)
                    else opening.story.setField(field, modelData.id)
                }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: Math.max(65, opening.textSize * 3.6)
            color: "#163c42"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24
                Label { text: qsTr("PROJECT GLOWING PANCAKE"); color: "#ebdfc4"; font.pixelSize: opening.textSize * 0.9; font.letterSpacing: 1.2; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Button { objectName: "foundingMainMenuButton"; text: qsTr("Main menu"); font.pixelSize: opening.textSize; onClicked: opening.menuRequested() }
            }
        }
        ScrollView {
            id: body
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: body.availableWidth
                spacing: 18
                Item { Layout.preferredHeight: 4 }
                Pane {
                    id: readingPanel
                    Layout.fillWidth: true
                    Layout.maximumWidth: 860
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: 24; Layout.rightMargin: 24
                    padding: 24
                    background: Rectangle { color: "#edf7f0e2"; radius: 8; border.color: "#b9c5bca9" }
                    contentItem: ColumnLayout {
                    spacing: 15
                    Copy { text: story.page === 0 ? qsTr("A COMPANY. A LIFETIME. A CHANGING WORLD.") : story.page < opening.entryPage ? qsTr("BEFORE THE COMPANY · %1").arg(opening.chapter.date || "1997") : qsTr("THE FOUNDING · CALIFORNIA, 2015"); color: "#8d5a32"; font.letterSpacing: 1.5; font.pixelSize: opening.textSize * 0.85 }
                    ColumnLayout {
                        visible: story.page === 0
                        Layout.fillWidth: true; spacing: 16
                        Title { text: qsTr("Where does your story begin?") }
                        Copy { text: qsTr("A child born in California in 1997 will one day help shape the AI age. Begin a century-long campaign, or choose the world you want to play in.") }
                        Choices { field: "mode"; rows: [
                            {id:"campaign",name:qsTr("Play the campaign"),detail:qsTr("Live the formative years, found an institution at 18, and follow its story until age 118. Continue afterward if you want more.")},
                            {id:"sandbox",name:qsTr("Create a sandbox"),detail:qsTr("Choose founding conditions, resources, people and goals. The biography is optional, and the campaign deadline does not end play.")}
                        ] }
                        Button { visible: Object.keys(opening.form.choices || {}).length > 0; text: qsTr("Continue your saved introduction"); font.pixelSize: opening.textSize; onClicked: { let p=3; while(p<opening.entryPage && opening.form.choices[story.chapters[p-3].id]) ++p; story.page=p } }
                    }
                    ColumnLayout {
                        visible: story.page === 1
                        Layout.fillWidth: true; spacing: 12
                        Title { text: qsTr("California, 1997. A new beginning.") }
                        Copy { text: qsTr("What is the baby's name? These are details of your fictional character, saved only on this computer.") }
                        Field { objectName: "foundingFirstName"; field: "first_name"; placeholderText: qsTr("First name") }
                        Field { objectName: "foundingLastName"; field: "last_name"; placeholderText: qsTr("Last name") }
                        Copy { text: qsTr("Gender and pronouns are independent. They shape how the character is addressed, not their ability or opportunities.") }
                        Field { field: "gender"; placeholderText: qsTr("Gender") }
                        Field { field: "pronouns"; placeholderText: qsTr("Pronouns, for example she/her, he/him or they/them") }
                        Choices { field: "sociability"; rows: [{id:"outgoing",name:qsTr("Outgoing"),detail:qsTr("Drawn to people and conversation.")},{id:"introverted",name:qsTr("Introverted"),detail:qsTr("Drawn to reflection and quieter company.")}] }
                        Choices { field: "activity"; rows: [{id:"athletic",name:qsTr("Athletic"),detail:qsTr("Enjoys movement and physical practice.")},{id:"sedentary",name:qsTr("Sedentary"),detail:qsTr("Prefers less physically active pursuits.")}] }
                        Choices { field: "disposition"; rows: [{id:"deliberate",name:qsTr("Deliberate"),detail:qsTr("Likes time to weigh a decision.")},{id:"impulsive",name:qsTr("Impulsive"),detail:qsTr("Often follows an immediate idea.")}] }
                    }
                    ColumnLayout {
                        visible: story.page === 2
                        Layout.fillWidth: true; spacing: 14
                        Title { text: qsTr("Family, circumstance, and a leg up") }
                        Copy { text: qsTr("Choose your parents' circumstances, then one exceptional advantage. Neither poverty nor introversion limits your inherent talent.") }
                        Choices { field: "family"; rows: story.families }
                        Copy { text: qsTr("Choose exactly one leg up"); font.bold: true }
                        Choices { field: "leg"; rows: story.legs }
                        Button { visible: opening.form.mode === "sandbox"; text: qsTr("Skip childhood and customize the company"); onClicked: story.page = opening.entryPage }
                    }
                    ColumnLayout {
                        visible: story.page >= 3 && story.page < opening.entryPage
                        Layout.fillWidth: true; spacing: 16
                        Title { text: opening.chapter.title || "" }
                        Copy { text: opening.chapter.text || ""; font.pixelSize: opening.textSize * 1.12 }
                        Choices { scene: opening.chapter.id || ""; rows: opening.chapter.choices || [] }
                        Copy { text: qsTr("These choices become part of your history. Later people and events can remember them."); color: "#6d7770" }
                    }
                    ColumnLayout {
                        visible: story.page === opening.entryPage
                        Layout.fillWidth: true; spacing: 14
                        Title { text: qsTr("Eighteen, and a door opens") }
                        Copy { text: qsTr("There is no finished general-purpose chatbot waiting for you. Choose how you enter the startup, then decide what kind of institution it will be.") }
                        Field { objectName: "foundingCompanyName"; field: "company"; placeholderText: qsTr("Company or institution name") }
                        Choices { field: "legal_form"; rows: [{id:"corporation",name:qsTr("For-profit corporation"),detail:qsTr("Can sell equity and distribute earned profits. Applicable corporate taxes and negotiated board rights apply.")},{id:"nonprofit",name:qsTr("Nonprofit research institution"),detail:qsTr("A recognized mission-serving institution with trustees and donors. No founder-owned equity, dividends or ordinary VC equity rounds.")}] }
                        Choices { field: "route"; rows: story.routes }
                    }
                    ColumnLayout {
                        visible: story.page === opening.entryPage + 1
                        Layout.fillWidth: true; spacing: 14
                        Title { text: qsTr("Why are you building this?") }
                        Copy { text: qsTr("Your founding mission selects the campaign's victory condition. Its exact progress and sustained conditions remain visible in Objectives.") }
                        Choices { field: "mission"; rows: [
                            {id:"survival",name:qsTr("Build a lab that lasts"),detail:qsTr("Simple Survival: keep a real, solvent operating institution through 2115.")},
                            {id:"utopia",name:qsTr("Improve life broadly"),detail:qsTr("World Utopia: sustain the published global and regional welfare conditions for the final ten years.")},
                            {id:"monopoly",name:qsTr("Build the leading AI platform"),detail:qsTr("World Monopoly: dominate actual useful delivered work across the final five years.")},
                            {id:"domination",name:qsTr("Steer the AI age"),detail:qsTr("World Domination: obtain durable institutional authority, beyond ordinary market share.")},
                            {id:"asi",name:qsTr("Be first to demonstrate ASI"),detail:qsTr("ASI Race: independently validate the game's advanced research thresholds before your rivals.")},
                            {id:"profit",name:qsTr("Create a financial legacy"),detail:qsTr("Profit: highest founder-legacy net wealth at the deadline. Nonprofit assets do not count as personal wealth."),available:opening.form.legal_form!=="nonprofit",reason:opening.form.legal_form==="nonprofit"?qsTr("Choose a mission compatible with nonprofit ownership."):""}
                        ] }
                    }
                    ColumnLayout {
                        visible: story.page === opening.entryPage + 2
                        Layout.fillWidth: true; spacing: 14
                        Title { text: opening.form.legal_form === "nonprofit" ? qsTr("Who supports the mission?") : qsTr("Who backs the company?") }
                        Copy { text: qsTr("Select any compatible backers whose terms you want to accept, or proceed with your founding package alone. Their money and rights become real company terms.") }
                        Choices { multiple: true; rows: story.backers }
                        Copy { text: story.preview }
                    }
                    ColumnLayout {
                        visible: story.page === opening.entryPage + 3
                        Layout.fillWidth: true; spacing: 14
                        Title { text: qsTr("An office, and the first researcher") }
                        Choices { field: "office"; rows: story.offices }
                        Choices { field: "researcher"; rows: story.researchers }
                        Copy { text: story.preview }
                    }
                    ColumnLayout {
                        visible: story.page === opening.entryPage + 4
                        Layout.fillWidth: true; spacing: 14
                        Title { text: qsTr("The first morning") }
                        Copy { text: opening.form.mode === "sandbox" ? qsTr("Review the starting conditions for %1, then enter your office.").arg(opening.form.company) : qsTr("%1 %2, age 18. %3 begins in 2015. You will live until the campaign closes at 118 in 2115; what your institution becomes is still unwritten.").arg(opening.form.first_name).arg(opening.form.last_name).arg(opening.form.company) }
                        Copy { text: qsTr("%1 · %2\n%3\n%4 · %5").arg(opening.selectedName(story.families,form.family)).arg(opening.selectedName(story.legs,form.leg)).arg(opening.selectedName(story.routes,form.route)).arg(opening.selectedName(story.offices,form.office)).arg(opening.selectedName(story.researchers,form.researcher)) }
                        Copy { text: story.preview; font.bold: true }
                        Copy { text: qsTr("The game starts paused in your office. Research, data, compute and future hires use real resources. There is no finished commercial product at the start.") }
                        Copy { text: qsTr("World seed") }
                        Field { field: "seed"; placeholderText: qsTr("Whole-number world seed") }
                        ComboBox { Layout.fillWidth: true; font.pixelSize: opening.textSize; model: [qsTr("California"),qsTr("Texas"),qsTr("Washington")]; Accessible.name: qsTr("Headquarters"); currentIndex: ["US-CA","US-TX","US-WA"].indexOf(opening.form.headquarters); onActivated: story.setField("headquarters",["US-CA","US-TX","US-WA"][currentIndex]) }
                        ComboBox { Layout.fillWidth: true; font.pixelSize: opening.textSize; model: [qsTr("Forgiving"),qsTr("Standard"),qsTr("Demanding")]; Accessible.name: qsTr("Difficulty"); currentIndex: ["difficulty.forgiving","difficulty.standard","difficulty.demanding"].indexOf(opening.form.difficulty); onActivated: story.setField("difficulty",["difficulty.forgiving","difficulty.standard","difficulty.demanding"][currentIndex]) }
                        ColumnLayout {
                            visible: opening.form.mode === "sandbox"
                            Layout.fillWidth: true
                            Copy { text: qsTr("Custom sandbox resources (optional)") }
                            Field { field: "sandbox_cash"; placeholderText: qsTr("Available starting dollars, up to 1,000,000,000") }
                            Field { field: "sandbox_staff"; placeholderText: qsTr("Research staff, from 1 to 10,000") }
                        }
                        Button { objectName: "foundingStartButton"; text: qsTr("Enter your office"); font.pixelSize: opening.textSize * 1.1; enabled: campaign.ready && !campaign.busy; onClicked: campaign.startFounderCampaign() }
                    }
                    Copy { visible: campaign.notice.length > 0 || story.error.length > 0; text: campaign.notice || story.error; color: "#8e3e27"; Accessible.role: Accessible.AlertMessage }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { objectName: "foundingBackButton"; visible: story.page > 0; text: qsTr("Back"); font.pixelSize: opening.textSize; onClicked: { story.page--; body.contentItem.contentY=0; campaign.clearNotice() } }
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "foundingNextButton"
                            visible: story.page < opening.entryPage + 4
                            enabled: story.page < 3 || story.page >= opening.entryPage || !!opening.form.choices[opening.chapter.id]
                            text: story.page === 0 ? qsTr("Begin") : qsTr("Continue")
                            font.pixelSize: opening.textSize
                            onClicked: { story.page++; body.contentItem.contentY=0; campaign.clearNotice() }
                        }
                    }
                    Copy { text: qsTr("Your introduction draft is saved locally as you go. The gameplay tutorial will be added once gameplay is finalized."); font.pixelSize: opening.textSize * 0.85; color: "#6d7770" }
                    }
                }
                Item { Layout.preferredHeight: 24 }
            }
        }
    }
}
