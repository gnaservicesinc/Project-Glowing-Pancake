import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    id:card
    property int textSize:14
    property bool personal:false
    property string amount:""
    property string caption:""
    property string detail:""
    property real progress:-1
    property string progressLabel:""
    property bool expanded:false
    implicitHeight:body.implicitHeight+32
    radius:14;color:personal?"#45323e":"#233f40";border.color:hover.hovered?"#d8bb7a":"#667c7c"
    Behavior on color {ColorAnimation {duration:campaign.reducedMotion?0:150}}
    HoverHandler {id:hover}
    ColumnLayout {
        id:body;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:16;spacing:10
        RowLayout {
            Layout.fillWidth:true;spacing:14
            Image {source:card.personal?"qrc:/pgp/assets/finance/personal-card.svg":"qrc:/pgp/assets/finance/company-checkbook.svg";Layout.preferredWidth:Math.min(100,card.width*0.22);Layout.preferredHeight:70;fillMode:Image.PreserveAspectFit;sourceSize.width:240;Accessible.ignored:true;rotation:hover.hovered && !campaign.reducedMotion?-4:0;Behavior on rotation {NumberAnimation {duration:160;easing.type:Easing.OutQuad}}}
            ColumnLayout {
                Layout.fillWidth:true
                Label {Layout.fillWidth:true;text:card.caption;wrapMode:Text.Wrap;font.pixelSize:card.textSize;color:"#d9d8cb"}
                Label {Layout.fillWidth:true;text:card.amount;wrapMode:Text.Wrap;font.pixelSize:card.textSize*1.35;font.bold:true;color:"#f1e4bd"}
            }
        }
        Rectangle {
            visible:card.progress>=0;Layout.fillWidth:true;implicitHeight:12;radius:6;color:"#172b35"
            Rectangle {width:parent.width*Math.max(0,Math.min(1,card.progress));height:parent.height;radius:6;color:card.progress>=1?"#d9bc70":"#83bfc0";Behavior on width {NumberAnimation {duration:campaign.reducedMotion?0:250}}}
            Accessible.ignored:true
        }
        Label {visible:card.progress>=0;Layout.fillWidth:true;text:card.progressLabel;wrapMode:Text.Wrap;font.pixelSize:card.textSize;color:"#ead89e"}
        WorldButton {text:card.expanded?qsTr("Hide account note"):qsTr("Whose money is this?");textSize:card.textSize;onClicked:card.expanded=!card.expanded}
        Label {visible:card.expanded;Layout.fillWidth:true;text:card.detail;wrapMode:Text.Wrap;font.pixelSize:card.textSize;color:"#e5e5dc"}
    }
}
