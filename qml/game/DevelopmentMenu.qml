import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: tools
    property int textSize: 16
    property bool openAfterChoosing: false
    readonly property bool dialogOpen: studio.visible || playtest.visible || founderEditor.visible || checkoutPicker.visible
    property bool savingFounder: false
    signal sessionReset()
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        MenuButton {
            objectName: "contentStudioButton"
            Layout.fillWidth: true
            textSize: tools.textSize
            text: qsTr("Content Studio")
            onClicked: {
                if (developmentTools.checkout.length && developmentTools.openContentStudio()) return
                studio.open()
                if (!developmentTools.checkout.length) {
                    tools.openAfterChoosing = true
                    checkoutPicker.open()
                }
            }
        }
        WorldButton {
            objectName: "contentStudioSettingsButton"
            text: "⋯"
            textSize: tools.textSize
            quiet: true
            Accessible.name: qsTr("Content Studio repository settings")
            onClicked: studio.open()
        }
    }
    MenuButton {
        objectName: "playtestEnvironmentButton"
        Layout.fillWidth: true
        textSize: tools.textSize
        text: qsTr("Playtest Environment")
        onClicked: playtest.open()
    }

    FolderDialog {
        id: checkoutPicker
        title: qsTr("Choose the Content Studio repository")
        onAccepted: {
            if (developmentTools.selectCheckout(selectedFolder) && tools.openAfterChoosing && developmentTools.openContentStudio()) studio.close()
            tools.openAfterChoosing = false
        }
        onRejected: tools.openAfterChoosing = false
    }
    GameDialog {
        id: studio
        title: qsTr("Content Studio")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(600, (Overlay.overlay ? Overlay.overlay.width : 600) - 32)
        font.pixelSize: tools.textSize
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: developmentTools.checkout || qsTr("Choose your repository folder.")
                font.pixelSize: Math.max(14, tools.textSize * 0.78)
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }
            MenuButton {
                objectName: "openContentStudioButton"
                Layout.fillWidth: true
                textSize: tools.textSize
                text: qsTr("Open Studio")
                enabled: developmentTools.checkout.length > 0
                onClicked: if (developmentTools.openContentStudio()) studio.close()
            }
            MenuButton {
                objectName: "contentStudioFolderButton"
                Layout.fillWidth: true
                textSize: tools.textSize
                text: qsTr("Choose repository…")
                onClicked: checkoutPicker.open()
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: text.length > 0
                text: developmentTools.message
                font.pixelSize: tools.textSize
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: "#ebc887"
            }
        }
    }
    GameDialog {
        id: playtest
        title: qsTr("Playtest Environment")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(480, (Overlay.overlay ? Overlay.overlay.width : 480) - 32)
        font.pixelSize: tools.textSize
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            MenuButton {
                objectName: "openGodModeButton"
                visible: !godMode
                text: qsTr("God mode")
                textSize: tools.textSize
                Layout.fillWidth: true
                onClicked: {
                    if (developmentTools.openGodMode()) playtest.close()
                    else playtestMessage.text = qsTr("Couldn't open God mode.")
                }
            }
            MenuButton {
                objectName: "playtestFounderButton"
                visible: godMode
                text: qsTr("Edit founder traits…")
                textSize: tools.textSize
                Layout.fillWidth: true
                enabled: campaign.currentCampaign && campaign.companyEditable && !campaign.busy
                onClicked: {
                    const founder = campaign.playtestFounder()
                    if (!founder.traits) {
                        playtestMessage.text = qsTr("This campaign has no founder traits to edit.")
                        return
                    }
                    traits.text = founder.traits.join("\n")
                    genius.checked = founder.genius
                    founderError.text = ""
                    playtest.close()
                    founderEditor.open()
                }
            }
            MenuButton {
                objectName: "developmentResetButton"
                text: qsTr("Reset local data")
                textSize: tools.textSize
                Layout.fillWidth: true
                enabled: campaign.ready && !campaign.busy
                Accessible.description: qsTr("Deletes local saves, autosaves, setup, drafts and settings for this game.")
                onClicked: { playtest.close(); campaign.resetLocalData() }
            }
            Label {
                id: playtestMessage
                visible: text.length > 0
                text: ""
                font.pixelSize: tools.textSize
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
    }
    GameDialog {
        id: founderEditor
        objectName: "playtestFounderEditor"
        title: qsTr("Founder traits")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, (Overlay.overlay ? Overlay.overlay.width : 620) - 32)
        height: Math.min(implicitHeight, (Overlay.overlay ? Overlay.overlay.height : 600) - 24)
        font.pixelSize: tools.textSize
        contentItem: ColumnLayout {
            Label {
                text: qsTr("One trait per line")
                font.pixelSize: tools.textSize
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 48
                Layout.preferredHeight: Math.min(210, (Overlay.overlay ? Overlay.overlay.height : 600) * 0.31)
                contentWidth: availableWidth
                clip: true
                TextArea {
                    id: traits
                    objectName: "playtestFounderTraits"
                    font.pixelSize: tools.textSize
                    Accessible.name: qsTr("Founder traits, one per line")
                    wrapMode: TextEdit.Wrap
                }
            }
            GameCheck {
                id: genius
                objectName: "playtestFounderGenius"
                text: qsTr("Actual Genius")
                font.pixelSize: tools.textSize
            }
            Label {
                id: founderError
                visible: text.length > 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                font.pixelSize: Math.max(14, tools.textSize * 0.78)
                color: "#ebc887"
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
        footer: DialogButtonBox {
            WorldButton {
                objectName: "savePlaytestFounder"
                text: qsTr("Apply")
                textSize: tools.textSize
                enabled: !campaign.busy
                onClicked: {
                    tools.savingFounder = true
                    campaign.editPlaytestFounder(traits.text.split("\n").map(line => line.trim()).filter(line => line.length), genius.checked)
                    if (!campaign.busy) tools.savingFounder = false
                }
            }
            WorldButton {
                text: qsTr("Cancel")
                textSize: tools.textSize
                enabled: !campaign.busy
                onClicked: founderEditor.close()
            }
        }
        onOpened: traits.forceActiveFocus()
    }
    Connections {
        target: campaign
        function onDevelopmentResetFinished() { tools.sessionReset() }
        function onCommandFinished(accepted, explanation) {
            if (!tools.savingFounder) return
            tools.savingFounder = false
            if (accepted) founderEditor.close()
            else founderError.text = explanation
        }
    }
}
