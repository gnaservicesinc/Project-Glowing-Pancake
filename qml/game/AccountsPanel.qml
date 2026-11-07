import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var manager
    required property int textSize
    required property bool readyForForecast
    property string section: "statements"
    property var forecast: ({})
    readonly property var statements: visible && section === "statements" ? manager.financialStatements : ({})
    spacing: 9

    component Line: Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: panel.textSize
    }
    component Heading: Line {
        font.bold: true
        color: "#ffd897"
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: [
                { id: "statements", label: qsTr("Statements") },
                { id: "scenarios", label: qsTr("Cash scenarios") },
                { id: "bills", label: qsTr("Bills and debt") }
            ]
            Button {
                required property var modelData
                objectName: "accounts_" + modelData.id
                text: modelData.label
                font.pixelSize: panel.textSize
                checked: panel.section === modelData.id
                Accessible.name: text
                onClicked: panel.section = modelData.id
            }
        }
    }
    Line { text: qsTr("Company accounts · USD in 2014 purchasing power") }
    ColumnLayout {
        visible: panel.section === "statements"
        Layout.fillWidth: true
        Heading { text: qsTr("Balance sheet · %1").arg(panel.statements.date || "") }
        Line { objectName: "financeAssets"; text: panel.statements.assets || "" }
        Line { objectName: "financeLiabilities"; text: panel.statements.liabilities || "" }
        Line { text: panel.statements.equity || "" }
        Heading { text: qsTr("Last 30 settled days") }
        Line {
            text: panel.statements.period || qsTr("The first settled day will appear here after time advances.")
        }
        Heading { visible: Boolean(panel.statements.period); text: qsTr("Profit and loss") }
        Line { objectName: "financeProfit"; visible: Boolean(panel.statements.period); text: panel.statements.profit || "" }
        Heading { visible: Boolean(panel.statements.period); text: qsTr("Cash flow") }
        Line { objectName: "financeCashFlow"; visible: Boolean(panel.statements.period); text: panel.statements.cash || "" }
        Heading { visible: Boolean(panel.statements.period); text: qsTr("Unit economics") }
        Line { objectName: "financeUnits"; visible: Boolean(panel.statements.period); text: panel.statements.units || "" }
    }
    ColumnLayout {
        visible: panel.section === "scenarios"
        Layout.fillWidth: true
        Heading { text: qsTr("Cash scenarios for 3 and 12 months") }
        Line {
            text: qsTr("Compare how demand could affect cash and when a bill might go unpaid. Pause time to calculate from the current accounts.")
        }
        Button {
            objectName: "calculateCashScenariosButton"
            text: qsTr("Calculate cash scenarios")
            font.pixelSize: panel.textSize
            Accessible.name: text
            enabled: panel.readyForForecast
            onClicked: panel.forecast = panel.manager.cashScenarios()
        }
        Line {
            objectName: "cashScenarioAssumptions"
            visible: text.length > 0
            text: panel.forecast.error || (panel.forecast.date
                ? qsTr("As of %1\n%2").arg(panel.forecast.date).arg(panel.forecast.assumptions) : "")
        }
        Repeater {
            model: panel.forecast.cases || []
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Heading { text: modelData.title }
                Line { text: modelData.sales }
                Line { text: qsTr("3 months\n%1").arg(modelData.near) }
                Line { text: qsTr("12 months\n%1").arg(modelData.year) }
                Line { text: modelData.risk }
            }
        }
    }
    Connections {
        target: panel.manager
        function onChanged() { panel.forecast = ({}) }
    }
}
