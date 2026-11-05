import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    property string room: "office"
    property int textSize: 14
    property string section: "data"
    property string officeSection: "offers"
    property var manager: campaign.management
    property var trainingData: []
    readonly property bool editable: campaign.paused && !campaign.busy
    spacing: 10

    component Copy: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Action: Button {
        id: action
        Layout.maximumWidth: panel.width
        contentItem: Text { text: action.text; textFormat: Text.PlainText; font: action.font; color: action.enabled ? "#f2f4f7" : "#aebbc7"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; wrapMode: Text.Wrap }
        font.pixelSize: panel.textSize
        Accessible.name: text
        enabled: panel.editable
    }
    component Field: ColumnLayout {
        property string label
        property alias value: input.text
        Layout.fillWidth: true
        Copy { text: parent.label }
        TextField {
            id: input
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            Accessible.name: parent.label
            selectByMouse: true
            maximumLength: 15
        }
    }
    component Choice: ColumnLayout {
        id: choice
        property string label
        property var options: []
        property var rows: null
        property string value: rows ? (rows.count > 0 ? rows.idAt(0) : "") : (options.length ? options[0].id : "")
        readonly property var selectedOption: options.find(o => o.id === value) || ({})
        Layout.fillWidth: true
        Copy { text: choice.label }
        ComboBox {
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            Accessible.name: choice.label
            objectName: choice.objectName + "Input"
            model: choice.rows || choice.options
            textRole: choice.rows ? "title" : "label"
            valueRole: choice.rows ? "entityId" : "id"
            currentIndex: choice.rows ? choice.rows.indexOf(choice.value) : choice.options.findIndex(o => o.id === choice.value)
            onActivated: index => choice.value = choice.rows ? choice.rows.idAt(index) : choice.options[index].id
        }
    }
    component Selection: ColumnLayout {
        id: selection
        property string heading
        property var rows
        Layout.fillWidth: true
        spacing: 8
        Copy { text: selection.heading; font.bold: true; color: "#ffd897" }
        StableEntityList {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(125, Math.min(340, selection.rows.count * panel.textSize * 6))
            rows: selection.rows
            textSize: panel.textSize
            reducedMotion: campaign.reducedMotion
            emptyText: qsTr("No %1 yet").arg(selection.heading.toLowerCase())
            onSelected: entityId => selection.rows.selectedId = entityId
        }
        Copy { text: selection.rows.selectedRow.detail || ""; visible: text.length > 0 }
    }

    Copy { visible: !campaign.paused; text: qsTr("Time is running. Pause to change projects, purchases and customer promises."); color: "#ffd897" }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        visible: panel.room === "lab"
        Repeater {
            model: [{id:"data",label:qsTr("Data")}, {id:"research",label:qsTr("Research")}, {id:"train",label:qsTr("Train")},
                    {id:"models",label:qsTr("Checkpoints")}, {id:"evaluate",label:qsTr("Evaluate")}, {id:"deploy",label:qsTr("Deploy")}, {id:"projects",label:qsTr("Projects")}]
            Button {
                required property var modelData
                objectName: "lab_" + modelData.id
                text: modelData.label
                font.pixelSize: panel.textSize
                checked: panel.section === modelData.id
                Accessible.name: text
                onClicked: panel.section = modelData.id
            }
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        visible: panel.room === "office"
        Repeater {
            model: [{id:"offers",label:qsTr("Compare offers")}, {id:"products",label:qsTr("Services")}, {id:"contracts",label:qsTr("Contracts")},
                    {id:"rivals",label:qsTr("Rivals")}, {id:"accounts",label:qsTr("Accounts")}, {id:"funding",label:qsTr("Funding and team")}, {id:"policies",label:qsTr("Policies and alerts")}, {id:"score",label:qsTr("Slice score")}]
            Button {
                required property var modelData
                objectName: "office_" + modelData.id
                text: modelData.label
                font.pixelSize: panel.textSize
                checked: panel.officeSection === modelData.id
                Accessible.name: text
                onClicked: panel.officeSection = modelData.id
            }
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "data"
        Copy { text: qsTr("License rights and coverage before training. Purchased data affects new training; an existing checkpoint keeps its original mixture.") }
        Selection { heading: qsTr("Data offers"); rows: manager.dataOffers }
        Action {
            objectName: "buyDataButton"
            text: qsTr("Buy selected license")
            enabled: panel.editable && manager.dataOffers.selectedRow.actionable === true
            onClicked: manager.buyData(manager.dataOffers.selectedId)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "research"
        Selection { heading: qsTr("Research topics"); rows: manager.researchTopics }
        Choice { id: researchSource; label: qsTr("Research compute source"); options: manager.sources }
        Field { id: researchRate; label: qsTr("Research CU per day"); value: manager.recommended.testingRate || "" }
        Field { id: researchStaff; label: qsTr("Research staff"); value: manager.recommended.researchStaff || "" }
        Action {
            objectName: "startResearchButton"
            text: qsTr("Pay and start research")
            enabled: panel.editable && manager.researchTopics.selectedRow.actionable === true
            onClicked: manager.research(manager.researchTopics.selectedId, researchSource.value, researchRate.value, researchStaff.value)
        }
    }
    ColumnLayout {
        id: training
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "train"
        property var preview: ({text: qsTr("Choose data and a recipe to preview."), ready: false})
        onVisibleChanged: if (visible) refresh()
        Connections { target: campaign; function onStateChanged() { if (training.visible && campaign.paused) training.refresh() } }
        Connections { target: manager; function onChanged() { training.refresh() } }
        function refresh() { if (!visible || !campaign.paused || campaign.busy) return; if (!recipe || !trainingSource || !trainingRate || !trainingStaff || !trainingWork || !continueTraining || !parentModel) return; preview = manager.trainingPreview(recipe.value, trainingSource.value, panel.trainingData, trainingRate.value, trainingStaff.value, trainingWork.value, continueTraining.checked ? parentModel.value : "") }
        Choice { id: recipe; objectName: "recipeChoice"; label: qsTr("Training recipe"); options: manager.recipes; onValueChanged: training.refresh() }
        Copy { text: recipe.selectedOption.detail || "" }
        Copy { text: qsTr("Training mixture (licenses are purchased in Data)") }
        Repeater {
            model: manager.dataOffers
            CheckBox {
                required property string entityId
                objectName: "trainingData_" + entityId
                required property string title
                required property string status
                text: title + " · " + status
                font.pixelSize: panel.textSize
                Accessible.name: text
                checked: panel.trainingData.indexOf(entityId) >= 0
                onClicked: {
                    panel.trainingData = checked ? panel.trainingData.concat([entityId]) : panel.trainingData.filter(id => id !== entityId)
                    training.refresh()
                }
                contentItem: Text {
                    text: parent.text; color: "#f2f4f7"; font: parent.font
                    leftPadding: parent.indicator.width + parent.spacing
                    wrapMode: Text.Wrap
                }
                Layout.fillWidth: true
            }
        }
        Choice { id: trainingSource; label: qsTr("Training compute source"); options: manager.sources; onValueChanged: training.refresh() }
        Field { id: trainingRate; label: qsTr("Training CU per day"); value: manager.recommended.trainingRate || ""; onValueChanged: training.refresh() }
        Field { id: trainingStaff; label: qsTr("Training staff"); value: manager.recommended.trainingStaff || ""; onValueChanged: training.refresh() }
        Field { id: trainingWork; label: qsTr("Additional training budget, total CU"); value: manager.recommended.trainingWork || ""; onValueChanged: training.refresh() }
        CheckBox {
            id: continueTraining
            Layout.fillWidth: true
            text: qsTr("Continue a stopped checkpoint with the same recipe and data")
            font.pixelSize: panel.textSize
            Accessible.name: text
            onCheckedChanged: training.refresh()
            contentItem: Text {
                text: continueTraining.text; font: continueTraining.font; color: "#f2f4f7"
                leftPadding: continueTraining.indicator.width + continueTraining.spacing
                wrapMode: Text.Wrap
            }
        }
        Choice { id: parentModel; label: qsTr("Parent checkpoint"); rows: manager.models; visible: continueTraining.checked; onValueChanged: training.refresh() }
        Copy { objectName: "trainingPreview"; text: training.preview.text || qsTr("Choose data and a recipe to preview.") }
        Action {
            objectName: "startTrainingButton"
            text: qsTr("Reserve resources and start training")
            enabled: panel.editable && training.preview.ready === true
            onClicked: manager.train(recipe.value, trainingSource.value, panel.trainingData, trainingRate.value, trainingStaff.value, trainingWork.value, continueTraining.checked ? parentModel.value : "")
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "models"
        Selection { heading: qsTr("Model checkpoints"); rows: manager.models }
        Action {
            text: qsTr("Clone selected stopped checkpoint")
            enabled: panel.editable && manager.models.selectedRow.actionable === true
            onClicked: manager.clone(manager.models.selectedId)
        }
        Action {
            text: qsTr("Stop selected training; keep consumed work")
            enabled: panel.editable && !!manager.models.selectedRow.fields && manager.models.selectedRow.fields.training === true
            onClicked: campaign.projectCancel(manager.models.selectedRow.fields.projectId)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "evaluate"
        Copy { text: qsTr("Independent held-out tests measure task domains separately. Headline tests can be contaminated and cannot qualify a deployment. A wider audit needs more time and its research prerequisite.") }
        Choice { id: evaluationModel; objectName: "evaluationModelChoice"; label: qsTr("Checkpoint to evaluate"); rows: manager.models }
        Choice { id: evaluationPlan; objectName: "evaluationPlanChoice"; label: qsTr("Evaluation plan and cash fee"); options: manager.evaluations }
        Copy { text: evaluationPlan.selectedOption.detail || "" }
        Choice { id: evaluationSource; label: qsTr("Evaluation compute source"); options: manager.sources }
        Field { id: evaluationRate; label: qsTr("Evaluation CU per day"); value: manager.recommended.testingRate || "" }
        Field { id: evaluationStaff; label: qsTr("Evaluation staff"); value: manager.recommended.testingStaff || "" }
        Action {
            objectName: "evaluateButton"
            text: qsTr("Pay and start evaluation")
            enabled: panel.editable && evaluationModel.value.length > 0
            onClicked: manager.evaluate(evaluationModel.value, evaluationPlan.value, evaluationSource.value, evaluationRate.value, evaluationStaff.value)
        }
        Selection { heading: qsTr("Measured evidence"); rows: manager.evidence }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "lab" && panel.section === "deploy"
        Copy { text: qsTr("Match the service promise to independent evidence. Document-only scope drops scanned tables. Human review adds staff and monthly cost and limits throughput. Targeted retraining can qualify the full task scope without that control.") }
        Choice { id: deploymentService; objectName: "deploymentServiceChoice"; label: qsTr("Customer task promise"); options: manager.services }
        Choice { id: deploymentModel; objectName: "deploymentModelChoice"; label: qsTr("Stopped model to serve"); rows: manager.models }
        Choice { id: deploymentEvidence; objectName: "deploymentEvidenceChoice"; label: qsTr("Independent evidence for that model"); rows: manager.evidence }
        Choice { id: deploymentSource; label: qsTr("Serving compute source"); options: manager.sources }
        CheckBox {
            id: humanReview
            Layout.fillWidth: true
            contentItem: Text { text: humanReview.text; font: humanReview.font; color: humanReview.enabled ? "#f2f4f7" : "#aebbc7"; leftPadding: humanReview.indicator.width + humanReview.spacing; wrapMode: Text.Wrap }
            text: qsTr("Add paid human review of scanned tables")
            font.pixelSize: panel.textSize
            Accessible.name: text
            enabled: deploymentService.selectedOption.reviewAllowed === true
            onEnabledChanged: if (!enabled) checked = false
        }
        Copy { text: manager.servicePreview(deploymentService.value, deploymentModel.value, humanReview.checked) }
        Action {
            objectName: "deployButton"
            text: qsTr("Qualify and deploy / replace service")
            enabled: panel.editable && deploymentModel.value.length > 0 && deploymentEvidence.value.length > 0
            onClicked: manager.deploy(deploymentService.value, deploymentModel.value, deploymentEvidence.value, deploymentSource.value, humanReview.checked)
        }
        Selection { heading: qsTr("Deployed services"); rows: manager.products }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "offers"
        Copy { text: qsTr("Compare the monthly payment, task promise and term together. Customer offers need a qualified service. A compute lease adds capacity and a bill for its finite term.") }
        Selection { heading: qsTr("Business offers"); rows: manager.offers }
        Action {
            objectName: "acceptOfferButton"
            text: qsTr("Accept selected terms")
            enabled: panel.editable && manager.offers.selectedRow.actionable === true
            onClicked: manager.acceptOffer(manager.offers.selectedId)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "products"
        Selection { heading: qsTr("Deployed services"); rows: manager.products }
        Action { text: qsTr("Stop selected service"); enabled: panel.editable && manager.products.selectedId.length > 0; onClicked: campaign.projectCancel(manager.products.selectedRow.fields.projectId) }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "contracts"
        Selection { heading: qsTr("Customer contracts"); rows: manager.contracts }
        Action { text: qsTr("Cancel selected contract…"); enabled: panel.editable && manager.contracts.selectedRow.actionable === true; onClicked: cancelDialog.open() }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "rivals"
        Copy { text: qsTr("Public notices show competitor activity and the heavyweight mobilization window. Their budgets, private checkpoints and future results are not visible.") }
        Selection { heading: qsTr("Public rival notices"); rows: manager.rivals }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "accounts"
        Copy { text: manager.briefing }
        Selection { heading: qsTr("Unpaid obligations"); rows: manager.payables }
        Action {
            text: qsTr("Pay selected outstanding amount")
            enabled: panel.editable && manager.payables.selectedId.length > 0
            onClicked: {
                if (manager.payables.selectedRow.fields.debt) manager.repayDebt(manager.payables.selectedId)
                else manager.payInvoice(manager.payables.selectedId)
            }
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "funding"
        Copy { text: qsTr("Financing is offered once at fixed terms. Equity dilutes ownership; a loan creates interest and a principal deadline. A specialist adds paid staff capacity. The document-specialist service requires efficient-serving research and an independently tested efficient recipe.") }
        Selection { heading: qsTr("Financing offers"); rows: manager.funding }
        Action { objectName: "takeFinancingButton"; text: qsTr("Accept selected financing terms…"); enabled: panel.editable && manager.funding.selectedRow.actionable === true; onClicked: fundingDialog.open() }
        Selection { heading: qsTr("Specialists"); rows: manager.specialists }
        Action { objectName: "hireSpecialistButton"; text: qsTr("Hire selected specialist and start payroll"); enabled: panel.editable && manager.specialists.selectedRow.actionable === true; onClicked: manager.hireSpecialist(manager.specialists.selectedId) }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "policies"
        Copy { text: qsTr("Authorize a finite policy. Each attempt rechecks its remaining budget, cash floor, methodology and available resources. Refused actions spend nothing and suspend the policy. Disabling or re-enabling preserves counters. A policy never chooses a story response or deploys a model.") }
        Choice { id: policyKind; objectName: "policyKindChoice"; label: qsTr("Policy type"); options: [{id:"evaluation",label:qsTr("Schedule independent evaluations")},{id:"lease_renewal",label:qsTr("Renew a paid compute lease")}] }
        Choice { id: policyModel; label: qsTr("Model checkpoint (may still be training)"); rows: manager.models; visible: policyKind.value === "evaluation" }
        Choice { id: policyPlan; label: qsTr("Independent evaluation plan"); options: manager.evaluations; visible: policyKind.value === "evaluation" }
        Choice { id: policySource; label: qsTr("Evaluation source"); options: manager.sources; visible: policyKind.value === "evaluation" }
        Field { id: policyRate; label: qsTr("Evaluation CU/day ceiling"); value: manager.recommended.testingRate || ""; visible: policyKind.value === "evaluation" }
        Field { id: policyStaff; label: qsTr("Evaluation staff ceiling"); value: manager.recommended.testingStaff || ""; visible: policyKind.value === "evaluation" }
        Field { id: policyInterval; label: qsTr("Days between evaluations"); value: manager.policyDefaults.interval || ""; visible: policyKind.value === "evaluation" }
        Choice { id: policyLease; label: qsTr("Accepted renewable lease and conservative term cost"); options: manager.renewableLeases; visible: policyKind.value === "lease_renewal" }
        Field { id: policyMonthly; label: qsTr("Maximum lease bill per month, dollars"); value: manager.policyDefaults.monthly || ""; visible: policyKind.value === "lease_renewal" }
        Field { id: policyBudget; label: qsTr("Total policy commitment budget, dollars"); value: manager.policyDefaults.budget || "" }
        Field { id: policyReserve; label: qsTr("Minimum cash after commitment, dollars"); value: manager.policyDefaults.reserve || "" }
        Field { id: policyRuns; objectName: "policyRunsField"; label: qsTr("Maximum authorized runs"); value: manager.policyDefaults.runs || "" }
        Copy { visible: policyKind.value === "lease_renewal"; text: qsTr("Renewal occurs on the expiry morning. The policy debits a conservative full-term commitment ceiling (setup plus enough monthly bills for the entire term); the ledger pays actual setup and daily accrued bills. It cannot renew a missed deadline retroactively.") }
        Action {
            objectName: "authorizePolicyButton"
            text: qsTr("Authorize bounded policy")
            onClicked: {
                if (policyKind.value === "evaluation") manager.authorizeEvaluation(policyModel.value, policyPlan.value, policySource.value, policyRate.value, policyStaff.value, policyBudget.value, policyReserve.value, policyInterval.value, policyRuns.value)
                else manager.authorizeRenewal(policyLease.value, policyBudget.value, policyReserve.value, policyMonthly.value, policyRuns.value)
            }
        }
        Selection { heading: qsTr("Authorized policies"); rows: manager.policies }
        Action { text: qsTr("Suspend / resume selected policy"); enabled: panel.editable && manager.policies.selectedId.length > 0; onClicked: manager.setPolicyEnabled(manager.policies.selectedId, !manager.policies.selectedRow.fields.enabled) }
        Action { text: qsTr("Renew selected lease explicitly on expiry morning"); enabled: panel.editable && policyLease.value.length > 0; onClicked: manager.renewLease(policyLease.value) }
        Selection { heading: qsTr("Policy decision log"); rows: manager.decisions }
        Selection { heading: qsTr("Company alerts and deadlines"); rows: manager.alerts }
        Action { objectName: "acknowledgeAlertButton"; text: qsTr("Acknowledge selected alert"); enabled: panel.editable && manager.alerts.selectedRow.actionable === true; onClicked: manager.acknowledgeAlert(manager.alerts.selectedId) }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "score"
        Copy { text: manager.scoreText }
        Copy { text: qsTr("Survival is awarded at the ending; work counts actual delivered tasks. Profit excludes financing proceeds. Evidence counts independently measured domains used by deployed checkpoints. Ownership points reflect retained founder stake. Scores remain provisional while time can advance.") }
    }
    Dialog {
        id: fundingDialog
        objectName: "fundingConfirmation"
        title: qsTr("Accept fixed financing terms?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, Overlay.overlay.width - 36)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: manager.funding.selectedRow.detail || ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        onAccepted: manager.takeFinancing(manager.funding.selectedId)
    }
    Dialog {
        id: cancelDialog
        title: qsTr("Cancel customer commitment?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, Overlay.overlay.width - 36)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: manager.contracts.selectedRow.detail || ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        onAccepted: manager.cancelContract(manager.contracts.selectedId)
    }
}
