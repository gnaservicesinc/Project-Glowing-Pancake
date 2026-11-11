import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    signal openApp(string app,string section)
    signal revealRequested(Item target)
    property string page: "team"
    property string selectedApplicant: campaign.world.draft("people/applicant", "")
    readonly property bool showDiscussion: page === "applicants" && discussion.offer.id !== undefined
    readonly property bool messagesOnly: campaign.world.navigation.app === "people" && campaign.world.navigation.section === "messages"
    readonly property bool editable: campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    function selectApplicant(id) {
        selectedApplicant=id
        campaign.world.setDraft("people/applicant", id)
        Qt.callLater(() => discussion.focusDiscussion())
    }
    function changePage(value) {page=value;campaign.world.setDraft("people/page",value)}
    function restorePage() {
        const value=messagesOnly?"messages":campaign.world.draft("people/page","team")
        page=["team","applicants","branches","messages","university","social","industry"].includes(value)?value:"team"
    }
    Component.onCompleted:restorePage()
    Connections {
        target:campaign.world
        function onDraftsReset(){panel.restorePage();panel.selectedApplicant=campaign.world.draft("people/applicant","")}
        function onNavigationChanged(){if(campaign.world.navigation.app==="people")panel.restorePage()}
    }
    spacing:16
    Label {visible:!panel.showDiscussion && panel.page!=="messages" && panel.page!=="university" && panel.page!=="social" && panel.page!=="industry";text:qsTr("The people behind the company");Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*1.4;font.bold:true;color:"#eee5d2"}
    Label {visible:!panel.showDiscussion && panel.page!=="messages" && panel.page!=="university" && panel.page!=="social" && panel.page!=="industry";text:qsTr("Your founder and named HQ specialists. Site operators remain grouped with their facilities.");Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#b9cfc4"}
    Flow {
        visible:!panel.showDiscussion && !panel.messagesOnly && panel.page!=="university" && panel.page!=="social" && panel.page!=="industry"
        Layout.fillWidth:true;spacing:8
        WorldButton {objectName:"peopleIndustryTab";visible:campaign.world.industryPeople.length>0;text:qsTr("Around the industry");textSize:panel.textSize;onClicked:panel.changePage("industry")}
        WorldButton {objectName:"peopleSocialTab";visible:!!campaign.world.relationships.people;text:qsTr("Social circle");textSize:panel.textSize;onClicked:panel.changePage("social")}
        WorldButton {objectName:"peopleUniversityTab";visible:!!campaign.world.talent.skills;text:qsTr("Online university");textSize:panel.textSize;checked:panel.page==="university";onClicked:panel.changePage("university")}
        WorldButton {objectName:"peopleTeamTab";text:qsTr("HQ team");textSize:panel.textSize;checked:panel.page==="team";onClicked:panel.changePage("team")}
        WorldButton {objectName:"peopleApplicantsTab";text:qsTr("Applicants");textSize:panel.textSize;checked:panel.page==="applicants";onClicked:panel.changePage("applicants")}
        WorldButton {objectName:"peopleBranchesTab";text:qsTr("Branch departments");textSize:panel.textSize;checked:panel.page==="branches";onClicked:panel.changePage("branches")}
        WorldButton {objectName:"peopleMessagesTab";visible:campaign.world.contactsAvailable;text:qsTr("Messages");textSize:panel.textSize;checked:panel.page==="messages";onClicked:panel.changePage("messages")}
    }
    Label {visible:!panel.showDiscussion && panel.page!=="messages" && panel.page!=="university" && panel.page!=="social" && panel.page!=="industry" && campaign.world.recruitingSummary.length>0;Layout.fillWidth:true;text:campaign.world.recruitingSummary;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#b9cfc4"}
    Flow {
        visible:!panel.showDiscussion && panel.page==="applicants" && campaign.world.recruitingSummary.length>0
        Layout.fillWidth:true;spacing:8
        Label {text:qsTr("Pay for next cohort");font.pixelSize:panel.textSize;color:"#c9dfd0";height:42;verticalAlignment:Text.AlignVCenter}
        GameSelect {textSize:panel.textSize;
            id:pay;objectName:"recruitingPay";model:[75,100,125,150];currentIndex:Math.max(0,model.indexOf(campaign.world.recruitingTerms.pay || 100))
            displayText:currentText+"%";Accessible.name:qsTr("Recruiting salary percent")
            enabled:campaign.paused && !campaign.busy && !campaign.storageBlocked
            onActivated:campaign.world.setRecruitingTerms(model[currentIndex],campaign.world.recruitingTerms.mission || "research")
        }
        GameSelect {textSize:panel.textSize;
            id:mission;objectName:"recruitingMission";model:["research","public benefit","commercial"];currentIndex:Math.max(0,model.indexOf(campaign.world.recruitingTerms.mission || "research"))
            Accessible.name:qsTr("Recruiting mission")
            enabled:campaign.paused && !campaign.busy && !campaign.storageBlocked
            onActivated:campaign.world.setRecruitingTerms(campaign.world.recruitingTerms.pay || 100,model[currentIndex])
        }
    }
    RecruitmentDiscussion {
        id:discussion
        Layout.fillWidth:true
        visible:panel.showDiscussion
        applicantId:panel.selectedApplicant
        textSize:panel.textSize
        editable:panel.editable
        onCloseRequested:panel.selectApplicant("")
        onSuppliersRequested:panel.openApp("facilities","equipment")
    }
    Repeater {
        model:panel.showDiscussion || panel.page==="branches" || panel.page==="messages" || panel.page==="university" || panel.page==="social" || panel.page==="industry" ? [] : panel.page==="applicants" ? campaign.world.applicants : campaign.world.people
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
                    Label {
                        visible:!!modelData.mood
                        Layout.fillWidth:true;wrapMode:Text.Wrap
                        text:modelData.mood?qsTr("Mood %1 / 100 · %2").arg(modelData.mood.score).arg(modelData.mood.label):""
                        font.pixelSize:panel.textSize;font.bold:true
                        color:modelData.mood && modelData.mood.score<45?"#f1ba92":"#b5dca9"
                    }
                    Label {Layout.fillWidth:true;text:modelData.detail || "";wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c5d7cb"}
                    Label {
                        visible:panel.page==="applicants" && !!modelData.discussion
                        Layout.fillWidth:true;text:modelData.discussion || "";wrapMode:Text.Wrap
                        font.pixelSize:panel.textSize;color:"#e6ca91"
                    }
                    WorldButton {
                        objectName:"hireApplicant_"+(modelData.id || "")
                        visible:panel.page==="applicants"
                        text:modelData.negotiated?qsTr("Discuss terms"):qsTr("Review offer for %1").arg(modelData.name)
                        textSize:panel.textSize
                        enabled:modelData.negotiated || panel.editable
                        onClicked:{
                            if(modelData.negotiated) panel.selectApplicant(modelData.id)
                            else {hireDialog.applicant=modelData;hireDialog.open()}
                        }
                    }
                    Label {visible:!!modelData.bio;Layout.fillWidth:true;text:modelData.bio || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#c5d7cb"}
                }
            }
        }
    }
    WorldButton {objectName:"peopleBackToTeam";visible:panel.page==="university" || panel.page==="social" || panel.page==="industry";text:qsTr("Back to people");textSize:panel.textSize;onClicked:panel.changePage("team")}
    IndustryPeoplePanel {Layout.fillWidth:true;visible:panel.page==="industry";textSize:panel.textSize}
    SocialPanel {Layout.fillWidth:true;visible:panel.page==="social";textSize:panel.textSize}
    UniversityPanel {Layout.fillWidth:true;visible:panel.page==="university";textSize:panel.textSize}
    PersonProfileDialog {id:profileDialog;textSize:panel.textSize}
    ContactsPanel {
        Layout.fillWidth:true
        visible:panel.page==="messages" && campaign.world.contactsAvailable
        showHeading:!panel.messagesOnly
        textSize:panel.textSize
        onRevealRequested:target=>panel.revealRequested(target)
        onGovernanceRequested:panel.openApp("office","governance")
    }
    ManagerPanel {Layout.fillWidth:true;visible:panel.page==="branches" && Object.keys(campaign.world.operationsManagement).length>0;textSize:panel.textSize;onProfileRequested:person=>{profileDialog.person=person;profileDialog.open()}}
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
    GameDialog {
        id:hireDialog;objectName:"employeeHireDialog"
        property var applicant:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(540,Overlay.overlay.width-32)
        modal:true;focus:true;title:qsTr("Hire %1?").arg(applicant.name || "")
        background:Rectangle {radius:12;color:"#142d32";border.color:"#bba775"}
        contentItem:Label {text:hireDialog.applicant.detail || "";textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#d7e3d7"}
        footer:RowLayout {
            Item {Layout.fillWidth:true}
            WorldButton {text:qsTr("Cancel");textSize:panel.textSize;onClicked:hireDialog.reject()}
            WorldButton {text:qsTr("Sign employment");textSize:panel.textSize;primary:true;onClicked:hireDialog.accept()}
        }
        onAccepted:campaign.world.hireEmployee(applicant.id)
    }
    Flow {
        visible:!panel.showDiscussion
        Layout.fillWidth:true;spacing:8
        WorldButton {text:qsTr("Recruit & assign specialists");textSize:panel.textSize;onClicked:panel.openApp("office","funding")}
        WorldButton {text:qsTr("Executives & board");textSize:panel.textSize;onClicked:panel.openApp("office","governance")}
        WorldButton {text:qsTr("Succession & retirement");textSize:panel.textSize;onClicked:panel.openApp("leadership","")}
    }
}
