import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    id: tools
    property int textSize: 16
    property bool openAfterChoosing: false
    readonly property bool dialogOpen: studio.visible || playtest.visible || playtestSetup.visible || founderEditor.visible || personEditor.visible || checkoutPicker.visible || deleteCampaignDialog.visible
    signal playtestStarted()
    Component.onCompleted: if (setupRequested) Qt.callLater(playtestSetup.open)
    property bool savingFounder: false
    property bool savingPerson: false
    property bool usingTool: false
    readonly property Component deleteAction: Component {
        MenuButton {
            objectName: "deleteCurrentGameButton"
            text: qsTr("Delete current game…")
            textSize: tools.textSize
            enabled: campaign.ready && !campaign.busy
            onClicked: deleteCampaignDialog.open()
        }
    }
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

    GameDialog {
        id: deleteCampaignDialog
        objectName: "deleteCampaignDialog"
        anchors.centerIn: Overlay.overlay
        width: Math.min(520, (Overlay.overlay ? Overlay.overlay.width : 520) - 32)
        font.pixelSize: tools.textSize
        title: qsTr("Delete current game?")
        modal: true
        focus: true
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTr("Delete the current campaign and any unfinished founder setup? Manual saves and settings will still be available.")
                font.pixelSize: tools.textSize
                wrapMode: Text.Wrap
            }
            WorldButton {
                textSize: tools.textSize
                objectName: "confirmDeleteCurrentGame"
                text: qsTr("Delete current game")
                onClicked: {
                    deleteCampaignDialog.close()
                    campaign.deleteCurrentGame()
                }
            }
        }
        onOpened: standardButton(Dialog.Cancel).forceActiveFocus()
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
        height: Math.min(implicitHeight, (Overlay.overlay ? Overlay.overlay.height : 600) - 24)
        font.pixelSize: tools.textSize
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                MenuButton {
                    objectName: "openPlaytestSetupButton"
                    text: qsTr("Custom playtest setup & start…")
                    textSize: tools.textSize
                    Layout.fillWidth: true
                    enabled: campaign.ready && !campaign.busy
                    onClicked: {
                        playtest.close()
                        if (godMode) playtestSetup.open()
                        else if (!developmentTools.openGodMode(true)) playtest.open()
                    }
                }
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
                    objectName: "playtestPersonButton"
                    visible: godMode
                    text: qsTr("Edit a person…")
                    textSize: tools.textSize
                    Layout.fillWidth: true
                    enabled: campaign.currentCampaign && campaign.companyEditable && !campaign.busy
                    onClicked: {
                        personPicker.model = campaign.playtestPeople()
                        if (!personPicker.count) return
                        personPicker.currentIndex = 0
                        personEditor.load()
                        playtest.close()
                        personEditor.open()
                    }
                }
                Repeater {
                    model: godMode ? [{id:"resources",label:qsTr("Enable unlimited resources")}, {id:"delivery",label:qsTr("Deliver outstanding orders")}]:[]
                    MenuButton {
                        required property var modelData
                        objectName: "playtestTool_" + modelData.id
                        text: modelData.label
                        textSize: tools.textSize
                        Layout.fillWidth: true
                        enabled: campaign.currentCampaign && campaign.companyEditable && !campaign.busy
                        onClicked: {
                            tools.usingTool = true
                            campaign.usePlaytestTool(modelData.id, "")
                            if (!campaign.busy) tools.usingTool = false
                        }
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
    }
    PlaytestSetupPage {
        id: playtestSetup
        onStarted: tools.playtestStarted()
        textSize: tools.textSize
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
    GameDialog {
        id: personEditor
        objectName: "playtestPersonEditor"
        title: qsTr("Edit a person")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(570, (Overlay.overlay ? Overlay.overlay.width : 570) - 32)
        height: Math.min(580, (Overlay.overlay ? Overlay.overlay.height : 580) - 24)
        font.pixelSize: tools.textSize
        function load() {
            const row = personPicker.currentIndex < 0 ? null : personPicker.model[personPicker.currentIndex]
            if (!row) return
            personTraits.text = row.traits.join("\n")
            experience.text = String(row.experience)
            research.text = String(row.research)
            engineering.text = String(row.engineering)
            evaluation.text = String(row.evaluation)
            missionFit.text = String(row.missionFit)
            personError.text = ""
        }
        component StatField: TextField {
            required property string label
            property int maximum: 100
            Layout.fillWidth: true
            font.pixelSize: tools.textSize
            Accessible.name: label
            validator: IntValidator { bottom: 0; top: maximum }
        }
        contentItem: ScrollView {
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                ComboBox {
                    id: personPicker
                    objectName: "playtestPersonSelector"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    textRole: "label"
                    valueRole: "id"
                    font.pixelSize: tools.textSize
                    Accessible.name: qsTr("Person")
                    onActivated: personEditor.load()
                }
                Label { text: qsTr("Traits, one per line"); font.pixelSize: tools.textSize }
                TextArea {
                    id: personTraits
                    objectName: "playtestPersonTraits"
                    Layout.fillWidth: true
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: tools.textSize
                    Accessible.name: qsTr("Traits, one per line")
                }
                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    Label { text: experience.label; font.pixelSize: tools.textSize }
                    StatField { id: experience; label: qsTr("Experience"); maximum: personPicker.currentIndex<0?0:personPicker.model[personPicker.currentIndex].maxExperience }
                    Label { text: research.label; font.pixelSize: tools.textSize }
                    StatField { id: research; label: qsTr("Research") }
                    Label { text: engineering.label; font.pixelSize: tools.textSize }
                    StatField { id: engineering; label: qsTr("Engineering") }
                    Label { text: evaluation.label; font.pixelSize: tools.textSize }
                    StatField { id: evaluation; objectName: "playtestPersonEvaluation"; label: qsTr("Evaluation") }
                    Label { text: missionFit.label; font.pixelSize: tools.textSize }
                    StatField { id: missionFit; label: qsTr("Mission fit") }
                }
                Label {
                    id: personError
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    font.pixelSize: tools.textSize
                    visible: text.length > 0
                }
            }
        }
        footer: DialogButtonBox {
            WorldButton {
                objectName: "savePlaytestPerson"
                text: qsTr("Apply")
                textSize: tools.textSize
                enabled: !campaign.busy && personPicker.currentIndex >= 0 && experience.acceptableInput && research.acceptableInput
                         && engineering.acceptableInput && evaluation.acceptableInput && missionFit.acceptableInput
                onClicked: {
                    tools.savingPerson = true
                    campaign.editPlaytestPerson(personPicker.currentValue, personTraits.text.split("\n").map(line => line.trim()).filter(line => line.length),
                                                Number(experience.text), Number(research.text), Number(engineering.text), Number(evaluation.text), Number(missionFit.text))
                    if (!campaign.busy) tools.savingPerson = false
                }
            }
            WorldButton { text: qsTr("Cancel"); textSize: tools.textSize; onClicked: personEditor.close() }
        }
        onOpened: personPicker.forceActiveFocus()
    }
    Connections {
        target: campaign
        function onCommandFinished(accepted, explanation) {
            if (tools.usingTool) {
                tools.usingTool = false
                playtestMessage.text = explanation
            }
            if (tools.savingPerson) {
                tools.savingPerson = false
                if (accepted) personEditor.close()
                else personError.text = explanation
            }
            if (!tools.savingFounder) return
            tools.savingFounder = false
            if (accepted) founderEditor.close()
            else founderError.text = explanation
        }
    }
}
