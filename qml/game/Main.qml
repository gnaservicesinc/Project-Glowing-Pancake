import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    objectName: "gameWindow"
    title: qsTr("Project Glowing Pancake")
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
    readonly property int textSize: Math.round(14 * campaign.textScalePercent / 100)
    readonly property bool narrow: width < 1030
    font.pixelSize: textSize

    component Heading: Label {
        Layout.fillWidth: true
        color: "#ffd897"
        font.pixelSize: Math.round(window.textSize * 1.22)
        font.bold: true
        wrapMode: Text.Wrap
    }
    component Copy: Label {
        Layout.fillWidth: true
        font.pixelSize: window.textSize
        wrapMode: Text.Wrap
    }
    component Action: Button {
        id: action
        Accessible.name: text
        font.pixelSize: window.textSize
        topPadding: 5
        bottomPadding: 5
        leftPadding: 12
        rightPadding: 12
        contentItem: Text {
            text: action.text
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
        title: qsTr("Load campaign")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Campaign saves (*.pgpsave)")]
        currentFolder: campaign.saveFolder
        onAccepted: campaign.loadFrom(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Panel {
            padding: 10
            contentItem: ColumnLayout {
                spacing: 5
                Copy {
                    objectName: "statusLine"
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
                        objectName: "pauseButton"
                        text: campaign.paused ? qsTr("Play") : qsTr("Pause")
                        enabled: !campaign.busy && (!campaign.paused || campaign.canPlay)
                        onClicked: campaign.togglePaused()
                    }
                    Action {
                        objectName: "stepButton"; text: qsTr("Step day")
                        enabled: campaign.paused && campaign.canAdvance
                        onClicked: campaign.step()
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
                    visible: campaign.requiredKind !== "none"
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
                                objectName: "acknowledgeWarning"; text: qsTr("Acknowledge warning")
                                visible: campaign.requiredKind === "liquidity"
                                enabled: !campaign.busy
                                onClicked: campaign.acknowledgeWarning()
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
                    visible: window.settingsOpen
                    contentItem: ColumnLayout {
                        Heading { text: qsTr("Local preferences") }
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
                                checked: campaign.reducedMotion
                                enabled: !campaign.busy
                                onClicked: campaign.reducedMotion = !campaign.reducedMotion
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

                GridLayout {
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

    onActiveFocusItemChanged: scroll.reveal(activeFocusItem)

    Dialog {
        id: cancelDialog
        property string projectId: ""
        title: qsTr("Cancel this project?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: campaign.projectCancel(projectId)
    }
}
