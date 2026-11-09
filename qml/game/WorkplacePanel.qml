import QtQuick

Loader {
    id:panel
    property int textSize:14
    signal visitRoom(string place)
    signal openInfrastructure()
    sourceComponent:Object.keys(campaign.world.headquarters).length?physical:legacy
    Component {id:physical;HeadquartersPanel {textSize:panel.textSize;onVisitRoom:place=>panel.visitRoom(place);onOpenInfrastructure:panel.openInfrastructure()}}
    Component {id:legacy;LegacyWorkplacePanel {textSize:panel.textSize;onVisitRoom:place=>panel.visitRoom(place);onOpenInfrastructure:panel.openInfrastructure()}}
}
