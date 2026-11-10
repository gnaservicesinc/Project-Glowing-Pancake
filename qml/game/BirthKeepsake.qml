import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GameDialog {
    id: record
    required property int textSize
    required property string founder
    required property string surname
    required property string birthSex
    title: qsTr("Family keepsake")
    anchors.centerIn: Overlay.overlay
    width: Math.min((Overlay.overlay ? Overlay.overlay.width : 650) - 32, textSize * 27)
    height: Math.min((Overlay.overlay ? Overlay.overlay.height : 480) - 32, textSize * 24)
    modal: true
    focus: true
    standardButtons: Dialog.Close
    font.pixelSize: textSize
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: campaign.reducedMotion ? 0 : 150 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: campaign.reducedMotion ? 0 : 100 } }

    component PaperLabel: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: "#35464b"
        font.pixelSize: record.textSize
    }

    contentItem: ScrollView {
        id: recordScroll
        contentWidth: availableWidth
        clip: true
        Rectangle {
            objectName: "familyCertificate"
            width: recordScroll.availableWidth
            height: Math.max(recordCopy.implicitHeight + 56, width * 0.56)
            color: "#cfaa72"
            border.width: 6
            border.color: "#a0794b"
            radius: 4
            Image {
                anchors.fill: parent
                anchors.margins: 6
                source: "qrc:/pgp/assets/founding/birth-certificate.svg"
                Accessible.ignored: true
            }
            ColumnLayout {
                id: recordCopy
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 27
                spacing: 7
                PaperLabel { text: qsTr("CALIFORNIA  /  JANUARY 1997"); font.pixelSize: record.textSize * 0.66 }
                PaperLabel { text: qsTr("Certificate of Birth"); font.bold: true; Layout.rightMargin: recordCopy.width * 0.13 }
                PaperLabel { text: record.founder; font.pixelSize: record.textSize * 1.15 }
                PaperLabel {
                    text: qsTr("Sex at birth · %1").arg(record.birthSex === "female" ? qsTr("Female")
                        : record.birthSex === "male" ? qsTr("Male") : qsTr("Indeterminate"))
                    font.pixelSize: record.textSize * 0.86
                }
                PaperLabel {
                    text: qsTr("The %1 family").arg(record.surname)
                    font.family: "Snell Roundhand"
                    font.italic: true
                    color: "#314865"
                }
            }
        }
    }
}
