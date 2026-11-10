import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GameDialog {
    id: panel
    required property int textSize
    required property bool reducedMotion
    objectName: "musicCreditsDialog"
    title: qsTr("Music and credits")
    modal: true
    padding: 14
    font.pixelSize: textSize
    closePolicy: Popup.CloseOnEscape

    function reveal(item) {
        let ancestor = item
        while (ancestor && ancestor !== creditPage)
            ancestor = ancestor.parent
        if (!ancestor)
            return
        const y = item.mapToItem(creditPage, 0, 0).y
        const top = viewport.contentY
        const target = y < top ? y : y + item.height > top + scroll.availableHeight
                                     ? y + Math.min(item.height, scroll.availableHeight) - scroll.availableHeight : top
        viewport.contentY = Math.max(0, Math.min(target, creditPage.height - scroll.availableHeight))
    }

    component Copy: Label {
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentItem: Flickable {
            id: viewport
            contentHeight: creditPage.height
            boundsBehavior: panel.reducedMotion ? Flickable.StopAtBounds : Flickable.DragOverBounds
        }
        ColumnLayout {
            id: creditPage
            width: scroll.availableWidth
            height: implicitHeight
            spacing: 10
            GameCheck {
                id: enableMusic
                objectName: "musicEnabledControl"
                text: qsTr("Play background music")
                Accessible.name: text
                enabled: !campaign.busy
                checked: campaign.musicEnabled
                onClicked: campaign.musicEnabled = checked
            }
            Copy { text: soundtrack.status }
            Label {
                text: qsTr("Music volume: %1%").arg(volume.value)
                font.pixelSize: panel.textSize
            }
            GameSlider {
                id: volume
                objectName: "musicVolumeControl"
                Layout.fillWidth: true
                Accessible.name: qsTr("Music volume")
                enabled: !campaign.busy
                from: 0
                to: 100
                stepSize: 5
                live: false
                value: campaign.musicVolume
                onMoved: campaign.musicVolume = Math.round(value)
            }
            GameSelect {
                id: track
                objectName: "musicTrackControl"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Accessible.name: qsTr("Soundtrack credits by track")
                model: soundtrack.credits
                textRole: "title"
                font.pixelSize: panel.textSize
            }
            Copy {
                readonly property var credit: track.currentIndex < 0 ? null : soundtrack.credits[track.currentIndex]
                text: credit ? qsTr("Music by %1 · %2\nExcerpt edited for length, level and fades.")
                    .arg(credit.creator).arg(credit.license_id) : qsTr("No soundtrack credits available.")
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                WorldButton {
                    objectName: "musicExcerptControl"
                    text: qsTr("Play selected excerpt")
                    Accessible.name: text
                    enabled: campaign.musicEnabled && track.currentIndex >= 0
                    onClicked: soundtrack.playExcerpt(track.currentIndex)
                }
                WorldButton {
                    text: qsTr("Track source")
                    Accessible.name: qsTr("Open the selected music source in your browser")
                    enabled: track.currentIndex >= 0
                    onClicked: soundtrack.openTrackSource(track.currentIndex)
                }
            }
            Copy {
                text: qsTr("Domestic map: U.S. Census Bureau, 2024 Cartographic Boundary Files, 1:5,000,000. Prepared geometry with separately scaled Alaska and Hawaii insets and an enlarged DC marker. County populations and city context: U.S. Census Bureau, 2010 Gazetteer Files.")
            }
            Copy {
                text: qsTr("Portrait head and hair shapes: Bitclone Basic Face Kit 2 by VexStrips / future, Openclipart CC0. Laptop icon: b.gaultier; browser icon: m1981; mail icon: creator unnamed in the supplied SVG. These UI icons retain their embedded Openclipart public-domain declarations. Equipment illustrations and complementary portrait layers are original game artwork.")
            }
            Copy {text:qsTr("Money artwork: Credit Card by Algot Runeman (2020), supplied SVG with CC0 1.0 declaration. Checkbook and receipt: unnamed creators, supplied SVGs with Openclipart public-domain declarations. Receipt example text removed; actual financial values are rendered by the game.")}
            Copy {
                text: qsTr("Project Glowing Pancake uses GPLv3. The game source and asset preparation records are available at github.com/gnaservicesinc/Project-Glowing-Pancake. Qt, nlohmann/json, miniaudio and stb_vorbis retain their own licenses.")
            }
            Copy {
                text: qsTr("Qt Core, Gui, QML, Quick and Quick Controls use the Qt open source license terms. Qt sources and third-party notices are available at download.qt.io/archive/qt and doc.qt.io/qt-6.12/licensing.html.")
            }
            GameSelect {
                id: notice
                objectName: "licenseSelector"
                Layout.fillWidth: true
                Accessible.name: qsTr("Bundled license notices")
                model: [qsTr("Music · CC BY 4.0"), qsTr("Domestic map terms"), qsTr("Game · GPLv3"),
                        qsTr("Qt · LGPLv3"), qsTr("nlohmann/json · MIT"),
                        qsTr("miniaudio · MIT-0"), qsTr("stb_vorbis · MIT")]
                font.pixelSize: panel.textSize
            }
            TextArea {
                color: "#e8e6d3"
                selectionColor: "#52746b"
                background: Rectangle { radius: 7; color: "#193539"; border.color: "#59776c" }
                objectName: "licenseText"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Accessible.name: qsTr("License text")
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                font.pixelSize: panel.textSize
                text: panel.visible ? soundtrack.licenseText(notice.currentIndex) : ""
            }
        }
    }
    footer: DialogButtonBox {
        WorldButton {
            objectName: "closeMusicCredits"
            text: qsTr("Close")
            Accessible.name: text
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
    onOpened: enableMusic.forceActiveFocus()
}
