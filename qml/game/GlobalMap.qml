import QtQuick
import QtQuick.Controls

Item {
    id: map
    property var geometry: ({})
    property string selectedId: "USA"
    property string hoverName: ""
    property string hoverId: ""
    property string hoverRegion: ""
    property int textSize: 14
    property point hoverPosition: Qt.point(0,0)
    property var regions: null
    property var offices: []
    property var briefing: ({})
    readonly property string hoverSummary: (briefing.title ? qsTr("Regional context · %1").arg(briefing.title) : "")+
        (briefing.detail ? "\n"+briefing.detail.split("\n").slice(0,2).join("\n") : "")+"\n"+
        qsTr("Your operating offices in this region: %1").arg(offices.filter(row=>row.operating && row.region===hoverRegion).length)
    readonly property var countries: geometry.countries || []
    signal countryPicked(string id, string region)
    Accessible.name: qsTr("World countries. Use the country list for keyboard selection and regional details.")
    Accessible.role: Accessible.Graphic
    function refreshBriefing() { briefing=regions && hoverRegion.length ? regions.sourceRow(hoverRegion) : ({}) }
    onRegionsChanged: refreshBriefing()
    onHoverRegionChanged: refreshBriefing()
    Connections { target:map.regions;function onReconciled(){map.refreshBriefing()} }
    function hover(country) {
        const id=country ? country.id : ""
        if(id===hoverId) return
        hoverId=id;hoverName=country ? country.name : "";hoverRegion=country ? country.region : ""
        canvas.requestPaint()
    }
    function trace(ctx, country) {
        ctx.beginPath()
        for (let ring of country.rings) {
            for (let i=0;i<ring.length;++i) {
                let x=ring[i][0]/36000*canvas.width, y=ring[i][1]/18000*canvas.height
                if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y)
            }
            ctx.closePath()
        }
    }
    function hit(x,y) {
        let ctx=canvas.getContext("2d")
        for(let country of countries) {trace(ctx,country);if(ctx.isPointInPath(x,y)) return country}
        return null
    }
    onGeometryChanged: canvas.requestPaint()
    onSelectedIdChanged: canvas.requestPaint()
    onVisibleChanged: {if(visible)canvas.requestPaint();else hover(null)}
    onTextSizeChanged: canvas.requestPaint()
    Canvas {
        id:canvas;objectName:"worldCountryCanvas";anchors.centerIn:parent;width:Math.min(parent.width,parent.height*2);height:width/2
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onPaint: {
            let ctx=getContext("2d");ctx.reset();ctx.fillStyle="#102b33";ctx.fillRect(0,0,width,height)
            ctx.lineWidth=Math.max(0.6,map.textSize/14*0.6);ctx.strokeStyle="#14363c"
            for(let country of map.countries) {
                map.trace(ctx,country)
                ctx.fillStyle=country.id===map.selectedId?"#dfbd7b":country.name===map.hoverName?"#a8cab5":"#628c84"
                ctx.fill();ctx.stroke()
            }
        }
    }
    MouseArea {
        anchors.fill:canvas;hoverEnabled:true
        onPositionChanged:mouse=>{map.hoverPosition=mapToItem(Overlay.overlay,mouse.x,mouse.y);map.hover(map.hit(mouse.x,mouse.y))}
        onExited:map.hover(null)
        onClicked:mouse=>{let country=map.hit(mouse.x,mouse.y);if(country) map.countryPicked(country.id,country.region)}
    }
    MapHoverCard {
        objectName:"countryHoverCard";host:map
        visible:map.hoverId.length>0
        heading:map.hoverName;detail:map.hoverSummary
        pointer:map.hoverPosition;textSize:map.textSize
    }
}
