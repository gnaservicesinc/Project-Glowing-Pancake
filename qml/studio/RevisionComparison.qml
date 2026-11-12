import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: dialog
    required property var reviewer
    required property string checkout
    anchors.centerIn: parent
    width: Math.min(1160, parent.width - 32)
    height: parent.height - 32
    modal: true
    title: qsTr("Compare finalized content and saves")
    objectName: "finalizedRevisionComparison"
    standardButtons: Dialog.Close
    property int page: 0
    readonly property string fixtureConfiguration: [beforeScenario.currentText, afterScenario.currentText, headquarters.currentText, seed.text].join("|")
    property string preparedConfiguration: ""
    onOpened: {
        reviewer.refresh(checkout)
        older.currentIndex = older.count - 2
        newer.currentIndex = newer.count - 1
        tabs.currentIndex = 0
        preparedConfiguration = ""
        sourceSave.clear()
        migrationSummary.clear()
        exportName.clear()
        older.forceActiveFocus()
    }

    function selectPage(index) {
        page = index
        reviewer.filter(section.currentIndex > 0 ? section.currentText : "", page)
        diffList.positionViewAtBeginning()
    }

    component Readable: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
    }
    component Readout: ScrollView {
        id: report
        required property string detail
        required property string caption
        Layout.fillWidth: true
        Layout.preferredHeight: 300
        contentWidth: availableWidth
        clip: true
        TextArea {
            text: report.detail
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            Accessible.name: report.caption
        }
    }

    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: older
                objectName: "compareOlderGeneration"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: dialog.reviewer.revisions
                Accessible.name: qsTr("Earlier finalized generation")
            }
            ComboBox {
                id: newer
                objectName: "compareNewerGeneration"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: dialog.reviewer.revisions
                Accessible.name: qsTr("Later finalized generation")
            }
            Button {
                objectName: "compareSelectedGenerations"
                text: qsTr("Compare")
                enabled: older.currentIndex >= 0 && newer.currentIndex > older.currentIndex
                onClicked: {
                    if (dialog.reviewer.compare(older.currentIndex, newer.currentIndex)) {
                        section.currentIndex = 0
                        dialog.selectPage(0)
                        const matching = afterScenario.find(beforeScenario.currentText)
                        if (matching >= 0) afterScenario.currentIndex = matching
                        dialog.preparedConfiguration = ""
                        acknowledged.checked = false
                    }
                }
            }
        }
        Readable {
            objectName: "revisionComparisonMessage"
            visible: text.length > 0
            text: dialog.reviewer.message
        }
        TabBar {
            id: tabs
            objectName: "revisionComparisonTabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("Changes") }
            TabButton { text: qsTr("Matched fixtures") }
            TabButton { text: qsTr("Save migration") }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            ColumnLayout {
                Readout {
                    visible: dialog.reviewer.comparison.length > 0
                    caption: qsTr("Compared content versions")
                    detail: dialog.reviewer.comparison
                    Layout.preferredHeight: Math.min(135, dialog.height / 4)
                }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: section
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        model: [qsTr("All changed fields")].concat(dialog.reviewer.sections)
                        Accessible.name: qsTr("Changed definition or collection section")
                        onActivated: dialog.selectPage(0)
                    }
                    Button { text: qsTr("Previous"); enabled: dialog.page > 0; onClicked: dialog.selectPage(dialog.page - 1) }
                    Label { text: dialog.reviewer.pageCount > 0 ? qsTr("%1 / %2").arg(dialog.page + 1).arg(dialog.reviewer.pageCount) : qsTr("0 changes") }
                    Button { text: qsTr("Next"); enabled: dialog.page + 1 < dialog.reviewer.pageCount; onClicked: dialog.selectPage(dialog.page + 1) }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ListView {
                        id: diffList
                        objectName: "finalizedFieldDiff"
                        clip: true
                        keyNavigationEnabled: true
                        model: dialog.reviewer.differences
                        Accessible.name: qsTr("Changed fields in the selected content revisions")
                        delegate: ColumnLayout {
                            required property var modelData
                            width: ListView.view.width
                            Readable { text: modelData.path; font.bold: true }
                            Readable { text: qsTr("Before · %1").arg(modelData.before) }
                            Readable { text: qsTr("After · %1").arg(modelData.after) }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#a7adb5" }
                        }
                    }
                }
            }
            ScrollView {
                id: fixtures
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: fixtures.availableWidth
                    spacing: 8
                    Readable { text: qsTr("Use the same headquarters, seed and actions in both authored scenarios. Each report shows the latest real runtime step, including trigger scope, path, effects, postings and waits.") }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        Label { text: qsTr("Earlier scenario") }
                        ComboBox {
                            id: beforeScenario
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: dialog.reviewer.beforeScenarios
                            Accessible.name: qsTr("Earlier scenario")
                        }
                        Label { text: qsTr("Later scenario") }
                        ComboBox {
                            id: afterScenario
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            model: dialog.reviewer.afterScenarios
                            Accessible.name: qsTr("Later scenario")
                        }
                        Label { text: qsTr("Headquarters") }
                        ComboBox {
                            id: headquarters
                            Layout.fillWidth: true
                            model: dialog.reviewer.headquarters(beforeScenario.currentText, afterScenario.currentText)
                            Accessible.name: qsTr("Shared headquarters")
                        }
                        Label { text: qsTr("Shared seed") }
                        TextField {
                            id: seed
                            Layout.fillWidth: true
                            text: "20140101"
                            maximumLength: 20
                            inputMethodHints: Qt.ImhDigitsOnly
                            Accessible.name: qsTr("Shared unsigned seed")
                        }
                    }
                    RowLayout {
                        Button {
                            objectName: "startMatchedFixtures"
                            text: qsTr("Prepare both")
                            enabled: headquarters.currentIndex >= 0 && seed.text.length > 0
                            onClicked: {
                                if (dialog.reviewer.startFixture(beforeScenario.currentText, afterScenario.currentText, headquarters.currentText, seed.text))
                                    dialog.preparedConfiguration = dialog.fixtureConfiguration
                            }
                        }
                        Button {
                            objectName: "advanceMatchedFixtures"
                            text: qsTr("Advance one day")
                            enabled: dialog.reviewer.fixtureReady && dialog.preparedConfiguration === dialog.fixtureConfiguration
                            onClicked: dialog.reviewer.advanceFixture()
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: dialog.width >= 960 ? 2 : 1
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Readable { text: qsTr("Earlier fixture"); font.bold: true }
                            ComboBox {
                                id: beforeChoice
                                visible: count > 0
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: dialog.reviewer.beforeChoices
                                textRole: "label"
                                Accessible.name: qsTr("Earlier pending response")
                            }
                            Readout { caption: qsTr("Earlier fixture runtime trace"); detail: dialog.reviewer.beforeTrace }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Readable { text: qsTr("Later fixture"); font.bold: true }
                            ComboBox {
                                id: afterChoice
                                visible: count > 0
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: dialog.reviewer.afterChoices
                                textRole: "label"
                                Accessible.name: qsTr("Later pending response")
                            }
                            Readout { caption: qsTr("Later fixture runtime trace"); detail: dialog.reviewer.afterTrace }
                        }
                    }
                    Button {
                        visible: beforeChoice.count > 0 || afterChoice.count > 0
                        enabled: beforeChoice.currentIndex >= 0 && afterChoice.currentIndex >= 0 &&
                            dialog.preparedConfiguration === dialog.fixtureConfiguration
                        text: qsTr("Choose matched response")
                        onClicked: dialog.reviewer.chooseFixture(beforeChoice.currentIndex, afterChoice.currentIndex)
                    }
                }
            }
            ScrollView {
                id: saves
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: saves.availableWidth
                    spacing: 8
                    Readable {
                        text: dialog.reviewer.canMigrate
                            ? qsTr("Review a save from the earlier generation against the current finalized revision. Enter explicit renames. Preserve the original file and export a separate campaign with its review receipt.")
                            : qsTr("Compare an earlier generation with the current finalized generation to review a save migration.")
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: dialog.reviewer.canMigrate
                        Readable { text: qsTr("Original save path"); font.bold: true }
                        TextField {
                            id: sourceSave
                            objectName: "migrationSourceSave"
                            Layout.fillWidth: true
                            maximumLength: 4096
                            placeholderText: qsTr("Absolute path to a local .pgpsave")
                            Accessible.name: qsTr("Original campaign save path")
                            onTextChanged: dialog.reviewer.clearMigration()
                        }
                        Readable { text: qsTr("Identity mappings · add any flow rename before its node renames"); font.bold: true }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            Label { text: qsTr("Family") }
                            ComboBox {
                                id: mappingKind
                                objectName: "migrationKind"
                                Layout.fillWidth: true
                                model: [qsTr("Flow"), qsTr("Node"), qsTr("Scenario"), qsTr("Deal"), qsTr("Scoped variable"), qsTr("Text key")]
                                Accessible.name: qsTr("Typed identity family")
                            }
                            Label { visible: mappingKind.currentIndex === 1; text: qsTr("Source flow") }
                            ComboBox {
                                id: owner
                                objectName: "migrationOwner"
                                visible: mappingKind.currentIndex === 1
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: dialog.reviewer.comparison.length ? dialog.reviewer.mappingIds(0, "", false) : []
                                Accessible.name: qsTr("Source flow owning the renamed node")
                            }
                            Label { text: qsTr("Earlier reference") }
                            ComboBox {
                                id: from
                                objectName: "migrationFrom"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: dialog.reviewer.comparison.length ? dialog.reviewer.mappingIds(mappingKind.currentIndex, owner.currentText, false) : []
                                Accessible.name: qsTr("Source identity")
                            }
                            Label { text: qsTr("Replacement reference") }
                            ComboBox {
                                id: to
                                objectName: "migrationTo"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: {
                                    const version = dialog.reviewer.comparison
                                    const currentMappings = dialog.reviewer.mappings
                                    return version.length ? dialog.reviewer.mappingIds(mappingKind.currentIndex, owner.currentText, true) : []
                                }
                                Accessible.name: qsTr("Destination identity")
                            }
                        }
                        Button {
                            objectName: "addMigrationMapping"
                            text: qsTr("Add mapping")
                            enabled: from.currentIndex >= 0 && to.currentIndex >= 0 && from.currentText !== to.currentText
                            onClicked: dialog.reviewer.addMapping(mappingKind.currentIndex, owner.currentText, from.currentText, to.currentText)
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: existingMappings.count > 0
                            ComboBox {
                                id: existingMappings
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                model: dialog.reviewer.mappings
                                textRole: "label"
                                Accessible.name: qsTr("Staged identity mappings")
                            }
                            Button { text: qsTr("Remove mapping"); onClicked: dialog.reviewer.removeMapping(existingMappings.currentIndex) }
                        }
                        Readable { text: qsTr("Review note"); font.bold: true }
                        TextField {
                            id: migrationSummary
                            objectName: "migrationSummary"
                            Layout.fillWidth: true
                            maximumLength: 1024
                            Accessible.name: qsTr("Explanation of the reviewed migration")
                            onTextChanged: dialog.reviewer.clearMigration()
                        }
                        Button {
                            objectName: "reviewSaveMigration"
                            text: qsTr("Validate save and mappings")
                            enabled: sourceSave.text.length > 0 && migrationSummary.text.trim().length > 0
                            onClicked: {
                                acknowledged.checked = false
                                dialog.reviewer.reviewMigration(sourceSave.text, migrationSummary.text)
                            }
                        }
                        Readout {
                            visible: dialog.reviewer.planId.length > 0
                            caption: qsTr("Validated migration evidence")
                            detail: dialog.reviewer.migrationReport
                        }
                        ColumnLayout {
                            visible: dialog.reviewer.planId.length > 0
                            Layout.fillWidth: true
                            CheckBox {
                                id: acknowledged
                                objectName: "acknowledgeSaveMigration"
                                text: qsTr("I reviewed this migration")
                            }
                            TextField {
                                id: exportName
                                objectName: "migrationExportName"
                                Layout.fillWidth: true
                                maximumLength: 128
                                placeholderText: qsTr("New local export folder name")
                                Accessible.name: qsTr("New migration export name")
                            }
                            Button {
                                objectName: "exportMigratedCopy"
                                text: qsTr("Export reviewed copy")
                                enabled: acknowledged.checked && exportName.text.length > 0
                                onClicked: dialog.reviewer.exportCopy(dialog.reviewer.planId, exportName.text)
                            }
                        }
                        Readable {
                            visible: dialog.reviewer.exportedPath.length > 0
                            text: qsTr("Exported campaign and review receipt. Open the new campaign in the destination game build.\n%1").arg(dialog.reviewer.exportedPath)
                        }
                    }
                }
            }
        }
    }
}
