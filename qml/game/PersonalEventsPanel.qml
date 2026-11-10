import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var events: campaign.world.personalEvents
    readonly property bool editable: campaign.paused && campaign.companyEditable && !campaign.busy && !campaign.storageBlocked
    property string packageId: campaign.world.draft("events/package", "office_party")
    property int propertyId: Number(campaign.world.draft("events/venue", "0"))
    property var invited: []
    readonly property var selectedPackage: (events.packages || []).find(row => row.id === packageId) || ({})
    readonly property int venue: selectedPackage.venue === "office" ? 0 : propertyId
    readonly property var quote: {
        campaign.world.reviewToken
        return campaign.world.personalEventQuote(packageId,venue,eventDate.text,invited)
    }
    spacing: 12
    DraftState {
        id: guestsDraft
        draftKey: "personal-event/guests"
        accepts: value => Array.isArray(value) && value.length <= 32 && value.every(id => typeof id === "string") && new Set(value).size === value.length
        onRestored: value => panel.invited = value.filter(id => (panel.events.guests || []).some(person => person.id === id))
        onReset: panel.invited = []
    }
    function toggle(id) {
        invited = invited.includes(id) ? invited.filter(person => person !== id) : invited.concat(id)
        guestsDraft.remember(invited)
    }
    function reviewCancellation(event) {
        confirmation.reading=false
        confirmation.eventId=event.id
        confirmation.revision=campaign.world.reviewToken
        confirmation.copy=qsTr("Cancel %1 on %2? The refund to your personal savings is %3.").arg(event.name).arg(event.day).arg(event.cancellation)
        confirmation.open()
    }
    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
        color: "#d0e0d4"
    }
    Copy { text: qsTr("Gather people around a table"); font.bold: true; font.pixelSize: panel.textSize*1.25; color: "#f1e3bc" }
    Copy { text: panel.events.summary || "" }
    GameSelect {
        objectName: "gatheringPackage"
        Layout.fillWidth: true
        textSize: panel.textSize
        model: panel.events.packages || []
        textRole: "name"; valueRole: "id"
        currentIndex: model.findIndex(row => row.id === panel.packageId)
        Accessible.name: qsTr("Gathering package")
        onActivated: { panel.packageId=currentValue; campaign.world.setDraft("events/package",currentValue) }
    }
    Copy { text: (panel.selectedPackage.description || "") + "\n" + (panel.selectedPackage.terms || "") }
    Copy { visible: panel.selectedPackage.venue === "office"; text: qsTr("Venue · your current headquarters. You are personally paying for this office gathering.") }
    GameSelect {
        objectName: "gatheringVenue"
        visible: panel.selectedPackage.venue === "home"
        Layout.fillWidth: true
        textSize: panel.textSize
        model: panel.events.venues || []
        textRole: "name"; valueRole: "id"
        currentIndex: model.findIndex(row => row.id === panel.propertyId)
        Accessible.name: qsTr("Owned home for the gathering")
        onActivated: { panel.propertyId=currentValue; campaign.world.setDraft("events/venue",String(currentValue)) }
    }
    Copy { visible: panel.selectedPackage.venue === "home" && (panel.events.venues || []).length === 0; text: qsTr("Buy a personal home to host this gathering.") }
    Copy { text: qsTr("Event date · YYYY-MM-DD") }
    DraftField {
        id: eventDate
        objectName: "gatheringDate"
        Layout.fillWidth: true
        defaultText: panel.selectedPackage.date || ""
        font.pixelSize: panel.textSize
        Accessible.name: qsTr("Gathering date")
    }
    Copy { text: qsTr("Invite your contacts · %1 selected").arg(panel.invited.length); font.bold: true }
    Copy { visible: (panel.events.guests || []).length === 0; text: qsTr("Meet an employee at HQ and exchange contact details, or continue a conversation from the home opening.") }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: panel.events.guests || []
            WorldButton {
                required property var modelData
                objectName: "gatheringGuest_" + modelData.id
                width: Math.min(panel.width,Math.max(190,panel.textSize*12))
                height: guestCopy.implicitHeight+16
                padding: 8
                text: modelData.name
                textSize: panel.textSize
                checked: panel.invited.includes(modelData.id)
                Accessible.checkable: true
                Accessible.checked: checked
                contentItem: RowLayout {
                    id: guestCopy
                    Image {visible:!!modelData.portrait;source:modelData.portrait || "";Layout.preferredWidth:panel.textSize*2;Layout.preferredHeight:Layout.preferredWidth;fillMode:Image.PreserveAspectFit}
                    ColumnLayout {
                        Layout.fillWidth: true
                        Copy { text: (panel.invited.includes(modelData.id)?"✓ ":"+ ")+modelData.name; font.bold: true }
                        Copy { text: modelData.role || ""; font.pixelSize: panel.textSize*0.8 }
                    }
                }
                onClicked: panel.toggle(modelData.id)
            }
        }
    }
    Copy {
        objectName: "gatheringQuote"
        text: panel.quote.cost ? qsTr("Booking total %1 · cancellation refund %2\n%3").arg(panel.quote.cost).arg(panel.quote.refund).arg(panel.quote.reason)
                              : panel.quote.reason || ""
        color: panel.quote.valid ? "#bddda7" : "#ecc299"
    }
    WorldButton {
        objectName: "gatheringBook"
        text: qsTr("Review booking and invitations")
        textSize: panel.textSize
        Layout.maximumWidth: panel.width
        enabled: panel.editable && !!panel.quote.valid
        onClicked: {
            confirmation.reading=false
            confirmation.eventId=0
            confirmation.revision=campaign.world.reviewToken
            confirmation.packageId=panel.packageId
            confirmation.propertyId=panel.venue
            confirmation.day=eventDate.text
            confirmation.guests=panel.invited.slice()
            confirmation.copy=qsTr("%1 · %2\nPay %3 from personal savings. Cancellation before the date returns %4.\n\n%5\n\nInvitations are answered at booking. Current colleagues who finish onboarding by the event date and signed opening partners accept. Other opening contacts decline. Check attendance after the event.")
                .arg(panel.selectedPackage.name).arg(eventDate.text).arg(panel.quote.cost).arg(panel.quote.refund)
                .arg((panel.events.guests || []).filter(person=>panel.invited.includes(person.id)).map(person=>person.name).join("\n"))
            confirmation.open()
        }
    }
    WorldButton {
        objectName: "gatheringAdvance"
        visible: !!panel.events.upcoming
        text: qsTr("Advance to next gathering")
        textSize: panel.textSize
        Layout.maximumWidth: panel.width
        enabled: panel.editable
        onClicked: campaign.advanceTo("gathering")
    }
    Copy { text: qsTr("Your calendar and recent gatherings"); font.bold: true; font.pixelSize: panel.textSize*1.15 }
    Repeater {
        model: panel.events.history || []
        Rectangle {
            required property var modelData
            objectName: "gatheringRecord_" + modelData.id
            Layout.fillWidth: true
            implicitHeight: recordCopy.implicitHeight+24
            color: "#234247"; border.color: "#64877e"; radius: 10
            ColumnLayout {
                id: recordCopy
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12
                RowLayout {
                    Layout.fillWidth:true
                    EquipmentGlyph {kind:"table";Layout.preferredWidth:panel.textSize*2;Layout.preferredHeight:Layout.preferredWidth}
                    Copy { text: modelData.name+" · "+modelData.day; font.bold: true; color: "#f1e3bc" }
                }
                Copy { text: modelData.status+" · "+modelData.venue }
                Copy { text: qsTr("Paid %1 · Refunded %2").arg(modelData.paid).arg(modelData.refund) }
                WorldButton {
                    objectName:"gatheringRead_"+modelData.id
                    visible:!!modelData.story
                    text:qsTr("Read about the evening")
                    textSize:panel.textSize
                    Layout.maximumWidth:recordCopy.width
                    onClicked:{confirmation.reading=true;confirmation.copy=modelData.story;confirmation.open()}
                }
                Repeater {
                    model: modelData.guests
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth:true
                        Image {visible:!!modelData.portrait;source:modelData.portrait || "";Layout.preferredWidth:panel.textSize*1.8;Layout.preferredHeight:Layout.preferredWidth;fillMode:Image.PreserveAspectFit}
                        Copy {text:modelData.name+" · "+modelData.status}
                    }
                }
                WorldButton {
                    objectName: "gatheringCancel_" + modelData.id
                    visible: modelData.pending
                    text: qsTr("Review cancellation")
                    textSize: panel.textSize
                    enabled: panel.editable
                    onClicked: panel.reviewCancellation(modelData)
                }
            }
        }
    }
    GameDialog {
        id: confirmation
        property int eventId: 0
        property bool reading: false
        property string revision: ""
        property string packageId: ""
        property int propertyId: 0
        property string day: ""
        property var guests: []
        property string copy: ""
        title: reading?qsTr("The evening"):eventId ? qsTr("Cancel gathering") : qsTr("Host a gathering")
        modal: true; focus: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(Overlay.overlay.width-32,panel.textSize*38)
        height: Math.min(Overlay.overlay.height-32,panel.textSize*28)
        header: Item {visible:false;implicitHeight:0}
        onOpened: if(confirmationScroll.contentItem)confirmationScroll.contentItem.contentY=0
        enter: Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:campaign.reducedMotion?0:150}}
        exit: Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:campaign.reducedMotion?0:100}}
        contentItem: ScrollView {
            id: confirmationScroll
            contentWidth: availableWidth; clip: true
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            ColumnLayout {
                width: confirmationScroll.availableWidth
                Copy { text: confirmation.title; font.bold:true; font.pixelSize:panel.textSize*1.2; color:"#f1e3bc" }
                Copy { text: confirmation.copy }
            }
        }
        footer: Flow {
            width: confirmation.availableWidth; padding: 8; spacing: 8
            WorldButton {text:confirmation.reading?qsTr("Close"):qsTr("Back");textSize:panel.textSize;onClicked:confirmation.close()}
            WorldButton {
                objectName: "gatheringConfirm"
                visible:!confirmation.reading
                text: confirmation.eventId ? qsTr("Cancel and refund") : qsTr("Pay and send invitations")
                width: Math.min(implicitWidth,parent.width-16)
                textSize: panel.textSize; primary: true; enabled: panel.editable
                onClicked: {
                    if(confirmation.eventId)campaign.world.cancelPersonalEvent(confirmation.eventId,confirmation.revision)
                    else campaign.world.bookPersonalEvent(confirmation.packageId,confirmation.propertyId,confirmation.day,confirmation.guests,confirmation.revision)
                    confirmation.close()
                }
            }
        }
    }
}
