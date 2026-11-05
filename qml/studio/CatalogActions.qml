import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: actions
    required property QtObject author
    property bool ready: true
    property string messageKey: ""
    readonly property bool selected: author.selectedIdentity.length > 0

    RowLayout {
        visible: actions.author.catalogTemplates.length > 0
        Layout.fillWidth: true
        ComboBox {
            id: templates
            Layout.fillWidth: true
            model: actions.author.catalogTemplates
            textRole: "label"
            valueRole: "value"
            Accessible.name: qsTr("Typed template from the collection")
        }
        Button {
            text: qsTr("Use template")
            enabled: actions.ready && templates.currentIndex >= 0
            onClicked: actions.author.beginCatalogTemplate(templates.currentValue)
        }
    }
    Label {
        visible: actions.selected
        Layout.fillWidth: true
        text: actions.author.selectedFamily + " · " + actions.author.selectedIdentity
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        font.bold: true
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Button {
            text: qsTr("Open flow")
            visible: actions.author.selectedFamily === "flow"
            enabled: actions.ready
            onClicked: actions.author.openSelectedFlow()
        }
        Button {
            objectName: "catalogNewMessage"
            text: qsTr("New message")
            enabled: actions.ready
            onClicked: { actions.messageKey = ""; translations.open() }
        }
        Button {
            objectName: "catalogTranslations"
            visible: actions.author.selectedFamily === "text"
            text: qsTr("Edit localized text")
            enabled: actions.ready
            onClicked: { actions.messageKey = actions.author.selectedIdentity; translations.open() }
        }
        Button {
            visible: actions.author.canDeprecate
            text: actions.author.selectedDeprecated ? qsTr("Mark current") : qsTr("Deprecate")
            enabled: actions.ready
            onClicked: actions.author.deprecateIdentity(!actions.author.selectedDeprecated)
        }
        Button {
            visible: actions.selected
            text: qsTr("Remove unused record")
            enabled: actions.ready && actions.author.referenceUses.length === 0
            onClicked: removeReview.open()
        }
    }
    RowLayout {
        visible: actions.selected
        Layout.fillWidth: true
        TextField {
            id: newId
            objectName: "catalogNewId"
            Layout.fillWidth: true
            maximumLength: 128
            placeholderText: qsTr("New stable ID")
            Accessible.name: qsTr("New stable catalog ID")
        }
        Button {
            objectName: "catalogReviewRename"
            text: qsTr("Review rename")
            enabled: actions.ready && newId.text.length > 0
            onClicked: { if (actions.author.reviewRename(newId.text.trim())) renameReview.open() }
        }
        Button {
            visible: actions.author.selectedFamily !== "text"
            text: qsTr("Duplicate")
            enabled: actions.ready && newId.text.length > 0
            onClicked: { if (actions.author.cloneCatalogEntry(newId.text.trim())) newId.clear() }
        }
    }
    Label {
        visible: actions.selected
        Layout.fillWidth: true
        text: qsTr("Impact across the collection · %1 uses").arg(actions.author.referenceUses.length)
        wrapMode: Text.Wrap
        font.bold: true
    }
    ScrollView {
        visible: actions.author.referenceUses.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: 155
        ListView {
            model: actions.author.referenceUses
            clip: true
            Accessible.name: qsTr("Typed reference impact")
            keyNavigationEnabled: true
            Keys.onReturnPressed: {
                const row = model[currentIndex]
                if (row && actions.ready) actions.author.followReference(row.owner,row.path)
            }
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.owner + "\n" + modelData.path
                contentItem: Label { text: parent.text; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText }
                enabled: actions.ready
                onClicked: actions.author.followReference(modelData.owner,modelData.path)
            }
        }
    }
    Dialog {
        id: removeReview
        anchors.centerIn: Overlay.overlay
        width: Math.min(Overlay.overlay.width - 24, 440)
        implicitWidth: 440
        modal: true
        title: qsTr("Remove unused record?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: actions.author.removeIdentity()
        contentItem: Label { text: actions.author.selectedIdentity; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere }
    }
    Dialog {
        id: renameReview
        objectName: "catalogRenameDialog"
        anchors.centerIn: Overlay.overlay
        width: Math.min(Overlay.overlay.width - 24, 780)
        height: Math.min(Overlay.overlay.height - 24, 490)
        modal: true
        title: qsTr("Review typed reference changes")
        standardButtons: Dialog.Cancel
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                text: qsTr("Every listed declaration and typed use will change together. This creates one undo step.")
                wrapMode: Text.Wrap
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                ListView {
                    clip: true
                    model: actions.author.renameChanges
                    Accessible.name: qsTr("Reviewed reference changes")
                    delegate: Label {
                        required property var modelData
                        width: ListView.view.width
                        padding: 8
                        text: modelData.owner + "\n" + modelData.path + "\n" + modelData.before + " → " + modelData.after
                        wrapMode: Text.WrapAnywhere
                        textFormat: Text.PlainText
                    }
                }
            }
            Button {
                objectName: "catalogApplyRename"
                text: qsTr("Apply reviewed rename")
                enabled: actions.author.renameChanges.length > 0
                onClicked: { if (actions.author.applyRename()) { newId.clear(); renameReview.close() } }
            }
        }
    }
    Dialog {
        id: translations
        objectName: "catalogTranslationDialog"
        anchors.centerIn: Overlay.overlay
        width: Math.min(Overlay.overlay.width - 24, 720)
        height: Math.min(Overlay.overlay.height - 24, 500)
        modal: true
        title: qsTr("Localized message")
        standardButtons: Dialog.Cancel
        onOpened: {
            key.text = actions.messageKey
            locale.text = "en"
            translated.text = actions.author.messageValue(key.text,locale.text)
        }
        contentItem: ColumnLayout {
            TextField {
                id: key
                objectName: "catalogMessageKey"
                Layout.fillWidth: true
                maximumLength: 128
                placeholderText: qsTr("Stable message key")
                Accessible.name: qsTr("Stable message key")
                onEditingFinished: translated.text = actions.author.messageValue(text,locale.text)
            }
            TextField {
                id: locale
                objectName: "catalogLocale"
                Layout.fillWidth: true
                maximumLength: 5
                Accessible.name: qsTr("Message locale")
                onEditingFinished: translated.text = actions.author.messageValue(key.text,text)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Use en for the default, or a locale such as fr or fr-CA. An empty translation restores fallback.")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: translated
                    objectName: "catalogMessageText"
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    Accessible.name: qsTr("Localized text")
                }
            }
            Label {
                Layout.fillWidth: true
                visible: locale.text !== "en"
                text: qsTr("Current resolved text") + "\n" + actions.author.messageValue(key.text,locale.text,true)
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
                maximumLineCount: 5
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: actions.author.message
                wrapMode: Text.WrapAnywhere
                textFormat: Text.PlainText
            }
            Button {
                objectName: "catalogSaveMessage"
                text: qsTr("Apply message")
                enabled: key.text.length > 0 && locale.text.length > 0
                onClicked: { if (actions.author.saveMessage(key.text.trim(),locale.text.trim(),translated.text)) translations.close() }
            }
        }
    }
}
