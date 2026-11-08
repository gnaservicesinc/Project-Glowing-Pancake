import QtQuick
Canvas {
    id:map
    property var rows:[]
    property string selectedId:""
    signal countyPicked(string id)
    Accessible.name:qsTr("County locations. Choose a county from the adjacent list for keyboard access.")
    Accessible.role:Accessible.Graphic
    readonly property var bounds: {
        let west=180,east=-180,south=90,north=-90
        for(const row of rows) {west=Math.min(west,row.longitude);east=Math.max(east,row.longitude);south=Math.min(south,row.latitude);north=Math.max(north,row.latitude)}
        return {west:west,east:east,south:south,north:north}
    }
    function point(row) {
        const b=bounds,ratio=Math.min((width-50)/Math.max(0.1,b.east-b.west),(height-50)/Math.max(0.1,b.north-b.south))
        return {x:width/2+(row.longitude-(b.west+b.east)/2)*ratio,y:height/2-((row.latitude-(b.south+b.north)/2))*ratio}
    }
    onRowsChanged:requestPaint()
    onSelectedIdChanged:requestPaint()
    onWidthChanged:requestPaint()
    onHeightChanged:requestPaint()
    onPaint:{
        const ctx=getContext("2d");ctx.reset();ctx.fillStyle="#10292f";ctx.fillRect(0,0,width,height)
        for(const row of rows) {
            const p=point(row),selected=row.id===selectedId
            ctx.beginPath();ctx.arc(p.x,p.y,selected?7:4,0,Math.PI*2)
            ctx.fillStyle=selected?"#f1c98c":row.economy>5100?"#b4d6ad":row.economy<4900?"#c48d76":"#75b0a5";ctx.fill()
            if(selected) {ctx.font="14px sans-serif";ctx.fillStyle="#f3ead2";ctx.textAlign=p.x>width/2?"right":"left";ctx.fillText(row.name,p.x+(p.x>width/2?-12:12),p.y-10)}
        }
    }
    MouseArea {
        anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor
        function pick(x,y) {let best=null,distance=18;for(const row of map.rows) {const p=map.point(row),d=Math.hypot(x-p.x,y-p.y);if(d<distance) {best=row;distance=d}};if(best)map.countyPicked(best.id)}
        onClicked:mouse=>pick(mouse.x,mouse.y)
        onPositionChanged:mouse=>pick(mouse.x,mouse.y)
    }
}
