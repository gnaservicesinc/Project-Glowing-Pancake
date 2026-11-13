import QtQuick

Item {
    id: frame
    property string kind: "laptop"
    property int textSize: 14
    default property alias screenContent: screen.data
    readonly property bool laptop: kind === "laptop"
    readonly property bool handheld: kind === "phone" || kind === "tablet"
    readonly property int rim: Math.max(6, Math.round(textSize * 0.55))
    readonly property int topRim: rim + (handheld ? 8 : 5)
    readonly property int deck: laptop ? Math.max(13, Math.round(textSize * 1.1)) : handheld ? rim + 5 : rim

    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: frame.laptop ? frame.deck / 2 : 0
        anchors.leftMargin: frame.laptop ? frame.rim : 0
        anchors.rightMargin: anchors.leftMargin
        radius: frame.handheld ? 22 : 16
        gradient: Gradient {
            GradientStop { position: 0; color: "#5e7377" }
            GradientStop { position: 0.08; color: "#233339" }
            GradientStop { position: 0.92; color: "#17252a" }
            GradientStop { position: 1; color: "#4a5f62" }
        }
        border.color: "#839a91"
        border.width: 1
        Rectangle {
            anchors.fill: parent; anchors.margins: 1; radius: parent.radius - 1
            color: "transparent"; border.color: "#22ffffff"
        }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(3, (frame.topRim - height)/2)
        width: frame.handheld ? 36 : 7
        height: frame.handheld ? 5 : 7
        radius: height/2
        color: "#091317"
        border.color: "#6b847e"
        Rectangle {
            visible: !frame.handheld
            anchors.centerIn: parent; width: 3; height: 3; radius: 1.5; color: "#58969e"
        }
    }
    Rectangle {
        id: screen
        anchors.fill: parent
        anchors.leftMargin: frame.laptop ? frame.rim*2 : frame.rim
        anchors.rightMargin: anchors.leftMargin
        anchors.topMargin: frame.topRim
        anchors.bottomMargin: frame.deck
        radius: 8
        color: "#152536"
        border.color: "#0b171d"
        border.width: 1
        clip: true
    }
    Rectangle {
        visible: frame.laptop
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: frame.deck*0.68
        radius: 4
        gradient: Gradient {
            GradientStop {position:0;color:"#8fa19a"}
            GradientStop {position:0.28;color:"#657775"}
            GradientStop {position:1;color:"#304249"}
        }
        border.color: "#607877"
        Rectangle {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width*0.14
            height: Math.max(3,parent.height*0.5)
            radius: 3
            color: "#34494e"
        }
    }
    Rectangle {
        visible: frame.handheld
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.max(2, (frame.deck-height)/2)
        width: Math.min(parent.width*0.16,85)
        height: 3
        radius: 2
        color: "#869b92"
    }
}
