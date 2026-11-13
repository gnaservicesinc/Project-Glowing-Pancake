import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:album
    property int textSize:14
    signal recruitmentRequested()
    readonly property var family:campaign.world.familyAlbum
    readonly property var members:family.members || []
    readonly property var tracks:family.tracks || []
    readonly property bool editable:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    property string selected:campaign.world.draft("family/person","")
    readonly property var person:members.find(p=>p.id===selected) || members[0] || ({})
    property string action:campaign.world.draft("family/plan","adopt")
    property string band:campaign.world.draft("family/age","baby")
    property string babyName:campaign.world.draft("family/name","")
    property bool withPartner:false
    property string track:person.track || "programming"
    readonly property var draft:({action:action,band:band,name:action==="birth"?babyName:"",withPartner:action==="birth" || withPartner})
    readonly property var preview:{const revision=family.revision;return campaign.world.familyQuote(draft)}
    readonly property var learning:{const revision=family.revision;return campaign.world.familyQuote({action:"track",child:person.id || "",track:track})}
    readonly property var adoption:{const revision=family.revision;return campaign.world.familyQuote({action:"adopt_placement",child:person.id || "",withPartner:withPartner})}
    readonly property var application:{const revision=family.revision;return campaign.world.familyQuote({action:"apply",child:person.id || ""})}
    function review(draft,title,preview){confirmation.terms=Object.assign({},draft);confirmation.revision=campaign.world.reviewToken;confirmation.heading=title;confirmation.copy=preview.explanation+(preview.due?"\n\n"+qsTr("Review or arrival: %1").arg(preview.due):"");confirmation.open()}
    spacing:14;visible:!!family.available
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:album.textSize;color:"#dae5d8"}
    Copy {text:qsTr("The family album");font.pixelSize:album.textSize*1.5;font.bold:true;color:"#edd6a0"}
    Copy {text:qsTr("Little beginnings. Big chapters.")}
    Copy {objectName:"familyReply";visible:!!album.family.reply;text:album.family.reply || "";color:"#e5c58c";font.bold:true}
    Rectangle {
        Layout.fillWidth:true;implicitHeight:planner.implicitHeight+32;radius:16;color:"#2c4240";border.color:"#a6b58c"
        ColumnLayout {
            id:planner;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
            Copy {text:qsTr("Room for a new chapter?");font.bold:true;color:"#ead49d"}
            Flow {Layout.fillWidth:true;spacing:8
                Repeater {model:[{id:"birth",name:qsTr("Welcome a baby")},{id:"adopt",name:qsTr("Adoption")},{id:"foster",name:qsTr("Foster care")}]
                    WorldButton {required property var modelData;objectName:"familyPlan_"+modelData.id;text:modelData.name;textSize:album.textSize;checked:album.action===modelData.id;onClicked:{album.action=modelData.id;campaign.world.setDraft("family/plan",album.action)}}
                }
            }
            Flow {visible:album.action!=="birth";Layout.fillWidth:true;spacing:8
                Repeater {model:[{id:"baby",name:qsTr("Baby · 0–1")},{id:"child",name:qsTr("Child · 4–8")},{id:"teen",name:qsTr("Teen · 13–17")}]
                    WorldButton {required property var modelData;objectName:"familyAge_"+modelData.id;text:modelData.name;textSize:album.textSize;checked:album.band===modelData.id;onClicked:{album.band=modelData.id;campaign.world.setDraft("family/age",album.band)}}
                }
            }
            GameCheck {objectName:"familyWithPartner";Layout.fillWidth:true;visible:!!album.family.married && album.action!=="birth";text:qsTr("Plan together with my spouse");textSize:album.textSize;checked:album.withPartner;onToggled:album.withPartner=checked}
            GameField {objectName:"familyBabyName";visible:album.action==="birth";Layout.fillWidth:true;textSize:album.textSize;maximumLength:128;text:album.babyName;placeholderText:qsTr("Baby's name · leave blank for a surprise");onTextEdited:{album.babyName=text;campaign.world.setDraft("family/name",text)} Accessible.name:qsTr("Baby's name")}
            Copy {text:album.preview.blocker || album.preview.explanation || "";color:album.preview.valid?"#dce1b8":"#e7c38f"}
            Copy {objectName:"familyPlanDate";visible:!!album.preview.valid && !!album.preview.due;text:qsTr("Review or arrival if agreed: %1").arg(album.preview.due || "")}
            WorldButton {objectName:"familyPlanReview";text:qsTr("Talk through this plan");textSize:album.textSize;enabled:album.editable && !!album.preview.valid;onClicked:album.review(album.draft,qsTr("Begin this family chapter?"),album.preview)}
        }
    }
    Repeater {
        model:album.family.plans || []
        Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:planBody.implicitHeight+28;radius:14;color:"#233b40";border.color:"#6e9188"
            ColumnLayout {id:planBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:10
                Copy {text:modelData.title+" · "+modelData.status;font.bold:true;color:"#e4cb91"}
                Copy {objectName:"familyPlanStatus_"+modelData.id;text:modelData.detail}
                Rectangle {Layout.fillWidth:true;implicitHeight:12;radius:6;color:"#172d32";Rectangle {height:parent.height;width:parent.width*modelData.progress/100;radius:6;color:"#9ec3a2"} Accessible.role:Accessible.ProgressBar;Accessible.name:qsTr("Family plan %1 percent of waiting period elapsed").arg(modelData.progress)}
            }
        }
    }
    Copy {text:qsTr("People in your album");font.bold:true}
    GameSelect {objectName:"familyPerson";Layout.fillWidth:true;textSize:album.textSize;model:album.members;textRole:"name";currentIndex:Math.max(0,album.members.findIndex(p=>p.id===album.person.id));onActivated:{album.selected=album.members[currentIndex].id;album.track=album.members[currentIndex].track;campaign.world.setDraft("family/person",album.selected)} Accessible.name:qsTr("Family member")}
    Rectangle {
        objectName:"familyMemberCard";visible:!!album.person.id;Layout.fillWidth:true;implicitHeight:memberBody.implicitHeight+32;radius:16;color:"#354238";border.color:"#b6b184"
        ColumnLayout {id:memberBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
            RowLayout {Layout.fillWidth:true
                EmployeePortrait {Layout.preferredWidth:80;Layout.preferredHeight:80;portraitSource:album.person.portrait || "";personName:album.person.name || "";reducedMotion:campaign.reducedMotion;onClicked:{profile.person=album.person;profile.open()}}
                ColumnLayout {Layout.fillWidth:true
                    Copy {objectName:"familyMemberName";text:album.person.name || "";font.bold:true;color:"#f0d79e"}
                    Copy {objectName:"familyMemberAge";text:qsTr("Age %1 · %2").arg(album.person.age || 0).arg(album.person.role || "")}
                }
            }
            Copy {text:album.person.detail || ""}
            Copy {text:album.person.parents || qsTr("Your parent · part of your birth family.")}
            Copy {text:album.person.age<6?qsTr("Room to play, time to grow. Practical learning paths open at school age."):qsTr("Learning path: %1").arg(album.person.trackName || "")}
            Copy {text:album.person.skills || "";color:"#c0d2c0"}
            ColumnLayout {visible:!!album.person.care && album.person.age>=6 && album.person.age<18;Layout.fillWidth:true;spacing:10
                Flow {Layout.fillWidth:true;spacing:8
                    Repeater {model:album.tracks
                        WorldButton {required property var modelData;objectName:"familyTrack_"+modelData.id;text:modelData.name;textSize:album.textSize;checked:album.track===modelData.id;onClicked:album.track=modelData.id}
                    }
                }
                Copy {text:album.learning.blocker || album.learning.explanation || "";color:"#cbd8bd"}
                WorldButton {objectName:"familyLearningReview";text:qsTr("Explore this interest together");textSize:album.textSize;enabled:album.editable && !!album.learning.valid;onClicked:album.review({action:"track",child:album.person.id,track:album.track},qsTr("Explore a learning path?"),album.learning)}
            }
            ColumnLayout {visible:!!album.person.foster;Layout.fillWidth:true
                Copy {text:album.adoption.blocker || album.adoption.explanation || ""}
                WorldButton {objectName:"familyPlacementAdopt";text:qsTr("Discuss an adoption review");textSize:album.textSize;enabled:album.editable && !!album.adoption.valid;onClicked:album.review({action:"adopt_placement",child:album.person.id,withPartner:album.withPartner},qsTr("Request an adoption review?"),album.adoption)}
            }
            ColumnLayout {visible:album.person.age>=18;Layout.fillWidth:true
                Copy {objectName:"familyApplication";text:album.person.application || album.application.blocker || album.application.explanation || ""}
                WorldButton {objectName:"familyApplicationReview";visible:!album.person.application;text:qsTr("Invite an application");textSize:album.textSize;enabled:album.editable && !!album.application.valid;onClicked:album.review({action:"apply",child:album.person.id},qsTr("Let them know we're hiring?"),album.application)}
                WorldButton {visible:!!album.person.application;text:qsTr("Open recruitment");textSize:album.textSize;onClicked:album.recruitmentRequested()}
            }
        }
    }
    Copy {text:qsTr("Family connections");font.bold:true}
    Repeater {model:album.family.edges || [];Copy {required property var modelData;text:modelData.names+" · "+modelData.label}}
    GameDialog {
        id:confirmation;objectName:"familyReviewDialog";property var terms:({});property string revision:"";property string heading:"";property string copy:""
        title:heading;anchors.centerIn:Overlay.overlay;width:Math.min(album.textSize*37,Overlay.overlay.width-32);height:Math.min(album.textSize*25,Overlay.overlay.height-32);modal:true;focus:true
        contentItem:ScrollView {id:reviewScroll;contentWidth:availableWidth;clip:true;Label {width:reviewScroll.availableWidth;text:confirmation.copy;wrapMode:Text.Wrap;font.pixelSize:album.textSize}}
        footer:Flow {spacing:8;padding:10
            WorldButton {text:qsTr("Keep thinking");textSize:album.textSize;onClicked:confirmation.close()}
            WorldButton {objectName:"familyConfirm";text:qsTr("Have the conversation");textSize:album.textSize;enabled:album.editable;onClicked:{campaign.world.manageFamily(confirmation.terms,confirmation.revision);confirmation.close()}}
        }
    }
    PersonProfileDialog {id:profile;textSize:album.textSize}
}
