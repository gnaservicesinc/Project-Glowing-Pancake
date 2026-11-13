import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id:panel
    property int textSize:14
    readonly property var circle:campaign.world.relationships
    readonly property var people:circle.people || []
    readonly property var network:circle.nodes || []
    property string selected:campaign.world.draft("social/person","")
    property string focusPerson:"founder"
    property int page:0
    property bool showNetwork:false
    readonly property var person:people.find(p=>p.id===selected) || people[0] || ({})
    readonly property var connections:(circle.edges || []).filter(e=>e.first===focusPerson || e.second===focusPerson)
    readonly property var visibleConnections:connections.slice(page*8,page*8+8)
    readonly property var center:network.find(n=>n.id===focusPerson) || ({})
    readonly property bool editable:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    function choose(id){selected=id;campaign.world.setDraft("social/person",id)}
    function focusOn(id){focusPerson=id;page=0}
    function neighbor(edge){return network.find(n=>n.id===(edge.first===focusPerson?edge.second:edge.first)) || ({})}
    spacing:14
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#dbe4e3"}
    Copy {text:qsTr("Your social circle");font.pixelSize:panel.textSize*1.5;font.bold:true}
    Copy {text:qsTr("A coworker can also be a friend. Nearby labs make it easier to meet and keep in touch; distant friendships need more effort. Make time, discover shared interests, and grow your circle. There is room for more than one best friend.")}
    Flow {
        Layout.fillWidth:true;spacing:8
        WorldButton {objectName:"socialPeople";text:qsTr("Spend time together");textSize:panel.textSize;checked:!panel.showNetwork;onClicked:panel.showNetwork=false}
        WorldButton {objectName:"socialNetwork";text:qsTr("Close connections");textSize:panel.textSize;checked:panel.showNetwork;onClicked:panel.showNetwork=true}
    }
    MissionReview {
        Layout.fillWidth: true
        visible: !panel.showNetwork && mission.length > 0
        mission: panel.circle.mission || ""
        editable: panel.editable
        textSize: panel.textSize
    }
    RomancePanel {Layout.fillWidth:true;textSize:panel.textSize;contact:panel.person;visible:!panel.showNetwork && !panel.person.rival && !!campaign.world.romance.available;onInspectPerson:person=>{profile.person=person;profile.open()}}
    ColumnLayout {
        visible:!panel.showNetwork;Layout.fillWidth:true;spacing:12
        Copy {visible:panel.people.length===0;text:qsTr("Your circle starts with the people you meet. Bring someone onto the HQ team to begin.")}
        GameSelect {objectName:"socialPerson";Layout.fillWidth:true;textSize:panel.textSize;visible:panel.people.length>0;model:panel.people;textRole:"name";currentIndex:Math.max(0,panel.people.findIndex(p=>p.id===panel.person.id));onActivated:panel.choose(panel.people[currentIndex].id);Accessible.name:qsTr("Person to spend time with")}
        Rectangle {
            objectName:"socialCard";visible:!!panel.person.id;Layout.fillWidth:true;implicitHeight:body.implicitHeight+32;color:"#293e44";radius:16;border.color:"#668780"
            ColumnLayout {
                id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:12
                RowLayout {
                    Layout.fillWidth:true
                    EmployeePortrait {portraitSource:panel.person.portrait || "";personName:panel.person.name || "";Layout.preferredWidth:80;Layout.preferredHeight:80;reducedMotion:campaign.reducedMotion;onClicked:{profile.person=panel.person;profile.open()}}
                    ColumnLayout {
                        Layout.fillWidth:true
                        Copy {text:panel.person.name || "";font.bold:true}
                        Copy {objectName:"socialRelationship";text:(panel.person.relationship || "")+(panel.person.coworker?qsTr(" · Coworker"):panel.person.external?qsTr(" · Industry contact"):qsTr(" · Former coworker"));color:"#e2cc8d"}
                    }
                }
                MissionConcern {
                    Layout.fillWidth: true
                    person: panel.person
                    mediators: panel.circle.mediators || []
                    textSize: panel.textSize
                    editable: panel.editable
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !!panel.person.rival && !panel.person.concern
                    spacing: 8
                    Copy {visible:!!panel.person.organization;text:panel.person.organization || "";color:"#c0d8d2"}
                    Copy {visible:!!panel.person.memory;text:panel.person.memory || "";font.italic:true;color:"#e2c7bd"}
                    Copy {visible:!!panel.person.blocker;text:panel.person.blocker || "";color:"#d7b2a6"}
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !panel.person.rival
                    spacing: 12
                    Copy {visible:!!panel.person.organization;text:panel.person.organization || "";color:"#c0d8d2"}
                    Copy {visible:!!panel.person.distance;text:panel.person.distance==="city"?qsTr("Same city · easy to make time together"):panel.person.distance==="state"?qsTr("Same state or province · regional conversations"):qsTr("Online contact · slow growth; established ties take effort");color:"#c0d8d2"}
                    Copy {text:panel.person.memory || "";font.italic:true}
                    Rectangle {
                        Layout.fillWidth:true;implicitHeight:12;radius:6;color:"#172b32"
                        Rectangle {height:parent.height;width:parent.width*(panel.person.affinity || 0)/1000;radius:6;color:"#d8b776";Behavior on width {NumberAnimation {duration:campaign.reducedMotion?0:250}}}
                        Accessible.role:Accessible.ProgressBar;Accessible.name:qsTr("Friendship strength %1 of 1000").arg(panel.person.affinity || 0)
                    }
                    Copy {text:panel.person.next>0?qsTr("Friendship %1 / 1000 · %2 for the next milestone").arg(panel.person.affinity || 0).arg(panel.person.next):qsTr("Best friends · keep making memories together")}
                    Flow {
                        Layout.fillWidth:true;spacing:8
                        WorldButton {objectName:"socialChat";text:qsTr("Take a break together");textSize:panel.textSize;enabled:panel.editable && !panel.person.blocker;onClicked:campaign.world.socialize(panel.person.id,"chat",campaign.world.reviewToken)}
                        WorldButton {objectName:"socialListen";text:qsTr("Ask, then listen");textSize:panel.textSize;enabled:panel.editable && !panel.person.blocker;onClicked:campaign.world.socialize(panel.person.id,"listen",campaign.world.reviewToken)}
                        WorldButton {objectName:"socialHobby";text:qsTr("Explore an interest");textSize:panel.textSize;enabled:panel.editable && !panel.person.blocker;onClicked:campaign.world.socialize(panel.person.id,"hobby",campaign.world.reviewToken)}
                    }
                    Flow {
                        Layout.fillWidth:true;spacing:8
                        WorldButton {objectName:"socialIntroduction";text:qsTr("Ask for an introduction");textSize:panel.textSize;enabled:panel.editable && !panel.person.blocker && !!panel.person.introduction;onClicked:campaign.world.socialize(panel.person.id,"introduce",campaign.world.reviewToken)}
                        WorldButton {objectName:"socialReferral";text:qsTr("Ask for a recruiting referral");textSize:panel.textSize;enabled:panel.editable && !panel.person.blocker && !panel.person.referralBlocker;onClicked:campaign.world.socialize(panel.person.id,"referral",campaign.world.reviewToken)}
                    }
                    Copy {visible:!panel.person.introduction;text:qsTr("Close friends may introduce you to their own close connections as those relationships develop.");color:"#b5c6c5"}
                    Copy {visible:!!panel.person.referralBlocker;text:panel.person.referralBlocker || "";color:"#c6d5ba"}
                    Copy {objectName:"socialAvailability";text:panel.person.blocker || qsTr("One intentional meetup per day; reconnect with this person after %1 days. No fee for spending time together.").arg(panel.circle.interval || 7);color:"#c7d5b3"}
                    Copy {text:qsTr("What you know");font.bold:true}
                    Copy {text:panel.person.traits || qsTr("You are still getting to know one another.")}
                    Copy {text:qsTr("Pronouns and professional skills are public. Other traits are shared through contact and trust; outgoing people tend to open up sooner. What they have shared stays known.");color:"#b6c6ca"}
                    Copy {text:qsTr("Friends at work: +%1 morale (up to +%2). Close friends contribute more.").arg(panel.person.moodBonus || 0).arg(panel.circle.moodCap || 0);color:"#d9c68f"}
                }
                ResearchJam {Layout.fillWidth:true;textSize:panel.textSize;contact:panel.person;onInspectPerson:person=>{profile.person=person;profile.open()}}
            }
        }
    }
    ColumnLayout {
        visible:panel.showNetwork;Layout.fillWidth:true;spacing:12
        Copy {text:qsTr("Known close friendships, shared romantic relationships and family links appear here. Friends may tell you about their own close connections as you get to know both people.")}
        WorldButton {objectName:"socialGraphHome";visible:panel.focusPerson!=="founder";text:qsTr("Back to my connections");textSize:panel.textSize;onClicked:panel.focusOn("founder")}
        GameSelect {objectName:"socialGraphFocus";Layout.fillWidth:true;textSize:panel.textSize;model:panel.network;textRole:"name";currentIndex:Math.max(0,panel.network.findIndex(n=>n.id===panel.focusPerson));onActivated:panel.focusOn(panel.network[currentIndex].id);Accessible.name:qsTr("Center the relationship map on")}
        Copy {visible:panel.connections.length===0;text:qsTr("No known close connections here yet. Little conversations are how it starts.")}
        Rectangle {
            id:graph;objectName:"socialGraph";Layout.fillWidth:true;implicitHeight:Math.max(430,panel.textSize*25);radius:16;color:"#192f39";border.color:"#63837e";visible:panel.width>=Math.max(560,panel.textSize*45) && panel.connections.length>0
            readonly property real cx:width/2
            readonly property real cy:height/2
            readonly property real rx:width*0.34
            readonly property real ry:height*0.34
            function point(i){const angle=2*Math.PI*i/Math.max(1,panel.visibleConnections.length)-Math.PI/2;return {x:cx+rx*Math.cos(angle),y:cy+ry*Math.sin(angle)}}
            Canvas {
                id:graphLines
                anchors.fill:parent
                onPaint:{const ctx=getContext("2d");ctx.reset();ctx.strokeStyle="#a9c0aa";ctx.lineWidth=2;for(let i=0;i<panel.visibleConnections.length;i++){const p=graph.point(i);ctx.beginPath();ctx.moveTo(graph.cx,graph.cy);ctx.lineTo(p.x,p.y);ctx.stroke()}}
                Connections {target:panel;function onVisibleConnectionsChanged(){graphLines.requestPaint()}}
                onWidthChanged:requestPaint();onHeightChanged:requestPaint()
            }
            Rectangle {
                x:graph.cx-width/2;y:graph.cy-height/2;width:Math.min(graph.width*0.28,190);height:centerBody.implicitHeight+20;radius:16;color:"#324840";border.color:"#d9bc78"
                ColumnLayout {id:centerBody;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:10;spacing:6
                    EmployeePortrait {Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:56;Layout.preferredHeight:56;portraitSource:panel.center.portrait || "";personName:panel.center.name || "";reducedMotion:campaign.reducedMotion;onClicked:{profile.person=panel.center;profile.open()}}
                    Label {Layout.fillWidth:true;text:panel.center.name || "";wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;font.pixelSize:panel.textSize;font.bold:true;color:"#f1d693"}
                }
            }
            Repeater {
                model:panel.visibleConnections
                WorldButton {
                    id:connection;required property var modelData;required property int index;objectName:"socialConnection_"+index
                    x:graph.point(index).x-width/2;y:graph.point(index).y-height/2;width:Math.min(graph.width*0.27,210);height:Math.max(90,panel.textSize*5)
                    text:panel.neighbor(modelData).name || "";textSize:panel.textSize;onClicked:panel.focusOn(panel.neighbor(modelData).id)
                    Accessible.name:qsTr("Explore %1, %2").arg(text).arg(modelData.label)
                    contentItem:RowLayout {spacing:8
                        Image {Layout.preferredWidth:44;Layout.preferredHeight:44;source:panel.neighbor(connection.modelData).portrait || "";fillMode:Image.PreserveAspectFit;Accessible.ignored:true}
                        ColumnLayout {Layout.fillWidth:true
                            Label {Layout.fillWidth:true;text:connection.text;wrapMode:Text.Wrap;font.pixelSize:panel.textSize;color:"#e5ece6"}
                            Label {Layout.fillWidth:true;text:"★ "+connection.modelData.label;wrapMode:Text.Wrap;font.pixelSize:panel.textSize*0.85;color:"#e0c783"}
                        }
                    }
                }
            }
        }
        Flow {
            visible:panel.connections.length>8;Layout.fillWidth:true;spacing:8
            WorldButton {text:qsTr("Previous connections");textSize:panel.textSize;enabled:panel.page>0;onClicked:panel.page--}
            WorldButton {text:qsTr("Next connections");textSize:panel.textSize;enabled:(panel.page+1)*8<panel.connections.length;onClicked:panel.page++}
        }
        Copy {visible:panel.connections.length>0;text:qsTr("Connections list");font.bold:true}
        Repeater {
            model:panel.visibleConnections
            ColumnLayout {
                required property var modelData;Layout.fillWidth:true
                Copy {text:modelData.names+" · "+modelData.label}
                WorldButton {text:qsTr("Explore %1").arg(panel.neighbor(modelData).name || "");textSize:panel.textSize;onClicked:panel.focusOn(panel.neighbor(modelData).id)}
            }
        }
    }
    PersonProfileDialog {id:profile;textSize:panel.textSize}
}
