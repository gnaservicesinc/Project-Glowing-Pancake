import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    property int textSize:14
    readonly property var catalog:campaign.world.talent
    property var workers:[]
    property int previous:0
    property bool restoring:true
    function restore(value) {
        if(typeof value === "string") {try {value=JSON.parse(value)} catch(error) {value={}}}
        if(!value || typeof value !== "object")value={}
        restoring=true
        const rows=catalog.briefs || []
        briefChoice.currentIndex=Math.max(0,rows.findIndex(b=>b.id===value.brief))
        titleInput.text=value.name || "First workspace"
        aesthetics.currentIndex=Math.max(0,aesthetics.model.findIndex(b=>b.id===value.aesthetic))
        navigation.currentIndex=Math.max(0,navigation.model.findIndex(b=>b.id===value.navigation))
        priorities.currentIndex=Math.max(0,priorities.model.findIndex(b=>b.id===value.focus))
        workers=Array.isArray(value.workers)?value.workers.filter(id=>(catalog.people || []).some(p=>p.id===id)):[]
        automation.currentIndex=Math.max(0,(catalog.aiSources || []).findIndex(b=>b.id===value.ai_source))
        previous=Number.isInteger(value.previous)?value.previous:0
        Qt.callLater(()=>{restoring=false;campaign.world.setDraft("software/design",JSON.stringify(design))})
    }
    Component.onCompleted:restore(campaign.world.draft("software/design","{}"))
    Connections {target:campaign.world;function onDraftsReset(){panel.restore(campaign.world.draft("software/design","{}"))}}
    onDesignChanged:if(!restoring)campaign.world.setDraft("software/design",JSON.stringify(design))
    readonly property var brief:(catalog.briefs || [])[briefChoice.currentIndex] || ({})
    readonly property var design:({brief:brief.id || "",name:titleInput.text,aesthetic:aesthetics.currentValue || "calm",navigation:navigation.currentValue || "sidebar",focus:priorities.currentValue || "balanced",workers:workers,previous:previous,ai_source:automation.currentValue || ""})
    readonly property var quote:{const revision=campaign.world.reviewToken;return campaign.world.softwareQuote(design)}
    readonly property bool editable:campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    spacing:14
    component Copy:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#d9e3e9"}
    Copy {text:qsTr("Software Studio");font.pixelSize:panel.textSize*1.5;font.bold:true}
    Copy {text:qsTr("Shape the interface, choose what matters, and give a qualified team time to build it. The strongest released design improves customer utility for matching services, up to %1%; designs do not stack. Released designs can be refined. Model evaluation and commercial service deployment remain in the Lab.").arg(panel.catalog.maximumDesignBonus || 0)}
    Copy {text:qsTr("Product brief")}
    GameSelect {id:briefChoice;objectName:"softwareBrief";Layout.fillWidth:true;textSize:panel.textSize;model:panel.catalog.briefs || [];textRole:"name";onActivated:panel.previous=0;Accessible.name:qsTr("Software type")}
    Copy {text:(panel.brief.description || "")+"\n"+(panel.brief.advanced?qsTr("Requires Programming and Software Architecture."):qsTr("Requires Programming."))}
    GameField {id:titleInput;objectName:"softwareTitle";Layout.fillWidth:true;text:"First workspace";maximumLength:80;font.pixelSize:panel.textSize;Accessible.name:qsTr("Software name")}
    GridLayout {
        Layout.fillWidth:true;columns:panel.width<700?1:3
        ColumnLayout {
            Layout.fillWidth:true
            Copy {text:qsTr("Visual aesthetic")}
            GameSelect {id:aesthetics;objectName:"softwareAesthetic";Layout.fillWidth:true;textSize:panel.textSize;textRole:"name";valueRole:"id";Accessible.name:qsTr("Visual aesthetic");model:[{id:"calm",name:qsTr("Calm and spacious")},{id:"dense",name:qsTr("Dense and technical")},{id:"expressive",name:qsTr("Expressive and colorful")}]}
        }
        ColumnLayout {
            Layout.fillWidth:true
            Copy {text:qsTr("Navigation")}
            GameSelect {id:navigation;objectName:"softwareNavigation";Layout.fillWidth:true;textSize:panel.textSize;textRole:"name";valueRole:"id";Accessible.name:qsTr("Navigation placement");model:[{id:"sidebar",name:qsTr("Sidebar")},{id:"top",name:qsTr("Top navigation")},{id:"command",name:qsTr("Command first")}]}
        }
        ColumnLayout {
            Layout.fillWidth:true
            Copy {text:qsTr("Development focus")}
            GameSelect {id:priorities;objectName:"softwareFocus";Layout.fillWidth:true;textSize:panel.textSize;textRole:"name";valueRole:"id";Accessible.name:qsTr("Development focus");model:[{id:"balanced",name:qsTr("Balanced")},{id:"usability",name:qsTr("Usability")},{id:"reliability",name:qsTr("Reliability")},{id:"capability",name:qsTr("Capability")}]}
        }
    }
    Rectangle {
        id:prototype;objectName:"softwarePrototype"
        Layout.fillWidth:true;implicitHeight:300
        readonly property real unit:Math.min(panel.textSize,18)
        color:panel.design.aesthetic==="calm"?"#edf0e9":panel.design.aesthetic==="dense"?"#182431":"#30254b"
        border.color:"#8398a8";radius:12
        readonly property color ink:panel.design.aesthetic==="calm"?"#263940":"#e6e9f5"
        readonly property real rail:panel.design.navigation==="sidebar"?Math.min(width*0.3,panel.textSize*10):0
        Label {x:16;y:12;width:parent.width-32;text:qsTr("LIVE DESIGN · %1").arg(titleInput.text);elide:Text.ElideRight;font.pixelSize:prototype.unit;color:prototype.ink;font.bold:true}
        Rectangle {x:12;y:prototype.unit*3;width:prototype.rail?prototype.rail-12:parent.width-24;height:prototype.rail?parent.height-prototype.unit*4:prototype.unit*2;color:panel.design.aesthetic==="calm"?"#d8e1dc":"#43546c";radius:6
            Label {anchors.fill:parent;anchors.margins:8;wrapMode:Text.Wrap;elide:Text.ElideRight;text:panel.design.navigation==="command"?qsTr("⌘  Search, ask, or run an action"):(panel.brief.features || []).join(prototype.rail?"\n\n":"   ·   ");font.pixelSize:prototype.unit*0.85;color:prototype.ink}
        }
        Column {
            x:prototype.rail+24;y:panel.design.navigation==="sidebar"?prototype.unit*3:prototype.unit*6
            width:parent.width-x-16;spacing:12
            Label {width:parent.width;text:panel.brief.name || "";wrapMode:Text.Wrap;font.pixelSize:prototype.unit*1.3;font.bold:true;color:prototype.ink}
            Repeater {model:panel.brief.id==="chat"?2:3;Rectangle {required property int index;width:parent.width*(panel.brief.id==="chat" && index===0?0.75:1);height:prototype.unit*(panel.design.aesthetic==="dense"?1.25:1.8);color:panel.design.aesthetic==="calm"?"#c1d3cd":"#5d6781";radius:panel.design.aesthetic==="dense"?2:8;Label {anchors.centerIn:parent;text:(panel.brief.features || [])[index] || qsTr("Workspace");font.pixelSize:prototype.unit*0.75;color:prototype.ink}}}
        }
        Accessible.role:Accessible.Graphic
        Accessible.name:qsTr("Interface prototype: %1, %2, %3").arg(panel.brief.name || "").arg(aesthetics.currentText).arg(navigation.currentText)
    }
    Copy {text:qsTr("AI development assistance");font.bold:true}
    Copy {text:panel.catalog.aiLevel>0?qsTr("Verified AI can cover %1. Assisted work takes %2% of normal staff-days and reserves %3 CU/day. A person with a computer still supervises the project.").arg(panel.catalog.aiLevel>1?qsTr("programming and architecture"):qsTr("architecture")).arg(panel.catalog.aiWorkPercent).arg(panel.catalog.aiCompute):qsTr("Human skills are required until independently qualified AGI demonstrates reliable software and autonomous workflow capability.")}
    GameSelect {id:automation;objectName:"softwareAutomation";Layout.fillWidth:true;textSize:panel.textSize;model:panel.catalog.aiSources || [];textRole:"name";valueRole:"id";Accessible.name:qsTr("AI compute allocation");enabled:panel.catalog.aiLevel>0}
    Copy {text:qsTr("Development team");font.bold:true}
    Repeater {
        model:panel.catalog.people || []
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            GameCheck {objectName:"softwareWorker_"+modelData.id;text:modelData.name;textSize:panel.textSize;checked:panel.workers.includes(modelData.id);onToggled:panel.workers=checked?panel.workers.concat([modelData.id]):panel.workers.filter(id=>id!==modelData.id)}
            Copy {text:modelData.detail+(modelData.blocker?"\n"+modelData.blocker:"");color:modelData.blocker?"#e5b990":"#a7c4d0"}
        }
    }
    Copy {visible:panel.previous>0;text:qsTr("Refining released design #%1: reuse reduces work; improvements carry forward.").arg(panel.previous);color:"#d9c58b"}
    Repeater {
        model:[{name:qsTr("Usability"),value:panel.quote.usability || 0},{name:qsTr("Reliability"),value:panel.quote.reliability || 0},{name:qsTr("Capability"),value:panel.quote.capability || 0}]
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            Copy {text:modelData.name+" · "+modelData.value+" / 100"}
            Rectangle {Layout.fillWidth:true;implicitHeight:8;color:"#172732";radius:4;Rectangle {width:parent.width*modelData.value/100;height:8;radius:4;color:"#d7ba78"}}
        }
    }
    Copy {objectName:"softwareQuote";text:panel.quote.valid?qsTr("%1 setup · %2 staff-days. Salaries continue during development. Focus trades strength in one area against the others.").arg(panel.quote.fee).arg(panel.quote.work):(panel.quote.blocker || qsTr("Choose a team to review this design."));color:panel.quote.valid?"#b9d9b1":"#e6bb93"}
    WorldButton {objectName:"softwareBuild";text:qsTr("Review development");textSize:panel.textSize;enabled:panel.editable && !!panel.quote.valid;onClicked:review.open()}
    Copy {text:qsTr("Design portfolio");font.bold:true}
    Repeater {
        model:panel.catalog.projects || []
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            Copy {text:modelData.name;font.bold:true}
            Copy {text:modelData.detail}
            Flow {
                Layout.fillWidth:true;spacing:8
                WorldButton {visible:modelData.canRelease;text:qsTr("Release completed design");textSize:panel.textSize;enabled:panel.editable;onClicked:campaign.world.releaseSoftware(modelData.id,campaign.world.reviewToken)}
                WorldButton {visible:modelData.canPause;text:qsTr("Pause development");textSize:panel.textSize;enabled:panel.editable;onClicked:campaign.world.stopSoftware(modelData.id,false,campaign.world.reviewToken)}
                WorldButton {visible:modelData.canCancel;text:qsTr("Cancel development · no refund");textSize:panel.textSize;enabled:panel.editable;onClicked:campaign.world.stopSoftware(modelData.id,true,campaign.world.reviewToken)}
                WorldButton {visible:modelData.canResume;text:qsTr("Resume with current team");textSize:panel.textSize;enabled:panel.editable;onClicked:campaign.world.resumeSoftware(modelData.id,campaign.world.reviewToken)}
                WorldButton {visible:modelData.released;text:qsTr("Refine this design");textSize:panel.textSize;onClicked:panel.restore({brief:modelData.brief,name:modelData.name+" II",aesthetic:modelData.aesthetic,navigation:modelData.navigation,focus:modelData.focus,previous:modelData.id,workers:panel.workers,ai_source:panel.design.ai_source})}
            }
        }
    }
    GameDialog {
        id:review;objectName:"softwareReview";property var draft:({});property var terms:({})
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*38,Overlay.overlay.width-32);height:Math.min(panel.textSize*17,Overlay.overlay.height-32)
        title:qsTr("Start software development?");modal:true;focus:true
        onAboutToShow:{draft=JSON.parse(JSON.stringify(panel.design));terms=panel.quote}
        contentItem:ScrollView {id:reviewScroll;contentWidth:availableWidth;clip:true;Label {width:reviewScroll.availableWidth;text:(review.draft.name || "")+"\n"+qsTr("Pay %1 now and reserve %2 people for %3 staff-days. Work pauses if their required equipment or skills become unavailable.").arg(review.terms.fee || "").arg((review.draft.workers || []).length).arg(review.terms.work || 0);wrapMode:Text.Wrap;font.pixelSize:panel.textSize}}
        footer:Flow {padding:10;spacing:8;WorldButton {text:qsTr("Cancel");textSize:panel.textSize;onClicked:review.close()} WorldButton {objectName:"softwareConfirm";text:qsTr("Pay and begin");textSize:panel.textSize;enabled:panel.editable;onClicked:{campaign.world.startSoftware(review.draft,review.terms.revision);review.close()}}}
    }
}
