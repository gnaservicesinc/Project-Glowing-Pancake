import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:romance
    property int textSize:14
    property var contact:({})
    signal inspectPerson(var person)
    readonly property var personal:campaign.world.romance
    readonly property var story:{const revision=personal.revision;return campaign.world.romanceDetails(contact.id || "")}
    readonly property bool editable:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    property bool expanded:false
    property string plannedAction:"ask"
    property string plannedTitle:""
    readonly property var titles:({ask:qsTr("Ask on a date"),walk:qsTr("Walk together"),listen:qsTr("A good call"),hobby:qsTr("Share a hobby"),engage:qsTr("Talk about engagement"),marry:qsTr("Exchange vows"),separate:qsTr("End our relationship")})
    function plan(action){plannedAction=action;plannedTitle=titles[action];review.open()}
    spacing:12;visible:!!personal.available
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:romance.textSize;color:"#e4dadd"}
    Flow {
        Layout.fillWidth:true;spacing:8
        WorldButton {objectName:"romanceToggle";text:romance.expanded?qsTr("Fold this chapter away"):qsTr("♡ A personal chapter");textSize:romance.textSize;enabled:!!romance.contact.id;onClicked:romance.expanded=!romance.expanded}
        WorldButton {objectName:"romancePreferences";text:qsTr("My romantic preferences");textSize:romance.textSize;enabled:romance.editable;onClicked:preferences.open()}
    }
    Rectangle {
        objectName:"romanceBoard";visible:romance.expanded && !!romance.contact.id;Layout.fillWidth:true;implicitHeight:body.implicitHeight+32;radius:16;color:"#3c303e";border.color:"#bd94a3"
        ColumnLayout {
            id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
            RowLayout {
                Layout.fillWidth:true
                EmployeePortrait {Layout.preferredWidth:56;Layout.preferredHeight:56;portraitSource:campaign.world.founder.portrait || "";personName:campaign.world.founder.name || "";reducedMotion:campaign.reducedMotion;onClicked:romance.inspectPerson(campaign.world.founder)}
                Copy {text:"♡";horizontalAlignment:Text.AlignHCenter;font.pixelSize:romance.textSize*2;color:"#edb5c2";Accessible.ignored:true}
                EmployeePortrait {Layout.preferredWidth:56;Layout.preferredHeight:56;portraitSource:romance.contact.portrait || "";personName:romance.contact.name || "";reducedMotion:campaign.reducedMotion;onClicked:romance.inspectPerson(romance.contact)}
            }
            Copy {objectName:"romanceStatus";text:(romance.contact.name || "")+" · "+(romance.story.status || "");font.bold:true;font.pixelSize:romance.textSize*1.2;color:"#f3c7d0"}
            Copy {text:qsTr("A little time, a little courage. You both choose what comes next.")}
            Copy {objectName:"romanceIdentity";text:romance.story.identity || "";color:"#c8bccb"}
            Copy {objectName:"romanceReply";visible:!!romance.story.reply;text:romance.story.reply || "";font.bold:true;color:"#f4d39d"}
            Copy {visible:!!romance.story.memory;text:romance.story.memory || "";font.italic:true}
            ColumnLayout {
                visible:!!romance.story.together;Layout.fillWidth:true;spacing:8
                Copy {text:qsTr("Together since %1 · %2 dates · closeness %3 / 1000").arg(romance.story.since || "").arg(romance.story.dates || 0).arg(romance.story.affection || 0)}
                Rectangle {Layout.fillWidth:true;implicitHeight:12;radius:6;color:"#251f2c";Rectangle {height:parent.height;width:parent.width*(romance.story.affection || 0)/1000;radius:6;color:romance.story.strained?"#daa66c":"#dba9bf";Behavior on width {NumberAnimation {duration:campaign.reducedMotion?0:250}}} Accessible.role:Accessible.ProgressBar;Accessible.name:qsTr("Romantic closeness %1 of 1000").arg(romance.story.affection || 0)}
                Copy {text:romance.story.milestone || "";color:"#dcc9ab"}
                Copy {visible:!!romance.story.strained;text:qsTr("Time to reconnect? You have time to make space for each other before sustained strain ends the relationship.");color:"#ffd1a0"}
            }
            Flow {
                Layout.fillWidth:true;spacing:8
                Repeater {
                    model:romance.story.together?["walk","listen","hobby"]:["ask"]
                    WorldButton {
                        required property string modelData
                        objectName:"romance_"+modelData;text:romance.titles[modelData];textSize:romance.textSize
                        visible:modelData!=="engage" || romance.story.stage==="dating"
                        enabled:romance.editable && !(romance.story[modelData] || {}).blocker
                        ToolTip.visible:hovered && !!(romance.story[modelData] || {}).blocker;ToolTip.text:(romance.story[modelData] || {}).blocker || ""
                        onClicked:romance.plan(modelData)
                    }
                }
            }
            Flow {
                visible:!!romance.story.together;Layout.fillWidth:true;spacing:8
                WorldButton {objectName:"romance_engage";visible:romance.story.stage==="dating";text:romance.titles.engage;textSize:romance.textSize;enabled:romance.editable && !(romance.story.engage || {}).blocker;onClicked:romance.plan("engage")}
                WorldButton {objectName:"romance_marry";visible:romance.story.stage==="engaged";text:romance.titles.marry;textSize:romance.textSize;enabled:romance.editable && !(romance.story.marry || {}).blocker;onClicked:romance.plan("marry")}
                WorldButton {objectName:"romance_separate";text:romance.titles.separate;textSize:romance.textSize;enabled:romance.editable && !(romance.story.separate || {}).blocker;onClicked:romance.plan("separate")}
            }
            Copy {objectName:"romanceAvailability";text:romance.story.together?((romance.story.listen || {}).blocker || qsTr("Free time together. One intentional personal plan per day; dates once a week.")):((romance.story.ask || {}).blocker || qsTr("You can ask. Their answer is theirs to give."));color:"#d6c9b3"}
            Copy {visible:romance.story.stage==="dating" || romance.story.stage==="engaged";text:romance.story.stage==="dating"?((romance.story.engage || {}).blocker || qsTr("Ready to discuss engagement.")):((romance.story.marry || {}).blocker || qsTr("Ready to discuss marriage."));color:"#c9bdcc"}
        }
    }
    GameDialog {
        id:review;objectName:"romanceReviewDialog";property string person:"";property string action:"";property string revision:"";property string explanation:""
        title:romance.plannedTitle;anchors.centerIn:Overlay.overlay;width:Math.min(romance.textSize*36,Overlay.overlay.width-32);height:Math.min(romance.textSize*20,Overlay.overlay.height-32);modal:true;focus:true
        onAboutToShow:{person=romance.contact.id;action=romance.plannedAction;revision=campaign.world.reviewToken;explanation=(romance.story[action] || {}).explanation || ""}
        contentItem:ScrollView {id:reviewScroll;contentWidth:availableWidth;clip:true;Label {width:reviewScroll.availableWidth;text:(romance.contact.name || "")+"\n\n"+review.explanation;wrapMode:Text.Wrap;font.pixelSize:romance.textSize}}
        footer:Flow {spacing:8;padding:10
            WorldButton {text:qsTr("Give it some thought");textSize:romance.textSize;onClicked:review.close()}
            WorldButton {objectName:"romanceConfirm";text:review.action==="separate"?qsTr("Have the conversation"):qsTr("Make the invitation");textSize:romance.textSize;enabled:romance.editable;onClicked:{campaign.world.pursueRomance(review.person,review.action,review.revision);review.close()}}
        }
    }
    GameDialog {
        id:preferences;objectName:"romancePreferencesDialog";property bool interested:true;property var genders:[];property string revision:""
        title:qsTr("My romantic preferences");anchors.centerIn:Overlay.overlay;width:Math.min(romance.textSize*38,Overlay.overlay.width-32);height:Math.min(romance.textSize*30,Overlay.overlay.height-32);modal:true;focus:true
        onAboutToShow:{interested=romance.personal.interested;genders=(romance.personal.genders || []).slice();revision=campaign.world.reviewToken}
        function toggle(id,on){genders=on?genders.concat([id]):genders.filter(g=>g!==id)}
        contentItem:ScrollView {id:prefsScroll;contentWidth:availableWidth;clip:true;ColumnLayout {width:prefsScroll.availableWidth;spacing:12
            Label {Layout.fillWidth:true;text:qsTr("Romantic attraction is separate from sexual orientation.");wrapMode:Text.Wrap;font.pixelSize:romance.textSize}
            GameCheck {objectName:"romanceInterested";Layout.fillWidth:true;text:qsTr("I'm open to romance");checked:preferences.interested;onToggled:preferences.interested=checked;textSize:romance.textSize}
            Label {Layout.fillWidth:true;text:qsTr("I'm romantically interested in:");wrapMode:Text.Wrap;font.pixelSize:romance.textSize}
            Flow {Layout.fillWidth:true;spacing:8
            Repeater {model:[{id:"woman",name:qsTr("Women")},{id:"man",name:qsTr("Men")},{id:"nonbinary",name:qsTr("Nonbinary people")}]
                GameCheck {required property var modelData;objectName:"romancePreference_"+modelData.id;text:modelData.name;checked:preferences.genders.includes(modelData.id);onToggled:preferences.toggle(modelData.id,checked);textSize:romance.textSize}
            }
            }
            Label {visible:preferences.genders.length===0;Layout.fillWidth:true;text:qsTr("Keep at least one attraction preference saved. You can turn off seeking romance.");wrapMode:Text.Wrap;font.pixelSize:romance.textSize}
        }}
        footer:Flow {spacing:8;padding:10
            WorldButton {text:qsTr("Keep current");textSize:romance.textSize;onClicked:preferences.close()}
            WorldButton {objectName:"romancePreferencesSave";text:qsTr("Save");textSize:romance.textSize;enabled:romance.editable && preferences.genders.length>0;onClicked:{campaign.world.setRomanticPreferences(preferences.interested,preferences.genders,preferences.revision);preferences.close()}}
        }
    }
}
