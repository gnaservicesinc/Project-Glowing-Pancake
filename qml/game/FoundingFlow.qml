import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

FocusScope {
    id: opening
    objectName: "foundingScreen"
    required property real textScale
    readonly property var story: campaign.founding
    readonly property bool reducedMotion: campaign.reducedMotion
    readonly property int beatIndex: Math.max(0, script.beats.findIndex(page => page.id === story.beat))
    readonly property var beat: script.beats[beatIndex] || {id:"opening",kind:"narration",tone:"slate",art:"hospital"}
    readonly property string beatId: beat.id
    readonly property string sceneTone: beat.tone || "slate"
    readonly property bool narrative: beat.kind === "narration"
    readonly property bool birthPaper: beat.kind === "birth" || beat.kind === "certificate"
    readonly property bool choosingAdvantages: beat.kind === "advantages"
    readonly property int maxSelections: beat.maxSelections || 1
    readonly property bool multiSelect: beat.kind === "chapter" && maxSelections > 1
    readonly property bool compact: width < 850 || height < 620
    readonly property int typeSize: Math.round(16 * textScale)
    readonly property bool settling: reveal.running
    property bool initialized: false
    property real autograph: 0
    property string actionError: ""
    GameInk { id: ink; tone: opening.sceneTone }
    FoundingStory { id: script; story: opening.story }

    function selectedTokens() {
        if (choosingAdvantages) return Array.from(story.form.advantages || [])
        if (beat.kind !== "chapter") return []
        const raw = (story.form.choices || {})[beat.chapter]
        return raw ? String(raw).split(",").filter(Boolean) : []
    }
    function presentBeat() {
        if (!initialized || !visible) return
        actionError = ""
        signing.stop()
        autograph = beatId === "certificate" && reducedMotion ? 1 : 0
        if (beatId === "certificate" && !reducedMotion) signing.start()
        if (birthPaper) {
            givenName.text = story.form.first_name
            surname.text = story.form.last_name
        }
        if (choiceView.contentItem) choiceView.contentItem.contentY = 0
        if (narrativeView.contentItem) narrativeView.contentItem.contentY = 0
        reveal.restart()
        forceActiveFocus()
    }
    function moveTo(id) { campaign.clearNotice(); story.beat = id }
    function advance() {
        if (beatId === "certificate" && autograph < 1) return
        if (multiSelect && selectedTokens().length === 0) return
        if (beat.kind === "birth") {
            actionError = story.answerText("first_name", givenName.text)
            if (!actionError) actionError = story.answerText("last_name", surname.text)
            if (actionError) return
        }
        menuAudio.select()
        let next = script.beats[beatIndex + 1]
        if (next) moveTo(next.id)
    }
    function answer(id) {
        if (beatId === "pronouns" && id === "other") { moveTo("other_pronouns"); return }
        if (beatId === "other_pronouns") { story.setField("pronouns",id); moveTo("other_pronouns-after"); return }
        if (choosingAdvantages) story.setAdvantage(id, !selectedTokens().includes(id))
        else if (beat.kind === "chapter") story.choose(beat.chapter, id)
        else story.setField(beatId, id)
        if (multiSelect || choosingAdvantages) {
            menuAudio.select()
            // The updated choices can rebuild the delegates.
            Qt.callLater(() => {
                const index = decisions.rows.findIndex(option => option.id === id)
                const choice = choiceOptions.itemAt(index)
                if (choice && choice.visible && choice.enabled)
                    choice.forceActiveFocus(Qt.OtherFocusReason)
            })
            return
        }
        advance()
    }
    function previousBeat() {
        if (beatId === "other_pronouns") { moveTo("pronouns"); return }
        moveTo(script.beats[Math.max(0,beatIndex-1)].id)
    }
    Component.onCompleted: { initialized = true; presentBeat() }
    onBeatIdChanged: presentBeat()
    onVisibleChanged: if (visible) presentBeat()
    onReducedMotionChanged: if (reducedMotion) { reveal.stop(); signing.stop(); autograph = 1 }
    NumberAnimation { id: reveal; target: body; property: "opacity"; from: 0; to: 1; duration: opening.reducedMotion ? 0 : 340 }
    NumberAnimation { id: signing; target: opening; property: "autograph"; from: 0; to: 1; duration: 1400 }
    Keys.onPressed: event => {
        if (settling && [Qt.Key_Space,Qt.Key_Return,Qt.Key_Enter].includes(event.key)) {
            reveal.complete(); event.accepted = true
        }
    }
    FoundingSceneArt {
        anchors.fill: parent
        art: opening.beat.art || "hospital"
        tone: opening.sceneTone
        mood: opening.beat.mood || "calm"
        tint: opening.beat.tint || ""
        section: opening.beat.section || ""
        reducedMotion: opening.reducedMotion
    }
    Rectangle {
        id: scrim
        anchors.fill: parent
        readonly property color tintBg: Qt.tint(Qt.darker(ink.stage, 2.2), "#d00c1522")
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: Qt.rgba(scrim.tintBg.r, scrim.tintBg.g, scrim.tintBg.b, 0.97) }
            GradientStop { position: 0.62; color: Qt.rgba(scrim.tintBg.r, scrim.tintBg.g, scrim.tintBg.b, 0.89) }
            GradientStop { position: 1; color: Qt.rgba(scrim.tintBg.r, scrim.tintBg.g, scrim.tintBg.b, 0.42) }
        }
    }
    ColumnLayout {
        id: stage; objectName: "foundingCanvas"
        visible: opening.beat.kind !== "home"
        anchors.fill: parent; anchors.margins: opening.compact ? 16 : Math.min(52,opening.width*0.04)
        spacing: opening.compact ? 10 : 20
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    text: opening.beat.date || ""
                    color: ink.accent
                    font.pixelSize: opening.typeSize*0.8; font.letterSpacing: 2; font.bold: true
                    elide: Text.ElideRight
                }
                Row {
                    id: sectionStrip
                    visible: !opening.compact && !!opening.beat.section && (opening.beat.timeline || []).length > 0
                    spacing: 6
                    Repeater {
                        model: opening.beat.timeline || []
                        Rectangle {
                            required property var modelData
                            readonly property bool current: modelData.id === opening.beat.section
                            readonly property bool skipped: modelData.skipped
                            height: pillLabel.implicitHeight + 8
                            width: pillLabel.implicitWidth + 16
                            radius: height / 2
                            color: current ? ink.accent : skipped ? "#30182430" : modelData.active ? "#aa1e2f40" : "#5517222d"
                            border.width: 1
                            border.color: current ? ink.accent : modelData.active ? ink.border : "#40384b5c"
                            Text {
                                id: pillLabel
                                anchors.centerIn: parent
                                text: modelData.step + ". " + modelData.label
                                textFormat: Text.PlainText
                                font.pixelSize: Math.max(10, opening.typeSize * 0.68)
                                font.bold: current
                                font.strikeout: skipped
                                color: current ? "#12202c" : skipped ? "#7d8892" : modelData.active ? "#d9e1e6" : "#80909b"
                            }
                        }
                    }
                }
            }
        }
        Item {
            id: body
            Layout.fillWidth: true; Layout.fillHeight: true
            ScrollView {
                id: choiceView
                objectName: "foundingChoiceScroll"
                anchors.fill: parent
                visible: !opening.birthPaper && opening.beat.kind !== "appearance" && !opening.narrative
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: choiceView.availableWidth
                    spacing: opening.compact ? 8 : 14
                    Label {
                        objectName: "foundingTitle"
                        Layout.fillWidth: true
                        text: opening.beat.title || ""; color: ink.paper
                        font.family: ink.displayFace; font.pixelSize: opening.typeSize*(opening.compact ? 1.25 : 2.6)
                        wrapMode: Text.Wrap
                    }
                    Label {
                        objectName: "foundingNarration"
                        Layout.fillWidth: true
                        Layout.maximumWidth: 1000
                        text: opening.beat.narration || ""; color: "#d5dce0"
                        font.pixelSize: opening.typeSize; wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            objectName: "foundingQuestion"
                            Layout.fillWidth: true
                            text: opening.beat.prompt || ""; color: ink.accent
                            font.pixelSize: opening.typeSize; wrapMode: Text.Wrap
                        }
                        Rectangle {
                            visible: opening.multiSelect
                            height: multiHint.implicitHeight + 6
                            width: multiHint.implicitWidth + 14
                            radius: height / 2
                            color: "#223446"
                            border.color: ink.accent
                            border.width: 1
                            Text {
                                id: multiHint
                                anchors.centerIn: parent
                                text: qsTr("%1 / %2 selected").arg(opening.selectedTokens().length).arg(opening.maxSelections)
                                font.pixelSize: Math.max(11, opening.typeSize * 0.78)
                                font.bold: true
                                color: ink.accent
                            }
                        }
                    }
                    GridLayout {
                        id: decisions
                        objectName: "foundingChoices"
                        Layout.fillWidth: true
                        readonly property var rows: opening.beat.choices || []
                        readonly property var chosenIds: opening.selectedTokens()
                        columns: width < opening.typeSize * 17 ? 1 : rows.length >= 3 && width >= opening.typeSize * 44 ? 3 : 2
                        rowSpacing: opening.compact ? 6 : 14; columnSpacing: rowSpacing
                        Repeater {
                            id: choiceOptions
                            model: decisions.rows
                            AbstractButton {
                                id: choice
                                required property var modelData
                                objectName: "foundingChoice_" + modelData.id
                                property string detail: modelData.available === false ? modelData.reason : modelData.detail || ""
                                readonly property bool selected: decisions.chosenIds.includes(modelData.id)
                                readonly property bool atLimit: opening.multiSelect && !selected && decisions.chosenIds.length >= opening.maxSelections
                                Layout.fillWidth: true; Layout.preferredWidth: 1; Layout.alignment: Qt.AlignTop
                                Layout.minimumWidth: 0; Layout.minimumHeight: 0
                                padding: opening.compact ? 9 : 14
                                enabled: modelData.available !== false && !atLimit
                                checked: selected
                                activeFocusOnTab: true
                                onActiveFocusChanged: {
                                    if (!activeFocus) return
                                    const view = choiceView.contentItem
                                    const top = mapToItem(view.contentItem, 0, 0).y
                                    if (height >= view.height || top < view.contentY) view.contentY = top
                                    else if (top + height > view.contentY + view.height)
                                        view.contentY = top + height - view.height
                                }
                                Accessible.name: modelData.name; Accessible.description: detail
                                Accessible.checkable: opening.multiSelect || opening.choosingAdvantages
                                Accessible.checked: choice.selected
                                ToolTip.visible: hovered && !!detail; ToolTip.text: detail
                                background: Rectangle {
                                    radius: 8
                                    color: choice.selected ? Qt.rgba(ink.accent.r, ink.accent.g, ink.accent.b, 0.22)
                                        : !choice.enabled ? "#a016212c"
                                        : choice.down ? "#43505c"
                                        : choice.hovered || choice.activeFocus ? "#314354"
                                        : "#d91c2b3a"
                                    border.color: choice.selected ? ink.accent
                                        : choice.activeFocus ? "#f5ead8"
                                        : !choice.enabled ? "#48525a"
                                        : choice.hovered ? ink.accent : "#556470"
                                    border.width: choice.selected || choice.activeFocus ? 2 : 1
                                    Rectangle {
                                        width: 4; height: parent.height * 0.45
                                        anchors.verticalCenter: parent.verticalCenter; radius: 2
                                        color: choice.selected ? ink.accent : "#c7a779"
                                        visible: choice.selected || choice.hovered || choice.activeFocus
                                    }
                                }
                                implicitHeight: choiceCopy.implicitHeight + topPadding + bottomPadding
                                contentItem: Column {
                                    id: choiceCopy
                                    spacing: opening.compact ? 2 : 8
                                    RowLayout {
                                        width: parent.width
                                        spacing: 8
                                        Text {
                                            id: choiceName
                                            Layout.fillWidth: true
                                            text: (modelData.available === false ? "🔒 " : "") + choice.modelData.name
                                            textFormat: Text.PlainText
                                            color: choice.selected ? ink.accent : choice.enabled ? "#f5ebda" : "#8f98a0"
                                            font.pixelSize: opening.typeSize
                                            font.weight: choice.selected ? Font.DemiBold : Font.Medium
                                            wrapMode: Text.Wrap
                                        }
                                        Rectangle {
                                            visible: choice.selected
                                            width: checkBadge.implicitWidth + 10
                                            height: checkBadge.implicitHeight + 4
                                            radius: height / 2
                                            color: ink.accent
                                            Text {
                                                id: checkBadge
                                                anchors.centerIn: parent
                                                text: "✓"
                                                font.pixelSize: Math.max(10, opening.typeSize * 0.72)
                                                font.bold: true
                                                color: "#12202c"
                                            }
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        visible: !!text
                                        text: choice.detail; textFormat: Text.PlainText
                                        color: choice.enabled ? "#c2cfd5" : "#879199"
                                        font.pixelSize: opening.typeSize*0.84; wrapMode: Text.Wrap
                                    }
                                }
                                onClicked: opening.answer(modelData.id)
                            }
                        }
                    }
                }
            }
            ScrollView {
                id: narrativeView
                anchors.fill:parent
                visible:opening.narrative
                contentWidth:availableWidth;clip:true
                ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
                ColumnLayout {
                    width:Math.min(narrativeView.availableWidth,1000)
                    spacing:opening.compact?16:28
                    Label {Layout.fillWidth:true;text:opening.beat.title || "";color:ink.paper;font.family:ink.displayFace;font.pixelSize:opening.typeSize*(opening.compact?1.3:2.6);wrapMode:Text.Wrap}
                    Label {Layout.fillWidth:true;text:opening.beat.narration || "";color:"#c9d0d4";font.pixelSize:opening.typeSize*1.1;wrapMode:Text.Wrap}
                    Label {objectName:"foundingAfterword";Layout.fillWidth:true;text:opening.beat.afterword || "";color:"#f2dfbd";font.family:ink.displayFace;font.pixelSize:opening.typeSize*1.1;wrapMode:Text.Wrap}
                }
            }
            Rectangle {
                id: paper; objectName: "foundingBirthCertificate"
                visible: opening.birthPaper
                anchors.centerIn: parent
                width: Math.min(parent.width,1000); height: parent.height
                color: "#f1e6cf"; radius: 3; border.color: "#c4a374"; border.width: 2
                Rectangle { anchors.fill: parent; anchors.margins: 7; color: "transparent"; border.color: "#b2966a" }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: opening.compact ? 18 : 34
                    spacing: opening.compact ? 6 : 14
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("CALIFORNIA · 1997"); color: "#786044"; font.pixelSize: opening.typeSize*0.7; font.letterSpacing: 2 }
                            Label { text: qsTr("Certificate of Birth"); color: "#273945"; font.family: ink.displayFace; font.pixelSize: opening.typeSize*(opening.compact ? 1.3 : 2.2) }
                        }
                        Label { text: "✦"; font.pixelSize: opening.typeSize*2; color: "#a27b42"; Accessible.ignored: true }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#b9a078" }
                    GridLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        columns: 2; columnSpacing: 22; rowSpacing: opening.compact ? 3 : 12
                        Label { text: qsTr("GIVEN NAME"); color: "#635640"; font.pixelSize: opening.typeSize*0.75 }
                        Label { text: qsTr("SURNAME"); color: "#635640"; font.pixelSize: opening.typeSize*0.75 }
                        GameField {
                            id: givenName; objectName: "foundingAnswer"
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            parchment: true; textSize: opening.typeSize * 1.2
                            readOnly: opening.beatId === "certificate"; selectByMouse: true
                            maximumLength: 63; Accessible.name: qsTr("Given name")
                            onAccepted: opening.advance()
                        }
                        GameField {
                            id: surname; objectName: "foundingSurname"
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            parchment: true; textSize: opening.typeSize * 1.2
                            readOnly: opening.beatId === "certificate"; selectByMouse: true
                            maximumLength: 63; Accessible.name: qsTr("Surname")
                            onAccepted: opening.advance()
                        }
                        Label { Layout.columnSpan: 2; text: qsTr("SEX RECORDED AT BIRTH"); color: "#635640"; font.pixelSize: opening.typeSize*0.75 }
                        RowLayout {
                            Layout.columnSpan: 2; Layout.fillWidth: true
                            Repeater {
                                model: [{id:"female",name:qsTr("Female")},{id:"male",name:qsTr("Male")},{id:"indeterminate",name:qsTr("Indeterminate")}]
                                AbstractButton {
                                    required property var modelData
                                    objectName: "birthSex_"+modelData.id
                                    Layout.fillWidth: true; Layout.minimumWidth: 0
                                    implicitHeight: Math.max(36, opening.typeSize * 2.1)
                                    hoverEnabled: true
                                    enabled: opening.beatId !== "certificate"
                                    text: modelData.name; font.pixelSize: opening.typeSize*0.85
                                    checked: opening.story.form.birth_sex === modelData.id
                                    contentItem: Text { text: parent.text; font: parent.font; color: parent.checked ? "#fff8e7" : "#35434a"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 3; color: parent.checked ? "#344e5b" : parent.hovered ? "#eddcb9" : "#e4d5b9"; border.color: parent.activeFocus ? "#293f51" : "#ad9168"; border.width: parent.activeFocus ? 2 : 1 }
                                    onClicked: opening.story.setField("birth_sex",modelData.id)
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item {
                            objectName: "foundingSignature"
                            Layout.fillWidth: true; Layout.preferredHeight: opening.typeSize*1.8
                            clip: true
                            Text { width: parent.width*opening.autograph; height: parent.height; clip: true; text: qsTr("The %1 family").arg(opening.story.form.last_name); color: "#344f64"; font.family: "Snell Roundhand"; font.italic: true; font.pixelSize: opening.typeSize*1.5 }
                        }
                        Label { text: opening.beatId === "certificate" ? "✦" : qsTr("1 JAN 1997"); color: "#8c4f42"; font.pixelSize: opening.typeSize; opacity: opening.beatId === "certificate" ? opening.autograph : 1 }
                    }
                }
            }
            AppearanceEditor { anchors.fill: parent; visible: opening.beat.kind === "appearance"; story: opening.story; textScale: opening.textScale }
        }
        Label {
            objectName: "foundingError"
            Layout.fillWidth: true
            text: opening.actionError || opening.story.error
            visible: !!text; color: "#ffbb9c"; wrapMode: Text.Wrap; font.pixelSize: opening.typeSize*0.85
            Accessible.role: Accessible.AlertMessage
        }
        RowLayout {
            Layout.fillWidth: true
            WorldButton { objectName: "foundingBackButton"; visible: opening.beatIndex > 0; text: qsTr("← Back"); textSize: opening.typeSize; padding: opening.compact ? 8 : 14; onClicked: opening.previousBeat() }
            Item { Layout.fillWidth: true }
            WorldButton {
                id: continueAction; objectName: "foundingNextButton"
                accent: ink.accent
                visible: opening.narrative || opening.birthPaper || opening.beat.kind === "appearance" || opening.multiSelect || opening.choosingAdvantages
                text: opening.multiSelect
                    ? qsTr("Continue (%1/%2) →").arg(opening.selectedTokens().length).arg(opening.maxSelections)
                    : opening.beat.kind === "birth" ? qsTr("Sign birth record →")
                    : opening.beat.kind === "appearance" ? qsTr("Start the morning →")
                    : qsTr("Continue →")
                textSize: opening.typeSize; padding: opening.compact ? 8 : 14; primary: true
                enabled: !campaign.busy && (opening.beatId !== "certificate" || opening.autograph >= 1) && (!opening.multiSelect || opening.selectedTokens().length > 0)
                onClicked: opening.advance()
            }
        }
    }
    HomeStart { anchors.fill: parent; visible: opening.beat.kind === "home"; story: opening.story; textScale: opening.textScale; onAppearanceRequested: opening.moveTo("appearance") }
}
