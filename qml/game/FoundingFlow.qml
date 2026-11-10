import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: opening
    objectName: "foundingScreen"
    required property real textScale
    signal menuRequested()

    readonly property var story: campaign.founding
    readonly property bool reducedMotion: campaign.reducedMotion
    readonly property int beatIndex: Math.max(0, script.beats.findIndex(page => page.id === story.beat))
    readonly property var beat: script.beats[beatIndex] || {id:"opening",kind:"narration",tone:"night",art:"hospital"}
    readonly property string beatId: beat.id
    readonly property string sceneTone: beat.tone
    readonly property bool narrative: beat.kind === "narration"
    readonly property bool birthPaper: ["first_name", "last_name", "birth_sex", "certificate"].includes(beatId)
    readonly property bool settling: textSequence.running
    readonly property real readWidth: 660 + 170 * Math.max(0, Math.min(textScale, 2) - 1)
    readonly property real zoom: Math.min(2, textScale)

    property bool initialized: false
    property real titleProgress: 1
    property real narrationProgress: 1
    property real afterwordProgress: 1
    property real actionsProgress: 1
    property int choicePage: 0
    property int appearanceFeature: 0
    property string actionError: ""
    property string inputBeat: ""
    property real autograph: 0
    property color sceneAccent: ink.accent

    Behavior on sceneAccent {
        enabled: !opening.reducedMotion
        ColorAnimation { duration: 460 }
    }

    GameInk {
        id: ink
        tone: opening.sceneTone
    }

    FoundingStory {
        id: script
        story: opening.story
    }

    function presentBeat() {
        if (!initialized || !visible) return
        textSequence.stop()
        signing.stop()
        autograph = beatId === "certificate" && reducedMotion ? 1 : 0
        if (beatId === "certificate" && !reducedMotion) signing.start()
        choicePage = 0
        actionError = ""
        const source = "qrc:/pgp/assets/founding/" + beat.art + ".svg"
        if (scene.source.toString() !== source || beatId === "opening") {
            artFade.stop()
            previousScene.source = beatId === "opening" ? "" : scene.source
            scene.opacity = reducedMotion ? 1 : 0
            scene.source = source
            if (!reducedMotion) artFade.start()
        }

        if (beat.kind === "field" && inputBeat !== beatId) editor.text = story.form[beatId] || ""
        inputBeat = beatId
        forceActiveFocus()
        if (reducedMotion) finishReveal()
        else {
            titleProgress = 0
            narrationProgress = 0
            afterwordProgress = 0
            actionsProgress = 0
            textSequence.start()
        }
    }

    function finishReveal() {
        textSequence.stop()
        titleProgress = 1
        narrationProgress = 1
        afterwordProgress = 1
        actionsProgress = 1
        Qt.callLater(() => {
            if (!visible || settling) return
            if (beat.kind === "field") {
                editor.forceActiveFocus()
                editor.selectAll()
            } else if (continueAction.visible && continueAction.enabled) {
                continueAction.forceActiveFocus()
            } else {
                // Leave the decision unselected until the player reaches a choice.
                opening.forceActiveFocus()
            }
        })
    }

    function moveTo(id) {
        campaign.clearNotice()
        story.beat = id
    }

    function advance() {
        if (beatId === "certificate" && autograph < 1) return
        if (beat.kind === "review") {
            campaign.startFounderCampaign()
            return
        }
        if (beat.kind === "field") {
            actionError = story.answerText(beatId, editor.text)
            if (actionError.length) {
                editor.forceActiveFocus()
                return
            }
        }
        menuAudio.select()
        moveTo(script.beats[Math.min(beatIndex+1,script.beats.length-1)].id)
    }

    function answer(id) {
        if(beatId==="pronouns" && id==="other") {moveTo("other_pronouns");return}
        if(beatId==="other_pronouns") {story.setField("pronouns",id);advance();return}
        if (beat.kind === "chapter") story.choose(beat.chapter, id)
        else if (beat.kind === "offer") {
            actionError = story.answerOffer(beat.offer, id === "accept")
            if (actionError.length) return
        } else story.setField(beatId, id)
        advance()
    }

    function previousBeat() {
        moveTo(script.beats[Math.max(0,beatIndex-1)].id)
    }
    function sceneFill(source) {
        return width < height * 1.32 && source.toString().endsWith("/hospital.svg")
            ? Image.PreserveAspectFit : Image.PreserveAspectCrop
    }

    Component.onCompleted: {
        initialized = true
        presentBeat()
    }
    onVisibleChanged: if (visible) presentBeat()
    onBeatIdChanged: presentBeat()
    onReducedMotionChanged: if (visible && reducedMotion) {
        artFade.stop()
        scene.opacity = 1
        if (beatId === "certificate") { signing.stop(); autograph = 1 }
        finishReveal()
    }
    NumberAnimation {
        id: signing
        target: opening; property: "autograph"
        from: 0; to: 1; duration: 1800; easing.type: Easing.InOutSine
        onFinished: if (!opening.settling) continueAction.forceActiveFocus()
    }

    Keys.onPressed: event => {
        if (settling && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            event.accepted = true
            if (!event.isAutoRepeat) finishReveal()
        }
    }

    HomeStart {
        anchors.fill:parent;visible:opening.beat.kind==="home";z:10
        story:opening.story;textScale:opening.textScale
        onMenuRequested:opening.menuRequested()
        onAppearanceRequested:opening.moveTo("appearance")
    }
    Rectangle { anchors.fill: parent; color: ink.ink }
    Item {
        anchors.fill: parent
        clip: true
        Image {
            id: previousScene
            anchors.fill: parent
            fillMode: opening.sceneFill(source)
            horizontalAlignment: Image.AlignRight
        }
        Image {
            id: scene
            objectName: "foundingScene"
            anchors.fill: parent
            fillMode: opening.sceneFill(source)
            horizontalAlignment: Image.AlignRight
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#f0070d16" }
                GradientStop { position: 0.36; color: "#d7070d16" }
                GradientStop { position: 0.65; color: "#a3070d16" }
                GradientStop { position: 1; color: "#16070d16" }
            }
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.27
            gradient: Gradient {
                GradientStop { position: 0; color: "#00090d13" }
                GradientStop { position: 1; color: "#d9090d13" }
            }
        }
        TapHandler {
            enabled: opening.settling
            onTapped: opening.finishReveal()
        }
    }

    NumberAnimation {
        id: artFade
        target: scene
        property: "opacity"
        to: 1
        duration: 2100
        easing.type: Easing.InOutSine
        onFinished: previousScene.source = ""
    }
    SequentialAnimation {
        id: textSequence
        PauseAnimation { duration: opening.narrative ? 520 : 150 }
        NumberAnimation { target: opening; property: "titleProgress"; to: 1; duration: 420; easing.type: Easing.OutSine }
        NumberAnimation { target: opening; property: "narrationProgress"; to: 1; duration: opening.narrative ? 680 : 330; easing.type: Easing.OutCubic }
        PauseAnimation { duration: opening.narrative ? 240 : 0 }
        NumberAnimation { target: opening; property: "afterwordProgress"; to: 1; duration: opening.narrative ? 640 : 0; easing.type: Easing.OutCubic }
        NumberAnimation { target: opening; property: "actionsProgress"; to: 1; duration: 240; easing.type: Easing.OutSine }
        onFinished: opening.finishReveal()
    }

    component StoryText: Text {
        color: ink.paper
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        fontSizeMode: Text.Fit
        minimumPixelSize: 16
        elide: Text.ElideRight
        Accessible.ignored: !visible || opacity === 0
    }

    component StoryAction: Item {
        id: action
        required property string caption
        property string detail: ""
        property bool indented: false
        property bool selected: false
        property bool alignRight: false
        property bool onPaper: false
        property real typeSize: 20
        property real nameHeight: Math.min(42, height * 0.55)
        readonly property bool lit: pointer.containsMouse || activeFocus
        signal activated()
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: caption
        Accessible.description: detail
        Accessible.onPressAction: if (enabled) activated()

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                event.accepted = true
                if (enabled && !event.isAutoRepeat) activated()
            }
        }
        Text {
            visible: action.indented
            width: 26
            height: title.height
            text: action.selected ? "◆" : "›"
            color: action.onPaper ? "#78593c" : action.lit || action.selected ? opening.sceneAccent : ink.muted
            font.pixelSize: action.selected ? 11 : 24
            verticalAlignment: Text.AlignVCenter
        }
        StoryText {
            id: title
            x: action.indented ? 26 : 0
            width: parent.width - x
            height: action.detail.length ? action.nameHeight : parent.height
            text: action.caption
            font.pixelSize: Math.round(action.typeSize)
            color: action.onPaper ? (action.lit || action.selected ? "#704827" : "#293d43")
                : !action.enabled ? ink.muted : action.lit || action.selected ? opening.sceneAccent : ink.paper
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: action.alignRight ? Text.AlignRight : Text.AlignLeft
            font.weight: action.lit ? Font.DemiBold : Font.Normal
            Accessible.ignored: true
        }
        StoryText {
            visible: action.detail.length > 0
            x: title.x
            y: title.height - 2
            width: title.width
            height: Math.max(0, parent.height - y - 3)
            text: action.detail
            font.pixelSize: Math.round(action.typeSize * 0.74)
            minimumPixelSize: 15
            color: ink.secondary
            Accessible.ignored: true
        }
        Rectangle {
            x: title.x
            y: parent.height - 1
            width: title.width
            height: 1
            color: action.onPaper ? "#a98557" : Qt.alpha(opening.sceneAccent, action.lit && action.enabled ? 1.0 : 0.23)
            visible: action.indented || action.lit
        }
        MouseArea {
            id: pointer
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: action.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: action.activated()
        }
    }

    // Logical pixels for the prologue. Fit the same stage at every window size.
    Item {
        id: stage
        visible:opening.beat.kind!=="home"
        objectName: "foundingCanvas"
        anchors.centerIn: parent
        width: 1120
        height: 700
        scale: Math.min(opening.width / width, opening.height / height)
        readonly property int margin: 76

        StoryText {
            x: stage.margin
            y: 52
            width: 700
            height: 28
            text: opening.beat.date || ""
            font.pixelSize: Math.round(15 * Math.min(opening.zoom, 1.5))
            font.letterSpacing: 3
            color: opening.sceneAccent
            opacity: opening.titleProgress
        }
        Rectangle {
            x: stage.margin
            y: 88
            width: 55
            height: 2
            color: opening.sceneAccent
            opacity: opening.titleProgress * 0.8
        }
        StoryAction {
            objectName: "foundingMainMenuButton"
            x: stage.width - stage.margin - width
            y: 40
            width: 174
            height: 50
            caption: qsTr("Main menu")
            typeSize: 18 * Math.min(opening.zoom, 1.55)
            alignRight: true
            enabled: !campaign.busy
            onActivated: opening.menuRequested()
        }
        StoryText {
            objectName: "foundingTitle"
            visible: !opening.birthPaper
            x: stage.margin
            y: (opening.narrative ? 192 : 119) + 10 * (1 - opening.titleProgress)
            width: opening.readWidth + 60
            height: 91
            text: opening.beat.title || ""
            font.family: ink.displayFace
            font.pixelSize: Math.round((opening.narrative ? 65 : 49) * opening.zoom)
            minimumPixelSize: 31
            verticalAlignment: Text.AlignVCenter
            color: ink.paper
            opacity: opening.titleProgress
        }
        StoryText {
            objectName: "foundingNarration"
            visible: !opening.birthPaper
            x: stage.margin
            y: (opening.narrative ? 294 : 219) + 9 * (1 - opening.narrationProgress)
            width: opening.readWidth
            height: opening.narrative ? 104 : 101
            text: opening.beat.narration || ""
            font.pixelSize: Math.round(26 * opening.zoom)
            minimumPixelSize: 20
            opacity: opening.narrationProgress
        }
        StoryText {
            objectName: "foundingAfterword"
            visible: opening.narrative
            x: stage.margin
            y: 408 + 8 * (1 - opening.afterwordProgress)
            width: opening.readWidth
            height: 111
            text: opening.beat.afterword || ""
            font.pixelSize: Math.round(26 * opening.zoom)
            minimumPixelSize: 20
            opacity: opening.afterwordProgress
        }
        StoryText {
            objectName: "foundingQuestion"
            visible: !opening.birthPaper && !!opening.beat.prompt
            x: stage.margin
            y: 323
            width: opening.readWidth
            height: 56
            text: opening.beat.prompt || ""
            font.family: ink.displayFace
            font.pixelSize: Math.round(29 * opening.zoom)
            minimumPixelSize: 22
            opacity: opening.actionsProgress
            color: opening.sceneAccent
        }
        Item {
            id: answerLine
            parent: opening.birthPaper ? paperFields : stage
            x: opening.birthPaper ? 0 : stage.margin
            y: opening.birthPaper ? (opening.beatId === "first_name" ? 157 : 234) : 392
            width: opening.birthPaper ? paperFields.width : Math.min(opening.readWidth, 660)
            height: opening.birthPaper ? 55 : 62
            visible: opening.beat.kind === "field"
            opacity: opening.actionsProgress
            enabled: !opening.settling
            TextInput {
                id: editor
                objectName: "foundingAnswer"
                anchors.fill: parent
                anchors.bottomMargin: 6
                clip: true
                activeFocusOnTab: true
                selectByMouse: true
                maximumLength: opening.beat.limit || 128
                font.family: ink.displayFace
                font.pixelSize: Math.round(34 * Math.min(1.42, opening.zoom))
                color: opening.birthPaper ? "#263d48" : ink.paper
                selectionColor: opening.birthPaper ? "#395968" : ink.stage
                selectedTextColor: "#fff7e5"
                verticalAlignment: TextInput.AlignVCenter
                Accessible.name: opening.beat.prompt || ""
                onAccepted: opening.advance()
                onTextEdited: opening.actionError = ""
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 2
                color: opening.birthPaper ? "#78593c" : Qt.alpha(opening.sceneAccent, editor.activeFocus ? 0.85 : 0.4)
            }
        }

        Item {
            id: decisions
            parent: opening.birthPaper ? paperFields : stage
            readonly property var rows: opening.beat.choices || []
            readonly property int pageSize: opening.birthPaper ? 3 : 2
            readonly property int pages: Math.ceil(rows.length / pageSize)
            readonly property var shown: rows.slice(opening.choicePage * pageSize, (opening.choicePage + 1) * pageSize)
            x: opening.birthPaper ? 0 : stage.margin
            y: opening.birthPaper ? 313 : 381 + 7 * (1 - opening.actionsProgress)
            width: opening.birthPaper ? paperFields.width : opening.readWidth
            height: opening.birthPaper ? 56 : 221
            opacity: opening.actionsProgress
            visible: rows.length > 0
            enabled: !opening.settling
            function changePage(delta) {
                opening.choicePage += delta
                Qt.callLater(() => choiceButtons.itemAt(0)?.forceActiveFocus(Qt.TabFocusReason))
            }
            Grid {
                id: options
                columns: opening.birthPaper ? 3 : 1
                spacing: 7
                Repeater {
                    id: choiceButtons
                    model: decisions.shown
                    delegate: StoryAction {
                        required property var modelData
                        objectName: "foundingChoice_" + modelData.id
                        width: (decisions.width - (options.columns - 1) * options.spacing) / options.columns
                        height: opening.birthPaper ? decisions.height : decisions.pages > 1 ? 86 : 106
                        nameHeight: Math.min(38, height * 0.48)
                        caption: modelData.name
                        detail: modelData.available === false ? modelData.reason : modelData.detail || ""
                        typeSize: 21 * opening.zoom
                        indented: true
                        onPaper: opening.birthPaper
                        selected: opening.beat.kind === "chapter"
                            ? opening.story.form.choices[opening.beat.chapter] === modelData.id
                            : opening.beat.kind === "offer"
                                ? modelData.id === "accept" && opening.story.form.backers.includes(opening.beat.offer)
                                : opening.story.form[opening.beatId] === modelData.id
                        enabled: modelData.available !== false && decisions.enabled
                        onActivated: opening.answer(modelData.id)
                    }
                }
            }
            Row {
                y: options.height + 6
                visible: decisions.pages > 1
                spacing: 10
                StoryAction {
                    objectName: "foundingPreviousChoices"
                    visible: opening.choicePage > 0
                    width: 120; height: 33
                    caption: qsTr("‹  Previous")
                    typeSize: 20 * Math.min(opening.zoom, 1.45)
                    onActivated: decisions.changePage(-1)
                }
                StoryText {
                    width: 54; height: 33
                    text: (opening.choicePage + 1) + " / " + decisions.pages
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 19
                    color: ink.secondary
                }
                StoryAction {
                    objectName: "foundingMoreChoices"
                    visible: opening.choicePage + 1 < decisions.pages
                    width: 170; height: 33
                    caption: qsTr("More choices  ›")
                    typeSize: 20 * Math.min(opening.zoom, 1.45)
                    onActivated: decisions.changePage(1)
                }
            }
        }
        Item {
            x: stage.margin; y: 381; width: opening.readWidth; height: 221
            visible: opening.beat.kind === "appearance"
            enabled: visible && !opening.settling
            opacity: opening.actionsProgress
            Image {
                objectName: "foundingPortraitPreview"
                width: 210; height: 210
                source: opening.story.portrait
                sourceSize: Qt.size(420,420)
                fillMode: Image.PreserveAspectFit
                Accessible.role: Accessible.Graphic
                Accessible.name: qsTr("Your customized portrait")
            }
            ScrollView {
                x:230;width:parent.width-230;height:parent.height;clip:true;contentWidth:availableWidth
                ScrollBar.vertical.policy:ScrollBar.AlwaysOn
                ColumnLayout {
                    width:parent.width;spacing:6
                    Repeater {
                        model:opening.story.appearanceParts
                        RowLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth:true
                            Label {Layout.fillWidth:true;text:modelData.name;color:"#ece4cf";font.pixelSize:16*opening.zoom;wrapMode:Text.Wrap}
                            WorldButton {objectName:"appearancePrev_"+index;text:"‹";textSize:16;Accessible.name:qsTr("Previous %1").arg(modelData.name);onClicked:opening.story.cycleAppearance(index,-1)}
                            Label {text:String(Number(index<15?opening.story.form.appearance[index]:opening.story.form.style[index-15])+1)+" / "+modelData.count;color:"#c7d8cc";font.pixelSize:15}
                            WorldButton {objectName:"appearanceNext_"+index;text:"›";textSize:16;Accessible.name:qsTr("Next %1").arg(modelData.name);onClicked:opening.story.cycleAppearance(index,1)}
                        }
                    }
                }
            }

        }
        Item {
            id: paper
            objectName: "foundingBirthCertificate"
            x: stage.margin
            y: 116
            width: 620
            height: 490
            opacity: opening.birthPaper ? 1 : 0
            visible: opacity > 0
            enabled: opening.birthPaper
            transform: Translate { y: 12 * (1 - paper.opacity) }
            Behavior on opacity { enabled: !opening.reducedMotion; NumberAnimation { duration: 240 } }
            Image {
                anchors.fill: parent
                source: "qrc:/pgp/assets/founding/birth-certificate.svg"
                Accessible.ignored: true
            }
            Item {
                id: paperFields
                anchors.fill: parent
                anchors.margins: 26
                component PaperText: StoryText {
                    width: paperFields.width
                    color: "#35464b"
                    minimumPixelSize: 17
                    font.pixelSize: Math.round(21 * opening.zoom)
                }
                PaperText {
                    y: 5; width: 440; height: 25
                    text: qsTr("CALIFORNIA  /  JANUARY 1997")
                    font.pixelSize: 17; font.letterSpacing: 2
                    color: "#725637"
                }
                PaperText {
                    y: 29; width: 450; height: 55
                    text: qsTr("Certificate of Birth")
                    font.family: ink.displayFace
                    font.pixelSize: Math.round(47 * Math.min(1.3, opening.zoom))
                    minimumPixelSize: 34
                }
                PaperText {
                    y: 91; height: 46
                    text: opening.beat.narration || ""
                    color: "#604d3b"
                }
                PaperText { y: 140; height: 25; text: qsTr("Given name"); font.bold: true }
                PaperText {
                    visible: opening.beatId !== "first_name"
                    y: 165; height: 44; text: opening.story.form.first_name || ""
                    font.family: ink.displayFace; font.pixelSize: 33
                }
                Rectangle { visible: opening.beatId !== "first_name"; y: 211; width: parent.width; height: 1; color: "#ab8b65" }
                PaperText { y: 217; height: 25; text: qsTr("Surname"); font.bold: true }
                PaperText {
                    visible: opening.beatId !== "last_name"
                    y: 242; height: 44; text: opening.story.form.last_name || ""
                    font.family: ink.displayFace; font.pixelSize: 33
                }
                Rectangle { visible: opening.beatId !== "last_name"; y: 288; width: parent.width; height: 1; color: "#ab8b65" }
                PaperText { y: 297; height: 26; text: qsTr("Sex at birth"); font.bold: true }
                PaperText {
                    y: 329; height: 36
                    visible: opening.beatId === "certificate"
                    text: opening.story.form.birth_sex === "female" ? qsTr("Female")
                        : opening.story.form.birth_sex === "male" ? qsTr("Male") : qsTr("Indeterminate")
                    font.family: ink.displayFace; font.pixelSize: 29
                }
                Item {
                    objectName: "foundingSignature"
                    y: 367; height: 62
                    width: (paperFields.width - 65) * opening.autograph
                    visible: opening.beatId === "certificate"
                    clip: true
                    Text {
                        width: paperFields.width - 65; height: parent.height
                        text: qsTr("The %1 family").arg(opening.story.form.last_name)
                        font.family: "Snell Roundhand"; font.italic: true
                        font.pixelSize: 42; minimumPixelSize: 23; fontSizeMode: Text.HorizontalFit
                        verticalAlignment: Text.AlignVCenter; color: "#314865"
                    }
                }
                Rectangle {
                    x: paperFields.width - width; y: 374
                    width: 52; height: 52; radius: 26
                    color: "#903f42"; border.color: "#61272c"; border.width: 2
                    visible: opening.beatId === "certificate"
                    opacity: Math.max(0, (opening.autograph - 0.8) * 5)
                    scale: 1.28 - 0.28 * opacity
                    Text { anchors.centerIn: parent; text: "✦"; font.pixelSize: 27; color: "#f6dab1"; Accessible.ignored: true }
                }
            }
        }
        StoryText {
            objectName: "foundingTerms"
            visible: opening.beat.kind === "review"
            x: stage.margin
            y: 334
            width: opening.readWidth
            height: 259
            text: visible ? opening.story.preview : ""
            font.pixelSize: Math.round(23 * opening.zoom)
            lineHeight: 1.2
            opacity: opening.actionsProgress
        }
        StoryText {
            objectName: "foundingError"
            x: stage.margin
            y: 607
            width: stage.width - 2 * stage.margin
            height: 37
            text: opening.actionError || campaign.notice || opening.story.error
            visible: text.length > 0
            color: "#f4bc98"
            font.pixelSize: Math.round(18 * opening.zoom)
            Accessible.role: Accessible.AlertMessage
        }
        Item {
            x: stage.margin
            y: 646
            width: stage.width - 2 * stage.margin
            height: 44
            opacity: opening.actionsProgress
            enabled: !opening.settling && !campaign.busy

            StoryAction {
                objectName: "foundingBackButton"
                visible: opening.beatIndex > 0
                width: 140
                height: parent.height
                caption: qsTr("‹  Back")
                typeSize: 20 * opening.zoom
                onActivated: opening.previousBeat()
            }
            StoryAction {
                objectName: "foundingSeedButton"
                visible: opening.beat.kind === "review"
                x: 180
                width: 196
                height: parent.height
                caption: qsTr("World seed")
                typeSize: 17 * opening.zoom
                onActivated: opening.moveTo("seed")
            }
            StoryAction {
                id: continueAction
                objectName: opening.beat.kind === "review" ? "foundingStartButton" : "foundingNextButton"
                visible: opening.narrative || opening.beat.kind === "field" || opening.beat.kind === "review" || opening.beat.kind === "appearance" || opening.beat.kind === "certificate"
                x: Math.min(opening.readWidth - width, parent.width - width)
                width: 280
                height: parent.height
                caption: opening.beat.kind === "review" ? qsTr("Enter the office  →") : qsTr("Continue  →")
                typeSize: 22 * opening.zoom
                alignRight: true
                enabled: campaign.ready && parent.enabled && (opening.beatId !== "certificate" || opening.autograph >= 1)
                onActivated: opening.advance()
            }
        }
    }
}
