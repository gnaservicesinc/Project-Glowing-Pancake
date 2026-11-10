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
    Layout.preferredHeight: Math.max(230, textSize * 10)
    color: bedroom ? "#9d9f8c" : "#8a9890"
    radius: 8
    clip: true

    Rectangle { y: parent.height * 0.58; width: parent.width; height: parent.height - y; color: "#786852" }
    Rectangle {
        x: parent.width * 0.08; y: 28; width: parent.width * 0.23; height: parent.height * 0.35
        color: "#84b6c4"; border.width: 9; border.color: "#e6d7b8"
        Rectangle { anchors.centerIn: parent; width: 4; height: parent.height - 18; color: parent.border.color }
        Rectangle { anchors.centerIn: parent; width: parent.width - 18; height: 4; color: parent.border.color }
    }
    Rectangle {
        visible: scene.bedroom
        x: parent.width * 0.07; y: parent.height * 0.60; width: parent.width * 0.35; height: parent.height * 0.28
        color: "#b0b9a5"; radius: 10
        Rectangle { x: 5; y: 5; width: parent.width * 0.27; height: parent.height - 10; radius: 7; color: "#ded6ba" }
    }
    Rectangle {
        visible: scene.showComputer
        x: parent.width * 0.50; y: parent.height * 0.53; width: parent.width * 0.42; height: parent.height * 0.08; color: "#c19b6e"
    }
    Image {
        visible: scene.showComputer
        x: parent.width * 0.63; y: parent.height * 0.21; width: parent.width * 0.18; height: parent.height * 0.36
        source: "qrc:/pgp/assets/ui/laptop.svg"; fillMode: Image.PreserveAspectFit
        TapHandler { onTapped: scene.computerRequested() }
    }
    Rectangle {
        visible: scene.showRecord
        x: parent.width * 0.38; y: parent.height * 0.14; width: parent.width * 0.13; height: parent.height * 0.22
        color: "#f2e4be"; border.width: 5; border.color: "#79573c"; radius: 2
        Column {
            anchors.centerIn: parent
            width: parent.width * 0.66
            spacing: 4
            Repeater {
                model: 3
                Rectangle { width: parent.width; height: 2; color: "#a38667" }
            }
        }
        TapHandler { onTapped: scene.recordRequested() }
    }
    Flow {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10
        spacing: 8
        WorldButton {
            objectName: "familyBirthRecord"
            visible: scene.showRecord
            text: qsTr("Read birth record"); textSize: scene.textSize
            width: Math.min(implicitWidth, parent.width)
            onClicked: scene.recordRequested()
        }
        WorldButton {
            objectName: "homeComputer"
            visible: scene.showComputer
            text: scene.computerLabel; textSize: scene.textSize
            width: Math.min(implicitWidth, parent.width)
            onClicked: scene.computerRequested()
        }
    }
}
