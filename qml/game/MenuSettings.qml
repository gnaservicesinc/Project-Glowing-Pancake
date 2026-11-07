import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    property int textSize: 16
    property bool videoSupported: false
    property bool draftFullScreen: true
    property bool draftMusic: true
    property bool draftReducedMotion: false
    property bool draftMotionBackground: true
    property int draftMusicVolume: 40
    property int draftEffectsVolume: 60
    property int draftTextScale: 100
    property int draftWidth: 1280
    property int draftHeight: 720
    signal applied()
    spacing: 24

    function reset() {
        draftFullScreen = campaign.fullScreen
        draftWidth = campaign.windowWidth
        draftHeight = campaign.windowHeight
        draftMusic = campaign.musicEnabled
        draftMusicVolume = campaign.musicVolume
        draftEffectsVolume = campaign.effectsVolume
        draftTextScale = campaign.textScalePercent
        draftReducedMotion = campaign.reducedMotion
        draftMotionBackground = campaign.motionBackground
    }
    component Copy: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: "#e8e4dc"
        font.pixelSize: page.textSize
    }
    component Section: Label {
        color: "#f3cb8d"
        font.pixelSize: page.textSize * 0.8
        font.letterSpacing: 2
        font.weight: Font.DemiBold
        topPadding: 8
    }
    component Choice: ComboBox {
        Layout.fillWidth: true
        font.pixelSize: page.textSize
        implicitHeight: Math.max(44, page.textSize * 2.4)
    }
    Section { text: qsTr("DISPLAY") }
    GridLayout {
        Layout.fillWidth: true
        columns: width < 660 ? 1 : 2
        columnSpacing: 36
        rowSpacing: 12
        Copy { text: qsTr("Display mode") }
        Choice {
            objectName: "menuDisplayMode"
            Accessible.name: qsTr("Display mode")
            model: [qsTr("Full screen"), qsTr("Windowed")]
            currentIndex: page.draftFullScreen ? 0 : 1
            onActivated: page.draftFullScreen = currentIndex === 0
        }
        Copy { text: qsTr("Window resolution") }
        Choice {
            objectName: "menuResolution"
            Accessible.name: qsTr("Window resolution")
            model: [{label:"1024 × 640", w:1024, h:640}, {label:"1280 × 720", w:1280, h:720},
                {label:"1440 × 900", w:1440, h:900}, {label:"1600 × 900", w:1600, h:900},
                {label:"1920 × 1080", w:1920, h:1080}, {label:"2560 × 1440", w:2560, h:1440}]
            textRole: "label"
            enabled: !page.draftFullScreen
            currentIndex: model.findIndex(row => row.w === page.draftWidth && row.h === page.draftHeight)
            onActivated: {
                const choice = model[currentIndex]
                if (choice) { page.draftWidth = choice.w; page.draftHeight = choice.h }
            }
        }
        Copy {
            Layout.columnSpan: parent.columns
            color: "#adb8bd"
            text: qsTr("Full screen uses your display’s native resolution. Window sizes fit within the available desktop.")
        }
        Copy { text: qsTr("Menu background") }
        Choice {
            objectName: "menuBackgroundChoice"
            Accessible.name: qsTr("Menu background")
            model: page.videoSupported ? [qsTr("Still image"), qsTr("Motion loop")] : [qsTr("Still image")]
            currentIndex: page.videoSupported && page.draftMotionBackground ? 1 : 0
            onActivated: page.draftMotionBackground = currentIndex === 1
        }
    }
    Section { text: qsTr("AUDIO") }
    CheckBox {
        objectName: "menuMusicEnabled"
        text: qsTr("Play music")
        font.pixelSize: page.textSize
        checked: page.draftMusic
        onToggled: page.draftMusic = checked
    }
    Copy { text: qsTr("Music · %1%").arg(Math.round(musicVolume.value)) }
    Slider {
        id: musicVolume
        objectName: "menuMusicVolume"
        Layout.fillWidth: true
        from: 0; to: 100; stepSize: 5
        value: page.draftMusicVolume
        Accessible.name: qsTr("Music volume")
        onMoved: page.draftMusicVolume = Math.round(value)
    }
    Copy { text: qsTr("Sound effects · %1%").arg(Math.round(effectsVolume.value)) }
    Slider {
        id: effectsVolume
        objectName: "menuEffectsVolume"
        Layout.fillWidth: true
        from: 0; to: 100; stepSize: 5
        value: page.draftEffectsVolume
        Accessible.name: qsTr("Sound effects volume")
        onMoved: page.draftEffectsVolume = Math.round(value)
    }
    Section { text: qsTr("ACCESSIBILITY") }
    Copy { text: qsTr("Text size") }
    Choice {
        objectName: "menuTextScale"
        model: [100, 125, 150, 175, 200]
        currentIndex: model.indexOf(page.draftTextScale)
        displayText: qsTr("%1%").arg(page.draftTextScale)
        Accessible.name: qsTr("Text size")
        onActivated: page.draftTextScale = model[currentIndex]
    }
    CheckBox {
        objectName: "menuReducedMotion"
        text: qsTr("Reduce motion")
        checked: page.draftReducedMotion
        font.pixelSize: page.textSize
        onToggled: page.draftReducedMotion = checked
    }
    Copy { color: "#adb8bd"; text: qsTr("Reduced motion keeps the background still. All preferences are saved on this computer.") }
    MenuButton {
        objectName: "menuApplySettings"
        text: qsTr("Apply settings")
        textSize: page.textSize * 1.15
        Layout.fillWidth: true
        enabled: campaign.ready && !campaign.busy
        onClicked: {
            campaign.clearNotice()
            campaign.applyPresentationSettings(page.draftFullScreen, page.draftWidth, page.draftHeight,
                page.draftMusic, page.draftMusicVolume, page.draftEffectsVolume, page.draftTextScale,
                page.draftReducedMotion, page.draftMotionBackground)
            page.applied()
        }
    }
}
