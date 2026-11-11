import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:jam
    property int textSize:14
    property var contact:({})
    signal inspectPerson(var person)
    readonly property var catalogue:campaign.world.researchJams
    readonly property var topics:catalogue.topics || []
    readonly property var workers:catalogue.workers || []
    readonly property var sources:catalogue.sources || []
    property bool expanded:false
    property string topic:campaign.world.draft("social/jam/topic","research.reproducible_tooling")
    property string worker:campaign.world.draft("social/jam/worker","")
    property string source:campaign.world.draft("social/jam/source","")
    property string rate:campaign.world.draft("social/jam/rate","4")
    readonly property var draft:({contact:contact.id || "",topic:topic,worker:worker,source:source,rate:rate})
    readonly property var preview:{const revision=catalogue.revision;return campaign.world.collaborationQuote(draft)}
    readonly property var agreements:(catalogue.agreements || []).filter(a=>a.contact===jam.contact.id)
    readonly property bool editable:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    spacing:12
    visible:!!contact.external && topics.length>0
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:jam.textSize;color:"#dbe4e3"}
    WorldButton {objectName:"researchJamToggle";text:jam.expanded?qsTr("Pack up the planning board"):qsTr("✦ Plan a research jam");textSize:jam.textSize;onClicked:jam.expanded=!jam.expanded}
    ColumnLayout {
        visible:jam.expanded;Layout.fillWidth:true;spacing:12
        Copy {text:qsTr("Two labs. One good idea.");font.bold:true;font.pixelSize:jam.textSize*1.25;color:"#e7ce8e"}
        Copy {text:qsTr("Trusted connections can bring labs together. Each side commits a real researcher and compute, pays its costs and keeps its results. Both must finish before you can publish together.")}
        Rectangle {
            objectName:"researchJamBoard";Layout.fillWidth:true;implicitHeight:boardBody.implicitHeight+28;radius:14;color:"#1d3439";border.color:"#72958a"
            RowLayout {
                id:boardBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:12
                EmployeePortrait {objectName:"researchJamFounder";Layout.preferredWidth:48;Layout.preferredHeight:48;portraitSource:campaign.world.founder.portrait || "";personName:campaign.world.founder.name || qsTr("Your founder");reducedMotion:campaign.reducedMotion;onClicked:jam.inspectPerson(campaign.world.founder)}
                Label {Layout.fillWidth:true;text:qsTr("Your lab");wrapMode:Text.Wrap;font.pixelSize:jam.textSize;font.bold:true;color:"#c5d9da"}
                Label {text:"✦";font.pixelSize:jam.textSize*1.8;color:"#e2bd6b";Accessible.ignored:true}
                ColumnLayout {
                    Layout.fillWidth:true
                    Label {Layout.fillWidth:true;text:jam.contact.name || "";wrapMode:Text.Wrap;font.pixelSize:jam.textSize;font.bold:true;color:"#e5d7b2"}
                    Label {Layout.fillWidth:true;text:jam.contact.organization || "";wrapMode:Text.Wrap;font.pixelSize:jam.textSize*0.85;color:"#b8cfca"}
                }
                EmployeePortrait {objectName:"researchJamPartner";Layout.preferredWidth:48;Layout.preferredHeight:48;portraitSource:jam.contact.portrait || "";personName:jam.contact.name || "";reducedMotion:campaign.reducedMotion;onClicked:jam.inspectPerson(jam.contact)}
            }
        }
        GridLayout {
            Layout.fillWidth:true;columns:jam.width>=jam.textSize*45?2:1;columnSpacing:14;rowSpacing:12
            ColumnLayout {
                Layout.fillWidth:true
                Copy {text:qsTr("The idea")}
                GameSelect {objectName:"researchJamTopic";Layout.fillWidth:true;textSize:jam.textSize;model:jam.topics;textRole:"name";currentIndex:Math.max(0,jam.topics.findIndex(t=>t.id===jam.topic));onActivated:{jam.topic=jam.topics[currentIndex].id;campaign.world.setDraft("social/jam/topic",jam.topic)} Accessible.name:qsTr("Joint research topic")}
            }
            ColumnLayout {
                Layout.fillWidth:true
                Copy {text:qsTr("Your teammate")}
                GameSelect {objectName:"researchJamWorker";Layout.fillWidth:true;textSize:jam.textSize;model:jam.workers;textRole:"name";currentIndex:Math.max(0,jam.workers.findIndex(p=>p.id===jam.worker));onActivated:{jam.worker=jam.workers[currentIndex].id;campaign.world.setDraft("social/jam/worker",jam.worker)} Accessible.name:qsTr("Your research teammate")}
                Copy {text:(jam.workers.find(p=>p.id===jam.worker) || {}).detail || "";color:"#b7cbc5"}
            }
            ColumnLayout {
                Layout.fillWidth:true
                Copy {text:qsTr("Your compute")}
                GameSelect {objectName:"researchJamSource";Layout.fillWidth:true;textSize:jam.textSize;model:jam.sources;textRole:"name";currentIndex:Math.max(0,jam.sources.findIndex(s=>s.id===jam.source));onActivated:{jam.source=jam.sources[currentIndex].id;campaign.world.setDraft("social/jam/source",jam.source)} Accessible.name:qsTr("Joint research compute source")}
            }
            ColumnLayout {
                Layout.fillWidth:true
                Copy {text:qsTr("Compute per lab · CU/day")}
                TextField {objectName:"researchJamRate";Layout.fillWidth:true;text:jam.rate;font.pixelSize:jam.textSize;onTextEdited:{jam.rate=text;campaign.world.setDraft("social/jam/rate",text)} Accessible.name:qsTr("Joint research compute in CU per day")}
            }
        }
        Copy {objectName:"researchJamQuote";text:jam.preview.valid?jam.preview.summary:(jam.preview.blocker || qsTr("Choose a topic and teammate."));color:jam.preview.valid?"#d6dcac":"#e2c78d"}
        Copy {text:qsTr("Research time, used compute and paid fees are not refunded if stopped. If one side cancels, the other lab may continue its own research; a joint paper needs both completed results.");color:"#b9c9ce"}
        WorldButton {objectName:"researchJamReview";text:qsTr("Review the research pact");textSize:jam.textSize;enabled:jam.editable && !!jam.preview.valid;onClicked:review.open()}
    }
    Repeater {
        model:jam.agreements
        Rectangle {
            required property var modelData
            Layout.fillWidth:true;implicitHeight:resultBody.implicitHeight+28;radius:14;color:"#243b3c";border.color:"#829680"
            ColumnLayout {
                id:resultBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14;spacing:10
                Copy {text:modelData.name;font.bold:true}
                Copy {objectName:"researchJamStatus_"+modelData.id;text:modelData.status;color:"#e5ca86"}
                Rectangle {Layout.fillWidth:true;implicitHeight:12;radius:6;color:"#172b32";Rectangle {height:parent.height;width:parent.width*modelData.progress/100;radius:6;color:modelData.canPublish?"#d8ba75":"#83b9b4";Behavior on width {NumberAnimation {duration:campaign.reducedMotion?0:250}}} Accessible.role:Accessible.ProgressBar;Accessible.name:qsTr("Joint research %1 percent complete").arg(modelData.progress)}
                Copy {text:modelData.detail}
                Flow {
                    Layout.fillWidth:true;spacing:8
                    WorldButton {text:qsTr("Manage my research");textSize:jam.textSize;onClicked:campaign.world.navigate("workspace","hq","laptop","lab","")}
                    WorldButton {objectName:"researchJamPublish_"+modelData.id;text:qsTr("Publish together");textSize:jam.textSize;visible:modelData.canPublish;enabled:jam.editable;onClicked:{publication.agreement=modelData.id;publication.open()}}
                }
            }
        }
    }
    GameDialog {
        id:review;objectName:"researchJamReviewDialog";property var terms:({});property string revision:"";property string summary:""
        anchors.centerIn:Overlay.overlay;width:Math.min(jam.textSize*38,Overlay.overlay.width-32);height:Math.min(jam.textSize*25,Overlay.overlay.height-32);modal:true;focus:true;title:qsTr("Make this research pact?")
        onAboutToShow:{terms=Object.assign({},jam.draft);revision=campaign.world.reviewToken;summary=jam.preview.partner+"\n\n"+jam.preview.summary+"\n\n"+qsTr("Both labs retain their results and agree to joint publication once both finish. Costs and work already used are not refundable.")}
        contentItem:ScrollView {id:reviewScroll;contentWidth:availableWidth;clip:true;Label {width:reviewScroll.availableWidth;text:review.summary;wrapMode:Text.Wrap;font.pixelSize:jam.textSize}}
        footer:Flow {spacing:8;padding:10
            WorldButton {text:qsTr("Keep planning");textSize:jam.textSize;onClicked:review.close()}
            WorldButton {objectName:"researchJamConfirm";text:qsTr("Sign pact and start both teams");textSize:jam.textSize;enabled:jam.editable;onClicked:{campaign.world.collaborateResearch(review.terms,review.revision);review.close()}}
        }
    }
    GameDialog {
        id:publication;property string agreement:"";property string revision:"";title:qsTr("Share the joint discovery?");modal:true;focus:true
        anchors.centerIn:Overlay.overlay;width:Math.min(jam.textSize*35,Overlay.overlay.width-32);height:Math.min(jam.textSize*18,Overlay.overlay.height-32)
        onAboutToShow:revision=campaign.world.reviewToken
        contentItem:ScrollView {id:paperScroll;contentWidth:availableWidth;clip:true;Label {width:paperScroll.availableWidth;text:qsTr("Both labs will publish this completed result. Other labs can learn from the public research under the ordinary adoption rules. There is no publication bonus payment.");wrapMode:Text.Wrap;font.pixelSize:jam.textSize}}
        footer:Flow {spacing:8;padding:10
            WorldButton {text:qsTr("Keep it between us");textSize:jam.textSize;onClicked:publication.close()}
            WorldButton {objectName:"researchJamPublishConfirm";text:qsTr("Publish the joint result");textSize:jam.textSize;enabled:jam.editable;onClicked:{campaign.world.publishCollaboration(publication.agreement,publication.revision);publication.close()}}
        }
    }
}
