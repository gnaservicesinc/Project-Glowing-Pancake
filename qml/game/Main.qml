import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    objectName: "gameWindow"
    title: menuVisible || campaign.welcome ? qsTr("Project Glowing Pancake") : qsTr("%1 · Project Glowing Pancake").arg(campaign.companyName)
    visible: false
    width: 1280
    height: 720
    minimumWidth: 650
    minimumHeight: 480
    color: "#141d28"
    palette.windowText: "#f2f4f7"
    palette.text: "#f2f4f7"
    palette.buttonText: "#f2f4f7"
    palette.window: "#141d28"
    palette.base: "#192634"
    palette.button: "#25384a"
    palette.mid: "#70869b"
    palette.highlight: "#335c7a"
    palette.highlightedText: "#ffffff"

    property string room: "office"
    property bool settingsOpen: false
    property bool menuVisible: !prototypeSmoke
    property bool sessionEntered: prototypeSmoke
    property bool pendingMenuLoad: false
    property bool pendingMenuSettings: false
    property string displayPreference: ""
    property bool exitConfirmed: false
    property bool resumeAfterExitCancel: false
    property bool exitCancelPending: false
    readonly property int textSize: Math.round(14 * campaign.textScalePercent / 100)
    readonly property bool narrow: width < 1030
    font.pixelSize: textSize

    function applyDisplayPreferences() {
        if (prototypeSmoke) return
        const preference = campaign.fullScreen + ":" + campaign.windowWidth + ":" + campaign.windowHeight
        if (preference === displayPreference) return
        displayPreference = preference
        if (campaign.fullScreen) showFullScreen()
        else {
            showNormal()
            width = Math.min(campaign.windowWidth, Screen.desktopAvailableWidth)
            height = Math.min(campaign.windowHeight, Screen.desktopAvailableHeight)
            x = Math.max(0, (Screen.desktopAvailableWidth - width) / 2)
            y = Math.max(0, (Screen.desktopAvailableHeight - height) / 2)
        }
    }
    function openMainMenu() {
        if (!campaign.paused) campaign.togglePaused()
        menuVisible = true
        frontMenu.home()
    }
    function loadCampaign(file) {
        pendingMenuLoad = menuVisible
        campaign.loadFrom(file)
        if (!campaign.busy) pendingMenuLoad = false
    }
    function requestExit() {
        if (exitDialog.visible) return
        resumeAfterExitCancel = !menuVisible && !campaign.paused
        exitCancelPending = false
        if (!campaign.paused) campaign.togglePaused()
        exitDialog.open()
    }
    function syncOverlayPause() {
        if (campaign.busy || !campaign.ready) return
        if ((menuVisible || exitDialog.visible) && !campaign.paused) campaign.togglePaused()
        else if (exitCancelPending) {
            exitCancelPending = false
            if (campaign.paused && campaign.canPlay) campaign.togglePaused()
        }
    }
    onClosing: close => {
        if (!exitConfirmed) { close.accepted = false; requestExit() }
    }
    Shortcut {
        sequence: "Escape"
        enabled: !saveDialog.visible && !loadDialog.visible && !restartDialog.visible && !musicCredits.visible && !cancelDialog.visible && !exitDialog.visible
        onActivated: window.requestExit()
    }
    Connections {
        target: campaign
        function onStateChanged() { window.syncOverlayPause() }
        function onSettingsChanged() { window.applyDisplayPreferences() }
        function onCommandFinished(accepted, explanation) {
            if (window.pendingMenuSettings) {
                window.pendingMenuSettings = false
                frontMenu.message = accepted ? qsTr("Settings saved.") : explanation
                if (accepted) menuAudio.select()
            }
            if (!window.pendingMenuLoad) return
            window.pendingMenuLoad = false
            if (accepted) {
                window.sessionEntered = true
                window.menuVisible = false
                window.room = "office"
            } else frontMenu.message = explanation
        }
    }

    component Heading: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        color: "#ffd897"
        font.pixelSize: Math.round(window.textSize * 1.22)
        font.bold: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        font.pixelSize: window.textSize
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }
    component Action: Button {
        id: action
        Layout.maximumWidth: window.width - 60
        Accessible.name: text
        font.pixelSize: window.textSize
        topPadding: 5
        bottomPadding: 5
        leftPadding: 12
        rightPadding: 12
        contentItem: Text {
            text: action.text
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font: action.font
            color: action.enabled ? "#f2f4f7" : "#aebbc7"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: parent.down || parent.checked ? "#375b76" : parent.hovered ? "#2d455a" : "#243648"
            border.width: parent.activeFocus ? 2 : 1
            border.color: parent.activeFocus ? "#ffd897" : "#536d83"
            opacity: parent.enabled ? 1 : 0.55
        }
    }
    component Panel: Frame {
        padding: 14
        Layout.fillWidth: true
        background: Rectangle { radius: 8; color: "#192634"; border.color: "#394c5e" }
    }

    Dialog {
        id: exitDialog
        objectName: "exitConfirmDialog"
        anchors.centerIn: Overlay.overlay
        width: Math.min(560, window.width - 48)
        title: qsTr("Exit game?")
        modal: true
        focus: true
        padding: 24
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "#101e29"; border.color: "#b18e5c"; radius: 8 }
        header: Label {
            text: exitDialog.title
            padding: 24
            bottomPadding: 0
            font.family: "Georgia"
            font.pixelSize: window.textSize * 1.8
            color: "#f5ecdd"
        }
        contentItem: ColumnLayout {
            spacing: 24
            Label {
                Layout.fillWidth: true
                text: window.sessionEntered && !campaign.welcome
                    ? qsTr("Any unsaved progress will be lost. Return to the desktop?")
                    : qsTr("Return to the desktop?")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: "#e8e4dc"
                font.pixelSize: window.textSize * 1.1
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Action {
                    id: cancelExitButton
                    objectName: "exitCancelButton"
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: exitDialog.reject()
                }
                Action {
                    objectName: "exitConfirmButton"
                    text: qsTr("Exit game")
                    Layout.fillWidth: true
                    onClicked: exitDialog.accept()
                }
            }
        }
        onOpened: cancelExitButton.forceActiveFocus()
        onAccepted: { window.exitConfirmed = true; window.close(); Qt.quit() }
        onRejected: {
            window.exitCancelPending = window.resumeAfterExitCancel
            window.resumeAfterExitCancel = false
            window.syncOverlayPause()
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save campaign")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pgpsave"
        nameFilters: [qsTr("Campaign saves (*.pgpsave)")]
        currentFolder: campaign.saveFolder
        onAccepted: campaign.saveTo(selectedFile)
    }
    FileDialog {
        id: loadDialog
        objectName: "campaignLoadDialog"
        title: qsTr("Load campaign")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Campaign saves (*.pgpsave)")]
        currentFolder: campaign.saveFolder
        onAccepted: window.loadCampaign(selectedFile)
        onRejected: if (window.menuVisible) frontMenu.home()
    }

    Dialog {
        id: restartDialog
        title: qsTr("Start another campaign?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(580, window.width - 32)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: qsTr("Save first if you want to return to this campaign. Starting a new one replaces the current unsaved session. Existing local save files remain available."); textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: window.textSize }
        onAccepted: {
            campaign.openWelcome()
            window.menuVisible = false
            window.sessionEntered = true
        }
    }
    MusicCredits {
        id: musicCredits
        anchors.centerIn: Overlay.overlay
        width: Math.min(740, window.width - 32)
        height: Math.min(620, window.height - 32)
        textSize: window.textSize
        reducedMotion: campaign.reducedMotion
        onClosed: musicCreditsButton.forceActiveFocus()
    }
    ColumnLayout {
        visible: !window.menuVisible && !(campaign.welcome && campaign.founding.available)
        enabled: visible
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Panel {
            padding: 10
            contentItem: ColumnLayout {
                spacing: 5
                Copy {
                    objectName: "statusLine"
                    Layout.maximumWidth: window.width - 44
                    text: qsTr("%1  ·  %2  ·  Cash %3  ·  Available %4")
                        .arg(campaign.date).arg(campaign.phaseText).arg(campaign.cashText).arg(campaign.availableText)
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Action {
                        objectName: "officeTab"; text: qsTr("Office")
                        checked: window.room === "office"
                        onClicked: window.room = "office"
                    }
                    Action {
                        objectName: "labTab"; text: qsTr("Lab")
                        checked: window.room === "lab"
                        onClicked: window.room = "lab"
                    }
                    Action {
                        objectName: "inboxTab"; text: qsTr("Inbox")
                        checked: window.room === "inbox"
                        onClicked: window.room = "inbox"
                    }
                    Action {
                        objectName: "operationsTab"; text: qsTr("Operations")
                        visible: !campaign.welcome && campaign.infrastructure.available
                        checked: window.room === "operations"
                        onClicked: window.room = "operations"
                    }
                    Action {
                        objectName: "civicTab"; text: qsTr("Civic")
                        visible: !campaign.welcome && campaign.civic.available
                        checked: window.room === "civic"
                        onClicked: window.room = "civic"
                    }
                    Action {
                        objectName: "societyTab"; text: qsTr("World")
                        visible: !campaign.welcome && campaign.society.available
                        checked: window.room === "society"
                        onClicked: window.room = "society"
                    }
                    Action {
                        objectName: "institutionsTab"; text: qsTr("Institutions")
                        visible: !campaign.welcome && campaign.institutions.available
                        checked: window.room === "institutions"
                        onClicked: window.room = "institutions"
                    }
                    Action {
                        objectName: "frontierTab"; text: qsTr("Frontier")
                        visible: !campaign.welcome && campaign.frontier.available
                        checked: window.room === "frontier"
                        onClicked: window.room = "frontier"
                    }
                    Action {
                        objectName: "leadershipTab"; text: qsTr("Leadership")
                        visible: !campaign.welcome && campaign.leadership.available
                        checked: window.room === "leadership"
                        onClicked: window.room = "leadership"
                    }
                    Action {
                        objectName: "objectivesTab"; text: qsTr("Objectives")
                        visible: !campaign.welcome && campaign.objectives.available
                        checked: window.room === "objectives"
                        onClicked: window.room = "objectives"
                    }
                    Action {
                        objectName: "pauseButton"
                        text: campaign.paused ? qsTr("Play") : qsTr("Pause")
                        enabled: !campaign.busy && (!campaign.paused || campaign.canPlay)
                        onClicked: campaign.togglePaused()
                    }
                    ComboBox {
                        objectName: "speedControl"
                        model: [1, 10, 30, 60]
                        currentIndex: model.indexOf(campaign.daysPerSecond)
                        displayText: qsTr("%1 days/s").arg(campaign.daysPerSecond)
                        Accessible.name: qsTr("Days per second")
                        font.pixelSize: window.textSize
                        implicitWidth: window.textSize * 8
                        onActivated: campaign.daysPerSecond = model[currentIndex]
                    }
                    Action {
                        objectName: "stepButton"; text: qsTr("Step day")
                        enabled: campaign.paused && campaign.canAdvance
                        onClicked: campaign.step()
                    }
                    ComboBox {
                        id: milestoneChoice
                        implicitWidth: window.textSize * 11
                        model: [{id:"project",label:qsTr("Next project")},{id:"evaluation",label:qsTr("Next evaluation")},{id:"deadline",label:qsTr("Next deadline")}]
                            .concat(campaign.leadership.available ? [{id:"leadership",label:qsTr("Next leadership date")}] : [])
                            .concat(campaign.objectives.nextCheck ? [{id:"objectives",label:qsTr("Next objective check")}] : [])
                        textRole: "label"
                        font.pixelSize: window.textSize
                        Accessible.name: qsTr("Advance target")
                    }
                    Action {
                        objectName: "advanceToButton"; text: qsTr("Advance to target")
                        enabled: campaign.paused && campaign.canPlay
                        onClicked: campaign.advanceTo(milestoneChoice.model[milestoneChoice.currentIndex].id)
                    }
                    Action {
                        objectName: "saveButton"; text: qsTr("Save")
                        enabled: campaign.ready && !campaign.busy
                        onClicked: saveDialog.open()
                    }
                    Action {
                        objectName: "loadButton"; text: qsTr("Load")
                        enabled: campaign.ready && !campaign.busy
                        onClicked: loadDialog.open()
                    }
                    Action {
                        text: qsTr("New campaign…")
                        visible: campaign.management.available && !campaign.welcome
                        enabled: !campaign.busy
                        onClicked: restartDialog.open()
                    }
                    Action {
                        objectName: "mainMenuButton"; text: qsTr("Main Menu")
                        onClicked: window.openMainMenu()
                    }
                    Action {
                        objectName: "settingsButton"; text: qsTr("Settings")
                        checked: window.settingsOpen
                        onClicked: window.settingsOpen = !window.settingsOpen
                    }
                }
            }
        }

        ScrollView {
            id: scroll
            objectName: "managementScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Binding {
                target: scroll.contentItem
                property: "boundsBehavior"
                value: campaign.reducedMotion ? Flickable.StopAtBounds : Flickable.DragOverBounds
                when: scroll.contentItem !== null
            }

            function reveal(item) {
                if (!item || item === scroll || !scroll.contentItem)
                    return
                let ancestor = item
                while (ancestor && ancestor !== page)
                    ancestor = ancestor.parent
                if (!ancestor)
                    return
                const point = item.mapToItem(page, 0, 0)
                const top = scroll.contentItem.contentY
                const bottom = top + scroll.availableHeight
                const target = point.y < top ? point.y : point.y + item.height > bottom ?
                    point.y + item.height - scroll.availableHeight : top
                scroll.contentItem.contentY = Math.max(0, Math.min(target, page.height - scroll.availableHeight))
            }

            ColumnLayout {
                id: page
                width: scroll.availableWidth
                spacing: 8

                Panel {
                    objectName: "requiredPanel"
                    visible: !campaign.welcome && campaign.requiredKind !== "none"
                    contentItem: ColumnLayout {
                        FlowPresentation {
                            Layout.fillWidth: true
                            visible: campaign.requiredKind === "content"
                            heading: campaign.requiredTitle
                            detail: campaign.requiredText
                            choices: campaign.requiredChoices
                            enabled: !campaign.busy
                            onChosen: choiceId => campaign.choose(choiceId)
                        }
                        Heading { visible: campaign.requiredKind !== "content"; text: campaign.requiredTitle }
                        Copy { visible: campaign.requiredKind !== "content"; text: campaign.requiredText }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Action {
                                objectName: "acceptDelivery"; text: qsTr("Accept delivery")
                                visible: campaign.requiredKind === "core"
                                enabled: !campaign.busy
                                onClicked: campaign.resolveCoreChoice(true)
                            }
                            Action {
                                objectName: "declineDelivery"; text: qsTr("Decline delivery")
                                visible: campaign.requiredKind === "core"
                                enabled: !campaign.busy
                                onClicked: campaign.resolveCoreChoice(false)
                            }
                            Action {
                                objectName: "acknowledgeManagement"; text: qsTr("Acknowledge management alert")
                                visible: campaign.requiredKind === "management"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeManagement()
                            }
                            Action {
                                objectName: "acknowledgeDeploymentIncident"; text: qsTr("Acknowledge deployment incident alert")
                                visible: campaign.requiredKind === "deploymentIncident"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeDeploymentIncident()
                            }
                            Action {
                                objectName: "acknowledgeSiteAlert"; text: qsTr("Review site disruption")
                                visible: campaign.requiredKind === "site"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeSiteAlert()
                            }
                            Action {
                                objectName: "acknowledgeWarning"; text: qsTr("Acknowledge warning")
                                visible: campaign.requiredKind === "liquidity"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeWarning()
                            }
                            Action {
                                objectName: "openInstitutionNotice"; text: qsTr("Open dispute")
                                visible: campaign.requiredKind === "institution" || campaign.requiredKind === "executive"
                                onClicked: { window.room = "institutions"; institutionsPanel.showDisputes() }
                            }
                            Action {
                                objectName: "acknowledgeInstitutionalNotice"; text: qsTr("Acknowledge notice")
                                visible: campaign.requiredKind === "institution" || campaign.requiredKind === "executive" || campaign.requiredKind === "governance"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeInstitutionalNotice()
                            }
                            Action {
                                objectName: "openLeadershipNotice"; text: qsTr("Open Leadership")
                                visible: campaign.requiredKind === "leadership"
                                onClicked: window.room = "leadership"
                            }
                            Action {
                                objectName: "acknowledgeLeadershipNotice"; text: qsTr("Acknowledge leadership notice")
                                visible: campaign.requiredKind === "leadership"
                                enabled: !campaign.busy
                                onClicked: campaign.leadership.acknowledge()
                            }
                            Action {
                                objectName: "openObjectivePostmortem"; text: qsTr("Open postmortem")
                                visible: campaign.requiredKind === "objective"
                                onClicked: window.room = "objectives"
                            }
                            Action {
                                objectName: "continueAfterObjectiveBreach"; text: qsTr("Continue scored play")
                                visible: campaign.requiredKind === "objective"
                                enabled: !campaign.busy
                                onClicked: campaign.objectives.acknowledge()
                            }
                        }
                    }
                }

                RoomScene {
                    objectName: "roomScene"
                    Layout.fillWidth: true
                    Layout.preferredHeight: window.narrow ? 48 : 66
                    room: campaign.welcome ? "welcome" : window.room
                }

                Panel {
                    visible: campaign.notice.length > 0
                    contentItem: RowLayout {
                        Copy { text: campaign.notice }
                        Action { text: qsTr("Dismiss"); onClicked: campaign.clearNotice() }
                    }
                }

                Panel {
                    visible: window.settingsOpen
                    contentItem: ColumnLayout {
                        Heading { text: qsTr("Local preferences") }
                        Copy { text: qsTr("Required choices, the capacity setback and actual due-date stops remain active at every speed. These preferences control optional completion and advance-warning pauses.") }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            ComboBox {
                                id: scaleControl
                                objectName: "scaleControl"
                                font.pixelSize: window.textSize
                                implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, window.textSize * 6.5)
                                Accessible.name: qsTr("Text size")
                                model: [100, 125, 150, 175, 200]
                                currentIndex: model.indexOf(campaign.textScalePercent)
                                displayText: qsTr("Text %1%").arg(campaign.textScalePercent)
                                enabled: !campaign.busy
                                onActivated: campaign.textScalePercent = model[currentIndex]
                            }
                            Action {
                                objectName: "compactControl"; text: qsTr("Compact")
                                checked: campaign.compactManagement
                                enabled: !campaign.busy
                                onClicked: campaign.compactManagement = !campaign.compactManagement
                            }
                            Action {
                                text: qsTr("Reduce motion")
                                objectName: "reduceMotionControl"
                                checked: campaign.reducedMotion
                                enabled: !campaign.busy
                                onClicked: campaign.reducedMotion = !campaign.reducedMotion
                            }
                            Action {
                                id: musicCreditsButton
                                objectName: "musicCreditsButton"
                                text: qsTr("Music and credits…")
                                onClicked: musicCredits.open()
                            }
                            CheckBox { objectName: "pauseProjectsControl"; text: qsTr("Pause on project completion"); font.pixelSize: window.textSize; Accessible.name: text; checked: campaign.autoPauseProjects; enabled: !campaign.busy; onClicked: campaign.autoPauseProjects = checked }
                            CheckBox { objectName: "pauseEvaluationsControl"; text: qsTr("Pause on evaluation results"); font.pixelSize: window.textSize; Accessible.name: text; checked: campaign.autoPauseEvaluations; enabled: !campaign.busy; onClicked: campaign.autoPauseEvaluations = checked }
                            CheckBox { objectName: "pauseAlertsControl"; text: qsTr("Pause on company alerts"); font.pixelSize: window.textSize; Accessible.name: text; checked: campaign.autoPauseAlerts; enabled: !campaign.busy; onClicked: campaign.autoPauseAlerts = checked }
                            Action {
                                text: qsTr("Show guide")
                                checked: campaign.showGuide
                                enabled: !campaign.busy
                                onClicked: campaign.showGuide = !campaign.showGuide
                            }
                            Action {
                                text: qsTr("Autosave")
                                checked: campaign.autosaveEnabled
                                enabled: !campaign.busy
                                onClicked: campaign.autosaveEnabled = !campaign.autosaveEnabled
                            }
                        }
                    }
                }

                Panel {
                    visible: campaign.welcome
                    contentItem: ColumnLayout {
                        id: campaignSetupForm
                        property bool customSeed: false
                        function resetWorldSeed() {
                            worldSeedInput.text = (campaign.scenarios[competitionChoice.currentIndex] || {}).seed || ""
                            customSeed = false
                        }
                        Heading { text: qsTr("Found your AI company") }
                        Copy { text: qsTr("Choose an introduction or a century campaign, name your institution and founder, then build it through research, products and partnerships.") }
                        Copy { text: qsTr("Company name") }
                        TextField {
                            id: companyNameInput
                            objectName: "companyNameInput"
                            Layout.fillWidth: true
                            text: qsTr("Daybreak Systems")
                            maximumLength: 32
                            font.pixelSize: window.textSize
                            selectByMouse: true
                            Accessible.name: qsTr("Company name")
                        }
                        Copy { text: qsTr("Founder name") }
                        TextField {
                            id: founderNameInput
                            objectName: "founderNameInput"
                            Layout.fillWidth: true
                            text: qsTr("Alex Vale")
                            maximumLength: 32
                            font.pixelSize: window.textSize
                            selectByMouse: true
                            Accessible.name: qsTr("Founder name")
                        }
                        Copy { text: qsTr("Founder pronouns (optional)") }
                        TextField {
                            id: founderPronounsInput
                            objectName: "founderPronounsInput"
                            Layout.fillWidth: true
                            text: qsTr("they/them")
                            maximumLength: 20
                            font.pixelSize: window.textSize
                            selectByMouse: true
                            Accessible.name: qsTr("Founder pronouns")
                        }
                        Action {
                            objectName: "generateCampaignNames"
                            text: qsTr("Generate fictional names")
                            onClicked: {
                                const names = campaign.generateCampaignNames()
                                companyNameInput.text = names.company
                                founderNameInput.text = names.founder
                            }
                        }
                        Copy { text: qsTr("Headquarters") }
                        ComboBox {
                            id: headquartersChoice
                            objectName: "headquartersChoice"
                            Layout.fillWidth: true
                            model: campaign.headquarters
                            textRole: "label"
                            valueRole: "id"
                            font.pixelSize: window.textSize
                            Accessible.name: qsTr("Headquarters")
                        }
                        ComboBox {
                            id: competitionChoice
                            objectName: "competitionChoice"
                            Layout.fillWidth: true
                            model: campaign.scenarios
                            textRole: "label"
                            valueRole: "id"
                            font.pixelSize: window.textSize
                            Accessible.name: qsTr("Competition and financing setup")
                            onCurrentIndexChanged: Qt.callLater(function() {
                                if (!campaignSetupForm.customSeed) campaignSetupForm.resetWorldSeed()
                            })
                            Component.onCompleted: {
                                for (let index = 0; index < campaign.scenarios.length; ++index)
                                    if (campaign.scenarios[index].id === "scenario.world_competition") currentIndex = index
                            }
                        }
                        Copy { text: (campaign.scenarios[competitionChoice.currentIndex] || {}).detail || "" }
                        Copy { text: qsTr("Founder background") }
                        ComboBox {
                            id: backgroundChoice
                            objectName: "backgroundChoice"
                            Layout.fillWidth: true
                            model: (campaign.scenarios[competitionChoice.currentIndex] || {}).backgrounds || []
                            textRole: "label"; valueRole: "id"
                            font.pixelSize: window.textSize
                            Accessible.name: qsTr("Founder background")
                        }
                        Copy { text: (backgroundChoice.model[backgroundChoice.currentIndex] || {}).detail || "" }
                        Copy { visible: charterChoice.count > 0; text: qsTr("Organizational charter") }
                        ComboBox {
                            id: charterChoice
                            objectName: "charterChoice"
                            Layout.fillWidth: true
                            visible: count > 0
                            model: (campaign.scenarios[competitionChoice.currentIndex] || {}).charters || []
                            textRole: "label"; valueRole: "id"
                            font.pixelSize: window.textSize
                            Accessible.name: qsTr("Organizational charter")
                        }
                        Copy { visible: charterChoice.count > 0; text: (charterChoice.model[charterChoice.currentIndex] || {}).detail || "" }
                        Copy { visible: difficultyChoice.count > 0; text: qsTr("Difficulty") }
                        ComboBox {
                            id: difficultyChoice
                            objectName: "difficultyChoice"
                            Layout.fillWidth: true
                            visible: count > 0
                            model: (campaign.scenarios[competitionChoice.currentIndex] || {}).difficulties || []
                            textRole: "label"; valueRole: "id"
                            font.pixelSize: window.textSize
                            Accessible.name: qsTr("Difficulty")
                            property string chosenId: "difficulty.standard"
                            function restoreChoice() {
                                for(let i=0;i<count;++i) if(model[i].id === chosenId) {currentIndex=i;return}
                                currentIndex=count > 0 ? 0 : -1
                            }
                            onActivated: chosenId = currentValue
                            onModelChanged: Qt.callLater(restoreChoice)
                            Component.onCompleted: Qt.callLater(restoreChoice)
                        }
                        Copy { visible: difficultyChoice.count > 0; text: (difficultyChoice.model[difficultyChoice.currentIndex] || {}).detail || "" }

                        Copy {
                            objectName: "campaignOpeningPreview"
                            text: campaign.setupPreview(competitionChoice.currentValue || "", backgroundChoice.currentValue || "", charterChoice.currentValue || "", difficultyChoice.currentValue || "")
                        }
                        Copy { text: qsTr("World seed") }
                        TextField {
                            id: worldSeedInput
                            objectName: "worldSeedInput"
                            Layout.fillWidth: true
                            maximumLength: 20
                            font.pixelSize: window.textSize
                            selectByMouse: true
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{1,20}/ }
                            Accessible.name: qsTr("World seed")
                            onTextEdited: campaignSetupForm.customSeed = true
                            Component.onCompleted: Qt.callLater(function() {
                                if (!campaignSetupForm.customSeed) campaignSetupForm.resetWorldSeed()
                            })
                        }
                        Copy { text: qsTr("The same seed and setup choices start the same world. Names are generated locally.") }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Action {
                                objectName: "generateWorldSeed"
                                text: qsTr("New random seed")
                                onClicked: {
                                    campaignSetupForm.customSeed = true
                                    worldSeedInput.text = campaign.generateWorldSeed()
                                }
                            }
                            Action { text: qsTr("Use scenario seed"); onClicked: campaignSetupForm.resetWorldSeed() }
                        }
                        ObjectivePicker {
                            id: openingObjectives
                            readonly property var selectedScenario: campaign.scenarios[competitionChoice.currentIndex] || {}
                            visible: choices.length > 0
                            Layout.fillWidth: true
                            textSize: window.textSize
                            namePrefix: "openingObjective"
                            choices: selectedScenario.objectives || []
                            defaultPrimary: selectedScenario.primary || ""
                            defaultSecondary: selectedScenario.secondary || []
                        }
                        Copy {
                            text: openingObjectives.selectedScenario.objectiveNote || ""
                            visible: text.length > 0
                        }
                        CheckBox {
                            id: includeStories
                            Layout.fillWidth: true
                            text: qsTr("Include optional story layers (base events always enabled)")
                            checked: true
                            font.pixelSize: window.textSize
                            Accessible.name: text
                            contentItem: Text {
                                text: includeStories.text; font: includeStories.font; color: "#f2f4f7"
                                leftPadding: includeStories.indicator.width + includeStories.spacing
                                wrapMode: Text.Wrap
                            }
                        }
                        Action {
                            objectName: "newCampaignButton"; text: qsTr("Start new campaign")
                            enabled: campaign.ready && !campaign.busy && companyNameInput.text.trim().length > 0
                                && founderNameInput.text.trim().length > 0 && worldSeedInput.acceptableInput
                                && (backgroundChoice.count === 0 || backgroundChoice.currentIndex >= 0) && (charterChoice.count === 0 || charterChoice.currentIndex >= 0)
                                && (difficultyChoice.count === 0 || difficultyChoice.currentIndex >= 0)
                            onClicked: campaign.newCampaignSetup(headquartersChoice.currentValue, includeStories.checked,
                                competitionChoice.currentValue, openingObjectives.visible ? openingObjectives.primary : "", openingObjectives.visible ? openingObjectives.secondary() : [],
                                companyNameInput.text, founderNameInput.text, founderPronounsInput.text, worldSeedInput.text, backgroundChoice.currentValue || "", charterChoice.currentValue || "", difficultyChoice.currentValue || "")
                        }
                        Action { text: qsTr("Load an existing local campaign…"); enabled: campaign.ready && !campaign.busy; onClicked: loadDialog.open() }
                    }
                }
                Panel {
                    visible: !campaign.welcome && !campaign.objectives.available && campaign.management.ending.length > 0
                    contentItem: ColumnLayout {
                        Heading { text: qsTr("Campaign outcome") }
                        Copy { text: campaign.management.ending }
                        Action { text: qsTr("Choose a new campaign"); onClicked: campaign.openWelcome() }
                    }
                }
                Panel {
                    visible: !campaign.welcome && campaign.objectives.ended && window.room !== "objectives"
                    contentItem: ColumnLayout {
                        Heading { text: qsTr("Campaign outcome recorded") }
                        Copy { text: campaign.objectives.overview }
                        Action { text: qsTr("Review objectives and postmortem"); onClicked: window.room = "objectives" }
                    }
                }
                Panel {
                    visible: !campaign.welcome && campaign.management.available && (window.room === "office" || window.room === "lab") && campaign.showGuide && campaign.management.ending.length === 0
                    contentItem: ColumnLayout {
                        Heading { text: qsTr("Getting started") }
                        Copy { text: campaign.management.guide }
                        Action { text: qsTr("Hide guide"); enabled: !campaign.busy; onClicked: campaign.showGuide = false }
                    }
                }
                Panel {
                    visible: !campaign.welcome && campaign.management.available && (window.room === "office" || window.room === "lab")
                    contentItem: ColumnLayout {
                        Heading { text: window.room === "lab" ? qsTr("Lab · data, experiments and evidence") : qsTr("%1 · %2").arg(campaign.companyName).arg(campaign.headquartersText) }
                        Copy { text: campaign.identitySummary }
                        Copy { visible: window.room === "office"; text: campaign.capacityText + "\n" + campaign.management.briefing }
                        ManagementPanel { id: managementPanel; Layout.fillWidth: true; room: window.room; textSize: window.textSize }
                    }
                }
                Panel {
                    visible: !campaign.welcome && campaign.infrastructure.available && window.room === "operations"
                    contentItem: InfrastructurePanel { textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.civic.available && window.room === "civic"
                    contentItem: CivicPanel { textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.society.available && window.room === "society"
                    contentItem: SocietyPanel { textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.institutions.available && window.room === "institutions"
                    contentItem: InstitutionsPanel { id: institutionsPanel; textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.frontier.available && window.room === "frontier"
                    contentItem: FrontierPanel { textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.leadership.available && window.room === "leadership"
                    contentItem: SuccessionPanel { textSize: window.textSize }
                }
                Panel {
                    visible: !campaign.welcome && campaign.objectives.available && window.room === "objectives"
                    contentItem: ObjectivesPanel { textSize: window.textSize }
                }
                GridLayout {
                    visible: !campaign.welcome && window.room !== "operations" && window.room !== "civic" && window.room !== "society" && window.room !== "institutions" && window.room !== "frontier" && window.room !== "leadership" && window.room !== "objectives" && (!campaign.management.available || window.room === "inbox" || (window.room === "lab" && managementPanel.section === "projects"))
                    Layout.fillWidth: true
                    columns: window.narrow || campaign.compactManagement ? 1 : 2
                    columnSpacing: 10
                    rowSpacing: 8

                    Panel {
                        visible: window.room === "office"
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 5
                        contentItem: ColumnLayout {
                            Heading { text: qsTr("Office") }
                            Copy { text: qsTr("Review the company’s cash, capacity and decisions. Start the clock or work one day at a time.") }
                            Copy { text: campaign.capacityText }
                            Action {
                                text: qsTr("Review inbox (%1)").arg(campaign.inbox.count)
                                onClicked: window.room = "inbox"
                            }
                        }
                    }

                    Panel {
                        visible: window.room !== "office"
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 5
                        contentItem: ColumnLayout {
                            Heading { text: window.room === "lab" ? qsTr("Lab projects") : qsTr("Inbox") }
                            RowLayout {
                                TextField {
                                    objectName: "projectSearch"
                                    Layout.fillWidth: true
                                    visible: window.room === "lab"
                                    placeholderText: qsTr("Filter projects")
                                    Accessible.name: placeholderText
                                    font.pixelSize: window.textSize
                                    onTextEdited: campaign.projects.filterText = text
                                }
                                TextField {
                                    objectName: "inboxSearch"
                                    Layout.fillWidth: true
                                    visible: window.room === "inbox"
                                    placeholderText: qsTr("Filter inbox")
                                    Accessible.name: placeholderText
                                    font.pixelSize: window.textSize
                                    onTextEdited: campaign.inbox.filterText = text
                                }
                                Action {
                                    visible: window.room === "inbox" && !window.narrow
                                    text: qsTr("Required only")
                                    checked: campaign.inbox.requiredOnly
                                    onClicked: campaign.inbox.requiredOnly = !campaign.inbox.requiredOnly
                                }
                                Action {
                                    text: qsTr("Sort A–Z")
                                    checked: window.room === "lab" ? campaign.projects.sortByTitle : campaign.inbox.sortByTitle
                                    onClicked: {
                                        const rows = window.room === "lab" ? campaign.projects : campaign.inbox
                                        rows.sortByTitle = !rows.sortByTitle
                                    }
                                }
                            }
                            StableEntityList {
                                objectName: "projectList"
                                visible: window.room === "lab"
                                Layout.fillWidth: true
                                Layout.preferredHeight: window.textSize * (campaign.compactManagement ? 5.75 : 9)
                                rows: campaign.projects
                                reducedMotion: campaign.reducedMotion
                                textSize: window.textSize
                                emptyText: qsTr("No matching projects")
                                onSelected: id => { campaign.selectedProjectId = id }
                            }
                            StableEntityList {
                                objectName: "inboxList"
                                visible: window.room === "inbox"
                                Layout.fillWidth: true
                                Layout.preferredHeight: window.textSize * (campaign.compactManagement ? 5.75 : 9)
                                rows: campaign.inbox
                                reducedMotion: campaign.reducedMotion
                                textSize: window.textSize
                                emptyText: qsTr("No matching messages")
                                onSelected: id => { campaign.selectedInboxId = id }
                            }
                        }
                    }

                    Panel {
                        id: inspector
                        objectName: "inspector"
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 4
                        readonly property var entry: window.room === "lab" ? campaign.projects.selectedRow : campaign.inbox.selectedRow
                        contentItem: ColumnLayout {
                            Heading { text: qsTr("Inspector") }
                            Copy {
                                text: inspector.entry.title || qsTr("Select a project or inbox entry to review its details.")
                                font.bold: true
                            }
                            Copy { visible: text.length > 0; text: inspector.entry.detail || "" }
                            Action {
                                text: qsTr("Acknowledge company alert")
                                visible: window.room === "inbox" && !!inspector.entry.fields && !!inspector.entry.fields.noticeId && !inspector.entry.fields.acknowledged
                                enabled: campaign.paused && !campaign.busy
                                onClicked: campaign.management.acknowledgeAlert(inspector.entry.fields.noticeId)
                            }
                            Action {
                                objectName: "pauseLabProject"
                                text: qsTr("Pause lab project")
                                visible: window.room === "lab" && !!inspector.entry.fields && inspector.entry.fields.canPause === true
                                enabled: campaign.paused && !campaign.busy
                                onClicked: campaign.projectPause(inspector.entry.entityId)
                            }
                            Action {
                                objectName: "resumeLabProject"
                                text: qsTr("Resume original allocation")
                                visible: window.room === "lab" && !!inspector.entry.fields && inspector.entry.fields.canResume === true
                                enabled: campaign.paused && !campaign.busy
                                onClicked: campaign.projectResume(inspector.entry.entityId)
                            }
                            Action {
                                objectName: "cancelProject"
                                visible: window.room === "lab" && !!inspector.entry.actionable
                                enabled: campaign.paused && !campaign.busy
                                text: qsTr("Cancel project…")
                                onClicked: {
                                    cancelDialog.projectId = inspector.entry.entityId
                                    cancelDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    onActiveFocusItemChanged: {
        if (window.menuVisible)
            frontMenu.reveal(activeFocusItem)
        else if (foundingFlow.visible)
            foundingFlow.reveal(activeFocusItem)
        else if (musicCredits.visible)
            musicCredits.reveal(activeFocusItem)
        else
            scroll.reveal(activeFocusItem)
    }

    Dialog {
        id: cancelDialog
        property string projectId: ""
        title: qsTr("Cancel this project?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: campaign.projectCancel(projectId)
    }
    FoundingFlow {
        id: foundingFlow
        anchors.fill: parent
        visible: !window.menuVisible && campaign.welcome && campaign.founding.available
        enabled: visible
        textSize: window.textSize
        onMenuRequested: window.openMainMenu()
    }
    MainMenu {
        id: frontMenu
        anchors.fill: parent
        visible: window.menuVisible
        enabled: visible
        focus: visible
        canResume: window.sessionEntered
        textSize: Math.round(16 * campaign.textScalePercent / 100)
        onNewGameRequested: {
            if (window.sessionEntered && !campaign.welcome) restartDialog.open()
            else {
                campaign.openWelcome()
                window.sessionEntered = true
                window.menuVisible = false
            }
        }
        onLoadRequested: { campaign.clearNotice(); loadDialog.open() }
        onResumeRequested: window.menuVisible = false
        onQuitRequested: window.requestExit()
        onSettingsSubmitted: window.pendingMenuSettings = true
    }
}
