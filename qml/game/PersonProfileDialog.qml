import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GameDialog {
        id:profileDialog;objectName:"employeeProfileDialog"
        property var person:({})
        property int textSize:14
        readonly property real maximumHeight:Overlay.overlay.height-32
        anchors.centerIn:Overlay.overlay
        width:Math.min(profileDialog.textSize*48,Overlay.overlay.width-32)
        height:Math.min(implicitHeight,maximumHeight)
        modal:true;focus:true;title:person.name || qsTr("Employee profile")
        standardButtons:Dialog.Close
        header:Label {
            text:profileDialog.title;textFormat:Text.PlainText;wrapMode:Text.Wrap
            font.pixelSize:profileDialog.textSize;font.bold:true
            padding:Math.round(profileDialog.textSize*0.6)
        }
        contentItem:ScrollView {
            id:profileScroll;objectName:"employeeProfileScroll";contentWidth:availableWidth;clip:true;focus:true
            implicitHeight:profileContent.implicitHeight
            readonly property real scrollOffset:contentItem?contentItem.contentY:0
            ColumnLayout {
                id:profileContent
                width:profileScroll.availableWidth;spacing:Math.round(profileDialog.textSize*1.14)
                EmployeePortrait {
                    objectName:"profilePortrait"
                    Layout.alignment:Qt.AlignHCenter
                    Layout.preferredWidth:Math.max(100,Math.min(profileDialog.textSize*23,profileScroll.availableWidth*0.65,profileDialog.maximumHeight*0.43))
                    Layout.preferredHeight:Layout.preferredWidth
                    portraitSource:profileDialog.person.portrait || "";personName:profileDialog.person.name || ""
                    visible:portraitSource.toString().length>0
                    enabled:false
                }
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.role || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize*1.15;font.bold:true}
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize}
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.bio || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize}
            }
        }
    }
