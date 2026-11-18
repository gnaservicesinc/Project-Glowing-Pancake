import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: desk
    required property int textSize
    required property bool decision
    required property var edition
    required property var colleague
    signal closed()
    signal datasetRequested(string id)
    signal researchRequested(string id)
    signal editionRequested(var edition)
    readonly property int clippingIndex: decision || colleague.id ? -1
        : campaign.deskEditions.findIndex(row => row.id === edition.id)

    signal choiceRequested(string id)
    property string failure: ""
    Connections {
        target: campaign
        function onCommandFinished(accepted, explanation) { desk.failure = accepted ? "" : explanation }
    }
    readonly property var speaker: decision ? (campaign.requiredParticipants.find(p => p.id !== "founder") || ({})) : colleague
    readonly property string headline: decision ? campaign.requiredTitle : colleague.id ? qsTr("A break with %1").arg(colleague.name) : edition.title || ""
    function restart() {
        if (storyScroll.contentItem) storyScroll.contentItem.contentY = 0
        headlineLabel.forceActiveFocus(Qt.OtherFocusReason)
    }
    Component.onCompleted: Qt.callLater(restart)
    onHeadlineChanged: if (visible) Qt.callLater(restart)

    // Consume pointer input over the page, including the margin around the letter.
    Rectangle { anchors.fill: parent; color: "#ec18292f" }
    MouseArea { anchors.fill: parent }
    Rectangle {
        id: sheet
        anchors.centerIn: parent
        width: Math.min(desk.width - 16, desk.textSize * 49)
        height: Math.min(desk.height - 12, article.implicitHeight + returnBar.height + 38)
        radius: 8
        color: "#f3ead7"
        border.color: "#aa9a7c"
        Rectangle { width: parent.width - 24; height: 3; color: "#437166"; anchors.top: parent.top; anchors.topMargin: 7; anchors.horizontalCenter: parent.horizontalCenter }
        ScrollView {
            id: storyScroll
            objectName: "deskStoryScroll"
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: returnBar.top; margins: 14 }
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            function reveal(item) {
                let ancestor = item
                while (ancestor && ancestor !== article) ancestor = ancestor.parent
                if (!ancestor || !contentItem) return
                const y = item.mapToItem(article, 0, 0).y
                const top = contentItem.contentY
                if (y < top) contentItem.contentY = y
                else if (y + item.height > top + availableHeight)
                    contentItem.contentY = Math.min(y, y + item.height - availableHeight)
            }
            Connections {
                target: desk.Window.window
                function onActiveFocusItemChanged() { storyScroll.reveal(target.activeFocusItem) }
            }
            ColumnLayout {
                id: article
                width: storyScroll.availableWidth
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    EmployeePortrait {
                        visible: !!desk.speaker.portrait
                        portraitSource: desk.speaker.portrait || ""
                        personName: desk.speaker.name || ""
                        reducedMotion: campaign.reducedMotion
                        enabled: false
                        Layout.preferredHeight: desk.textSize * 3.8
                        Layout.preferredWidth: Layout.preferredHeight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (desk.decision || desk.colleague.id) ? [desk.speaker.name, desk.speaker.role, campaign.date].filter(Boolean).join("  ·  ")
                            : [desk.edition.section, desk.edition.date].filter(Boolean).join("  ·  ")
                        wrapMode: Text.Wrap; textFormat: Text.PlainText
                        color: "#38594f"; font.bold: true; font.pixelSize: desk.textSize
                    }
                }
                Label {
                    id: headlineLabel
                    objectName: "deskStoryHeadline"
                    Layout.fillWidth: true
                    text: desk.headline; textFormat: Text.PlainText; wrapMode: Text.Wrap
                    color: "#253833"; font.family: "Georgia"; font.pixelSize: desk.textSize * 1.55; font.bold: true
                    Accessible.role: Accessible.Heading
                }
                Label {
                    Layout.fillWidth: true
                    text: desk.decision ? campaign.requiredText : desk.colleague.id
                        ? (desk.colleague.spokeToday ? desk.colleague.memory : desk.colleague.blocker || qsTr("You have a few minutes together. What would you like to talk about?"))
                        : desk.edition.detail || ""
                    textFormat: Text.PlainText; wrapMode: Text.Wrap
                    color: "#303931"; font.pixelSize: desk.textSize * 1.05; lineHeight: 1.16
                }
                Label {
                    visible: !!desk.failure
                    Layout.fillWidth: true
                    text: desk.failure; wrapMode: Text.Wrap; textFormat: Text.PlainText
                    color: "#823320"; font.pixelSize: desk.textSize
                    Accessible.role: Accessible.AlertMessage
                }
                WorldButton {
                    objectName: "deskOpenDataset"
                    visible: !desk.decision && !!desk.edition.dataset && !desk.colleague.id
                    Layout.fillWidth: true
                    textSize: desk.textSize
                    text: qsTr("See the dataset in the Lab")
                    primary: true
                    onClicked: desk.datasetRequested(desk.edition.dataset)
                }
                WorldButton {
                    objectName: "deskReadResearch"
                    visible: !desk.decision && !!desk.edition.research && !desk.colleague.id
                    Layout.fillWidth: true
                    textSize: desk.textSize
                    text: qsTr("Read the findings in the Lab")
                    primary: true
                    onClicked: desk.researchRequested(desk.edition.research)
                }
                RowLayout {
                    visible: desk.clippingIndex >= 0 && campaign.deskEditions.length > 1
                    Layout.fillWidth: true
                    WorldButton {
                        objectName: "deskOlderClipping"
                        Layout.fillWidth: true
                        text: qsTr("Older clipping")
                        textSize: desk.textSize
                        enabled: desk.clippingIndex + 1 < campaign.deskEditions.length
                        onClicked: desk.editionRequested(campaign.deskEditions[desk.clippingIndex+1])
                    }
                    WorldButton {
                        objectName: "deskNewerClipping"
                        Layout.fillWidth: true
                        text: qsTr("Newer clipping")
                        textSize: desk.textSize
                        enabled: desk.clippingIndex > 0
                        onClicked: desk.editionRequested(campaign.deskEditions[desk.clippingIndex-1])
                    }
                }
                Repeater {
                    id: choices
                    model: desk.decision ? campaign.requiredChoices : desk.colleague.id && !desk.colleague.blocker
                        ? [{choiceId:"chat",label:qsTr("Catch up over a break")},{choiceId:"listen",label:qsTr("Ask what’s on their mind")},{choiceId:"hobby",label:qsTr("Ask about an interest")}] : []
                    WorldButton {
                        required property var modelData
                        objectName: "deskChoice_" + modelData.choiceId
                        Layout.fillWidth: true
                        Layout.preferredWidth: 0
                        textSize: desk.textSize
                        text: modelData.label
                        implicitHeight: Math.max(desk.textSize * 2.6, contentItem.implicitHeight + topPadding + bottomPadding)
                        enabled: !campaign.busy && !campaign.storageBlocked
                        onClicked: desk.choiceRequested(modelData.choiceId)
                    }
                }
            }
        }
        RowLayout {
            id: returnBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 6 }
            WorldButton {
                objectName: "deskShowChoices"
                readonly property var firstChoice: choices.count ? choices.itemAt(0) : null
                visible: !!firstChoice && storyScroll.contentItem
                    && firstChoice.y + Math.min(firstChoice.height, storyScroll.availableHeight)
                        > storyScroll.contentItem.contentY + storyScroll.availableHeight + 1
                Layout.fillWidth: true
                text: qsTr("Jump to choices")
                textSize: desk.textSize
                onClicked: {
                    firstChoice.forceActiveFocus(Qt.ShortcutFocusReason)
                    storyScroll.reveal(firstChoice)
                }
            }
            WorldButton {
                objectName: "closeDeskStory"
                Layout.fillWidth: true
                text: desk.decision ? qsTr("Set aside for now") : qsTr("Back to headquarters")
                textSize: desk.textSize
                onClicked: desk.closed()
            }
        }
    }
}
