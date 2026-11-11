import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    readonly property var companies:campaign.world.industryPeople
    property string selected:campaign.world.draft("people/industry","")
    readonly property var company:companies.find(c=>c.id===selected) || companies[0] || ({})
    spacing:14
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#dce5dc"}
    Copy {text:qsTr("People around the industry");font.pixelSize:panel.textSize*1.5;font.bold:true}
    Copy {text:qsTr("Every lab has people behind its name. Explore the public HQ roster of the major companies that have opened their doors.")}
    GameSelect {objectName:"industryCompany";Layout.fillWidth:true;textSize:panel.textSize;model:panel.companies;textRole:"name";currentIndex:Math.max(0,panel.companies.findIndex(c=>c.id===panel.company.id));onActivated:{panel.selected=panel.companies[currentIndex].id;campaign.world.setDraft("people/industry",panel.selected)} Accessible.name:qsTr("Industry company")}
    Copy {text:panel.company.name || qsTr("No major HQs have opened yet.");font.bold:true}
    Copy {objectName:"industryLocation";text:(panel.company.city || "")+" · "+(panel.company.state || "")}
    Repeater {
        model:panel.company.people || []
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:body.implicitHeight+28;radius:14;color:"#293f45";border.color:"#6c8b81"
            RowLayout {
                id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:14
                EmployeePortrait {Layout.preferredWidth:Math.min(80,panel.width*0.18);Layout.preferredHeight:Layout.preferredWidth;portraitSource:modelData.portrait;personName:modelData.name;reducedMotion:campaign.reducedMotion;onClicked:{profile.person=modelData;profile.open()}}
                ColumnLayout {Layout.fillWidth:true
                    Copy {text:modelData.name;font.bold:true;color:"#e7d299"}
                    Copy {text:modelData.role}
                    Copy {text:modelData.detail}
                    WorldButton {objectName:"industryHello_"+modelData.id;text:qsTr("Say hello");textSize:panel.textSize;enabled:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked && !modelData.socialBlocker;onClicked:campaign.world.socialize(modelData.id,"chat",campaign.world.reviewToken)}
                    Copy {visible:!!modelData.socialBlocker;text:modelData.socialBlocker || "";color:"#b5c6c5"}
                }
            }
        }
    }
    PersonProfileDialog {id:profile;textSize:panel.textSize}
}
