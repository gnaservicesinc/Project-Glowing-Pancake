import QtQuick
import QtQuick.Controls

Item {
    id: map
    property var geometry: ({})
    property string selectedId: "USA"
    property string hoverName: ""
    readonly property var countries: geometry.countries || []
    signal countryPicked(string id, string region)
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
    Canvas {
        id:canvas;anchors.centerIn:parent;width:Math.min(parent.width,parent.height*2);height:width/2
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onPaint: {
            let ctx=getContext("2d");ctx.reset();ctx.fillStyle="#102b33";ctx.fillRect(0,0,width,height)
            ctx.lineWidth=0.6;ctx.strokeStyle="#14363c"
            for(let country of map.countries) {
                map.trace(ctx,country)
                ctx.fillStyle=country.id===map.selectedId?"#dfbd7b":country.name===map.hoverName?"#a8cab5":"#628c84"
                ctx.fill();ctx.stroke()
            }
        }
    }
    MouseArea {
        anchors.fill:canvas;hoverEnabled:true
        onPositionChanged:mouse=>{let country=map.hit(mouse.x,mouse.y);map.hoverName=country?country.name:"";canvas.requestPaint()}
        onExited:{map.hoverName="";canvas.requestPaint()}
        onClicked:mouse=>{let country=map.hit(mouse.x,mouse.y);if(country) map.countryPicked(country.id,country.region)}
    }
    Label {anchors.left:parent.left;anchors.bottom:parent.bottom;anchors.margins:8;text:map.hoverName;color:"#f2ead6";font.pixelSize:14}
}
