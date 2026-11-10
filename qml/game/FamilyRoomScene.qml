import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: scene
    required property int textSize
    required property bool bedroom
    property bool showRecord: false
    property bool showComputer: true
    property string computerLabel: qsTr("Open your computer")
    signal computerRequested()
    signal recordRequested()
    Layout.fillWidth: true
    Layout.preferredHeight: Math.max(300,Math.min(600,width*0.54))
    color: "#283c49"; radius: 6; clip: true
    Image { anchors.fill: parent; source: scene.bedroom ? "qrc:/pgp/assets/ui/family-bedroom.svg" : "qrc:/pgp/assets/founding/workshop.svg"; fillMode: Image.PreserveAspectCrop }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: controls.implicitHeight+28; color: "#dd182a37" }
    Flow {
        id: controls
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14; spacing: 10
        WorldButton { objectName: "familyBirthRecord"; visible: scene.showRecord; text: qsTr("Read birth record"); textSize: scene.textSize; onClicked: scene.recordRequested() }
        WorldButton { objectName: "homeComputer"; visible: scene.showComputer; text: scene.computerLabel+" →"; textSize: scene.textSize; primary: true; onClicked: scene.computerRequested() }
    }
}
