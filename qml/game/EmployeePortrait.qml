import QtQuick
import QtQuick.Controls
import QtQuick.Window

Button {
    id:portrait
    property url portraitSource
    property string personName:""
    property bool reducedMotion:true
    implicitWidth:96;implicitHeight:implicitWidth
    padding:3;hoverEnabled:true
    Accessible.name:qsTr("View %1's profile").arg(personName)
    ToolTip.visible:hovered
    ToolTip.text:Accessible.name
    background:Rectangle {
        color:portrait.hovered || portrait.visualFocus?"#d9b776":"transparent"
        radius:Math.max(8,width*0.12)
        border.color:portrait.visualFocus?"#fff5db":"#728e85"
        border.width:portrait.visualFocus?2:1
    }
    contentItem:Image {
        objectName:portrait.objectName+"Image"
        source:portrait.portraitSource
        fillMode:Image.PreserveAspectFit
        // Re-rasterize the vector at the actual displayed device resolution.
        readonly property int rasterEdge:Math.min(2048,Math.ceil(width*Screen.devicePixelRatio))
        sourceSize:Qt.size(rasterEdge,rasterEdge)
        smooth:true;cache:true
        scale:portrait.down?0.97:1
        Behavior on scale {enabled:!portrait.reducedMotion;NumberAnimation {duration:90}}
        Accessible.ignored:true
    }
}
