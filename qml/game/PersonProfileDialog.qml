import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GameDialog {
        id:profileDialog;objectName:"employeeProfileDialog"
        property var person:({})
        property int textSize:14
        readonly property real maximumHeight:Overlay.overlay.height-32
        readonly property bool compact:maximumHeight<textSize*19
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
            ScrollBar.vertical.policy:profileDialog.compact?ScrollBar.AlwaysOn:ScrollBar.AsNeeded
            readonly property real scrollOffset:contentItem?contentItem.contentY:0
            ColumnLayout {
                id:profileContent
                width:profileScroll.availableWidth;spacing:profileDialog.compact?10:Math.round(profileDialog.textSize*1.14)
                GridLayout {
                    Layout.fillWidth:true
                    columns:profileDialog.compact?2:1
                    columnSpacing:16
                    EmployeePortrait {
                        objectName:"profilePortrait"
                        Layout.alignment:Qt.AlignHCenter
                        Layout.preferredWidth:Math.max(84,Math.min(profileDialog.textSize*23,profileScroll.availableWidth*0.65,
                            profileDialog.maximumHeight*(profileDialog.compact?0.24:0.43)))
                        Layout.preferredHeight:Layout.preferredWidth
                        portraitSource:profileDialog.person.portrait || "";personName:profileDialog.person.name || ""
                        visible:portraitSource.toString().length>0
                        enabled:false
                    }
                    Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.role || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize*1.15;font.bold:true}
                }
                Rectangle {
                    id:moodCard
                    objectName:"employeeMoodCard"
                    readonly property var mood:profileDialog.person.mood || ({})
                    visible:!!mood.label
                    Layout.fillWidth:true;implicitHeight:moodDetails.implicitHeight+24
                    color:"#213e42";radius:10;border.color:"#597568"
                    ColumnLayout {
                        id:moodDetails
                        anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12
                        Label {objectName:"employeeMoodHeading";Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("Mood %1 / 100 · %2").arg(moodCard.mood.score || 0).arg(moodCard.mood.label || "");font.pixelSize:profileDialog.textSize*1.15;font.bold:true}
                        Rectangle {
                            Layout.fillWidth:true;Layout.preferredHeight:10;color:"#12292b";radius:5
                            Rectangle {width:parent.width*(moodCard.mood.score || 0)/100;height:parent.height;radius:5;color:(moodCard.mood.score || 0)<45?"#e6a87e":"#a4cda0"}
                        }
                        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;visible:!!moodCard.mood.temperament;text:qsTr("Temperament · %1").arg(moodCard.mood.temperament || "");font.pixelSize:profileDialog.textSize}
                        Repeater {
                            model:moodCard.mood.factors || []
                            RowLayout {
                                required property var modelData
                                Layout.fillWidth:true
                                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:modelData.name;font.pixelSize:profileDialog.textSize}
                                Label {text:(modelData.points>0?"+":"")+String(modelData.points);color:modelData.points<0?"#f1ba92":"#b5dca9";font.pixelSize:profileDialog.textSize}
                            }
                        }
                        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;visible:!!moodCard.mood.amenities;text:qsTr("Available at HQ · %1").arg(moodCard.mood.amenities || "");font.pixelSize:profileDialog.textSize}
                    }
                }
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize}
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.bio || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:profileDialog.textSize}
            }
        }
    }
