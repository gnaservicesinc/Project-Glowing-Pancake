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
    color: gameInk.ink
    palette.windowText: gameInk.paper
    palette.text: gameInk.paper
    palette.buttonText: gameInk.paper
    palette.window: gameInk.ink
    palette.base: gameInk.panel
    palette.button: "#25384a"
    palette.mid: "#70869b"
    palette.highlight: "#335c7a"
    palette.highlightedText: "#ffffff"

    property string room: "office"
    property bool deviceOpen: prototypeSmoke
    property bool startupRouted: false
    property string activeDevice: "laptop"
    function openApp(app, section, device) {
        if (!campaign.world.navigate(campaign.world.navigation.place || "workspace", campaign.world.navigation.scale || "hq", device || "laptop", app, section || "")) return false
        restoreNavigation()
        return true
    }
    function closeApp() {
        deviceOpen = false
        campaign.world.navigate(campaign.world.navigation.place || "workspace", campaign.world.navigation.scale || "hq", "", "", "")
    }
    function restoreNavigation() {
        const nav = campaign.world.navigation
        if (nav.app) {
            room = nav.app
            activeDevice = nav.device || ""
            if (nav.section && room === "office") managementPanel.officeSection = nav.section
            if (nav.section && room === "lab") managementPanel.section = nav.section
        }
        deviceOpen = !!nav.app || prototypeSmoke
    }
    function routeStartup() {
        if (startupRouted || !campaign.ready) return
        startupRouted = true
        if (campaign.currentCampaign || campaign.founding.hasDraft) {
            sessionEntered = true
            menuVisible = false
            restoreNavigation()
        }
    }
    property bool settingsOpen: false
    property bool menuVisible: !prototypeSmoke
    property bool sessionEntered: prototypeSmoke
    property bool pendingMenuLoad: false
    property bool pendingMenuSettings: false
    property string displayPreference: ""
    property bool exitConfirmed: false
    property bool resumeAfterExitCancel: false
    property bool exitCancelPending: false
    readonly property real viewportScale:Math.max(1,Math.min(4,Math.min(width/1920,height/1080)))
    readonly property int textSize: Math.round(14 * campaign.textScalePercent / 100 * viewportScale)
    readonly property bool narrow: width < 1030
    font.pixelSize: textSize

    Component.onCompleted: Qt.callLater(routeStartup)
    GameInk { id: gameInk }

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
        objectName:"gameEscapeShortcut"
        sequence: "Escape"
        enabled: !saveDialog.visible && !loadDialog.visible && !restartDialog.visible && !musicCredits.visible && !cancelDialog.visible && !exitDialog.visible
        onActivated: {
            if (window.menuVisible && window.sessionEntered) window.menuVisible = false
            else window.openMainMenu()
        }
    }
    Connections {
        target: campaign
        function onStateChanged() { window.routeStartup(); window.syncOverlayPause() }
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

    Connections {
        target: campaign.world
        function onNavigationChanged() { window.restoreNavigation() }
    }
    WorldDesktop {
        id: worldDesktop
        anchors.fill: parent
        visible: !window.menuVisible && !campaign.welcome
        enabled: visible
        textSize: window.textSize
        onOpenApp: (app,section,device) => window.openApp(app,section,device)
        onMenuRequested: window.openMainMenu()
    }
    Rectangle {
        anchors.fill: deviceWindow
        visible: deviceWindow.visible
        color: "#f0172b31"
        border.color: "#64857b"
        radius: 12
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
                    ? (campaign.storageBlocked ? qsTr("Progress could not be secured. Cancel and retry automatic saving to retain the latest actions.") : qsTr("Your company will resume here next time. Return to the desktop?"))
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
        title: qsTr("Recover or import a campaign")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Campaign and recovery files (*.pgpsave *.pgpsave.bak)")]
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
        contentItem: Label { text: qsTr("Your completed company will be archived locally before a new founder begins."); textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: window.textSize }
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
        id: deviceWindow
        visible: !window.menuVisible && !(campaign.welcome && campaign.founding.available) && (window.deviceOpen || campaign.welcome)
        enabled: visible
        anchors.fill: parent
        anchors.margins: 18
        anchors.topMargin: campaign.welcome ? 18 : worldDesktop.headerHeight + 8
        anchors.bottomMargin: campaign.welcome ? 18 : worldDesktop.footerHeight + 8
        spacing: 8
        Panel {
            padding: 10
            contentItem: RowLayout {
                Copy { text: ({devices:qsTr("Equipment"),office:qsTr("Company"),people:qsTr("People"),facilities:qsTr("Headquarters & blueprints"),lab:qsTr("Research notebook"),inbox:qsTr("News & decisions"),operations:qsTr("Infrastructure & travel"),civic:qsTr("Public affairs"),society:qsTr("World affairs"),institutions:qsTr("Institutions"),frontier:qsTr("Frontier"),leadership:qsTr("Leadership & retirement"),objectives:qsTr("Company mission")})[window.room] || window.room; font.bold:true }
                Label {visible:!window.narrow && window.activeDevice.length>0;text:window.activeDevice;font.pixelSize:window.textSize;color:"#b8d2c5"}
                Action {objectName:"closeDeviceButton";text:qsTr("Return to world");onClicked:window.closeApp()}
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


                Panel {
                    visible: campaign.notice.length > 0
                    contentItem: RowLayout {
                        Copy { text: campaign.notice }
                        Action { text: qsTr("Dismiss"); onClicked: campaign.clearNotice() }
                    }
                }

                Panel {
                    visible: !campaign.welcome && window.room === "devices"
                    contentItem: DevicePanel {textSize:window.textSize}
                }
                Panel {
                    visible: !campaign.welcome && window.room === "people"
                    contentItem: PeoplePanel {textSize:window.textSize;onOpenApp:(app,section)=>window.openApp(app,section,"laptop")}
                }
                Panel {
                    visible:!campaign.welcome && window.room === "facilities"
                    contentItem:WorkplacePanel {textSize:window.textSize;onVisitRoom:place=>{window.closeApp();campaign.world.navigate(place,"hq","","","")};onOpenInfrastructure:window.openApp("operations","","laptop")}
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
                        Action { text: qsTr("Recover an existing local campaign…"); enabled: campaign.ready && !campaign.busy; onClicked: loadDialog.open() }
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
                    visible: !campaign.welcome && window.room !== "people" && window.room !== "operations" && window.room !== "civic" && window.room !== "society" && window.room !== "institutions" && window.room !== "frontier" && window.room !== "leadership" && window.room !== "objectives" && (!campaign.management.available || window.room === "inbox" || (window.room === "lab" && managementPanel.section === "projects"))
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
        else if (musicCredits.visible)
            musicCredits.reveal(activeFocusItem)
        else if (!foundingFlow.visible)
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
    Rectangle {
        anchors.horizontalCenter:parent.horizontalCenter
        anchors.top:parent.top
        anchors.topMargin:worldDesktop.headerHeight+8
        width:Math.min(parent.width-32,650)
        height:storageMessage.implicitHeight+24
        color:"#49362b";radius:8
        visible:!window.menuVisible && !window.deviceOpen && campaign.notice.length>0
        ColumnLayout {
            id:storageMessage
            anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
            Label {Layout.fillWidth:true;text:campaign.notice;textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:window.textSize;color:"#fff0d8"}
            Flow {Layout.fillWidth:true;spacing:8
                Action {visible:campaign.storageBlocked && campaign.currentCampaign;text:qsTr("Retry saving");onClicked:campaign.retryPersistence()}
                Action {text:qsTr("Dismiss");visible:!campaign.storageBlocked;onClicked:campaign.clearNotice()}
            }
        }
    }
    FoundingFlow {
        id: foundingFlow
        anchors.fill: parent
        visible: !window.menuVisible && campaign.welcome && campaign.founding.available
        enabled: visible
        textScale: campaign.textScalePercent / 100
        onVisibleChanged: soundtrack.setIntroduction(visible)
        onMenuRequested: window.openMainMenu()
    }
    MainMenu {
        id: frontMenu
        anchors.fill: parent
        visible: window.menuVisible
        enabled: visible
        focus: visible
        canResume: campaign.currentCampaign || window.sessionEntered || (campaign.welcome && campaign.founding.hasDraft)
        textSize: Math.round(16 * campaign.textScalePercent / 100)
        onNewGameRequested: {
            if (window.sessionEntered && !campaign.welcome) restartDialog.open()
            else {
                campaign.openWelcome()
                window.sessionEntered = true
                window.menuVisible = false
            }
        }
        onLeadershipRequested: { window.menuVisible = false; window.openApp("leadership","","laptop") }
        onLoadRequested: { campaign.clearNotice(); loadDialog.open() }
        onResumeRequested: {
            window.sessionEntered = true
            window.menuVisible = false
        }
        onQuitRequested: window.requestExit()
        onSettingsSubmitted: window.pendingMenuSettings = true
    }
}
