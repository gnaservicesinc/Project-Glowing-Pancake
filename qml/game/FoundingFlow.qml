import QtQuick

FocusScope {
    id: opening
    objectName: "foundingScreen"
    required property real textScale
    signal menuRequested()

    readonly property var story: campaign.founding
    readonly property bool reducedMotion: campaign.reducedMotion
    readonly property int beatIndex: Math.max(0, script.beats.findIndex(page => page.id === story.beat))
    readonly property var beat: script.beats[beatIndex]
    readonly property string beatId: beat.id
    readonly property string sceneTone: beat.tone
    readonly property bool narrative: beat.kind === "narration"
    readonly property bool settling: textSequence.running
    readonly property real readWidth: 660 + 170 * Math.max(0, Math.min(textScale, 2) - 1)
    readonly property real zoom: Math.min(2, textScale)

    property bool initialized: false
    property real titleProgress: 1
    property real narrationProgress: 1
    property real afterwordProgress: 1
    property real actionsProgress: 1
    property int choicePage: 0
    property string actionError: ""
    property string inputBeat: ""
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
            } else if (continueAction.visible) {
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
        moveTo(beatId === "seed" ? "review" : script.beats[beatIndex + 1].id)
    }

    function answer(id) {
        if (beat.kind === "chapter") story.choose(beat.chapter, id)
        else if (beat.kind === "offer") {
            actionError = story.answerOffer(beat.offer, id === "accept")
            if (actionError.length) return
        } else story.setField(beatId, id)
        advance()
    }

    function previousBeat() {
        moveTo(beatId === "seed" ? "review" : script.beats[beatIndex - 1].id)
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
        finishReveal()
    }

    Keys.onPressed: event => {
        if (settling && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            event.accepted = true
            if (!event.isAutoRepeat) finishReveal()
        }
    }

    Rectangle { anchors.fill: parent; color: ink.ink }
    Item {
        anchors.fill: parent
        clip: true
        Image {
            id: previousScene
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            horizontalAlignment: Image.AlignRight
        }
        Image {
            id: scene
            objectName: "foundingScene"
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
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
            color: action.lit || action.selected ? opening.sceneAccent : ink.muted
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
            color: !action.enabled ? ink.muted : action.lit || action.selected ? opening.sceneAccent : ink.paper
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
            color: Qt.alpha(opening.sceneAccent, action.lit && action.enabled ? 1.0 : 0.23)
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
            text: opening.beat.date
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
            x: stage.margin
            y: (opening.narrative ? 192 : 119) + 10 * (1 - opening.titleProgress)
            width: opening.readWidth + 60
            height: 91
            text: opening.beat.title
            font.family: ink.displayFace
            font.pixelSize: Math.round((opening.narrative ? 65 : 49) * opening.zoom)
            minimumPixelSize: 31
            verticalAlignment: Text.AlignVCenter
            color: ink.paper
            opacity: opening.titleProgress
        }
        StoryText {
            objectName: "foundingNarration"
            x: stage.margin
            y: (opening.narrative ? 294 : 219) + 9 * (1 - opening.narrationProgress)
            width: opening.readWidth
            height: opening.narrative ? 104 : 101
            text: opening.beat.narration
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
            visible: opening.beat.prompt.length > 0
            x: stage.margin
            y: 323
            width: opening.readWidth
            height: 56
            text: opening.beat.prompt
            font.family: ink.displayFace
            font.pixelSize: Math.round(29 * opening.zoom)
            minimumPixelSize: 22
            opacity: opening.actionsProgress
            color: opening.sceneAccent
        }
        Item {
            id: answerLine
            x: stage.margin
            y: 392
            width: Math.min(opening.readWidth, 660)
            height: 62
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
                color: ink.paper
                selectionColor: ink.stage
                selectedTextColor: ink.paper
                verticalAlignment: TextInput.AlignVCenter
                Accessible.name: opening.beat.prompt
                onAccepted: opening.advance()
                onTextEdited: opening.actionError = ""
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 2
                color: Qt.alpha(opening.sceneAccent, editor.activeFocus ? 0.85 : 0.4)
            }
        }

        Item {
            id: decisions
            readonly property var rows: opening.beat.choices || []
            readonly property int pageSize: 6
            readonly property var shown: rows.slice(opening.choicePage * pageSize, (opening.choicePage + 1) * pageSize)
            x: stage.margin
            y: 381 + 7 * (1 - opening.actionsProgress)
            width: opening.readWidth
            height: 221
            opacity: opening.actionsProgress
            visible: rows.length > 0
            enabled: !opening.settling
            Grid {
                id: options
                readonly property int rowCount: Math.max(1, Math.ceil(decisions.shown.length / columns))
                columns: decisions.shown.length > 4 || (decisions.shown.length === 4 && opening.zoom >= 1.5) ? 2 : 1
                spacing: 7
                Repeater {
                    model: decisions.shown
                    delegate: StoryAction {
                        required property var modelData
                        objectName: "foundingChoice_" + modelData.id
                        width: (decisions.width - (options.columns - 1) * options.spacing) / options.columns
                        height: Math.min(92, (decisions.height - (options.rowCount - 1) * options.spacing) / options.rowCount)
                        nameHeight: options.columns > 1 && options.rowCount > 2 ? 29 : Math.min(42, height * 0.55)
                        caption: modelData.name
                        detail: modelData.available === false ? modelData.reason : modelData.detail || ""
                        typeSize: 21 * opening.zoom
                        indented: true
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
                visible: opening.choicePage > 0
                x: 160
                width: 180
                height: parent.height
                caption: qsTr("‹  More choices")
                onActivated: opening.choicePage--
            }
            StoryAction {
                visible: (opening.choicePage + 1) * decisions.pageSize < decisions.rows.length
                x: parent.width - width
                width: 180
                height: parent.height
                caption: qsTr("More choices  ›")
                alignRight: true
                onActivated: opening.choicePage++
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
                visible: opening.narrative || opening.beat.kind === "field" || opening.beat.kind === "review"
                x: Math.min(opening.readWidth - width, parent.width - width)
                width: 280
                height: parent.height
                caption: opening.beat.kind === "review" ? qsTr("Enter the office  →") : qsTr("Continue  →")
                typeSize: 22 * opening.zoom
                alignRight: true
                enabled: campaign.ready && parent.enabled
                onActivated: opening.advance()
            }
        }
    }
}
