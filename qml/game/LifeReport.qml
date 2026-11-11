import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: paper
    required property int textSize
    required property var report
    property string section: "all"
    function focusHeadline() {
        section="all"
        reportHeadline.forceActiveFocus(Qt.OtherFocusReason)
        return reportHeadline
    }
    readonly property var articles: (report.articles || []).filter(article=>section==="all" || article.id===section)
    function sectionName(article) {
        return ({all:qsTr("Full edition"),company:qsTr("Company"),world:qsTr("World"),
            industry:qsTr("Other labs"),people:qsTr("People"),departures:qsTr("Departure"),choices:qsTr("Choices")})[article.id] || article.title
    }
    color: "#e8dfc8"
    border.color: "#a99e84"
    radius: 3
    implicitHeight: edition.implicitHeight + 36
    ColumnLayout {
        id: edition
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 14
        Label {
            Layout.fillWidth: true
            text: paper.report.masthead || ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.family: "Georgia"
            font.pixelSize: paper.textSize * 1.65
            font.bold: true
            color: "#253632"
        }
        Label {
            Layout.fillWidth: true
            text: paper.report.date || ""
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: paper.textSize * 0.9
            color: "#46534b"
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 2; color: "#46534b" }
        Label {
            id: reportHeadline
            property bool revealAtTop:true
            objectName: "lifeReportHeadline"
            Layout.fillWidth: true
            text: paper.report.headline || ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font.family: "Georgia"
            font.pixelSize: paper.textSize * 2
            font.bold: true
            color: "#253632"
        }
        Label {
            Layout.fillWidth: true
            text: paper.report.deck || ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font.pixelSize: paper.textSize
            color: "#46534b"
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [{id:"all",title:qsTr("Full edition")}].concat(paper.report.articles || [])
                WorldButton {
                    required property var modelData
                    objectName: "lifeReportSection_" + modelData.id
                    text: paper.sectionName(modelData)
                    textSize: paper.textSize
                    width: Math.min(implicitWidth, parent.width)
                    checked: paper.section === modelData.id
                    onClicked: paper.section = modelData.id
                }
            }
        }
        Repeater {
            model: paper.articles
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 12
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#a99e84" }
                Label {
                    objectName: "lifeReportArticle_" + parent.modelData.id
                    Layout.fillWidth: true
                    text: modelData.title
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.family: "Georgia"
                    font.pixelSize: paper.textSize * 1.3
                    font.bold: true
                    color: "#253632"
                }
                Repeater {
                    model: parent.modelData.paragraphs
                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.family: "Georgia"
                        font.pixelSize: paper.textSize
                        lineHeight: 1.25
                        color: "#293930"
                    }
                }
            }
        }
    }
}
