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
    readonly property bool editable: campaign.paused && !campaign.busy && campaign.companyEditable
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
        Copy { objectName: selection.objectName.length > 0 ? selection.objectName + "Detail" : ""; text: selection.rows.selectedRow.detail || ""; visible: text.length > 0 }
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
                    {id:"markets",label:qsTr("Regional markets")},
                    {id:"rivals",label:qsTr("Rivals")}, {id:"accounts",label:qsTr("Accounts")}, {id:"funding",label:qsTr("Funding and team")}, {id:"governance",label:qsTr("Board and people")}, {id:"policies",label:qsTr("Policies and alerts")}, {id:"score",label:qsTr("Slice score")}]
            Button {
                required property var modelData
                objectName: "office_" + modelData.id
                text: modelData.label
                visible: (modelData.id !== "markets" || manager.regionalMarkets.count > 0) &&
                         (modelData.id !== "score" || !campaign.objectives.available)
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
        TextField {
            objectName: "dataOfferFilter"
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            placeholderText: qsTr("Filter data offers")
            Accessible.name: placeholderText
            text: manager.dataOffers.filterText
            onTextEdited: manager.dataOffers.filterText = text
        }
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
        TextField {
            objectName: "researchTopicFilter"
            Layout.fillWidth: true
            font.pixelSize: panel.textSize
            placeholderText: qsTr("Filter research topics")
            Accessible.name: placeholderText
            text: manager.researchTopics.filterText
            onTextEdited: manager.researchTopics.filterText = text
        }
        Selection { heading: qsTr("Research topics"); rows: manager.researchTopics }
        Choice { id: researchSource; objectName: "researchSourceChoice"; label: qsTr("Research compute source"); options: manager.sources }
        Field { id: researchRate; objectName: "researchRateField"; label: qsTr("Research CU per day"); value: manager.recommended.testingRate || "" }
        Field { id: researchStaff; objectName: "researchStaffField"; label: qsTr("Research staff"); value: manager.recommended.researchStaff || "" }
        Action {
            objectName: "startResearchButton"
            text: qsTr("Pay and start research")
            enabled: panel.editable && manager.researchTopics.selectedRow.actionable === true
            onClicked: manager.research(manager.researchTopics.selectedId, researchSource.value, researchRate.value, researchStaff.value)
        }
        Action {
            objectName: "publishResearchButton"
            text: qsTr("Publish completed research")
            enabled: panel.editable && (manager.plannedCompetition || campaign.institutions.available)
                     && (manager.researchTopics.selectedRow.fields || {}).completed === true
                     && (manager.researchTopics.selectedRow.fields || {}).published !== true
            onClicked: manager.publishResearch(manager.researchTopics.selectedId)
        }
        Action {
            objectName: "adoptPublicationButton"
            text: qsTr("Adopt available public research")
            enabled: panel.editable && (manager.researchTopics.selectedRow.fields || {}).canAdopt === true
            onClicked: manager.acquirePublication(manager.researchTopics.selectedId)
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
        Selection { objectName: "modelCheckpointSelection"; heading: qsTr("Training runs and immutable checkpoints"); rows: manager.models }
        Copy { text: qsTr("Pause releases compute and staff. Resume requests the original allocation. Save an immutable checkpoint to evaluate, deploy or continue elsewhere while the source keeps training. Spent work, licenses and existing bills remain.") }
        Action { objectName: "checkpointTrainingButton"; text: qsTr("Save checkpoint of selected model"); enabled: panel.editable && !!manager.models.selectedRow.fields && manager.models.selectedRow.fields.checkpointReady === true; onClicked: manager.checkpoint(manager.models.selectedId) }
        Action { objectName: "pauseTrainingButton"; text: qsTr("Pause selected training"); enabled: panel.editable && !!manager.models.selectedRow.fields && manager.models.selectedRow.fields.training === true && manager.models.selectedRow.fields.paused !== true; onClicked: campaign.projectPause(manager.models.selectedRow.fields.projectId) }
        Action { objectName: "resumeTrainingButton"; text: qsTr("Resume selected training"); enabled: panel.editable && !!manager.models.selectedRow.fields && manager.models.selectedRow.fields.paused === true; onClicked: campaign.projectResume(manager.models.selectedRow.fields.projectId) }
        Action {
            text: qsTr("Clone selected stopped checkpoint")
            enabled: panel.editable && manager.models.selectedRow.actionable === true
            onClicked: manager.clone(manager.models.selectedId)
        }
        Action {
            objectName: "stopTrainingButton"
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
        Copy { text: qsTr("Match the service promise to independent evidence. The distribution channel determines who supplies hardware, which customers qualify, and how quickly buyers switch.") }
        Choice { id: deploymentService; objectName: "deploymentServiceChoice"; label: qsTr("Customer task promise"); options: manager.services }
        Choice { id: deploymentModel; objectName: "deploymentModelChoice"; label: qsTr("Stopped model to serve"); rows: manager.models }
        Choice { id: deploymentEvidence; objectName: "deploymentEvidenceChoice"; label: qsTr("Independent evidence for that model"); rows: manager.evidence }
        Choice { id: deploymentSource; label: qsTr("Serving compute source"); options: manager.sources; visible: deploymentService.selectedOption.customerHardware !== true }
        Choice {
            id: deploymentRegion
            objectName: "deploymentRegionChoice"
            label: qsTr("Launch territory")
            options: manager.launchRegions
            visible: options.length > 0 && deploymentService.selectedOption.existing !== true
        }
        Copy { visible: manager.launchRegions.length > 0; text: qsTr("A service keeps its launch territory when you replace its model.") }
        Copy { visible: deploymentService.selectedOption.reviewRequired === true; text: qsTr("This agent requires paid human review.") }
        CheckBox {
            id: humanReview
            Layout.fillWidth: true
            contentItem: Text { text: humanReview.text; font: humanReview.font; color: humanReview.enabled ? "#f2f4f7" : "#aebbc7"; leftPadding: humanReview.indicator.width + humanReview.spacing; wrapMode: Text.Wrap }
            text: qsTr("Add paid human review")
            font.pixelSize: panel.textSize
            Accessible.name: text
            visible: deploymentService.selectedOption.reviewRequired !== true
            enabled: visible && deploymentService.selectedOption.reviewAllowed === true
            onEnabledChanged: if (!enabled) checked = false
        }
        Copy { text: manager.servicePreview(deploymentService.value, deploymentModel.value, humanReview.checked || deploymentService.selectedOption.reviewRequired === true) }
        Action {
            objectName: "deployButton"
            text: qsTr("Qualify and deploy / replace service")
            enabled: panel.editable && deploymentModel.value.length > 0 && deploymentEvidence.value.length > 0
            onClicked: manager.deploy(deploymentService.value, deploymentModel.value, deploymentEvidence.value,
                                      deploymentSource.visible ? deploymentSource.value : "",
                                      humanReview.checked || deploymentService.selectedOption.reviewRequired === true,
                                      deploymentRegion.visible ? deploymentRegion.value : "")
        }
        Selection { heading: qsTr("Deployed services"); rows: manager.products }
        Field { id: productPrice; objectName: "productPriceField"; label: qsTr("Price per task · constant 2014 cents"); value: (manager.products.selectedRow.fields || {}).priceCents || ""; visible: (manager.products.selectedRow.fields || {}).freeWeights !== true }
        Copy { visible: productPrice.visible; text: qsTr("Setting a price suspends any enabled pricing policy for this product. Review its retained bounds and counters in Office → Policies and alerts before resuming.") }
        Action {
            objectName: "setProductPriceButton"
            text: qsTr("Set service price")
            enabled: panel.editable && productPrice.visible && manager.products.selectedId.length > 0
            onClicked: manager.setProductPrice(manager.products.selectedId, productPrice.value)
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "offers"
        Copy { text: qsTr("Compare the monthly payment, task promise and term together. Customer offers need a qualified service. A compute lease adds capacity and a bill for its finite term.") }
        Selection { objectName: "businessOfferSelection"; heading: qsTr("Business offers"); rows: manager.offers }
        Action {
            objectName: "acceptOfferButton"
            readonly property bool prepaid: manager.offers.selectedRow.fields?.customerAdvance === true
            text: prepaid ? qsTr("Review customer advance…") : qsTr("Accept selected terms")
            enabled: panel.editable && manager.offers.selectedRow.actionable === true
            onClicked: {
                if (prepaid) {
                    advanceDialog.offerId = manager.offers.selectedId
                    advanceDialog.terms = manager.offers.selectedRow.detail
                    advanceDialog.open()
                } else {
                    manager.acceptOffer(manager.offers.selectedId)
                }
            }
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
        visible: panel.room === "office" && panel.officeSection === "markets"
        Copy { text: qsTr("The latest settled day shows tasks delivered by all vendors, your deliveries, and demand left with the outside option. Licensed and open-weight work uses the region's customer hardware.") }
        Selection { objectName: "regionalMarketSelection"; heading: qsTr("Market regions"); rows: manager.regionalMarkets }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "contracts"
        Selection { objectName: "customerContractSelection"; heading: qsTr("Customer contracts"); rows: manager.contracts }
        Action {
            objectName: "cancelCustomerContractButton"
            text: qsTr("Cancel selected contract…")
            enabled: panel.editable && manager.contracts.selectedRow.actionable === true
            onClicked: {
                cancelDialog.contractId = manager.contracts.selectedId
                cancelDialog.terms = manager.contracts.selectedRow.detail
                cancelDialog.open()
            }
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "rivals"
        Copy { text: qsTr("Public response briefings show pressure, announced budgets and warning/delivery boundaries. Private balances, checkpoints and future results remain hidden.") }
        Copy { text: manager.competitionSummary }
        Selection { objectName: "heavyweightBriefing"; heading: qsTr("Public heavyweight response briefings"); rows: manager.heavyweights }
        Selection { objectName: "competitiveOfferReview"; heading: qsTr("Funded competitive agreements"); rows: manager.competitiveOffers }
        Action {
            objectName: "acceptCompetitiveOfferButton"
            text: qsTr("Review and accept funded agreement…")
            enabled: panel.editable && manager.competitiveOffers.selectedRow.actionable === true
            onClicked: {
                competitiveDialog.offerId = manager.competitiveOffers.selectedId
                competitiveDialog.terms = manager.competitiveOffers.selectedRow.detail || ""
                competitiveDialog.open()
            }
        }
        Action { objectName: "declineCompetitiveOfferButton"; text: qsTr("Decline selected agreement"); enabled: panel.editable && manager.competitiveOffers.selectedRow.actionable === true; onClicked: manager.respondCompetitiveOffer(manager.competitiveOffers.selectedId, false) }
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
        Copy { text: qsTr("Compare one-time financing terms and restricted awards. Equity dilutes ownership and loans add interest and a principal deadline. Grants and credits cover eligible expenses. Specialists add paid staff capacity.") }
        Selection { objectName: "fundingOfferSelection"; heading: qsTr("Financing offers"); rows: manager.funding }
        Action { objectName: "takeFinancingButton"; text: qsTr("Accept selected financing terms…"); enabled: panel.editable && manager.funding.selectedRow.actionable === true; onClicked: fundingDialog.open() }
        Selection { heading: qsTr("Specialists"); rows: manager.specialists }
        Action { objectName: "hireSpecialistButton"; text: qsTr("Hire selected specialist and start payroll"); enabled: panel.editable && manager.specialists.selectedRow.actionable === true; onClicked: manager.hireSpecialist(manager.specialists.selectedId) }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "policies"
        Copy { text: qsTr("Each policy follows the limits you authorize. If an action is refused, the policy suspends and records the reason. You can suspend or resume a policy and keep its counters and past decisions.") }
        Copy { text: manager.operatingReserveText; font.bold: true }
        Choice {
            id: policyKind
            objectName: "policyKindChoice"
            label: qsTr("Policy type")
            options: [
                {id:"evaluation",label:qsTr("Schedule independent evaluations")},
                {id:"lease_renewal",label:qsTr("Renew a paid compute lease")},
                {id:"pricing",label:qsTr("Adjust a product price within bounds")},
                {id:"operating_reserve",label:qsTr("Maintain an operating cash reserve")},
                {id:"hardware_replacement",label:qsTr("Replace obsolete owned hardware")}
            ]
        }
        Choice { id: policyModel; label: qsTr("Model checkpoint (may still be training)"); rows: manager.models; visible: policyKind.value === "evaluation" }
        Choice { id: policyPlan; label: qsTr("Independent evaluation plan"); options: manager.evaluations; visible: policyKind.value === "evaluation" }
        Choice { id: policySource; label: qsTr("Evaluation source"); options: manager.sources; visible: policyKind.value === "evaluation" }
        Field { id: policyRate; label: qsTr("Evaluation CU/day ceiling"); value: manager.recommended.testingRate || ""; visible: policyKind.value === "evaluation" }
        Field { id: policyStaff; label: qsTr("Evaluation staff ceiling"); value: manager.recommended.testingStaff || ""; visible: policyKind.value === "evaluation" }
        Field { id: policyInterval; label: qsTr("Days between evaluations"); value: manager.policyDefaults.interval || ""; visible: policyKind.value === "evaluation" }
        Choice { id: policyLease; label: qsTr("Accepted renewable lease and conservative term cost"); options: manager.renewableLeases; visible: policyKind.value === "lease_renewal" }
        Field { id: policyMonthly; label: qsTr("Maximum lease bill per month, dollars"); value: manager.policyDefaults.monthly || ""; visible: policyKind.value === "lease_renewal" }
        ColumnLayout {
            visible: policyKind.value === "pricing"
            Layout.fillWidth: true
            Choice { id: priceProduct; objectName: "pricingProductChoice"; label: qsTr("Operating product and current quote"); options: manager.pricedProducts }
            Field { id: priceFloor; objectName: "pricingFloorField"; label: qsTr("Minimum price per task, dollars"); value: priceProduct.selectedOption.floor || "0.00" }
            Field { id: priceCeiling; objectName: "pricingCeilingField"; label: qsTr("Maximum price per task, dollars"); value: priceProduct.selectedOption.ceiling || "1.00" }
            Field { id: priceStep; objectName: "pricingStepField"; label: qsTr("Price adjustment per review, dollars"); value: priceProduct.selectedOption.step || "0.05" }
            Field { id: priceLow; objectName: "pricingLowField"; label: qsTr("Lower price at or below this utilization, whole percent"); value: "25" }
            Field { id: priceHigh; objectName: "pricingHighField"; label: qsTr("Raise price at or above this utilization, whole percent"); value: "85" }
            Field { id: priceInterval; objectName: "pricingIntervalField"; label: qsTr("Days between price reviews, 7–365"); value: "30" }
            Field { id: priceRuns; objectName: "pricingRunsField"; label: qsTr("Maximum price reviews, 1–10000"); value: "1200" }
            Copy { text: qsTr("The first review is tomorrow. Later reviews use up to seven settled days of discretionary deliveries and serving slots left after protected contracts. Low utilization lowers the quote by one step; high utilization raises it. The policy holds through a known capacity reduction. Every review, including a hold, uses one authorization. Manual price changes suspend the policy. No spending authority is granted; existing contracts keep their agreed terms.") }
            Copy {
                objectName: "pricingPreviewText"
                text: {
                    // Re-evaluate settled evidence even when all form values were edited.
                    const currentRevision = campaign.revision
                    return manager.pricingPreview(priceProduct.value, priceFloor.value, priceCeiling.value, priceStep.value, priceLow.value, priceHigh.value)
                }
            }
        }
        ColumnLayout {
            visible: policyKind.value === "hardware_replacement"
            Layout.fillWidth: true
            Choice { id: oldRack; objectName: "replacementRackChoice"; label: qsTr("Installed rack and first eligible date"); options: visible ? manager.replacementRacks : [] }
            Choice {
                id: replacementFamily
                objectName: "replacementFamilyChoice"
                label: qsTr("Compatible replacement and quoted cost")
                options: {
                    const currentRevision = campaign.revision
                    return visible ? manager.replacementFamilies(oldRack.value) : []
                }
            }
            Field { id: replacementPurchase; label: qsTr("Maximum replacement purchase, dollars"); value: replacementFamily.selectedOption.purchase || "" }
            Field { id: replacementMonthly; label: qsTr("Maximum replacement maintenance per month, dollars"); value: replacementFamily.selectedOption.maintenance || "" }
            Copy {
                text: oldRack.options.length === 0
                    ? qsTr("Commission owned hardware in Operations to schedule a replacement within this campaign.")
                    : qsTr("Order one rack when the original reaches its rated useful life. The site needs a spare rack slot and enough paid operators and power for both. After the replacement is operational, the old rack retires once its active and paused work, contracts and replicas have cleared. Metered electricity and water follow the site's actual use.")
            }
        }
        Copy {
            visible: policyKind.value === "operating_reserve"
            text: qsTr("Keep this amount available for operating costs. New purchases and delegated spending use cash above the target. Existing obligations, debt repayment and story consequences can draw it down. Incoming cash replenishes it automatically. Suspend the policy to release the reserve.")
        }
        Field { id: policyBudget; label: qsTr("Total policy commitment budget, dollars"); value: manager.policyDefaults.budget || ""; visible: policyKind.value === "evaluation" || policyKind.value === "lease_renewal" }
        Field {
            id: policyReserve
            objectName: "policyReserveField"
            label: policyKind.value === "operating_reserve" ? qsTr("Cash reserve target, dollars") : qsTr("Minimum cash after commitment, dollars")
            value: manager.policyDefaults.reserve || ""
        }
        Field { id: policyRuns; objectName: "policyRunsField"; label: qsTr("Maximum authorized runs"); value: manager.policyDefaults.runs || ""; visible: policyKind.value === "evaluation" || policyKind.value === "lease_renewal" }
        Copy { visible: policyKind.value === "lease_renewal"; text: qsTr("Renewal occurs on the expiry morning. The policy debits a conservative full-term commitment ceiling (setup plus enough monthly bills for the entire term); the ledger pays actual setup and daily accrued bills. It cannot renew a missed deadline retroactively.") }
        Action {
            objectName: "authorizePolicyButton"
            text: qsTr("Authorize bounded policy")
            enabled: panel.editable && (policyKind.value !== "hardware_replacement" || (oldRack.value.length > 0 && replacementFamily.value.length > 0))
            onClicked: {
                if (policyKind.value === "evaluation") manager.authorizeEvaluation(policyModel.value, policyPlan.value, policySource.value, policyRate.value, policyStaff.value, policyBudget.value, policyReserve.value, policyInterval.value, policyRuns.value)
                else if (policyKind.value === "pricing") manager.authorizePricing(priceProduct.value, priceFloor.value, priceCeiling.value, priceStep.value, priceLow.value, priceHigh.value, priceInterval.value, priceRuns.value, policyReserve.value)
                else if (policyKind.value === "operating_reserve") manager.authorizeOperatingReserve(policyReserve.value)
                else if (policyKind.value === "hardware_replacement") manager.authorizeHardwareReplacement(oldRack.value, replacementFamily.value, replacementPurchase.value, replacementMonthly.value, policyReserve.value)
                else manager.authorizeRenewal(policyLease.value, policyBudget.value, policyReserve.value, policyMonthly.value, policyRuns.value)
            }
        }
        Selection { heading: qsTr("Authorized policies"); rows: manager.policies }
        Action { objectName: "togglePolicyButton"; text: qsTr("Suspend / resume selected policy"); enabled: panel.editable && manager.policies.selectedId.length > 0; onClicked: manager.setPolicyEnabled(manager.policies.selectedId, !(manager.policies.selectedRow.fields || {}).enabled) }
        Action { text: qsTr("Renew selected lease explicitly on expiry morning"); visible: policyKind.value === "lease_renewal"; enabled: panel.editable && policyLease.value.length > 0; onClicked: manager.renewLease(policyLease.value) }
        Copy { objectName: "decisionPageLabel"; text: manager.decisionPageLabel }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Action { objectName: "newerPolicyDecisionsButton"; text: qsTr("Newer decisions"); enabled: manager.decisionPage > 0; onClicked: manager.setDecisionPage(manager.decisionPage - 1) }
            Action { objectName: "olderPolicyDecisionsButton"; text: qsTr("Older decisions"); enabled: manager.decisionPage + 1 < manager.decisionPageCount; onClicked: manager.setDecisionPage(manager.decisionPage + 1) }
        }
        Selection { heading: qsTr("Policy decision log"); rows: manager.decisions }
        Selection { heading: qsTr("Company alerts and deadlines"); rows: manager.alerts }
        Action { objectName: "acknowledgeAlertButton"; text: qsTr("Acknowledge selected alert"); enabled: panel.editable && manager.alerts.selectedRow.actionable === true; onClicked: manager.acknowledgeAlert(manager.alerts.selectedId) }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "score" && !campaign.objectives.available
        Copy { text: manager.scoreText }
        Copy { text: qsTr("Survival is awarded at the ending; work counts actual delivered tasks. Profit excludes financing proceeds. Evidence counts independently measured domains used by deployed checkpoints. Ownership points reflect retained founder stake. Scores remain provisional while time can advance.") }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.room === "office" && panel.officeSection === "governance"
        Copy { visible: !manager.governanceAvailable; text: qsTr("Select a full competition preset in a new campaign for negotiated financing, executives, board and mergers.") }
        Choice { id: corporatePage; objectName: "corporatePageChoice"; label: qsTr("Company governance section"); visible: manager.governanceAvailable; options: [{id:"people",label:qsTr("People and ownership")},{id:"finance",label:qsTr("Negotiate financing")},{id:"board",label:qsTr("Board votes and disputes")},{id:"mergers",label:qsTr("Acquisitions and subsidiaries")}] }
        ColumnLayout {
            Layout.fillWidth: true
            visible: manager.governanceAvailable && corporatePage.value === "people"
            Copy { objectName: "ownershipSummary"; text: manager.ownershipText }
            Selection { objectName: "executiveSelection"; heading: qsTr("Executive candidates and agreed terms"); rows: manager.executives }
            Action { objectName: "appointCashButton"; text: qsTr("Review cash-only appointment"); enabled: panel.editable && manager.executives.selectedRow.actionable === true; onClicked: corporateDialog.review("hireCash", manager.executives.selectedId, manager.executives.selectedRow.detail || "") }
            Action { objectName: "appointEquityButton"; text: qsTr("Review cash-plus-equity appointment"); enabled: panel.editable && manager.executives.selectedRow.actionable === true && (manager.executives.selectedRow.fields || {}).hybrid === true; onClicked: corporateDialog.review("hireEquity", manager.executives.selectedId, manager.executives.selectedRow.detail || "") }
            Field { id: poolBp; objectName: "poolExpansionField"; label: qsTr("New pool allocation in basis points (100 = 1%)"); value: "500" }
            Action { objectName: "proposePoolButton"; text: qsTr("Propose explicit pool-expansion dilution to board"); onClicked: manager.proposePool(poolBp.value) }
        }
        ColumnLayout {
            id: corporateFinancePage
            Layout.fillWidth: true
            visible: manager.governanceAvailable && corporatePage.value === "finance"
            readonly property bool listingSelected: (manager.corporateFinance.selectedRow.fields || {}).listing === true
            property int filingOffset: 0
            readonly property int currentOffset: Math.min(filingOffset, Math.max(0, manager.publicFilingCount - 1))
            Copy { text: qsTr("Submitting financing terms creates a board motion. Cash, dilution, interest and board rights take effect on approval.") }
            Selection { objectName: "corporateFinanceSelection"; heading: qsTr("Financing and public listing"); rows: manager.corporateFinance }
            Action {
                objectName: "negotiateFinanceButton"
                text: corporateFinancePage.listingSelected ? qsTr("Review IPO qualification") : qsTr("Submit selected counteroffer to board")
                enabled: panel.editable && manager.corporateFinance.selectedRow.actionable === true
                onClicked: {
                    const offer = manager.corporateFinance.selectedId
                    if (!corporateFinancePage.listingSelected) {
                        manager.negotiateFinance(offer)
                        return
                    }
                    const preview = manager.listingPreview(offer)
                    corporateDialog.review("listing", offer, preview.detail || "", preview.ready === true)
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: manager.publicListingText.length > 0
                Copy { text: qsTr("Public filings"); font.bold: true; color: "#ffd897" }
                Copy { text: manager.publicListingText }
                Copy {
                    objectName: "publicFilingDetail"
                    text: {
                        const count = manager.publicFilingCount
                        return manager.publicFiling(count === 0 ? 0 : corporateFinancePage.currentOffset)
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Action { text: qsTr("Newer filing"); enabled: corporateFinancePage.currentOffset > 0; onClicked: corporateFinancePage.filingOffset = corporateFinancePage.currentOffset - 1 }
                    Action { text: qsTr("Older filing"); enabled: corporateFinancePage.currentOffset + 1 < manager.publicFilingCount; onClicked: corporateFinancePage.filingOffset = corporateFinancePage.currentOffset + 1 }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: manager.governanceAvailable && corporatePage.value === "board"
            Copy { objectName: "boardSummary"; text: manager.boardText }
            Selection { objectName: "charterSelection"; heading: qsTr("Organizational charter and employee expectations"); rows: manager.charters }
            Action { objectName: "proposeCharterButton"; text: qsTr("Submit selected charter to board"); enabled: panel.editable && manager.charters.selectedRow.actionable === true; onClicked: manager.proposeCharter(manager.charters.selectedId) }
            Selection { objectName: "boardMotionSelection"; heading: qsTr("Board motions and recorded votes"); rows: manager.boardMotions }
            Action { objectName: "voteBoardYesButton"; text: qsTr("Review and vote in support"); enabled: panel.editable && manager.boardMotions.selectedRow.actionable === true; onClicked: corporateDialog.review("voteYes", manager.boardMotions.selectedId, manager.boardMotions.selectedRow.detail || "") }
            Action { objectName: "voteBoardNoButton"; text: qsTr("Vote against selected motion"); enabled: panel.editable && manager.boardMotions.selectedRow.actionable === true; onClicked: manager.voteBoard(manager.boardMotions.selectedId, false) }
            Selection { objectName: "corporateDisputeSelection"; heading: qsTr("Corporate concerns and response windows"); rows: manager.corporateDisputes }
            Action { objectName: "acknowledgeCorporateButton"; text: qsTr("Acknowledge cause and response window"); enabled: panel.editable && manager.corporateDisputes.selectedRow.actionable === true; onClicked: manager.respondDispute(manager.corporateDisputes.selectedId, false) }
            Action { objectName: "waiveCovenantButton"; text: qsTr("Review paid covenant waiver"); enabled: panel.editable && (manager.corporateDisputes.selectedRow.fields || {}).waiver === true; onClicked: corporateDialog.review("waiver", manager.corporateDisputes.selectedId, (manager.corporateDisputes.selectedRow.detail || "") + "\n" + qsTr("The agreed waiver fee is shown in the financing counteroffer. This permanently waives the cash-floor covenant and preserves principal, interest and maturity.")) }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: manager.governanceAvailable && corporatePage.value === "mergers"
            Selection { objectName: "mergerTargetSelection"; heading: qsTr("Acquisition targets and owned subsidiaries"); rows: manager.mergerTargets }
            Choice { id: mergerPackage; objectName: "mergerPackageChoice"; label: qsTr("Negotiated consideration"); options: [{id:"cash",label:qsTr("All cash")},{id:"equity",label:qsTr("Half cash plus equity counteroffer")}] }
            Action {
                objectName: "reviewMergerButton"
                text: qsTr("Review seller due diligence and submit terms")
                enabled: panel.editable && manager.mergerTargets.selectedRow.actionable === true
                onClicked: {
                    const id = manager.mergerTargets.selectedId
                    const equity = mergerPackage.value === "equity"
                    const terms = manager.acquisitionPreview(id, equity)
                    if (terms.length > 0) corporateDialog.review(equity ? "mergerEquity" : "mergerCash", id, terms)
                }
            }
            Field { id: groupDollars; objectName: "groupTransferField"; label: qsTr("Group transfer amount, dollars"); value: "1000.00" }
            Action { objectName: "fundSubsidiaryButton"; text: qsTr("Review paid subsidiary capital funding"); enabled: panel.editable && (manager.mergerTargets.selectedRow.fields || {}).owned === true; onClicked: { corporateDialog.groupAmount = groupDollars.value; corporateDialog.review("groupFund", manager.mergerTargets.selectedId, qsTr("Transfer $%1 from your company's available cash to its owned subsidiary as capital? Cash is conserved; the payment is an investment, not an expense or product revenue.").arg(groupDollars.value) + "\n" + (manager.mergerTargets.selectedRow.detail || "")) } }
            Action { objectName: "dividendSubsidiaryButton"; text: qsTr("Review earned-profit dividend"); enabled: panel.editable && (manager.mergerTargets.selectedRow.fields || {}).owned === true; onClicked: { corporateDialog.groupAmount = groupDollars.value; corporateDialog.review("groupDividend", manager.mergerTargets.selectedId, qsTr("Request $%1 of actual undistributed profit earned since acquisition? The subsidiary must retain its cash floor and projected bills. Internal dividend income is eliminated from group profit.").arg(groupDollars.value) + "\n" + (manager.mergerTargets.selectedRow.detail || "")) } }
        }
    }
    Dialog {
        id: corporateDialog
        objectName: "corporateConfirmation"
        property string actionKind: ""
        property string subjectId: ""
        property string terms: ""
        property string groupAmount: ""
        property bool confirmable: true
        function review(kind, subject, detail, canConfirm) {
            actionKind = kind
            subjectId = subject
            terms = detail
            confirmable = canConfirm !== false
            open()
        }
        title: actionKind === "listing" ? qsTr("Review public listing") : actionKind === "hireEquity" ? qsTr("Agree cash-plus-equity compensation?") : actionKind === "hireCash" ? qsTr("Agree cash-only compensation?") : qsTr("Confirm reviewed corporate terms?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(720, Overlay.overlay.width - 36)
        height: Math.min(620, Overlay.overlay.height - 60)
        standardButtons: confirmable ? (Dialog.Ok | Dialog.Cancel) : Dialog.Close
        contentItem: ScrollView {
            id: corporateTerms
            clip: true
            contentWidth: availableWidth
            Label { width: corporateTerms.availableWidth; text: corporateDialog.terms; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        }
        onAccepted: {
            if (!confirmable) return
            if (actionKind === "listing") manager.negotiateFinance(subjectId)
            else if (actionKind === "hireCash" || actionKind === "hireEquity") manager.appointExecutive(subjectId, actionKind === "hireEquity")
            else if (actionKind === "voteYes") manager.voteBoard(subjectId, true)
            else if (actionKind === "waiver") manager.respondDispute(subjectId, true)
            else if (actionKind === "groupFund" || actionKind === "groupDividend") manager.transferSubsidiaryFunds(subjectId, groupAmount, actionKind === "groupFund")
            else manager.proposeAcquisition(subjectId, actionKind === "mergerEquity")
        }
    }
    Dialog {
        id: fundingDialog
        objectName: "fundingConfirmation"
        title: qsTr("Accept fixed financing terms?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, Overlay.overlay.width - 36)
        standardButtons: Dialog.Ok | Dialog.Cancel
        height: Math.min(620, Overlay.overlay.height - 60)
        contentItem: ScrollView {
            id: fundingTerms
            clip: true
            contentWidth: availableWidth
            Label { width: fundingTerms.availableWidth; text: manager.funding.selectedRow.detail || ""; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        }
        onAccepted: manager.takeFinancing(manager.funding.selectedId)
    }
    Dialog {
        id: competitiveDialog
        objectName: "competitiveOfferConfirmation"
        property string offerId: ""
        property string terms: ""
        title: qsTr("Accept this funded agreement?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(720, Overlay.overlay.width - 36)
        height: Math.min(620, Overlay.overlay.height - 60)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            id: competitiveTerms
            clip: true
            contentWidth: availableWidth
            Label { width: competitiveTerms.availableWidth; text: competitiveDialog.terms; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        }
        onAccepted: manager.respondCompetitiveOffer(offerId, true)
    }
    Dialog {
        id: advanceDialog
        objectName: "customerAdvanceConfirmation"
        property string offerId: ""
        property string terms: ""
        title: qsTr("Accept contract and receive advance?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, Overlay.overlay.width - 36)
        height: Math.min(620, Overlay.overlay.height - 60)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            id: advanceTerms
            clip: true
            contentWidth: availableWidth
            Label { width: advanceTerms.availableWidth; text: advanceDialog.terms; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        }
        onAccepted: manager.acceptOffer(offerId)
    }
    Dialog {
        id: cancelDialog
        objectName: "cancelCustomerContractConfirmation"
        property string contractId: ""
        property string terms: ""
        title: qsTr("Cancel customer commitment?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, Overlay.overlay.width - 36)
        height: Math.min(560, Overlay.overlay.height - 60)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ScrollView {
            id: cancellationTerms
            clip: true
            contentWidth: availableWidth
            Label { width: cancellationTerms.availableWidth; text: cancelDialog.terms; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: panel.textSize }
        }
        onAccepted: manager.cancelContract(contractId)
    }
}
