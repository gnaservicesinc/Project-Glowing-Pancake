import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: editor
    objectName: "assetRegisterDialog"
    required property QtObject author
    required property QtObject session
    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 900)
    height: parent.height - 32
    modal: true
    standardButtons: Dialog.Close
    title: qsTr("Asset register and offline credits")
    property var record: ({})
    property string selectedId: ""
    property bool pinned: false

    function select(id) {
        selectedId = id
        record = author.record(id)
        pinned = session.assetPinned(id)
        importPath.clear()
    }
    function setField(key, value) {
        const next = Object.assign({}, record)
        next[key] = value
        record = next
    }
    onOpened: select("")
    contentItem: ColumnLayout {
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: assets
                objectName: "registeredAssets"
                Layout.fillWidth: true
                model: editor.author.entries
                textRole: "label"
                valueRole: "id"
                currentIndex: indexOfValue(editor.selectedId)
                Accessible.name: qsTr("Asset records")
                onActivated: editor.select(currentValue)
            }
            Button { text: qsTr("New asset"); onClicked: editor.select("") }
            Button { objectName: "verifyAssetRecord"; text: qsTr("Verify file and credits"); enabled: editor.selectedId.length > 0; onClicked: editor.author.inspect(editor.selectedId) }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Register a file already under assets, or copy an explicitly chosen local file into the checkout. Keep its license evidence with the project.")
            wrapMode: Text.Wrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                TextField {
                    id: importPath
                    objectName: "assetImportPath"
                    visible: editor.selectedId.length === 0
                    Layout.fillWidth: true
                    maximumLength: 4096
                    placeholderText: qsTr("Optional absolute path to copy")
                    Accessible.name: qsTr("Local asset file to import")
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: ["image", "audio", "font", "data"]
                    currentIndex: find(editor.record.kind || "image")
                    Accessible.name: qsTr("Asset media kind")
                    onActivated: editor.setField("kind", currentText)
                }
                Repeater {
                    model: [
                        { key: "id", label: qsTr("Stable asset ID") }, { key: "path", label: qsTr("Checkout-relative asset path") },
                        { key: "title", label: qsTr("Title") }, { key: "creator", label: qsTr("Creator") },
                        { key: "source_url", label: qsTr("Source URL") }, { key: "retrieved_on", label: qsTr("Retrieved on (YYYY-MM-DD)") },
                        { key: "license_id", label: qsTr("License ID") }, { key: "license_url", label: qsTr("License URL") },
                        { key: "license_evidence_path", label: qsTr("Checkout-relative license evidence path") },
                        { key: "modifications", label: qsTr("Modifications and lineage") }, { key: "purpose", label: qsTr("Purpose") },
                        { key: "generation_record", label: qsTr("Generation provenance (when AI generated)") },
                        { key: "credit_text", label: qsTr("Offline credit text") }, { key: "credit_group", label: qsTr("Credit group") },
                        { key: "review_status", label: qsTr("Review status") }
                    ]
                    delegate: ColumnLayout {
                        id: row
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: row.modelData.label; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        TextField {
                            objectName: "assetField/" + row.modelData.key
                            Layout.fillWidth: true
                            text: editor.record[row.modelData.key] || ""
                            readOnly: editor.selectedId.length > 0 && (row.modelData.key === "id" || row.modelData.key === "path")
                            maximumLength: 4096
                            Accessible.name: row.modelData.label
                            onTextEdited: editor.setField(row.modelData.key, text)
                        }
                    }
                }
                CheckBox {
                    text: qsTr("AI generated")
                    checked: !!editor.record.ai_generated
                    onToggled: editor.setField("ai_generated", checked)
                }
                Label {
                    visible: !!editor.record.sha256
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: "SHA-256 " + (editor.record.sha256 || "")
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: editor.author.message
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 6
            elide: Text.ElideRight
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Button {
                objectName: "saveAssetRecord"
                text: qsTr("Save asset record")
                enabled: !!editor.record.id
                onClicked: {
                    const id = editor.record.id
                    if (editor.author.save(editor.record, importPath.text.trim())) editor.select(id)
                }
            }
            Button {
                objectName: "pinAssetRecord"
                text: editor.pinned ? qsTr("Refresh review pin") : qsTr("Pin saved record to draft")
                enabled: editor.session.editing && editor.selectedId.length > 0
                onClicked: { if (editor.session.pinAsset(editor.selectedId,true)) editor.pinned = true }
            }
            Button {
                text: qsTr("Remove draft pin")
                visible: editor.pinned
                onClicked: { if (editor.session.pinAsset(editor.selectedId,false)) editor.pinned = false }
            }
        }
        Label {
            visible: editor.session.draftMessage.length > 0
            Layout.fillWidth: true
            text: editor.session.draftMessage
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }
    }
}
