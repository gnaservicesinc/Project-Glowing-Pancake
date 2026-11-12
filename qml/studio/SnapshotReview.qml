import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: review
    required property var finalizer
    anchors.centerIn: parent
    width: Math.min(1120, parent.width - 32)
    height: parent.height - 32
    modal: true
    title: qsTr("Review and finalize content revision")
    objectName: "contentRevisionReview"
    property string observedPlan: ""
    onOpened: {
        finalizer.refresh()
        source.currentIndex = source.find(finalizer.preferredSource)
        contentVersion.text = finalizer.suggestedVersion
        summary.clear()
        proposeGame.checked = false
        gameVersion.clear()
        acknowledged.checked = false
        source.forceActiveFocus()
    }
    function invalidate() { acknowledged.checked = false; finalizer.clearReview() }

    component DiffList: Frame {
        id: diffFrame
        required property var entries
        Layout.fillWidth: true
        Layout.preferredHeight: 280
        ScrollView {
            anchors.fill: parent
            ListView {
                objectName: diffFrame.objectName + "List"
                clip: true
                model: diffFrame.entries
                keyNavigationEnabled: true
                delegate: ColumnLayout {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 4
                    Label { Layout.fillWidth: true; text: modelData.path; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.bold: true }
                    Label { text: qsTr("Before"); font.bold: true }
                    TextArea { Layout.fillWidth: true; text: modelData.before; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; Accessible.name: qsTr("Before value") }
                    Label { text: qsTr("After"); font.bold: true }
                    TextArea { Layout.fillWidth: true; text: modelData.after; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; Accessible.name: qsTr("After value") }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#a7adb5" }
                }
            }
        }
    }
    contentItem: ScrollView {
        id: body
        objectName: "contentRevisionBody"
        contentWidth: availableWidth
        ColumnLayout {
            width: body.availableWidth
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: review.finalizer.currentStatus
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Choose one saved collection. Compare and validate the complete batch, then finalize its internal revision. Drafts and canvas layouts stay separate. This action does not commit, push, tag or publish.")
                wrapMode: Text.Wrap
            }
            Label { text: qsTr("Saved source collection"); font.bold: true }
            ComboBox {
                id: source
                objectName: "revisionSource"
                Layout.fillWidth: true
                model: review.finalizer.sources
                Accessible.name: qsTr("Collection to review")
                onActivated: review.invalidate()
            }
            Label { Layout.fillWidth: true; text: qsTr("Reviewed runtime target: data/runtime/first-playable.json"); wrapMode: Text.Wrap }
            Label { text: qsTr("Proposed content version"); font.bold: true }
            TextField {
                id: contentVersion
                objectName: "revisionContentVersion"
                Layout.fillWidth: true
                maximumLength: 128
                placeholderText: "0.2.0-dev.1"
                Accessible.name: qsTr("Proposed content version")
                onTextEdited: review.invalidate()
            }
            Label { Layout.fillWidth: true; text: qsTr("An additive development version is suggested. Choose a patch for a wording fix or a minor version for added behavior. The writer checks strict version ordering; no-op/layout-only batches keep the current content version."); wrapMode: Text.Wrap }
            Label { text: qsTr("Change summary"); font.bold: true }
            TextArea {
                id: summary
                objectName: "revisionSummary"
                Layout.fillWidth: true
                Layout.preferredHeight: 90
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                Accessible.name: qsTr("Reviewed batch change summary")
                onTextChanged: review.invalidate()
            }
            CheckBox {
                id: proposeGame
                objectName: "revisionProposeGame"
                text: qsTr("Also propose a new game version")
                onToggled: review.invalidate()
            }
            TextField {
                id: gameVersion
                objectName: "revisionGameVersion"
                visible: proposeGame.checked
                Layout.fillWidth: true
                maximumLength: 128
                placeholderText: qsTr("New game version satisfying all pending content requirements")
                Accessible.name: qsTr("Proposed game version")
                onTextEdited: review.invalidate()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Leaving the game version unchanged records an unresolved minimum version for later release preparation. The editor version and project phase stay as they are.")
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: review.finalizer.blocker
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            Button {
                objectName: "recoverContentRevision"
                visible: review.finalizer.pendingRecovery
                text: qsTr("Recover interrupted content batch")
                onClicked: review.finalizer.recover()
            }
            Label {
                objectName: "contentRevisionMessage"
                Layout.fillWidth: true
                visible: text.length > 0
                text: review.finalizer.message
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                color: "#9c3428"
            }
            Label {
                objectName: "contentRevisionOutcome"
                Layout.fillWidth: true
                visible: text.length > 0
                text: review.finalizer.outcome
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                font.bold: true
            }
            ColumnLayout {
                visible: review.finalizer.ready
                Layout.fillWidth: true
                spacing: 12
                Label { Layout.fillWidth: true; text: review.finalizer.versionSummary; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.bold: true }
                Label { Layout.fillWidth: true; text: review.finalizer.validationSummary; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                Label { Layout.fillWidth: true; text: review.finalizer.releaseSummary; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                Label { text: qsTr("Changed definitions and revisions"); font.bold: true }
                ComboBox {
                    id: definitions
                    objectName: "revisionDefinitions"
                    Layout.fillWidth: true
                    model: review.finalizer.changes
                    textRole: "id"
                    Accessible.name: qsTr("Changed definition")
                }
                Label {
                    Layout.fillWidth: true
                    readonly property var entry: review.finalizer.changes[definitions.currentIndex] || {}
                    text: entry.id ? entry.kind + " · " + entry.change + qsTr(" · revision %1 → %2").arg(entry.oldRevision).arg(entry.revision) : qsTr("No changed definitions")
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                }
                DiffList { objectName: "revisionDefinitionDiff"; visible: definitions.currentIndex >= 0; entries: (review.finalizer.changes[definitions.currentIndex] || {}).fields || [] }
                Label { visible: review.finalizer.textChanges.length > 0; text: qsTr("Changed visible text"); font.bold: true }
                DiffList { objectName: "revisionTextDiff"; visible: review.finalizer.textChanges.length > 0; entries: review.finalizer.textChanges }
                Label { visible: review.finalizer.collectionChanges.length > 0; text: qsTr("Changed collection variables"); font.bold: true }
                DiffList { visible: review.finalizer.collectionChanges.length > 0; entries: review.finalizer.collectionChanges }
                Label { text: qsTr("Files to update"); font.bold: true }
                DiffList { objectName: "revisionFileDiff"; entries: review.finalizer.writes }
                Label { visible: review.finalizer.notices.length > 0; text: qsTr("Compiler notices"); font.bold: true }
                TextArea { visible: review.finalizer.notices.length > 0; Layout.fillWidth: true; text: review.finalizer.notices.join("\n\n"); textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere }
            }
        }
    }
    footer: ColumnLayout {
        spacing: 6
        CheckBox {
            id: acknowledged
            objectName: "acknowledgeContentRevision"
            Layout.fillWidth: true
            enabled: review.finalizer.ready
            text: qsTr("I reviewed the proposed differences, versions and save policy")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button { objectName: "closeContentRevision"; text: qsTr("Close review"); onClicked: review.close() }
            Button {
                objectName: "reviewSavedContent"
                text: qsTr("Review saved batch")
                enabled: source.currentIndex >= 0 && !review.finalizer.pendingRecovery
                onClicked: review.finalizer.review(source.currentText, contentVersion.text, summary.text, proposeGame.checked ? gameVersion.text : "")
            }
            Button {
                objectName: "finalizeReviewedContent"
                text: qsTr("Finalize reviewed batch")
                enabled: acknowledged.checked && review.finalizer.canFinalize
                onClicked: review.finalizer.finalizeReview()
            }
        }
    }
    Connections {
        target: review.finalizer
        function onChanged() {
            if (review.observedPlan !== review.finalizer.planId) {
                acknowledged.checked = false
                review.observedPlan = review.finalizer.planId
            }
        }
    }
}
