import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var infra: campaign.infrastructure
    readonly property bool editable: campaign.paused && !campaign.busy && campaign.canAdvance
    property string section: "map"
    property bool showMap: true
    property var comparison: ({})
    property string comparisonDate: ""
    readonly property var gameOverlay: Overlay.overlay
    readonly property var stateRow: infra.states.selectedRow
    readonly property var siteRow: infra.sites.selectedRow
    readonly property var hardwareRow: infra.hardware.selectedRow
    readonly property var calendarRow: infra.calendar.selectedRow
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Copy { font.bold: true; color: "#ffd897" }
    component Action: Button {
        id: action
        Layout.maximumWidth: panel.width
        font.pixelSize: panel.textSize
        Accessible.name: text
        contentItem: Text {
            text: action.text; textFormat: Text.PlainText; font: action.font
            color: action.enabled ? "#f2f4f7" : "#aebbc7"
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.Wrap
        }
    }
    component Field: ColumnLayout {
        id: field
        property string label
        property string inputName
        property alias value: edit.text
        property int maximumLength: 96
        Layout.fillWidth: true
        Copy { text: field.label }
        TextField {
            id: edit
            objectName: field.inputName
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Accessible.name: field.label; font.pixelSize: panel.textSize
            selectByMouse: true; maximumLength: field.maximumLength
        }
    }
    component Picker: ColumnLayout {
        id: picker
        property string label
        property string inputName
        property var options: []
        property string value: options.length ? options[0].id : ""
        onOptionsChanged: if (!options.some(o => o.id === value)) value = options.length ? options[0].id : ""
        objectName: inputName + "Picker"
        Layout.fillWidth: true
        Copy { text: picker.label }
        ComboBox {
            objectName: picker.inputName
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Accessible.name: picker.label; font.pixelSize: panel.textSize
            model: picker.options; textRole: "label"; valueRole: "id"
            currentIndex: picker.options.findIndex(o => o.id === picker.value)
            onActivated: index => picker.value = picker.options[index].id
        }
    }
    component Selection: ColumnLayout {
        id: selection
        required property var rows
        required property string emptyText
        property string listName
        property bool showDetail: true
        property bool reuseRows: true
        Layout.fillWidth: true; Layout.minimumWidth: 0
        StableEntityList {
            objectName: selection.listName
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(250, Math.max(90, selection.rows.count * panel.textSize * 4))
            rows: selection.rows; textSize: panel.textSize; reducedMotion: campaign.reducedMotion; emptyText: selection.emptyText
            reuseItems: selection.reuseRows
            onSelected: id => selection.rows.selectedId = id
        }
        Copy { visible: selection.showDetail && text.length > 0; text: selection.rows.selectedRow.detail || "" }
    }

    Dialog {
        id: review
        objectName: "infrastructureReview"
        title: qsTr("Review infrastructure terms")
        modal: true
        anchors.centerIn: panel.gameOverlay
        width: Math.min(720, (panel.gameOverlay ? panel.gameOverlay.width : 720) - 32)
        font.pixelSize: panel.textSize
        contentItem: ScrollView {
            implicitHeight: Math.min(reviewCopy.implicitHeight + 8, (panel.gameOverlay ? panel.gameOverlay.height : 720) * 0.53)
            contentWidth: availableWidth
            clip: true
            Label {
                id: reviewCopy
                objectName: "infrastructureReviewText"
                width: parent.width
                font.pixelSize: panel.textSize; text: infra.reviewText; wrapMode: Text.Wrap; textFormat: Text.PlainText
            }
        }
        footer: DialogButtonBox {
            Button { objectName: "confirmInfrastructure"; text: qsTr("Confirm"); font.pixelSize: panel.textSize; enabled: infra.canConfirm && panel.editable; onClicked: { infra.confirm(); review.close() } }
            Button { objectName: "dismissInfrastructure"; text: qsTr("Back"); font.pixelSize: panel.textSize; onClicked: review.close() }
        }
        onClosed: infra.dismissReview()
    }
    Dialog {
        id: credits
        objectName: "mapCreditsDialog"
        title: qsTr("Domestic map credits")
        modal: true
        anchors.centerIn: panel.gameOverlay
        width: Math.min(740, (panel.gameOverlay ? panel.gameOverlay.width : 720) - 32)
        font.pixelSize: panel.textSize
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            implicitHeight: Math.min(creditCopy.implicitHeight + 16, (panel.gameOverlay ? panel.gameOverlay.height : 720) * 0.54)
            contentWidth: availableWidth; clip: true
            ColumnLayout {
                id: creditCopy
                width: parent.width
                Copy { text: infra.mapCredit + "\n\n" + infra.mapTerms }
                Action { text: qsTr("Open Census source in browser"); visible: infra.mapSource.toString().length > 0; onClicked: Qt.openUrlExternally(infra.mapSource) }
            }
        }
    }
    Connections {
        target: infra
        function onCatalogChanged() { panel.comparison = ({}); stateSearch.text = "" }
    }

    Heading { text: qsTr("Operations · domestic sites") }
    Copy { objectName: "executiveLocation"; text: infra.executiveLocation }
    Copy { visible: !campaign.paused; color: "#ffd897"; text: qsTr("Pause time to change infrastructure commitments.") }
    Flow {
        Layout.fillWidth: true; spacing: 6
        Action { objectName: "operationsMap"; text: qsTr("Map & states"); checked: panel.section === "map"; onClicked: panel.section = "map" }
        Action { objectName: "operationsSites"; text: qsTr("Owned sites"); checked: panel.section === "sites"; onClicked: panel.section = "sites" }
        Action { objectName: "operationsCalendar"; text: qsTr("Calendar"); checked: panel.section === "calendar"; onClicked: panel.section = "calendar" }
        Action { objectName: "operationsWholesale"; text: qsTr("Wholesale"); visible: infra.wholesalePlans.length > 0; checked: panel.section === "wholesale"; onClicked: panel.section = "wholesale" }
        Action { text: qsTr("Activity"); checked: panel.section === "activity"; onClicked: panel.section = "activity" }
        Action { objectName: "nextSiteMilestone"; text: qsTr("Next site milestone"); enabled: panel.editable; onClicked: campaign.advanceTo("site") }
    }

    ColumnLayout {
        visible: panel.section === "map"
        Layout.fillWidth: true; spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Action { objectName: "toggleStateMap"; text: panel.showMap ? qsTr("List only") : qsTr("Show map"); onClicked: panel.showMap = !panel.showMap }
            Action { objectName: "showMapCredits"; text: qsTr("Map credits"); onClicked: credits.open() }
        }
        GridLayout {
            Layout.fillWidth: true; columnSpacing: 12
            columns: panel.showMap && panel.width >= 820 ? 2 : 1
            ColumnLayout {
                visible: panel.showMap
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 550
                DomesticMap {
                    objectName: "domesticMap"
                    visible: !!geometry.features
                    Layout.fillWidth: true; Layout.preferredHeight: panel.textSize >= 24 ? 305 : 355
                    geometry: infra.mapGeometry; selectedId: infra.states.selectedId
                    onStatePicked: id => {
                        if (infra.states.indexOf(id) < 0)
                            stateSearch.text = ""
                        infra.states.selectedId = id
                    }
                }
                Copy { text: infra.mapGeometry.features ? qsTr("Alaska and Hawaii shown as insets. DC uses a larger marker.") : qsTr("The map is unavailable. Choose a jurisdiction from the state list.") }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 385
                Heading { text: qsTr("State list · 50 states and DC") }
                TextField {
                    id: stateSearch; objectName: "stateSearch"
                    Layout.fillWidth: true; placeholderText: qsTr("Filter by state name or code")
                    Accessible.name: placeholderText; font.pixelSize: panel.textSize
                    onTextChanged: infra.states.filterText = text
                }
                Selection { rows: infra.states; emptyText: qsTr("Domestic states"); showDetail: false; reuseRows: false; listName: "domesticStateList" }
            }
        }
        Copy { text: infra.assumptions }
        GridLayout {
            Layout.fillWidth: true; columns: !!panel.comparison.entityId && panel.width >= 820 ? 2 : 1
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Heading { text: panel.stateRow.title || qsTr("Choose a state") }
                Copy { objectName: "selectedStateProfile"; text: panel.stateRow.detail || "" }
            }
            ColumnLayout {
                visible: !!panel.comparison.entityId
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Heading { text: qsTr("%1 · pinned %2").arg(panel.comparison.title || "", panel.comparisonDate) }
                Copy { text: panel.comparison.detail || "" }
            }
        }
        Flow {
            Layout.fillWidth: true; spacing: 6
            Action { text: qsTr("Pin state for comparison"); enabled: !!panel.stateRow.entityId; onClicked: { panel.comparison = panel.stateRow; panel.comparisonDate = campaign.date } }
            Action { text: qsTr("Clear comparison"); visible: !!panel.comparison.entityId; onClicked: panel.comparison = ({}) }
        }
        Heading { text: qsTr("Order site construction") }
        GridLayout {
            Layout.fillWidth: true; columns: panel.width >= 720 ? 3 : 1
            Field { id: siteName; inputName: "ownedSiteName"; label: qsTr("Site name"); value: qsTr("First compute site") }
            Field { id: siteGrid; inputName: "ownedSiteGrid"; label: qsTr("Grid reservation (whole kW)"); value: "20"; maximumLength: 4 }
            Picker { id: cooling; inputName: "ownedSiteCooling"; label: qsTr("Cooling"); options: [{id: "air", label: qsTr("Air")}, {id: "evaporative", label: qsTr("Evaporative")} ] }
        }
        Action { objectName: "reviewBuildOwnedSite"; text: qsTr("Review construction"); enabled: panel.editable && !!infra.states.selectedId; onClicked: { infra.reviewBuild(infra.states.selectedId, siteName.value, siteGrid.value, cooling.value); review.open() } }
    }
    ColumnLayout {
        visible: panel.section === "sites"
        Layout.fillWidth: true; spacing: 9
        Heading { text: qsTr("Owned sites") }
        Selection { rows: infra.sites; emptyText: qsTr("No owned sites yet. Choose a state and order construction."); listName: "ownedSiteList" }
        ColumnLayout {
            visible: !!infra.sites.selectedId && !(panel.siteRow.fields || {}).closed
            Layout.fillWidth: true
            Heading { text: qsTr("Commit resources to %1").arg(panel.siteRow.title || "") }
            Picker { id: rack; inputName: "ownedRackFamily"; label: qsTr("Hardware family"); options: infra.rackOptions }
            Copy { text: (rack.options.find(o => o.id === rack.value) || {}).detail || "" }
            Action { objectName: "reviewOrderOwnedRack"; text: qsTr("Review hardware order"); enabled: panel.editable && !!rack.value; onClicked: { infra.reviewHardware(infra.sites.selectedId, rack.value); review.open() } }
            GridLayout {
                Layout.fillWidth: true; columns: panel.width >= 650 ? 2 : 1
                Field { id: operators; inputName: "ownedOperatorCount"; label: qsTr("Dedicated operators"); value: "1"; maximumLength: 2 }
                Action { objectName: "reviewHireOwnedOperators"; text: qsTr("Review operator hire"); enabled: panel.editable; onClicked: { infra.reviewHire(infra.sites.selectedId, operators.value); review.open() } }
                Field { id: budget; inputName: "ownedPowerBudget"; label: qsTr("Facility power budget (whole kW, 0 to power off)"); value: (panel.siteRow.fields || {}).powerKw || "0"; maximumLength: 4 }
                Action { objectName: "reviewOwnedPower"; text: qsTr("Review power budget"); enabled: panel.editable; onClicked: { infra.reviewPower(infra.sites.selectedId, budget.value); review.open() } }
            }
            Flow {
                Layout.fillWidth: true; spacing: 6
                Action { text: qsTr("Book an inspection"); onClicked: panel.section = "calendar" }
                Action { objectName: "reviewCloseOwnedSite"; text: qsTr("Review site closure"); enabled: panel.editable; onClicked: { infra.reviewClose(infra.sites.selectedId); review.open() } }
            }
        }
        Heading { text: qsTr("Paid hardware") }
        Selection { rows: infra.hardware; emptyText: qsTr("No hardware orders"); listName: "ownedHardwareList" }
        Action { objectName: "reviewRetireOwnedRack"; text: qsTr("Review retirement"); enabled: panel.editable && !!infra.hardware.selectedId && !(panel.hardwareRow.fields || {}).retired; onClicked: { infra.reviewRetire(infra.hardware.selectedId); review.open() } }
        Heading { text: qsTr("Opening cloud lease") }
        Copy { objectName: "openingLeaseStatus"; text: infra.leaseStatus }
        Action { objectName: "reviewEndOpeningLease"; text: qsTr("Review lease notice"); enabled: panel.editable && infra.canNoticeLease; onClicked: { infra.reviewLease(); review.open() } }
    }
    ColumnLayout {
        visible: panel.section === "calendar"
        Layout.fillWidth: true; spacing: 9
        Heading { text: qsTr("Inspection and travel calendar") }
        Copy { text: qsTr("Every site needs one inspection before running hardware. Remote and delegated appointments keep the executive at base; on-site visits include the trip. You can manage the company throughout.") }
        ComboBox {
            objectName: "inspectionSite"
            Layout.fillWidth: true; Layout.minimumWidth: 0
            model: infra.sites; textRole: "title"; valueRole: "entityId"; currentIndex: infra.sites.indexOf(infra.sites.selectedId)
            Accessible.name: qsTr("Site to inspect"); font.pixelSize: panel.textSize
            onActivated: index => infra.sites.selectedId = infra.sites.idAt(index)
        }
        GridLayout {
            Layout.fillWidth: true; columns: panel.width >= 760 ? 2 : 1
            Picker { id: method; inputName: "inspectionMethod"; label: qsTr("Inspection method"); options: [{id: "remote", label: qsTr("Remote")}, {id: "onsite", label: qsTr("On site")}, {id: "delegated", label: qsTr("Delegated")} ] }
            Field { id: visit; inputName: "inspectionDate"; label: qsTr("Inspection date (YYYY-MM-DD)"); value: infra.suggestedVisit(infra.sites.selectedId, method.value); maximumLength: 10 }
        }
        Flow {
            Layout.fillWidth: true; spacing: 6
            Action { text: qsTr("Suggest earliest date"); enabled: !!infra.sites.selectedId; onClicked: visit.value = infra.suggestedVisit(infra.sites.selectedId, method.value) }
            Action { objectName: "reviewSiteInspection"; text: qsTr("Review inspection"); enabled: panel.editable && !!infra.sites.selectedId && !(panel.siteRow.fields || {}).closed; onClicked: { infra.reviewAppointment(infra.sites.selectedId, visit.value, method.value); review.open() } }
        }
        Selection { rows: infra.calendar; emptyText: qsTr("No appointments booked"); listName: "siteCalendarList" }
        Action { objectName: "reviewCancelInspection"; text: qsTr("Review cancellation"); enabled: panel.editable && !!(panel.calendarRow.fields || {}).cancelable; onClicked: { infra.reviewCancel(infra.calendar.selectedId); review.open() } }
    }
    ColumnLayout {
        visible: panel.section === "wholesale" && infra.wholesalePlans.length > 0
        Layout.fillWidth: true
        Copy { text: qsTr("Sell powered rack capacity to an operating company for a fixed term. The buyer funds the reservation up front and earns fee credits during curtailment. Actual buyer work draws electricity at your site.") }
        Copy { visible: infra.wholesaleSources.length === 0; text: qsTr("Commission an owned rack to offer a reservation.") }
        Picker { id: wholesalePlan; inputName: "wholesalePlan"; label: qsTr("Reservation terms"); options: infra.wholesalePlans }
        Picker { id: wholesaleRack; inputName: "wholesaleRack"; label: qsTr("Commissioned rack"); options: infra.wholesaleSources }
        Picker { id: wholesaleBuyer; inputName: "wholesaleBuyer"; label: qsTr("Operating buyer"); options: infra.wholesaleBuyers }
        Action {
            objectName: "reviewWholesale"
            text: qsTr("Review wholesale agreement")
            enabled: panel.editable && wholesalePlan.value.length > 0 && wholesaleRack.value.length > 0 && wholesaleBuyer.value.length > 0
            onClicked: { infra.reviewWholesale(wholesalePlan.value, wholesaleRack.value, wholesaleBuyer.value); review.open() }
        }
        Heading { text: qsTr("Wholesale agreements") }
        Selection { rows: infra.wholesaleContracts; listName: "wholesaleContracts"; emptyText: qsTr("No wholesale agreements yet.") }
    }
    ColumnLayout {
        visible: panel.section === "activity"
        Layout.fillWidth: true
        Heading { text: qsTr("Recent infrastructure updates") }
        Selection { rows: infra.notices; emptyText: qsTr("No site activity yet"); listName: "infrastructureActivity" }
    }
}
