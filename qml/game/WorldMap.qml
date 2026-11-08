import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id:mapPage
    property string scale:"hq"
    property int textSize:14
    property string selectedCounty:"06037"
    property string selectedCountry:"USA"
    readonly property var counties:campaign.world.countyRows
    readonly property var selectedCountyRow:counties.find(row=>row.id===selectedCounty) || counties[0] || ({})
    signal openApp(string app,string section)
    signal visitHeadquarters()
    color:"#173035";radius:12;border.color:"#415d59"
    ColumnLayout {
        anchors.fill:parent;anchors.margins:16;spacing:12
        Flow {
            Layout.fillWidth:true;spacing:8
            Repeater {model:campaign.world.scales;WorldButton {required property var modelData;objectName:"mapScale_"+modelData.id;text:modelData.name;textSize:mapPage.textSize;checked:mapPage.scale===modelData.id;onClicked:campaign.world.navigate("map",modelData.id,"","","")}}
        }
        ScrollView {
            id: mapScroll
            Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
            ColumnLayout {
                width:mapScroll.availableWidth;spacing:12
                Label {Layout.fillWidth:true;text:mapPage.scale==="hq"?qsTr("Your headquarters"):mapPage.scale==="city"?campaign.world.location:mapPage.scale==="state"?qsTr("Regional conditions"):mapPage.scale==="country"?qsTr("United States"):qsTr("The wider world");font.pixelSize:mapPage.textSize*1.6;font.bold:true;color:"#eee5d2";wrapMode:Text.Wrap}
                Label {Layout.fillWidth:true;text:mapPage.scale==="hq"?campaign.world.setting:mapPage.scale==="city"?qsTr("Your facilities and scheduled visits. Travel follows your calendar."):mapPage.scale==="state"?qsTr("Select a state to inspect its local conditions and infrastructure."):mapPage.scale==="country"?qsTr("Explore states, infrastructure and the public affairs that affect your company."):qsTr("Public regional conditions and your operating footprint. Market access and institutional authority follow your company's actual progress.");font.pixelSize:mapPage.textSize;color:"#b7cec4";wrapMode:Text.Wrap}
                Flow {
                    visible:mapPage.scale==="hq";Layout.fillWidth:true
                    spacing:8
                    Repeater {model:campaign.world.rooms;WorldButton {required property var modelData;text:modelData.name;textSize:mapPage.textSize;onClicked:campaign.world.navigate(modelData.id,"hq","","","")}}
                }
                WorldButton {visible:mapPage.scale==="hq";text:qsTr("Return to the workspace");textSize:mapPage.textSize;primary:true;onClicked:mapPage.visitHeadquarters()}
                WorldButton {visible:mapPage.scale==="hq";text:qsTr("Expand facilities & capabilities");textSize:mapPage.textSize;onClicked:mapPage.openApp("operations","")}
                DomesticMap {
                    visible:mapPage.scale==="country"
                    Layout.fillWidth:true;Layout.preferredHeight:Math.max(220,Math.min(440,mapPage.height*0.55))
                    geometry:campaign.infrastructure.mapGeometry
                    selectedId:campaign.infrastructure.states.selectedId
                    onStatePicked:id=>{campaign.infrastructure.states.selectedId=id;campaign.world.countyState=id;mapPage.selectedCounty=""}
                }
                ComboBox {
                    visible:mapPage.scale==="state" || mapPage.scale==="country"
                    Layout.fillWidth:true;model:campaign.infrastructure.states;textRole:"title";valueRole:"entityId"
                    currentIndex:campaign.infrastructure.states.indexOf(campaign.infrastructure.states.selectedId)
                    font.pixelSize:mapPage.textSize;Accessible.name:qsTr("State")
                    onActivated:{campaign.infrastructure.states.selectedId=currentValue;campaign.world.countyState=currentValue;mapPage.selectedCounty=""}
                }
                Label {visible:mapPage.scale==="country";Layout.fillWidth:true;text:campaign.infrastructure.states.selectedRow.detail || "";textFormat:Text.PlainText;font.pixelSize:mapPage.textSize;color:"#d6e2d6";wrapMode:Text.Wrap}
                CountyMap {
                    visible:mapPage.scale==="state" && mapPage.counties.length>0
                    Layout.fillWidth:true;Layout.preferredHeight:Math.max(180,Math.min(260,mapPage.height*0.4))
                    rows:mapPage.counties;selectedId:mapPage.selectedCountyRow.id || ""
                    onCountyPicked:id=>mapPage.selectedCounty=id
                }
                ComboBox {
                    visible:mapPage.scale==="state" && mapPage.counties.length>0
                    Layout.fillWidth:true;model:mapPage.counties;textRole:"name";valueRole:"id"
                    currentIndex:Math.max(0,model.findIndex(row=>row.id===mapPage.selectedCounty));font.pixelSize:mapPage.textSize
                    Accessible.name:qsTr("County")
                    onActivated:mapPage.selectedCounty=currentValue
                }
                Label {visible:mapPage.scale==="state";Layout.fillWidth:true;text:mapPage.selectedCountyRow.detail || "";textFormat:Text.PlainText;font.pixelSize:mapPage.textSize;color:"#d6e2d6";wrapMode:Text.Wrap}
                Label {visible:mapPage.scale==="state";Layout.fillWidth:true;text:campaign.world.countySummary;font.pixelSize:mapPage.textSize*0.9;color:"#acc4b8";wrapMode:Text.Wrap}
                Label {visible:mapPage.scale==="city";Layout.fillWidth:true;text:campaign.infrastructure.executiveLocation;font.pixelSize:mapPage.textSize;color:"#d6e2d6";wrapMode:Text.Wrap}
                StableEntityList {visible:mapPage.scale==="city";Layout.fillWidth:true;Layout.preferredHeight:Math.min(240,Math.max(95,rows.count*80));rows:campaign.infrastructure.sites;textSize:mapPage.textSize;reducedMotion:campaign.reducedMotion;emptyText:qsTr("No additional facilities. Your headquarters is your company's base.");onSelected:id=>rows.selectedId=id}
                Label {visible:mapPage.scale==="city";Layout.fillWidth:true;text:campaign.infrastructure.sites.selectedRow.detail || "";font.pixelSize:mapPage.textSize;color:"#d6e2d6";wrapMode:Text.Wrap}
                GlobalMap {
                    visible:mapPage.scale==="world";Layout.fillWidth:true;Layout.preferredHeight:Math.max(180,Math.min(330,width*0.5))
                    geometry:campaign.world.worldGeometry;selectedId:mapPage.selectedCountry
                    onCountryPicked:(id,region)=>{mapPage.selectedCountry=id;campaign.society.regions.selectedId=region}
                }
                ComboBox {
                    visible:mapPage.scale==="world";Layout.fillWidth:true
                    model:campaign.world.worldGeometry.countries || [];textRole:"name";valueRole:"id";font.pixelSize:mapPage.textSize
                    currentIndex:Math.max(0,model.findIndex(row=>row.id===mapPage.selectedCountry));Accessible.name:qsTr("Country")
                    onActivated:{mapPage.selectedCountry=currentValue;campaign.society.regions.selectedId=model[currentIndex].region}
                }
                Label {visible:mapPage.scale==="world";Layout.fillWidth:true;text:qsTr("Conditions below describe the country's game region. Individual country economies and private company rankings are not estimated.");wrapMode:Text.Wrap;font.pixelSize:mapPage.textSize;color:"#acc4b8"}
                StableEntityList {visible:mapPage.scale==="world";Layout.fillWidth:true;Layout.preferredHeight:180;rows:campaign.society.regions;textSize:mapPage.textSize;reducedMotion:campaign.reducedMotion;emptyText:qsTr("Regional briefings will appear as this campaign develops.");onSelected:id=>rows.selectedId=id}
                Label {visible:mapPage.scale==="world";Layout.fillWidth:true;text:campaign.society.regions.selectedRow.detail || campaign.society.global;font.pixelSize:mapPage.textSize;color:"#d6e2d6";wrapMode:Text.Wrap}
                Flow {
                    Layout.fillWidth:true;spacing:8
                    WorldButton {visible:mapPage.scale!=="hq";text:qsTr("Facilities & travel");textSize:mapPage.textSize;onClicked:mapPage.openApp("operations","")}
                    WorldButton {visible:mapPage.scale==="country" || mapPage.scale==="state";text:qsTr("Public affairs");textSize:mapPage.textSize;onClicked:mapPage.openApp("civic","")}
                    WorldButton {visible:mapPage.scale==="world";text:qsTr("Markets & presence");textSize:mapPage.textSize;onClicked:mapPage.openApp("office","markets")}
                    WorldButton {visible:mapPage.scale==="world";text:qsTr("World affairs");textSize:mapPage.textSize;onClicked:mapPage.openApp("society","")}
                }
                Label {visible:mapPage.scale==="state" || mapPage.scale==="country";Layout.fillWidth:true;text:campaign.infrastructure.mapCredit;font.pixelSize:mapPage.textSize*0.8;color:"#9db9ae";wrapMode:Text.Wrap}
                Label {visible:mapPage.scale==="world";Layout.fillWidth:true;text:campaign.world.worldGeometry.credit || "";font.pixelSize:mapPage.textSize*0.8;color:"#9db9ae";wrapMode:Text.Wrap}
            }
        }
    }
}
