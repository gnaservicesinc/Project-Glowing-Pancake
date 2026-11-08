import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    signal openApp(string app,string section)
    property string page: "team"
    spacing:16
    Label {text:qsTr("The people behind the company");Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*1.4;font.bold:true;color:"#eee5d2"}
    Label {text:qsTr("Your founder and named HQ specialists. Site operators remain grouped with their facilities.");Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#b9cfc4"}
    Flow {
        Layout.fillWidth:true;spacing:8
        WorldButton {text:qsTr("HQ team");textSize:panel.textSize;checked:panel.page==="team";onClicked:panel.page="team"}
        WorldButton {objectName:"peopleApplicantsTab";text:qsTr("Applicants");textSize:panel.textSize;checked:panel.page==="applicants";onClicked:panel.page="applicants"}
        WorldButton {text:qsTr("Branch departments");textSize:panel.textSize;checked:panel.page==="branches";onClicked:panel.page="branches"}
    }
    Label {visible:campaign.world.recruitingSummary.length>0;Layout.fillWidth:true;text:campaign.world.recruitingSummary;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#b9cfc4"}
    Flow {
        visible:panel.page==="applicants" && campaign.world.recruitingSummary.length>0
        Layout.fillWidth:true;spacing:8
        Label {text:qsTr("Pay for next cohort");font.pixelSize:panel.textSize;color:"#c9dfd0";height:42;verticalAlignment:Text.AlignVCenter}
        ComboBox {
            id:pay;objectName:"recruitingPay";model:[75,100,125,150];currentIndex:Math.max(0,model.indexOf(campaign.world.recruitingTerms.pay || 100))
            displayText:currentText+"%";font.pixelSize:panel.textSize;Accessible.name:qsTr("Recruiting salary percent")
            enabled:campaign.paused && !campaign.busy && !campaign.storageBlocked
            onActivated:campaign.world.setRecruitingTerms(model[currentIndex],campaign.world.recruitingTerms.mission || "research")
        }
        ComboBox {
            id:mission;objectName:"recruitingMission";model:["research","public benefit","commercial"];currentIndex:Math.max(0,model.indexOf(campaign.world.recruitingTerms.mission || "research"))
            font.pixelSize:panel.textSize;Accessible.name:qsTr("Recruiting mission")
            enabled:campaign.paused && !campaign.busy && !campaign.storageBlocked
            onActivated:campaign.world.setRecruitingTerms(campaign.world.recruitingTerms.pay || 100,model[currentIndex])
        }
    }
    Repeater {
        model:panel.page==="branches" ? [] : panel.page==="applicants" ? campaign.world.applicants : campaign.world.people
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:person.implicitHeight+28;color:"#213b40";radius:10
            GridLayout {
                id:person;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;columnSpacing:14;rowSpacing:14
                columns:panel.width<560?1:2
                EmployeePortrait {
                    objectName:"personPortrait_"+(modelData.id || "")
                    Layout.preferredWidth:Math.max(76,Math.min(panel.width*0.14,panel.textSize*10))
                    Layout.preferredHeight:Layout.preferredWidth
                    Layout.alignment:Qt.AlignTop | Qt.AlignHCenter
                    portraitSource:modelData.portrait || "";personName:modelData.name || ""
                    reducedMotion:campaign.reducedMotion
                    onClicked:{profileDialog.person=modelData;profileDialog.open()}
                }
                ColumnLayout {
                    Layout.fillWidth:true;spacing:5
                    Label {Layout.fillWidth:true;text:modelData.name || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*1.2;font.bold:true;color:"#f3ead8"}
                    Label {Layout.fillWidth:true;text:modelData.role || "";wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#b2d6c4"}
                    Label {Layout.fillWidth:true;text:modelData.detail || "";wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c5d7cb"}
                    WorldButton {
                        objectName:"hireApplicant_"+(modelData.id || "")
                        visible:panel.page==="applicants"
                        text:qsTr("Review offer for %1").arg(modelData.name)
                        textSize:panel.textSize
                        enabled:campaign.paused && !campaign.busy && !campaign.storageBlocked
                        onClicked:{hireDialog.applicant=modelData;hireDialog.open()}
                    }
                    Label {visible:!!modelData.bio;Layout.fillWidth:true;text:modelData.bio || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c5d7cb"}
                }
            }
        }
    }
    Dialog {
        id:profileDialog;objectName:"employeeProfileDialog"
        property var person:({})
        anchors.centerIn:Overlay.overlay
        width:Math.min(panel.textSize*48,Overlay.overlay.width-32)
        height:Math.min(panel.textSize*54,Overlay.overlay.height-32)
        modal:true;focus:true;title:person.name || qsTr("Employee profile")
        standardButtons:Dialog.Close
        contentItem:ScrollView {
            id:profileScroll;objectName:"employeeProfileScroll";contentWidth:availableWidth;clip:true;focus:true
            readonly property real scrollOffset:contentItem?contentItem.contentY:0
            ColumnLayout {
                width:profileScroll.availableWidth;spacing:16
                EmployeePortrait {
                    objectName:"profilePortrait"
                    Layout.alignment:Qt.AlignHCenter
                    Layout.preferredWidth:Math.max(100,Math.min(panel.textSize*23,profileScroll.availableWidth*0.65,profileDialog.height*0.43))
                    Layout.preferredHeight:Layout.preferredWidth
                    portraitSource:profileDialog.person.portrait || "";personName:profileDialog.person.name || ""
                    enabled:false
                }
                Label {Layout.fillWidth:true;text:profileDialog.person.role || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize*1.15;font.bold:true}
                Label {Layout.fillWidth:true;text:profileDialog.person.detail || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}
                Label {visible:text.length>0;Layout.fillWidth:true;text:profileDialog.person.bio || "";wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize}
            }
        }
    }
    Repeater {
        model:panel.page==="branches" ? campaign.world.branches : []
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            Label {Layout.fillWidth:true;text:modelData.name;textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*1.2;color:"#f0e7d1"}
            Label {Layout.fillWidth:true;text:modelData.detail;textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c2d8ca"}
            WorldButton {text:qsTr("Manage facility");textSize:panel.textSize;onClicked:panel.openApp("operations","")}
        }
    }
    Label {visible:panel.page==="branches" && campaign.world.branches.length===0;Layout.fillWidth:true;text:qsTr("No branch departments yet. Hire site operators as a group when you build a facility.");wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c2d8ca"}
    Dialog {
        id:hireDialog;objectName:"employeeHireDialog"
        property var applicant:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(540,Overlay.overlay.width-32)
        modal:true;focus:true;title:qsTr("Hire %1?").arg(applicant.name || "")
        standardButtons:Dialog.Ok | Dialog.Cancel
        contentItem:Label {text:hireDialog.applicant.detail || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize}
        onAccepted:campaign.world.hireEmployee(applicant.id)
    }
    Flow {
        Layout.fillWidth:true;spacing:8
        WorldButton {text:qsTr("Recruit & assign specialists");textSize:panel.textSize;onClicked:panel.openApp("office","funding")}
        WorldButton {text:qsTr("Executives & board");textSize:panel.textSize;onClicked:panel.openApp("office","governance")}
        WorldButton {text:qsTr("Succession & retirement");textSize:panel.textSize;onClicked:panel.openApp("leadership","")}
    }
}
