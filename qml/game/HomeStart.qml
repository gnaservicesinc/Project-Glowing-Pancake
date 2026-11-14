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
    readonly property bool rich:story.form.family==="rich"
    readonly property string previewText: story.preview || ""
    readonly property bool hasDr: previewText.indexOf("Title: Dr.") >= 0
    readonly property bool delayedLaunch: previewText.indexOf("Delayed start:") >= 0
    readonly property bool hasStudentDebt: previewText.indexOf("Student debt") >= 0
    readonly property var offers: story.homeOffers
    readonly property var partners: offers.filter(offer => offer.kind === "partner")
    readonly property int waitingOffers: offers.filter(offer => offer.status === "offered").length
    property string app:story.form.home_app || "room"
    property string room:story.form.home_room || "Bedroom"
    property string message:""
    property bool signing:false
    property real signature:0
    property string pendingApp:"room"
    property bool desktopOpen:false
    readonly property bool transitioning:pageTransition.running
    signal appearanceRequested()
    color:"#142333"
    BirthKeepsake {
        id: birthKeepsake
        textSize: home.typeSize
        founder: home.story.form.first_name + " " + home.story.form.last_name
        surname: home.story.form.last_name
        birthSex: home.story.form.birth_sex
    }
    function revealPage() {
        const flick=homeScroll.contentItem
        const y=pages.mapToItem(flick.contentItem,0,0).y
        flick.contentY=Math.max(0,Math.min(y,flick.contentHeight-flick.height))
    }
    function showApp(id) {
        app=id;story.setField("home_app",id);message="";homeScroll.contentItem.contentY=0
        if(["messages","labs","investors"].includes(id)) Qt.callLater(revealPage)
    }
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
    function contact(id) {
        message=story.contactHomeOffer(id)
        if(message) return
        if(app==="messages") Qt.callLater(revealPage)
        else openApp("messages")
    }
    function reply(id, response) {
        message=story.respondHomeOffer(id,response)
        if(message) return
        if(response==="accept") openApp("formation")
        else Qt.callLater(revealPage)
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
        width:homeScroll.availableWidth;spacing:14
        Rectangle {
            Layout.fillWidth:true;implicitHeight:founderBar.implicitHeight+24;radius:14;color:"#1b2e41";border.color:"#4a657a"
            Rectangle { width:4; height:parent.height-20; radius:2; anchors.left:parent.left; anchors.leftMargin:8; anchors.verticalCenter:parent.verticalCenter; color:"#efc389" }
            ColumnLayout {
                id:founderBar;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;anchors.leftMargin:20;spacing:8
                RowLayout {
                    spacing:12
                    Image {source:home.story.portrait;Layout.preferredWidth:72;Layout.preferredHeight:72;sourceSize:Qt.size(144*Screen.devicePixelRatio,144*Screen.devicePixelRatio)}
                    ColumnLayout {
                        Layout.fillWidth:true;spacing:4
                        Heading {text:(home.hasDr?qsTr("Dr. "):"")+home.story.form.first_name+" "+home.story.form.last_name}
                        Copy {text:home.delayedLaunch?qsTr("Graduated & Launch Ready · Silicon Valley"):qsTr("January 2015 · Age 18 · Pre-Founding");font.pixelSize:home.typeSize*0.85;color:"#c9d6d0"}
                    }
                }
                Copy {text:qsTr("Personal savings %1 · Company funds $0 · Silicon Valley%2").arg(home.story.personalFunds.total).arg(home.hasStudentDebt?qsTr(" · Student loan active"):"");color:"#b9dba7";font.pixelSize:home.typeSize*0.92}
            }
        }
        Flow {
            Layout.fillWidth:true;spacing:8
            Repeater {model:[{id:"room",name:qsTr("⌂ Home")},{id:"browser",name:qsTr("◉ Browser")},{id:"messages",name:home.waitingOffers?qsTr("✉ Messages (%1)").arg(home.waitingOffers):qsTr("✉ Messages")},{id:"bank",name:qsTr("▤ Personal accounts")}]
                WorldButton {required property var modelData;objectName:"homeApp_"+modelData.id;text:modelData.name;textSize:home.typeSize;checked:home.app===modelData.id;onClicked: modelData.id==="browser" ? home.desktopOpen=true : home.openApp(modelData.id)}
            }
            WorldButton {text:qsTr("Appearance");textSize:home.typeSize;onClicked:{home.signing=false;home.appearanceRequested()}}
        }
        Rectangle {
            visible:campaign.showGuide && home.story.form.home_guide!=="dismissed" && ["room","browser"].includes(home.app)
            Layout.fillWidth:true;implicitHeight:guide.implicitHeight+20;color:"#354b5e";radius:10
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
                    FamilyRoomScene {
                        textSize: home.typeSize
                        bedroom: home.room === "Bedroom"
                        showRecord: true
                        onComputerRequested: home.desktopOpen=true
                        onRecordRequested: birthKeepsake.open()
                    }
                    Flow {Layout.fillWidth:true;spacing:8;Repeater {model:["Bedroom"].concat(home.story.familyHome.roomNames || []);WorldButton {required property string modelData;text:modelData;textSize:home.typeSize;checked:home.room===modelData;onClicked:{home.room=modelData;home.story.setField("home_room",modelData)}}}}
                    Copy {text:qsTr("%1 free workspaces · %2 × %3 m in total · the family keeps a room for you when the company moves out.").arg((home.story.familyHome.roomNames || []).length).arg(home.story.familyHome.width || 0).arg(home.story.familyHome.depth || 0)}
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
                            Layout.fillWidth:true;implicitHeight:service.implicitHeight+24;radius:12;color:"#2a3e52";border.color:"#63788c"
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
                HomeOffers {
                    visible:["messages","labs","investors"].includes(home.app)
                    Layout.fillWidth:true
                    mode:visible?home.app:"messages"
                    textSize:home.typeSize
                    offers:visible?home.offers:[]
                    selectedId:home.story.form.home_offer || ""
                    errorText:home.message || home.story.error
                    canMeetCofounder:home.rich && !home.partners.length
                    onOpened:id=>home.contact(id)
                    onReplied:(id,response)=>home.reply(id,response)
                    onCloseThread:{home.message=home.story.closeHomeOffer();if(!home.message)Qt.callLater(home.revealPage)}
                    onCofounderRequested:home.prepare("partnership","corporation")
                }
                ColumnLayout {
                    visible:home.app==="bank";Layout.fillWidth:true
                    Heading {text:qsTr("Redwood Mutual · Your money")}
                    Copy {text:qsTr("Cash savings: %1\nStocks: $0 · Bonds: $0 · %2\nNo company has been formed. Money stays yours until you explicitly invest it. Company expenses and salaries will use the company's separate accounts.").arg(home.story.personalFunds.total).arg(home.hasStudentDebt ? qsTr("Student loan obligation carries to launch (zero extra cash)") : qsTr("Personal debt: $0"))}
                    Rectangle {
                        visible:!!home.story.startupLoan.lender
                        Layout.fillWidth:true
                        implicitHeight:loanOffer.implicitHeight+28
                        radius:12
                        color:"#304657"
                        border.color:"#78908f"
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
                    GameSelect {objectName:"homeMission";Layout.fillWidth:true;textSize:home.typeSize;textRole:"name";valueRole:"id";enabled:!home.story.form.partner;model:[{id:"survival",name:qsTr("Build a lab that lasts")},{id:"utopia",name:qsTr("Make lives better")},{id:"monopoly",name:qsTr("Build the leading platform")},{id:"domination",name:qsTr("Steer the AI age")},{id:"asi",name:qsTr("Demonstrate ASI first")}];currentIndex:Math.max(0,model.findIndex(row=>row.id===home.story.form.mission));onActivated:home.story.setField("mission",currentValue);Accessible.name:qsTr("Mission")}
                    Copy {visible:!!home.story.form.partner;text:qsTr("Your cofounder’s agreement sets the opening mission.")}
                    Copy {visible:home.story.form.route!=="hired_ceo";text:qsTr("How will you fund the company?")}
                    GameSelect {
                        id:fundingRoute;objectName:"homeFundingRoute";visible:home.story.form.route!=="hired_ceo";Layout.fillWidth:true;textSize:home.typeSize;textRole:"name";valueRole:"id"
                        model:home.story.routes.filter(row=>row.id!=="hired_ceo" && row.available)
                        currentIndex:Math.max(0,model.findIndex(row=>row.id===home.story.form.route))
                        onActivated:home.story.setField("route",currentValue);Accessible.name:qsTr("Company funding route")
                    }
                    Copy {visible:fundingRoute.visible;text:(fundingRoute.model[fundingRoute.currentIndex] || {}).detail || ""}
                    Repeater {
                        model:home.story.form.route === "partnership" ? home.partners : []
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth:true
                            Heading {text:modelData.name}
                            Copy {text:modelData.terms}
                            WorldButton {
                                objectName:"homePartner_"+modelData.id
                                text:modelData.status === "selected" ? qsTr("Review agreed terms") : qsTr("Open partnership conversation")
                                textSize:home.typeSize;enabled:modelData.available
                                onClicked:home.contact(modelData.id)
                            }
                        }
                    }
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
                            readonly property var offer:home.offers.find(row=>row.id===modelData.id)
                            visible:modelData.available;Layout.fillWidth:true
                            Heading {text:modelData.name}
                            Copy {text:modelData.detail}
                            WorldButton {
                                objectName:"homeBacker_"+modelData.id
                                text:offer?.status==="listed"?qsTr("Contact about funding"):qsTr("Open conversation · %1").arg(offer?.statusText || "")
                                textSize:home.typeSize
                                Layout.maximumWidth:parent.width
                                onClicked:home.contact(modelData.id)
                            }
                        }
                    }
                    Heading {text:qsTr("Before you sign")}
                    Copy {objectName:"homeTerms";text:home.story.preview}
                    WorldButton {objectName:"homeSign";text:home.story.form.route==="hired_ceo"?qsTr("Sign the appointment"):qsTr("Sign the founding documents");primary:true;textSize:home.typeSize;onClicked:home.sign()}
                }
                Rectangle {
                    visible:home.signing;Layout.fillWidth:true;implicitHeight:ceremony.implicitHeight+48;color:"#ede2c7";border.color:"#ad8458";border.width:8
                    ColumnLayout {
                        id:ceremony;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:24
                        Label {Layout.fillWidth:true;text:home.story.form.route==="hired_ceo"?qsTr("APPOINTMENT OF CHIEF EXECUTIVE"):qsTr("ARTICLES OF INCORPORATION");color:"#655440";font.pixelSize:home.typeSize*1.2;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter}
                        Label {Layout.fillWidth:true;text:home.story.form.company;color:"#30474a";font.pixelSize:home.typeSize*1.7;font.bold:true;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter}
                        Label {Layout.fillWidth:true;text:home.story.form.route==="hired_ceo"?qsTr("You close the appointment letter. Across the valley, a team is waiting to hear from its new chief executive."):qsTr("A signature turns an idea into an obligation. People will depend on what you build next.");color:"#655440";font.pixelSize:home.typeSize;wrapMode:Text.Wrap}
                        Item {Layout.fillWidth:true;Layout.preferredHeight:home.typeSize*3;clip:true;Item {width:parent.width*home.signature;height:parent.height;clip:true;Label {width:ceremony.width;text:home.story.form.first_name+" "+home.story.form.last_name;font.family:"Snell Roundhand";font.pixelSize:home.typeSize*2;color:"#344b65"}}}
                        WorldButton {objectName:"homeEnterCompany";visible:home.signature===1;text:qsTr("Begin the first day →");primary:true;textSize:home.typeSize;enabled:!campaign.busy;onClicked:campaign.startFounderCampaign()}
                        WorldButton {text:qsTr("Review terms again");textSize:home.typeSize;enabled:!campaign.busy;onClicked:home.signing=false}
                    }
                }
                Copy {visible:(!!home.message && !["messages","labs","investors"].includes(home.app)) || !!campaign.notice;text:home.message || campaign.notice;color:"#f1c492"}
            }
        }
    }
}
    Rectangle {
        visible:home.desktopOpen
        anchors.fill:parent;color:"#111d29";z:20
        DeviceFrame {
            anchors.fill:parent;anchors.margins:home.width<800?6:22
            kind:"laptop";textSize:home.typeSize
            DesktopSurface {
                anchors.fill:parent;textSize:home.typeSize
                owner:home.story.form.first_name+" "+home.story.form.last_name
                date:qsTr("January 2015")
                status:qsTr("A new morning.\nA world of possibilities.")
                applications:[
                    {id:"browser",name:qsTr("Web browser"),glyph:"society",tint:"#638a9e"},
                    {id:"messages",name:qsTr("Messages"),glyph:"inbox",tint:"#9d8462"},
                    {id:"bank",name:qsTr("Personal accounts"),glyph:"office",tint:"#628a7c"},
                    {id:"legal",name:qsTr("Found a company"),glyph:"institutions",tint:"#96809f"},
                    {id:"labs",name:qsTr("Careers"),glyph:"lab",tint:"#607e9f"},
                    {id:"investors",name:qsTr("Founders’ Exchange"),glyph:"people",tint:"#a27b68"}
                ]
                onCloseRequested:home.desktopOpen=false
                onLaunched:(app,section)=>{home.desktopOpen=false;home.openApp(app)}
            }
        }
    }

}
