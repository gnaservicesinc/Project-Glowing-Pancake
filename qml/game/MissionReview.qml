import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property string mission
    required property bool editable
    required property int textSize
    readonly property var choices: [
        {id:"research", name:qsTr("Research")},
        {id:"public benefit", name:qsTr("Public benefit")},
        {id:"commercial", name:qsTr("Commercial")}
    ]
    function nameFor(id) { return (choices.find(choice => choice.id === id) || {}).name || id }

    Flow {
        Layout.fillWidth: true
        spacing: 8
        Label {
            text: qsTr("Company mission · %1").arg(panel.nameFor(panel.mission))
            font.pixelSize: panel.textSize
            color: "#dbe4e3"
        }
        WorldButton {
            objectName: "reviewCompanyMission"
            text: qsTr("Change direction")
            textSize: panel.textSize
            enabled: panel.editable
            onClicked: {
                review.revision = campaign.world.reviewToken
                review.open()
            }
        }
    }
    GameDialog {
        id: review
        objectName: "missionReviewDialog"
        title: qsTr("Change the company mission")
        anchors.centerIn: Overlay.overlay
        width: Math.min(panel.textSize*37,Overlay.overlay.width-32)
        height: Math.min(panel.textSize*23,Overlay.overlay.height-32)
        modal: true
        focus: true
        property string revision: ""
        property string selected: ""
        readonly property var options: panel.choices.filter(choice => choice.id !== panel.mission)
        readonly property var affected: selected ? campaign.world.missionPreview(selected) : []
        onOpened: selected = (options[0] || {}).id || ""

        contentItem: ScrollView {
            id: scroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12
                GameSelect {
                    objectName: "newCompanyMission"
                    Layout.fillWidth: true
                    textSize: panel.textSize
                    textRole: "name"
                    model: review.options
                    currentIndex: Math.max(0, review.options.findIndex(choice => choice.id === review.selected))
                    onActivated: review.selected = review.options[currentIndex].id
                    Accessible.name: qsTr("New company mission")
                }
                Label {
                    Layout.fillWidth: true
                    visible: review.affected.length > 0
                    text: qsTr("Likely to raise concerns\n%1").arg(review.affected.join(", "))
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: panel.textSize
                    color: "#eed69e"
                }
            }
        }
        footer: Flow {
            spacing: 8
            padding: 10
            WorldButton {
                text: qsTr("Cancel")
                textSize: panel.textSize
                onClicked: review.close()
            }
            WorldButton {
                objectName: "confirmCompanyMission"
                text: qsTr("Change mission")
                textSize: panel.textSize
                enabled: panel.editable && review.selected.length > 0
                onClicked: {
                    campaign.world.changeCompanyMission(review.selected,review.revision)
                    review.close()
                }
            }
        }
    }
}
