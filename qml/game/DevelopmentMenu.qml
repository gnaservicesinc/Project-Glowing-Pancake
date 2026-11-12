import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: tools
    property int textSize: 16
    property bool openAfterChoosing: false
    readonly property bool dialogOpen: studio.visible || playtest.visible || checkoutPicker.visible
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
                objectName: "developmentResetButton"
                text: qsTr("Reset local data")
                textSize: tools.textSize
                Layout.fillWidth: true
                enabled: campaign.ready && !campaign.busy
                Accessible.description: qsTr("Deletes local saves, autosaves, setup, drafts and settings for this game.")
                onClicked: { playtest.close(); campaign.resetLocalData() }
            }
        }
    }
    Connections {
        target: campaign
        function onDevelopmentResetFinished() { tools.sessionReset() }
    }
}
