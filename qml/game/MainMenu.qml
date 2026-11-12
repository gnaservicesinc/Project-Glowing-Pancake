import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Pgp.Menu 1.0

FocusScope {
    id: menu
    objectName: "mainMenu"
    property string page: "home"
    property bool canResume: false
    property bool canDelete: false
    property string message: ""
    property int textSize: 16
    property Item lastFocusItem: null
    readonly property real margin: width < 800 ? 28 : Math.max(56, width * 0.055)
    readonly property bool compact: width < 850 || height < 650
    readonly property bool auxiliaryDialogOpen: optionalMenu.item ? optionalMenu.item.dialogOpen : false
    signal newGameRequested()
    signal leadershipRequested()
    signal loadRequested()
    signal originalContentRequested()
    signal resumeRequested()
    signal quitRequested()
    signal settingsSubmitted()

    GameInk { id: ink }

    function home() {
        page = "home"
        if (resumeButton.visible) resumeButton.forceActiveFocus()
        else if (newGameButton.visible) newGameButton.forceActiveFocus()
    }
    function keepFocusVisible() {
        Qt.callLater(() => menu.reveal(menu.lastFocusItem))
    }
    onWidthChanged: keepFocusVisible()
    onHeightChanged: keepFocusVisible()
    onTextSizeChanged: keepFocusVisible()
    function openSettings() {
        settings.reset()
        message = ""
        page = "settings"
        backButton.forceActiveFocus()
    }
    function reveal(item) {
        if (notices.visible) { notices.reveal(item); return }
        const viewport = page === "home" ? homeScroll : detailScroll
        const content = page === "home" ? homeContent : detailContent
        if (!item || !viewport.contentItem) return
        let ancestor = item
        while (ancestor && ancestor !== content) ancestor = ancestor.parent
        if (!ancestor) return
        lastFocusItem = item
        const point = item.mapToItem(content, 0, 0)
        const top = viewport.contentItem.contentY
        const bottom = top + viewport.availableHeight
        if (point.y < top) viewport.contentItem.contentY = point.y
        else if (point.y + item.height > bottom)
            viewport.contentItem.contentY = Math.min(point.y + item.height - viewport.availableHeight,
                Math.max(0, content.height - viewport.availableHeight))
    }
    onPageChanged: detailScroll.contentItem.contentY = 0
    onVisibleChanged: if (visible) home()

    Image {
        id: still
        objectName: "menuStillImage"
        anchors.fill: parent
        source: "qrc:/pgp/assets/menu/horizon.jpg"
        fillMode: Image.PreserveAspectCrop
    }
    MenuVideo {
        id: video
        objectName: "menuVideo"
        anchors.fill: parent
        active: supported && menu.visible && campaign.motionBackground && !campaign.reducedMotion && Qt.application.state === Qt.ApplicationActive
        visible: active && frameReady
    }
    Rectangle {
        anchors.fill: parent
        color: ink.ink
        opacity: menu.page === "home" ? 0.08 : 0.89
    }
    RowLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: menu.compact ? 18 : 30
        anchors.leftMargin: menu.margin
        anchors.rightMargin: menu.margin
        spacing: 12
        Image { source: "qrc:/pgp/assets/menu/icon.png"; sourceSize.width: 40; sourceSize.height: 40; Layout.preferredWidth: 36; Layout.preferredHeight: 36 }
        Label {
            text: qsTr("PROJECT GLOWING PANCAKE")
            color: "#c8d0d0"
            font.pixelSize: 10
            font.letterSpacing: 2.5
            Layout.fillWidth: true
        }
        MenuButton {
            id: backButton
            objectName: "menuBackButton"
            visible: menu.page !== "home"
            text: qsTr("Back")
            textSize: menu.textSize
            implicitWidth: menu.textSize * 6
            onClicked: { menuAudio.select(); menu.home() }
        }
    }

    ScrollView {
        id: homeScroll
        visible: menu.page === "home"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.leftMargin: menu.margin
        anchors.rightMargin: menu.margin
        anchors.topMargin: menu.compact ? 70 : 85
        anchors.bottomMargin: 20
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ColumnLayout {
            id: homeContent
            onHeightChanged: menu.keepFocusVisible()
            width: homeScroll.availableWidth
            spacing: menu.compact ? 8 : 16
            Label {
                objectName: "menuTitle"
                text: qsTr("GLOWING\nPANCAKE")
                font.family: ink.displayFace
                font.pixelSize: Math.min(menu.compact ? 37 : 59, (menu.width - 2 * menu.margin) / 6.5)
                font.letterSpacing: 1.8
                lineHeight: 0.96
                color: ink.paper
                Accessible.name: qsTr("Project Glowing Pancake")
            }
            Item { Layout.preferredHeight: menu.compact ? 0 : 8 }
            ColumnLayout {
                spacing: 1
                Layout.preferredWidth: Math.min(340, menu.width - 2 * menu.margin)
                Layout.maximumWidth: Math.min(440, menu.width - 2 * menu.margin)
                MenuButton {
                    id: resumeButton
                    objectName: "menuResumeButton"
                    visible: menu.canResume
                    text: qsTr("Resume")
                    primary: true
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: { menuAudio.select(); menu.resumeRequested() }
                }
                Loader {
                    Layout.fillWidth: true
                    active: menu.canDelete
                    visible: active && status === Loader.Ready
                    sourceComponent: optionalMenu.item ? optionalMenu.item.deleteAction : null
                }
                MenuButton {
                    id: newGameButton
                    objectName: "menuNewGameButton"
                    number: "01"; text: qsTr("New Game"); primary: true
                    visible: campaign.canStartNew
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    enabled: campaign.ready && !campaign.busy
                    onClicked: { menuAudio.select(); menu.newGameRequested() }
                }
                MenuButton {
                    objectName: "menuLoadGameButton"
                    number: "02"; text: qsTr("Recover a campaign…")
                    visible: !campaign.currentCampaign || campaign.storageBlocked
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    enabled: campaign.ready && !campaign.busy
                    onClicked: { menuAudio.select(); menu.loadRequested() }
                }
                MenuButton {
                    objectName: "menuLeadershipButton"
                    text: qsTr("Leadership & retirement")
                    visible: campaign.currentCampaign
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: menu.leadershipRequested()
                }
                MenuButton {
                    objectName: "menuOriginalContentButton"
                    text: campaign.canRestoreBundledContent ? qsTr("Review original rules…") : qsTr("Locate original rules…")
                    visible: campaign.needsOriginalContent
                    enabled: campaign.ready && !campaign.busy
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: menu.originalContentRequested()
                }
                MenuButton {
                    text: qsTr("Retry automatic saving")
                    visible: campaign.storageBlocked && !campaign.needsOriginalContent
                    enabled: campaign.ready && !campaign.busy
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: campaign.retryPersistence()
                }
                MenuButton {
                    objectName: "menuSettingsButton"
                    number: "03"; text: qsTr("Settings")
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: { menuAudio.select(); menu.openSettings() }
                }
                MenuButton {
                    objectName: "menuCreditsButton"
                    number: "04"; text: qsTr("Credits")
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: { menuAudio.select(); menu.page = "credits"; backButton.forceActiveFocus() }
                }
                MenuButton {
                    objectName: "menuQuitButton"
                    number: "05"; text: qsTr("Quit")
                    textSize: menu.textSize * 1.3
                    Layout.fillWidth: true
                    onClicked: { menuAudio.select(); menu.quitRequested() }
                }
                Loader {
                    id: optionalMenu
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    visible: status === Loader.Ready
                    source: optionalMenuSource
                    Binding {
                        target: optionalMenu.item
                        property: "textSize"
                        value: menu.textSize
                        when: optionalMenu.status === Loader.Ready
                    }
                }
            }
            Label {
                visible: menu.message.length > 0 || campaign.notice.length > 0
                Layout.preferredWidth: Math.min(530, menu.width - 2 * menu.margin)
                wrapMode: Text.Wrap
                text: menu.message || campaign.notice
                textFormat: Text.PlainText
                font.pixelSize: menu.textSize
                color: ink.accent
            }
        }
    }

    ScrollView {
        id: detailScroll
        objectName: "menuDetailScroll"
        rightPadding: Math.max(20, menu.textSize)
        ScrollBar.vertical: ScrollBar {
            objectName: "menuDetailScrollbar"
            parent: detailScroll
            orientation: Qt.Vertical
            x: detailScroll.width - width
            y: detailScroll.topPadding
            height: detailScroll.availableHeight
            policy: ScrollBar.AlwaysOn
            active: true
            minimumSize: 0.08
            implicitWidth: Math.max(14, menu.textSize * 0.65)
            contentItem: Rectangle { radius: width / 2; color: parent.pressed ? "#f3cb8d" : "#a8c8bf" }
            background: Rectangle { radius: width / 2; color: "#31494c" }
        }
        visible: menu.page !== "home"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.topMargin: menu.compact ? 90 : 112
        anchors.leftMargin: menu.margin
        anchors.rightMargin: menu.margin
        anchors.bottomMargin: 24
        contentWidth: availableWidth
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            id: detailContent
            onHeightChanged: menu.keepFocusVisible()
            width: Math.min(900, detailScroll.availableWidth)
            spacing: 22
            Label {
                text: menu.page === "settings" ? qsTr("SETTINGS") : qsTr("CREDITS")
                font.pixelSize: 11
                font.letterSpacing: 3
                color: ink.accent
            }
            Label {
                text: menu.page === "settings" ? qsTr("Make it yours.") : qsTr("Made possible by.")
                font.family: ink.displayFace
                font.pixelSize: menu.compact ? 34 : 44
                color: ink.paper
            }
            MenuSettings {
                id: settings
                objectName: "menuSettingsPage"
                visible: menu.page === "settings"
                Layout.fillWidth: true
                textSize: menu.textSize
                videoSupported: video.supported
                onApplied: {
                    if (campaign.busy) { menu.message = qsTr("Saving settings…"); menu.settingsSubmitted() }
                    else menu.message = qsTr("Settings unchanged.")
                }
            }
            ColumnLayout {
                visible: menu.page === "credits"
                Layout.fillWidth: true
                spacing: 20
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Project Glowing Pancake\nAn independent, free, offline strategy game.")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: menu.textSize * 1.1
                    color: ink.paper
                }
                Label {
                    text: qsTr("MUSIC")
                    font.pixelSize: menu.textSize * 0.8
                    font.letterSpacing: 2
                    color: ink.accent
                }
                Repeater {
                    model: soundtrack.credits
                    ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: modelData.title + " — " + modelData.creator
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: ink.paper
                            font.pixelSize: menu.textSize
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.license_id + " · " + modelData.modifications
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: "#adb8bd"
                            font.pixelSize: menu.textSize * 0.85
                        }
                        MenuButton {
                            text: qsTr("Track source ↗")
                            textSize: menu.textSize * 0.85
                            onClicked: soundtrack.openTrackSource(index)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Original procedural menu artwork and selection sound.\nGame code: GNU GPLv3.\nBuilt with Qt, nlohmann/json, miniaudio and stb_vorbis.\nDomestic map: U.S. Census Bureau.")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: menu.textSize
                    color: "#e8e4dc"
                }
                MenuButton {
                    objectName: "menuNoticesButton"
                    text: qsTr("Licenses and notices")
                    textSize: menu.textSize
                    onClicked: notices.open()
                }
            }
        }
    }
    MusicCredits {
        id: notices
        anchors.centerIn: Overlay.overlay
        width: Math.min(740, menu.width - 32)
        height: Math.min(620, menu.height - 32)
        textSize: menu.textSize
        reducedMotion: campaign.reducedMotion
        onClosed: backButton.forceActiveFocus()
    }
    ColumnLayout {
        id: footer
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: menu.margin
        anchors.rightMargin: menu.margin
        anchors.bottomMargin: 22
        spacing: 8
        Label {
            visible: menu.page === "settings"
            Layout.fillWidth: true
            text: campaign.notice || menu.message || qsTr("Apply & save to keep your changes.")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font.pixelSize: menu.textSize
            color: ink.accent
        }
        MenuButton {
            objectName: "menuApplySettings"
            visible: menu.page === "settings"
            text: qsTr("Apply & save")
            primary: true
            textSize: menu.textSize * 1.15
            Layout.fillWidth: true
            enabled: campaign.ready && !campaign.busy
            onClicked: settings.apply()
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("A CENTURY OF POSSIBILITY")
                color: "#9daaaa"
                font.pixelSize: 9
                font.letterSpacing: 2
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Tab to navigate · Enter to select")
                visible: menu.width > 700
                font.pixelSize: 10
                color: "#9daaaa"
            }
        }
    }
    Component.onCompleted: newGameButton.forceActiveFocus()
}
