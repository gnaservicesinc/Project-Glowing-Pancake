import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property int textSize
    readonly property var frontier: campaign.frontier
    readonly property bool editable: frontier.editable && !campaign.busy && campaign.companyEditable
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Copy { font.bold: true }
    component Action: Button { font.pixelSize: panel.textSize }
    component Picker: DraftComboBox {
        id: picker
        draftOwner: picker
        draftProperty: "selectedId"
        draftResetValue: ""
        property string selectedId: ""
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        font.pixelSize: panel.textSize
        textRole: "label"
        valueRole: "id"
        currentValue: model && model.some(row => row.id === selectedId) ? selectedId : count > 0 ? model[0].id : ""
        onActivated: {
            selectedId = currentValue
            panel.frontier.clearReview()
        }
    }
    component Field: ColumnLayout {
        id: field
        required property string label
        property string initialValue: ""
        property string inputName: ""
        readonly property string value: entry.text
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Copy { text: field.label }
        DraftField {
            id: entry
            objectName: field.inputName
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            Accessible.name: field.label
            defaultText: field.initialValue
            onTextEdited: panel.frontier.clearReview()
        }
    }
    component Checks: ColumnLayout {
        id: choices
        required property var options
        required property string optionName
        property var selected: []
        DraftState {
            id: checkedDraft
            draftKey: "checks/" + choices.optionName
            accepts: value => Array.isArray(value) && value.length <= 64 && value.every(id => typeof id === "string") && new Set(value).size === value.length
            onRestored: value => choices.selected = value.filter(id => choices.options.some(option => option.id === id))
            onReset: choices.selected = []
        }
        onOptionsChanged: Qt.callLater(checkedDraft.restoreDraft)
        Layout.fillWidth: true
        Repeater {
            model: choices.options
            CheckBox {
                required property var modelData
                objectName: choices.optionName + "/" + modelData.id
                Layout.fillWidth: true
                font.pixelSize: panel.textSize
                text: modelData.label
                checked: choices.selected.includes(modelData.id)
                onClicked: {
                    choices.selected = checked ? choices.selected.concat([modelData.id])
                                               : choices.selected.filter(id => id !== modelData.id)
                    checkedDraft.remember(choices.selected)
                    panel.frontier.clearReview()
                }
            }
        }
    }
    component Records: ColumnLayout {
        id: records
        required property var rows
        required property string listName
        required property string emptyText
        Layout.fillWidth: true
        StableEntityList {
            objectName: records.listName
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, Math.max(90, records.rows.count * panel.textSize * 4))
            rows: records.rows
            emptyText: records.emptyText
            reducedMotion: campaign.reducedMotion
            textSize: panel.textSize
            onSelected: id => records.rows.selectedId = id
        }
        Copy {
            objectName: records.listName + "Detail"
            text: records.rows.selectedRow.detail || ""
            visible: text.length > 0
        }
    }
    component Budget: ColumnLayout {
        id: budget
        required property string prefix
        property bool showRuns: true
        readonly property var values: ({
            source: source.currentValue, rate: rate.value, staff: staff.value,
            limit: limit.value, reserve: reserve.value, interval: interval.value, runs: runs.value
        })
        Layout.fillWidth: true
        Picker {
            id: source
            objectName: budget.prefix + "Source"
            model: campaign.management.sources
            Accessible.name: qsTr("Compute source")
        }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.width >= panel.textSize * 29 ? 2 : 1
            Field {
                id: rate
                inputName: budget.prefix + "Rate"
                label: qsTr("Compute per day (CU)")
                initialValue: campaign.management.recommended.testingRate || ""
            }
            Field {
                id: staff
                inputName: budget.prefix + "Staff"
                label: qsTr("Staff per job")
                initialValue: campaign.management.recommended.researchStaff || ""
            }
            Field { id: limit; inputName: budget.prefix + "Limit"; label: qsTr("Total authorized spending (2014 dollars)") }
            Field { id: reserve; inputName: budget.prefix + "Reserve"; label: qsTr("Cash to keep available (2014 dollars)") }
            Field { id: interval; inputName: budget.prefix + "Interval"; label: qsTr("Minimum days between starts") }
            Field { id: runs; inputName: budget.prefix + "Runs"; visible: budget.showRuns; label: qsTr("Maximum starts") }
        }
    }

    Heading { text: qsTr("Frontier · future conditions and independent evidence") }
    Copy { objectName: "frontierOverview"; text: panel.frontier.overview }
    DraftComboBox {
        id: section
        objectName: "frontierSection"
        draftIds: ["assessments", "technology", "research", "refresh"]
        Layout.fillWidth: true
        font.pixelSize: panel.textSize
        Accessible.name: qsTr("Frontier section")
        model: [qsTr("Assessments"), qsTr("Technologies"), qsTr("Research programs"), qsTr("Product refresh")]
        onActivated: panel.frontier.clearReview()
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 0
        Heading { text: qsTr("Assess deployed systems") }
        Copy {
            text: panel.frontier.requirements
        }
        Field { id: systemName; inputName: "frontierSystemName"; label: qsTr("Name for the deployed system") }
        Checks { id: members; options: panel.frontier.services; optionName: "frontierMember" }
        Action {
            objectName: "reviewFrontierSystem"
            text: qsTr("Review deployed system")
            enabled: panel.editable && members.selected.length > 0 && systemName.value.length > 0
            onClicked: panel.frontier.reviewSystem(systemName.value, members.selected)
        }
        Records {
            listName: "frontierSystems"
            rows: panel.frontier.systems
            emptyText: qsTr("Register a deployed model or cooperating system to begin.")
        }
        Picker { id: protocol; objectName: "frontierProtocol"; model: panel.frontier.protocols; Accessible.name: qsTr("Independent protocol") }
        Picker { id: trialSource; objectName: "frontierTrialSource"; model: campaign.management.sources; Accessible.name: qsTr("Trial compute source") }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.width >= panel.textSize * 29 ? 2 : 1
            Field { id: trialRate; inputName: "frontierTrialRate"; label: qsTr("Trial compute per day (CU)"); initialValue: campaign.management.recommended.testingRate || "" }
            Field { id: trialStaff; inputName: "frontierTrialStaff"; label: qsTr("Trial staff"); initialValue: campaign.management.recommended.researchStaff || "" }
        }
        Action {
            objectName: "reviewFrontierTrial"
            text: qsTr("Review independent work")
            enabled: panel.editable && panel.frontier.systems.selectedId.length > 0 && protocol.currentIndex >= 0 && trialSource.currentIndex >= 0
            onClicked: panel.frontier.reviewTrial(protocol.currentValue, trialSource.currentValue, trialRate.value, trialStaff.value)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 1
        Heading { text: qsTr("Conditional technology availability") }
        Copy { text: qsTr("Research, data and recipes use the existing Lab. Order hardware through Operations after its technology and site requirements are met.") }
        Records { listName: "frontierTechnology"; rows: panel.frontier.technology; emptyText: qsTr("Technology definitions") }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 2
        Heading { text: qsTr("Authorize a research queue or standing track") }
        DraftComboBox {
            id: researchMode
            objectName: "frontierResearchMode"
            draftIds: ["queue", "standing"]
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            model: [qsTr("Ordered queue"), qsTr("Repeatable standing track")]
            Accessible.name: qsTr("Research program type")
            onActivated: panel.frontier.clearReview()
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: researchMode.currentIndex === 0
            property var topics: []
            id: queue
            objectName: "frontierQueueDraft"
            DraftState {
                id: queueDraft
                draftKey: "frontier/researchQueue"
                accepts: value => Array.isArray(value) && value.length <= 32 && value.every(id => typeof id === "string") && new Set(value).size === value.length
                onRestored: value => queue.topics = value.filter(id => panel.frontier.researchTopics.some(topic => topic.id === id))
                onReset: queue.topics = []
            }
            Picker { id: queuedTopic; objectName: "frontierQueuedTopic"; model: panel.frontier.researchTopics; Accessible.name: qsTr("Topic to add to queue") }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Action {
                    objectName: "appendFrontierQueue"
                    text: qsTr("Add topic")
                    enabled: !campaign.busy && queuedTopic.currentIndex >= 0 && !queue.topics.includes(queuedTopic.currentValue)
                    onClicked: {
                        queue.topics = queue.topics.concat([queuedTopic.currentValue])
                        queueDraft.remember(queue.topics)
                        panel.frontier.clearReview()
                    }
                }
                Action {
                    text: qsTr("Remove last topic")
                    enabled: queue.topics.length > 0
                    onClicked: {
                        queue.topics = queue.topics.slice(0, -1)
                        queueDraft.remember(queue.topics)
                        panel.frontier.clearReview()
                    }
                }
            }
            Repeater {
                model: queue.topics
                Copy {
                    required property string modelData
                    required property int index
                    text: qsTr("%1. %2").arg(index + 1).arg(
                        panel.frontier.researchTopics.find(row => row.id === modelData)?.label ?? modelData)
                }
            }
        }
        Picker {
            id: standingTopic
            objectName: "frontierStandingTopic"
            visible: researchMode.currentIndex === 1
            model: panel.frontier.repeatableTopics
            Accessible.name: qsTr("Repeatable topic")
        }
        Budget { id: researchBudget; prefix: "frontierResearch"; showRuns: researchMode.currentIndex === 1 }
        Action {
            objectName: "reviewFrontierResearch"
            text: qsTr("Review research authorization")
            enabled: panel.editable && (researchMode.currentIndex === 0 ? queue.topics.length > 0 : standingTopic.currentIndex >= 0)
            onClicked: panel.frontier.reviewResearch(researchMode.currentIndex === 0 ? queue.topics : [standingTopic.currentValue],
                                                       researchMode.currentIndex === 1, researchBudget.values)
        }
        Records { listName: "frontierResearchPrograms"; rows: panel.frontier.researchPrograms; emptyText: qsTr("No research authorizations") }
        Action {
            objectName: "cancelFrontierResearch"
            text: qsTr("Review cancelling research program")
            enabled: panel.editable && (panel.frontier.researchPrograms.selectedRow.fields?.active ?? false)
            onClicked: panel.frontier.reviewCancellation(false)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: section.currentIndex === 3
        Heading { text: qsTr("Authorize mature product refresh") }
        Copy { text: qsTr("Select a service and the candidate's licensed data. Keep the deployment method review current through Lab or a standing track. Each replacement needs better independent evidence and must preserve the service's existing terms.") }
        Picker { id: service; objectName: "frontierRefreshService"; model: panel.frontier.refreshServices; Accessible.name: qsTr("Service to refresh") }
        Picker { id: recipe; objectName: "frontierRefreshRecipe"; model: campaign.management.recipes; Accessible.name: qsTr("Candidate recipe") }
        Picker { id: evaluation; objectName: "frontierRefreshEvaluation"; model: campaign.management.evaluations; Accessible.name: qsTr("Independent evaluation plan") }
        Checks { id: data; options: panel.frontier.licensedData; optionName: "frontierRefreshData" }
        GridLayout {
            Layout.fillWidth: true
            columns: panel.width >= panel.textSize * 29 ? 2 : 1
            Field { id: work; inputName: "frontierRefreshWork"; label: qsTr("Candidate work (CU)") }
            Field { id: gain; inputName: "frontierRefreshGain"; label: qsTr("Required improvement (points out of 100)") }
        }
        Budget { id: refreshBudget; prefix: "frontierRefresh" }
        Action {
            objectName: "reviewFrontierRefresh"
            text: qsTr("Review refresh authorization")
            enabled: panel.editable && service.currentIndex >= 0 && recipe.currentIndex >= 0 && evaluation.currentIndex >= 0 && data.selected.length > 0
            onClicked: panel.frontier.reviewRefresh(service.currentValue, recipe.currentValue, evaluation.currentValue,
                                                      data.selected, work.value, gain.value, refreshBudget.values)
        }
        Records { listName: "frontierRefreshPrograms"; rows: panel.frontier.refreshPrograms; emptyText: qsTr("No refresh authorizations") }
        Action {
            objectName: "cancelFrontierRefresh"
            text: qsTr("Review cancelling refresh")
            enabled: panel.editable && (panel.frontier.refreshPrograms.selectedRow.fields?.active ?? false)
            onClicked: panel.frontier.reviewCancellation(true)
        }
    }

    Dialog {
        id: review
        title: qsTr("Review frontier decision")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(720, (Overlay.overlay ? Overlay.overlay.width : 720) - 32)
        font.pixelSize: panel.textSize
        contentItem: ScrollView {
            implicitHeight: Math.min(reviewCopy.implicitHeight + 8, (Overlay.overlay ? Overlay.overlay.height : 720) * 0.53)
            contentWidth: availableWidth
            clip: true
            Label {
                id: reviewCopy
                objectName: "frontierReviewText"
                width: parent.width
                text: panel.frontier.reviewText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: panel.textSize
            }
        }
        footer: DialogButtonBox {
            Action {
                objectName: "confirmFrontierAction"
                text: qsTr("Confirm")
                enabled: panel.editable && panel.frontier.reviewText.length > 0
                onClicked: { panel.frontier.confirm(); review.close() }
            }
            Action { text: qsTr("Back"); onClicked: review.close() }
        }
        onClosed: panel.frontier.clearReview()
    }
    Connections {
        target: panel.frontier
        function onReviewReady() { review.open() }
    }
}
