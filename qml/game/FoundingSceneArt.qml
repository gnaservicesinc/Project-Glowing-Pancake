import QtQuick

// Layered reactive backdrop combining curated SVGs with section and choice mood layers.
Item {
    id: root
    required property string art
    required property string tone
    property string mood: "calm"
    property string tint: ""
    property string section: ""
    property bool reducedMotion: false

    GameInk { id: ink; tone: root.tone }
    readonly property color moodColor: root.tint.length > 0 ? root.tint : ink.accent
    clip: true

    Rectangle { anchors.fill: parent; color: "#111d2a" }

    Image {
        id: scene
        objectName: "foundingScene"
        anchors.fill: parent
        source: "qrc:/pgp/assets/founding/" + root.art + ".svg"
        fillMode: Image.PreserveAspectCrop
        horizontalAlignment: Image.AlignRight
        Behavior on opacity { NumberAnimation { duration: root.reducedMotion ? 0 : 180 } }
    }

    // Reactive atmospheric light & choice tint wash over the right focal bay.
    Rectangle {
        anchors.fill: parent
        opacity: root.mood === "turbulent" ? 0.30 : 0.22
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.55; color: Qt.rgba(root.moodColor.r, root.moodColor.g, root.moodColor.b, 0.25) }
            GradientStop { position: 1.0; color: Qt.rgba(root.moodColor.r, root.moodColor.g, root.moodColor.b, 0.72) }
        }
    }

    // Layer 1: Academic / theorem grid & glowing arc (genius, prodigy, research labs)
    Item {
        anchors.fill: parent
        visible: root.mood === "academic"
        opacity: 0.24
        Repeater {
            model: 5
            Rectangle {
                required property int index
                x: parent.width * 0.54
                y: parent.height * (0.14 + index * 0.14)
                width: parent.width * 0.42
                height: 1
                color: root.moodColor
            }
        }
        Rectangle {
            x: parent.width * 0.62; y: parent.height * 0.18
            width: Math.min(parent.width * 0.28, 260); height: width; radius: width / 2
            color: "transparent"; border.color: root.moodColor; border.width: 1
        }
    }

    // Layer 2: Athletic stadium sunbeams
    Item {
        anchors.fill: parent
        visible: root.mood === "athletic"
        opacity: 0.22
        Repeater {
            model: 3
            Rectangle {
                required property int index
                x: parent.width * (0.58 + index * 0.13)
                y: -parent.height * 0.1
                width: Math.max(24, parent.width * 0.05)
                height: parent.height * 1.2
                rotation: -16
                gradient: Gradient {
                    GradientStop { position: 0.0; color: root.moodColor }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }
    }

    // Layer 3: Social / civic warm bokeh lights
    Item {
        anchors.fill: parent
        visible: root.mood === "social" || root.mood === "civic"
        opacity: 0.26
        Repeater {
            model: [
                { rx: 0.68, ry: 0.22, sz: 0.11 },
                { rx: 0.84, ry: 0.38, sz: 0.08 },
                { rx: 0.76, ry: 0.62, sz: 0.13 },
                { rx: 0.90, ry: 0.16, sz: 0.06 }
            ]
            Rectangle {
                required property var modelData
                width: Math.min(parent.width, parent.height) * modelData.sz
                height: width; radius: width / 2
                x: parent.width * modelData.rx - width / 2
                y: parent.height * modelData.ry - height / 2
                color: root.moodColor
            }
        }
    }

    // Layer 4: Creative prism accents
    Item {
        anchors.fill: parent
        visible: root.mood === "creative"
        opacity: 0.22
        Repeater {
            model: ["#efb1a0", "#d7b9dd", "#9ad6bc", "#efc389"]
            Rectangle {
                required property color modelData
                required property int index
                x: parent.width * (0.64 + index * 0.075)
                y: parent.height * 0.12
                width: Math.max(12, parent.width * 0.035)
                height: parent.height * 0.52
                radius: width / 2
                rotation: 12
                color: modelData
            }
        }
    }

    // Layer 5: Turbulent dramatic vignette slash (bullying, setbacks, substance stress)
    Rectangle {
        anchors.fill: parent
        visible: root.mood === "turbulent"
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#38120c10" }
            GradientStop { position: 0.5; color: "transparent" }
            GradientStop { position: 1.0; color: "#50280f12" }
        }
    }

    // Layer 6: Service / tactical horizon coordinate bars
    Item {
        anchors.fill: parent
        visible: root.mood === "service"
        opacity: 0.24
        Rectangle {
            x: parent.width * 0.56; y: parent.height * 0.48
            width: parent.width * 0.38; height: 2
            color: root.moodColor
        }
        Rectangle {
            x: parent.width * 0.70; y: parent.height * 0.24
            width: 2; height: parent.height * 0.42
            color: root.moodColor
        }
    }

    // Layer 7: Section-era architectural accents so each school chapter feels distinct.
    Item {
        anchors.fill: parent
        visible: root.section.length > 0
        opacity: 0.14
        Rectangle {
            visible: root.section === "elementary"
            x: parent.width * 0.56; y: parent.height * 0.14
            width: parent.width * 0.36; height: parent.height * 0.50; radius: 10
            color: "transparent"; border.color: root.moodColor; border.width: 2
        }
        Row {
            visible: root.section === "middle" || root.section === "undergrad"
            x: parent.width * 0.58; y: parent.height * 0.18
            spacing: Math.max(18, parent.width * 0.04)
            Repeater {
                model: root.section === "undergrad" ? 4 : 6
                Rectangle {
                    width: root.section === "undergrad" ? 10 : 4
                    height: parent.parent.height * 0.48
                    radius: 2
                    color: root.moodColor
                }
            }
        }
        Rectangle {
            visible: root.section === "high"
            x: parent.width * 0.52; y: parent.height * 0.66
            width: parent.width * 0.44; height: 2
            color: root.moodColor
        }
        Rectangle {
            visible: root.section === "graduation"
            x: parent.width * 0.68; y: parent.height * 0.10
            width: Math.min(parent.width * 0.28, 300); height: width; radius: width / 2
            color: "transparent"; border.color: root.moodColor; border.width: 3
        }
        Item {
            visible: root.section === "grad"
            anchors.fill: parent
            Rectangle {
                x: parent.width * 0.64; y: parent.height * 0.22
                width: Math.min(parent.width * 0.24, 240); height: width; radius: width / 2
                color: "transparent"; border.color: root.moodColor; border.width: 2
            }
            Rectangle {
                x: parent.width * 0.68; y: parent.height * 0.28
                width: Math.min(parent.width * 0.18, 170); height: width; radius: width / 2
                color: "transparent"; border.color: root.moodColor; border.width: 1
            }
        }
    }
}
