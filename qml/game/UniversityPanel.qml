import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    property int textSize: 14
    readonly property var university: campaign.world.talent
    readonly property var people: university.people || []
    readonly property var skills: university.skills || []
    property string selectedStudent:campaign.world.draft("university/student","founder")
    property string selectedSkill:campaign.world.draft("university/skill","programming")
    Connections {target:campaign.world;function onDraftsReset(){panel.selectedStudent=campaign.world.draft("university/student","founder");panel.selectedSkill=campaign.world.draft("university/skill","programming")}}
    readonly property var student: people[students.currentIndex] || ({})
    readonly property var course: skills[classes.currentIndex] || ({})
    readonly property var personalQuote: {const current=panel.university.revision;return campaign.world.courseQuote(panel.student.id || "",panel.course.id || "","personal")}
    readonly property var companyQuote: {const current=panel.university.revision;return campaign.world.courseQuote(panel.student.id || "",panel.course.id || "","company")}
    readonly property bool editable: campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    spacing: 12
    component Copy: Label { Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:panel.textSize;color:"#d9e3e9" }
    Copy { text:qsTr("Open Path University");font.pixelSize:panel.textSize*1.5;font.bold:true }
    Copy { text:qsTr("Build practical skills through part-time study. Education and acquired skills are separate: a degree alone does not qualify someone for a job.") }
    GridLayout {
        Layout.fillWidth:true;columns:panel.width<700?1:2
        MoneyCard {objectName:"universityPersonalMoney";Layout.fillWidth:true;textSize:panel.textSize;personal:true;caption:qsTr("Your pocket");amount:panel.university.personalCash || "";detail:qsTr("Your savings and paid compensation. Investing everything is allowed; hiring or company sponsorship can cover skills you do not yet have.")}
        MoneyCard {Layout.fillWidth:true;textSize:panel.textSize;caption:qsTr("Company purse · uncommitted cash");amount:panel.university.companyCash || "";detail:qsTr("Company sponsorship is an actual business expense. Paying tuition does not transfer company money to your personal account.")}
    }
    Copy { text:qsTr("Student") }
    GameSelect { id:students;objectName:"universityStudent";Layout.fillWidth:true;textSize:panel.textSize;model:panel.people;textRole:"name";currentIndex:Math.max(0,panel.people.findIndex(p=>p.id===panel.selectedStudent));onActivated:{panel.selectedStudent=panel.people[currentIndex].id;campaign.world.setDraft("university/student",panel.selectedStudent)} Accessible.name:qsTr("Student") }
    Copy { text:panel.student.detail || "" }
    MoneyCard {
        objectName:"universityStudySavings";Layout.fillWidth:true;textSize:panel.textSize;personal:true
        visible:panel.student.id!=="founder" && !!panel.student.learningBudget
        caption:qsTr("%1’s learning jar").arg(panel.student.name || "")
        amount:panel.student.learningBudget || ""
        progress:panel.course.teachable && panel.course.tuitionCents>0?Math.min(1,(panel.student.learningCents || 0)/panel.course.tuitionCents):-1
        progressLabel:progress>=1?qsTr("★ Tuition saved! Check the class requirements below."):qsTr("Tuition jar · %1% filled for this class").arg(Math.floor(progress*100))
        detail:qsTr("They set aside %1% of compensation actually received for new skills. Every %2 days they may choose an affordable research, engineering or design class. Unpaid wages do not fill the jar; other personal savings stay theirs.").arg(panel.university.employeeStudyShare || 0).arg(panel.university.employeeStudyInterval || 30)
    }
    Copy { text:qsTr("Course or professional qualification") }
    GameSelect { id:classes;objectName:"universityCourse";Layout.fillWidth:true;textSize:panel.textSize;model:panel.skills;textRole:"name";currentIndex:Math.max(0,panel.skills.findIndex(p=>p.id===panel.selectedSkill));onActivated:{panel.selectedSkill=panel.skills[currentIndex].id;campaign.world.setDraft("university/skill",panel.selectedSkill)} Accessible.name:qsTr("Course") }
    Copy { objectName:"universityCourseDetail";text:panel.course.detail || "" }
    Copy { visible:(panel.student.skills || []).includes(panel.course.id);text:qsTr("Already acquired.");color:"#b8dcaa" }
    Copy {objectName:"universitySelfStudyStatus";visible:panel.student.id!=="founder" && !!panel.course.teachable;text:panel.personalQuote.blocker || qsTr("They’re happy to pay from their learning jar. You can also offer a scholarship!");color:panel.personalQuote.valid?"#c3dca6":"#dfca84"}
    Flow {
        Layout.fillWidth:true;spacing:8
        WorldButton {
            objectName:"universityPersonal";text:panel.student.id==="founder"?qsTr("Pay from personal savings"):qsTr("Ask them to self-fund");textSize:panel.textSize
            enabled:panel.editable && !!panel.personalQuote.valid
            onClicked:{review.payer="personal";review.open()}
        }
        WorldButton {
            objectName:"universityCompany";text:qsTr("Sponsor with company funds");textSize:panel.textSize
            enabled:panel.editable && !!panel.companyQuote.valid
            onClicked:{review.payer="company";review.open()}
        }
    }
    Copy { text:qsTr("Your learning adventure");font.bold:true }
    Repeater {
        model:panel.university.courses || []
        ColumnLayout {
            required property var modelData
            Layout.fillWidth:true
            Copy { text:(modelData.completed?"★ ":"✦ ")+modelData.name;font.bold:true;color:modelData.completed?"#dfca84":"#d9e3e9" }
            Rectangle {Layout.fillWidth:true;implicitHeight:10;color:"#172b35";radius:5;Rectangle {width:parent.width*modelData.progress/100;height:10;radius:5;color:modelData.completed?"#d9bc70":"#83bfc0";Behavior on width {NumberAnimation {duration:campaign.reducedMotion?0:250}}}}
            Copy {visible:modelData.completed;text:qsTr("Skill unlocked. Put it to work!");color:"#c3dca6"}
            Copy { text:modelData.detail }
        }
    }
    GameDialog {
        id:review;objectName:"universityReview"
        property string payer:""
        property string studentId:""
        property string skillId:""
        property string revision:""
        property string copy:""
        anchors.centerIn:Overlay.overlay;width:Math.min(panel.textSize*38,Overlay.overlay.width-32)
        height:Math.min(panel.textSize*18,Overlay.overlay.height-32)
        modal:true;focus:true;title:qsTr("Enroll in this course?")
        onAboutToShow:{studentId=panel.student.id;skillId=panel.course.id;revision=campaign.world.reviewToken;copy=panel.student.name+"\n"+panel.course.name+"\n"+panel.course.detail+"\n"+(payer==="personal"?(studentId==="founder"?qsTr("Tuition leaves your personal savings now."):qsTr("They agreed to pay from their learning jar. Company funds and your savings are untouched.")):qsTr("Tuition is a company expense paid now."))}
        contentItem:ScrollView { id:reviewScroll;contentWidth:availableWidth;clip:true;Label { id:details;width:reviewScroll.availableWidth;text:review.copy;wrapMode:Text.Wrap;font.pixelSize:panel.textSize } }
        footer:Flow {
            spacing:8;padding:10
            WorldButton { text:qsTr("Cancel");textSize:panel.textSize;onClicked:review.close() }
            WorldButton { objectName:"universityConfirm";text:qsTr("Pay tuition and enroll");textSize:panel.textSize;enabled:panel.editable;onClicked:{campaign.world.enrollSkill(review.studentId,review.skillId,review.payer,review.revision);review.close()} }
        }
    }
}
