import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id:home
    objectName:"homeStart"
    required property var story
    required property real textScale
    readonly property int typeSize:Math.round(16*textScale)
    readonly property bool rich:story.form.family==="rich" || story.form.leg==="political"
    readonly property bool eligibleCEO:["genius","political","wealthy"].includes(story.form.leg)
    property string app:story.form.home_app || "room"
    property string room:story.form.home_room || "Bedroom"
    property string message:""
    property bool signing:false
    property real signature:0
    property string pendingApp:"room"
    readonly property bool transitioning:pageTransition.running
    signal menuRequested()
    signal appearanceRequested()
    color:"#102830"
    function showApp(id) {app=id;story.setField("home_app",id);message="";homeScroll.contentItem.contentY=0}
    function openApp(id) {
        if(app===id)return
        signing=false
        pendingApp=id
        if(campaign.reducedMotion)showApp(id)
        else pageTransition.restart()
    }
    function prepare(route,legal) {
        story.setField("legal_form",legal);story.setField("route",route);story.setField("headquarters","US-CA")
        if(story.form.investment==="-1")story.setField("investment","0")
        openApp("formation")
    }
    function sign() {
        message=story.signingError()
        if(message)return
        signing=true;signature=0
        homeScroll.contentItem.contentY=0
        if(campaign.reducedMotion)signature=1
        else autograph.start()
    }
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:home.typeSize;color:"#c7dad4"}
    component Heading:Copy {font.pixelSize:home.typeSize*1.4;font.bold:true;color:"#f1e3bc"}
    NumberAnimation {id:autograph;target:home;property:"signature";from:0;to:1;duration:1800;easing.type:Easing.InOutSine}
    SequentialAnimation {
        id:pageTransition
        NumberAnimation {target:pages;property:"opacity";to:0;duration:120}
        ScriptAction {script:home.showApp(home.pendingApp)}
        NumberAnimation {target:pages;property:"opacity";to:1;duration:200}
    }
    Connections {
        target:home.Window.window
        function onActiveFocusItemChanged() {
            const item=home.Window.window.activeFocusItem
            const flick=homeScroll.contentItem
            if(!item || !flick)return
            let ancestor=item
            while(ancestor && ancestor!==home)ancestor=ancestor.parent
            if(!ancestor)return
            const y=item.mapToItem(flick.contentItem,0,0).y
            if(y<flick.contentY)flick.contentY=y
            else if(y+item.height>flick.contentY+flick.height)flick.contentY=Math.max(0,Math.min(flick.contentHeight-flick.height,y+item.height-flick.height))
        }
    }
    ScrollView {
        id:homeScroll;anchors.fill:parent;anchors.margins:Math.min(28,home.width/30);contentWidth:availableWidth;clip:true
        ScrollBar.vertical.policy:ScrollBar.AlwaysOn
    ColumnLayout {
        width:homeScroll.availableWidth;spacing:12
        RowLayout {
            Image {source:home.story.portrait;Layout.preferredWidth:64;Layout.preferredHeight:64}
            ColumnLayout {
                Layout.fillWidth:true
                Heading {text:home.story.form.first_name+" "+home.story.form.last_name}
                Copy {text:qsTr("January 2015 · Age 18 · Unemployed · No company");font.pixelSize:home.typeSize*0.8}
            }
            WorldButton {objectName:"homeMenu";text:qsTr("Menu");textSize:home.typeSize;onClicked:home.menuRequested()}
        }
        Copy {text:qsTr("Personal savings %1 · Company funds $0 · Silicon Valley").arg(home.story.personalFunds.total);color:"#b9dba7"}
        Flow {
            Layout.fillWidth:true;spacing:8
            Repeater {model:[{id:"room",name:qsTr("⌂ Home")},{id:"browser",name:qsTr("◉ Browser")},{id:"messages",name:qsTr("✉ Messages")},{id:"bank",name:qsTr("▤ Personal accounts")}]
                WorldButton {required property var modelData;objectName:"homeApp_"+modelData.id;text:modelData.name;textSize:home.typeSize;checked:home.app===modelData.id;onClicked:home.openApp(modelData.id)}
            }
            WorldButton {text:qsTr("Appearance");textSize:home.typeSize;onClicked:{home.signing=false;home.appearanceRequested()}}
        }
        Rectangle {
            visible:campaign.showGuide && home.story.form.home_guide!=="dismissed"
            Layout.fillWidth:true;implicitHeight:guide.implicitHeight+20;color:"#31524e";radius:10
            RowLayout {
                id:guide;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:10
                Copy {text:qsTr("Your computer is your way into the world. Open the browser to find a lawyer, pitch an investor or apply to a lab. Nothing is signed until you review the terms.")}
                WorldButton {text:qsTr("Dismiss");textSize:home.typeSize;onClicked:home.story.setField("home_guide","dismissed")}
            }
        }
        ColumnLayout {
            id:pages
            enabled:!home.transitioning
            Layout.fillWidth:true
            ColumnLayout {
                Layout.fillWidth:true;spacing:16
                ColumnLayout {
                    visible:home.app==="room";Layout.fillWidth:true
                    Heading {text:qsTr("%1 · %2").arg(home.story.familyHome.name || qsTr("Your parents’ house")).arg(home.room)}
                    Copy {text:home.rich?qsTr("A quiet house in the hills and people who return your family's calls. The advantages are real. What you do with them is still yours."):qsTr("Yesterday's birthday cards are still on your desk. Your parents have offered the garage, basement and shed while you find your feet. There is no rent, and no company yet.")}
                    Rectangle {
                        Layout.fillWidth:true;Layout.preferredHeight:Math.max(230,home.typeSize*12);color:"#9d9f8c";radius:8;clip:true
                        Rectangle {x:0;y:parent.height*0.58;width:parent.width;height:parent.height*0.42;color:"#786852"}
                        Rectangle {x:parent.width*0.08;y:28;width:parent.width*0.23;height:parent.height*0.35;color:"#84b6c4";border.width:9;border.color:"#e6d7b8"}
                        Rectangle {x:parent.width*0.07;y:parent.height*0.6;width:parent.width*0.35;height:parent.height*0.28;color:"#b0b9a5";radius:10;visible:home.room==="Bedroom"}
                        Rectangle {x:parent.width*0.5;y:parent.height*0.53;width:parent.width*0.42;height:parent.height*0.08;color:"#c19b6e"}
                        Image {x:parent.width*0.63;y:parent.height*0.21;width:parent.width*0.18;height:parent.height*0.36;source:"qrc:/pgp/assets/ui/laptop.svg";fillMode:Image.PreserveAspectFit}
                        MouseArea {x:parent.width*0.5;y:parent.height*0.18;width:parent.width*0.42;height:parent.height*0.55;cursorShape:Qt.PointingHandCursor;onClicked:home.openApp("browser")}
                        WorldButton {anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:12;objectName:"homeComputer";text:qsTr("Open your computer");textSize:home.typeSize;onClicked:home.openApp("browser")}
                    }
                    Flow {Layout.fillWidth:true;spacing:8;Repeater {model:["Bedroom"].concat(home.story.familyHome.roomNames || []);WorldButton {required property string modelData;text:modelData;textSize:home.typeSize;checked:home.room===modelData;onClicked:{home.room=modelData;home.story.setField("home_room",modelData)}}}}
                    Copy {text:qsTr("%1 free workspaces · %2 × %3 m in total · the family keeps a room for you when the company moves out.").arg((home.story.familyHome.rooms || []).length).arg(home.story.familyHome.width || 0).arg(home.story.familyHome.depth || 0)}
                    Copy {text:home.room==="Bedroom"?qsTr("Your childhood room, your personal computer, and the first morning of something new."):qsTr("Free workspace · available to your company after formation. Equipment arrives through suppliers; empty rooms provide no compute.")}
                }
                ColumnLayout {
                    visible:home.app==="browser";Layout.fillWidth:true
                    Heading {text:qsTr("Valley Directory")}
                    Copy {text:qsTr("◉  directory.local / starting-out")}
                    Repeater {
                        model:[{id:"legal",name:"Cedar & Quill · business law",detail:qsTr("Found a corporation or nonprofit. Review registration costs, ownership and your investment.")},{id:"labs",name:"Alder Research · careers",detail:qsTr("Apply to lead an existing laboratory. Your biography determines whether the board will consider you.")},{id:"investors",name:"Founders’ Exchange · investors",detail:qsTr("Meet potential partners, compare funding and keep track of who wants a say.")},{id:"bank",name:"Redwood Mutual · personal banking",detail:qsTr("Inspect savings and choose the money you can afford to put at risk.")}]
                        Rectangle {
                            required property var modelData
                            Layout.fillWidth:true;implicitHeight:service.implicitHeight+24;radius:12;color:"#24444a";border.color:"#567c76"
                            ColumnLayout {id:service;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;Heading {text:modelData.name} Copy {text:modelData.detail} WorldButton {objectName:"homeService_"+modelData.id;text:qsTr("Visit website →");textSize:home.typeSize;onClicked:home.openApp(modelData.id)}}
                        }
                    }
                }
                ColumnLayout {
                    visible:home.app==="legal";Layout.fillWidth:true
                    Heading {text:qsTr("Cedar & Quill · Start something")}
                    Copy {text:qsTr("The lawyer lays out two paths. A corporation can issue shares. A nonprofit holds its assets for its mission. You will review the complete cost and funding before signing.")}
                    WorldButton {objectName:"homeFoundCorporation";text:qsTr("Found a corporation");textSize:home.typeSize;onClicked:home.prepare(home.rich?"bootstrap":"investment","corporation")}
                    WorldButton {objectName:"homeFoundNonprofit";text:qsTr("Found a nonprofit");textSize:home.typeSize;onClicked:home.prepare("mission_director","nonprofit")}
                }
                ColumnLayout {
                    visible:home.app==="labs";Layout.fillWidth:true
                    Heading {text:qsTr("Alder Research · A seat at the table")}
                    Copy {text:qsTr("An early-stage lab is looking for a leader who can attract backing and talent. Apply with your background and public work.")}
                    WorldButton {objectName:"homeApplyCEO";text:qsTr("Send leadership application");textSize:home.typeSize;onClicked:{if(home.eligibleCEO){home.story.setField("home_contact","alder");home.prepare("hired_ceo","corporation")}else home.message=qsTr("Alder: Thank you for your interest. We are still in the early planning stages and cannot offer you the leadership role. Your own company remains an option.")}}
                }
                ColumnLayout {
                    visible:home.app==="investors" || home.app==="messages";Layout.fillWidth:true
                    Heading {text:home.app==="messages"?qsTr("Your conversations"):qsTr("Founders’ Exchange")}
                    Copy {text:home.story.form.leg==="genius"?qsTr("Alder has heard about your work. A recruiter asks whether you would consider leading their lab. Investors are also interested in your next experiment."):home.rich?qsTr("Your family can make introductions. Funding still comes with terms, and influential partners expect to be heard."):qsTr("Your public work is your calling card. Put a clear proposal in front of a backer and find out whether they will take the meeting.")}
                    WorldButton {visible:home.eligibleCEO;text:qsTr("Read Alder’s leadership invitation");textSize:home.typeSize;onClicked:home.openApp("labs")}
                    WorldButton {text:qsTr("Pitch a company");textSize:home.typeSize;onClicked:home.prepare("investment","corporation")}
                    WorldButton {visible:home.rich;text:qsTr("Meet a potential cofounder");textSize:home.typeSize;onClicked:home.prepare("partnership","corporation")}
                }
                ColumnLayout {
                    visible:home.app==="bank";Layout.fillWidth:true
                    Heading {text:qsTr("Redwood Mutual · Your money")}
                    Copy {text:qsTr("Cash savings: %1\nStocks: $0 · Bonds: $0 · Personal debt: $0\nNo company has been formed. Money stays yours until you explicitly invest it. Company expenses and salaries will use the company's separate accounts.").arg(home.story.personalFunds.total)}
                    Rectangle {
                        visible:!!home.story.startupLoan.lender
                        Layout.fillWidth:true
                        implicitHeight:loanOffer.implicitHeight+28
                        radius:12
                        color:"#284a46"
                        border.color:"#648779"
                        ColumnLayout {
                            id:loanOffer
                            anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:14
                            Heading {text:qsTr("▤  %1 · Startup credit").arg(home.story.startupLoan.lender || "")}
                            Copy {text:home.story.startupLoan.detail || ""}
                            WorldButton {
                                objectName:"homeReviewStartupLoan"
                                text:qsTr("Apply with a new corporation →")
                                textSize:home.typeSize
                                onClicked:home.prepare("bank_loan","corporation")
                            }
                        }
                    }
                    WorldButton {text:qsTr("Explore founding options");textSize:home.typeSize;onClicked:home.openApp("legal")}
                }
                ColumnLayout {
                    visible:home.app==="formation" && !home.signing;Layout.fillWidth:true
                    Heading {text:home.story.form.route==="hired_ceo"?qsTr("Alder Research · Appointment offer"):qsTr("Cedar & Quill · Formation workspace")}
                    Copy {text:home.story.form.route==="hired_ceo"?qsTr("The board offers an existing team, office and a disclosed minority stake. The company owns its cash and equipment; your personal savings remain yours."):qsTr("Name the institution, choose its purpose, and decide what you will personally invest. Outside funding only enters the company when you accept an offer below.")}
                    GameField {objectName:"homeCompanyName";visible:home.story.form.route!=="hired_ceo";Layout.fillWidth:true;text:home.story.form.company;textSize:home.typeSize;Accessible.name:qsTr("Company name");onEditingFinished:home.story.answerText("company",text)}
                    GameSelect {objectName:"homeMission";Layout.fillWidth:true;textSize:home.typeSize;textRole:"name";valueRole:"id";model:[{id:"survival",name:qsTr("Build a lab that lasts")},{id:"utopia",name:qsTr("Make lives better")},{id:"monopoly",name:qsTr("Build the leading platform")},{id:"domination",name:qsTr("Steer the AI age")},{id:"asi",name:qsTr("Demonstrate ASI first")}];currentIndex:Math.max(0,model.findIndex(row=>row.id===home.story.form.mission));onActivated:home.story.setField("mission",currentValue);Accessible.name:qsTr("Mission")}
                    Copy {visible:home.story.form.route!=="hired_ceo";text:qsTr("How will you fund the company?")}
                    GameSelect {
                        id:fundingRoute;objectName:"homeFundingRoute";visible:home.story.form.route!=="hired_ceo";Layout.fillWidth:true;textSize:home.typeSize;textRole:"name";valueRole:"id"
                        model:home.story.routes.filter(row=>row.id!=="hired_ceo" && row.available)
                        currentIndex:Math.max(0,model.findIndex(row=>row.id===home.story.form.route))
                        onActivated:home.story.setField("route",currentValue);Accessible.name:qsTr("Company funding route")
                    }
                    Copy {visible:fundingRoute.visible;text:(fundingRoute.model[fundingRoute.currentIndex] || {}).detail || ""}
                    Copy {visible:home.story.form.route==="bank_loan";text:home.story.startupLoan.detail || "";color:"#b9dba7"}
                    Copy {visible:home.story.form.route!=="hired_ceo";text:qsTr("Optional founding bylaws")}
                    GameSelect {
                        id:bylaws;objectName:"homeBylaws";visible:home.story.form.route!=="hired_ceo";Layout.fillWidth:true;textSize:home.typeSize;textRole:"name";valueRole:"id"
                        model:home.story.charters;currentIndex:Math.max(0,model.findIndex(row=>row.id===home.story.form.charter))
                        onActivated:home.story.setField("charter",currentValue);Accessible.name:qsTr("Optional company bylaws")
                    }
                    Copy {visible:bylaws.visible;text:(bylaws.model[bylaws.currentIndex] || {}).detail || ""}
                    Copy {visible:home.story.form.route!=="hired_ceo";text:qsTr("Personal investment, whole dollars · savings available %1").arg(home.story.personalFunds.total)}
                    GameField {objectName:"homeInvestment";visible:home.story.form.route!=="hired_ceo";Layout.fillWidth:true;text:home.story.form.investment;textSize:home.typeSize;Accessible.name:qsTr("Personal investment in dollars");onEditingFinished:home.story.setField("investment",text)}
                    WorldButton {visible:home.story.form.route!=="hired_ceo";text:qsTr("Invest all available savings");textSize:home.typeSize;onClicked:home.story.setField("investment",home.story.personalFunds.dollars)}
                    Repeater {
                        model:home.story.backers
                        ColumnLayout {
                            required property var modelData
                            visible:modelData.available;Layout.fillWidth:true
                            Heading {text:modelData.name}
                            Copy {text:modelData.detail+"\n"+modelData.terms}
                            WorldButton {objectName:"homeBacker_"+modelData.id;text:home.story.form.backers.includes(modelData.id)?qsTr("Decline this offer"):qsTr("Accept these terms");textSize:home.typeSize;onClicked:home.message=home.story.answerOffer(modelData.id,!home.story.form.backers.includes(modelData.id))}
                        }
                    }
                    Heading {text:qsTr("Before you sign")}
                    Copy {objectName:"homeTerms";text:home.story.preview}
                    WorldButton {objectName:"homeSign";text:home.story.form.route==="hired_ceo"?qsTr("Accept the appointment"):qsTr("Sign the founding documents");primary:true;textSize:home.typeSize;onClicked:home.sign()}
                }
                Rectangle {
                    visible:home.signing;Layout.fillWidth:true;implicitHeight:ceremony.implicitHeight+48;color:"#ede2c7";border.color:"#ad8458";border.width:8
                    ColumnLayout {
                        id:ceremony;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:24
                        Label {Layout.fillWidth:true;text:home.story.form.route==="hired_ceo"?qsTr("APPOINTMENT OF CHIEF EXECUTIVE"):qsTr("ARTICLES OF INCORPORATION");color:"#655440";font.pixelSize:home.typeSize*1.2;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter}
                        Label {Layout.fillWidth:true;text:home.story.form.company;color:"#30474a";font.pixelSize:home.typeSize*1.7;font.bold:true;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter}
                        Label {Layout.fillWidth:true;text:qsTr("A signature turns an idea into an obligation. People will depend on what you build next.");color:"#655440";font.pixelSize:home.typeSize;wrapMode:Text.Wrap}
                        Item {Layout.fillWidth:true;Layout.preferredHeight:home.typeSize*3;clip:true;Item {width:parent.width*home.signature;height:parent.height;clip:true;Label {width:ceremony.width;text:home.story.form.first_name+" "+home.story.form.last_name;font.family:"Snell Roundhand";font.pixelSize:home.typeSize*2;color:"#344b65"}}}
                        WorldButton {objectName:"homeEnterCompany";visible:home.signature===1;text:qsTr("Begin the first day →");primary:true;textSize:home.typeSize;enabled:!campaign.busy;onClicked:campaign.startFounderCampaign()}
                        WorldButton {text:qsTr("Review terms again");textSize:home.typeSize;enabled:!campaign.busy;onClicked:home.signing=false}
                    }
                }
                Copy {visible:!!home.message || !!campaign.notice;text:home.message || campaign.notice;color:"#f1c492"}
            }
        }
    }
}
}
