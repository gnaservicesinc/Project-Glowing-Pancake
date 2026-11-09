import QtQuick
import QtQuick.Controls
import "StateProjection.js" as Atlas

Canvas {
    id: map
    property var rows: []
    property var geometry: ({})
    property string stateId: ""
    property string selectedId: ""
    property int textSize: 14
    readonly property real markerScale: textSize / 14
    readonly property var feature: (geometry.features || []).find(state => state.id === stateId) || null
    readonly property string projectionGroup: stateId === "US-AK" || stateId === "US-HI" ? stateId : "continental"
    readonly property var projection: Atlas.coefficients[projectionGroup]
    property string hoverId: ""
    property point hoverPosition: Qt.point(0, 0)
    readonly property var hovered: rows.find(row => row.id === hoverId) || ({})
    signal countyPicked(string id)

    Accessible.name: feature ? qsTr("%1 state outline and county locations. Use the county selector for keyboard access.").arg(feature.name)
                             : qsTr("County locations. Use the county selector for keyboard access.")
    Accessible.role: Accessible.Graphic

    readonly property var bounds: {
        let left = Infinity, right = -Infinity, top = Infinity, bottom = -Infinity
        function include(x,y) {
            left = Math.min(left,x)
            right = Math.max(right,x)
            top = Math.min(top,y)
            bottom = Math.max(bottom,y)
        }
        if (feature) {
            for (const ring of feature.rings)
                for (const vertex of ring) include(vertex[0],vertex[1])
        } else {
            for (const row of rows) {
                const p = atlasPoint(row)
                include(p.x,p.y)
            }
        }
        if (!Number.isFinite(left)) return {left:0,right:1,top:0,bottom:1}
        return {left:left,right:right,top:top,bottom:bottom}
    }
    readonly property real padding: Math.min(width/5, height/5, textSize*2)
    readonly property real ratio: Math.min(
        Math.max(1,width-padding*2) / Math.max(1,bounds.right-bounds.left),
        Math.max(1,height-padding*2) / Math.max(1,bounds.bottom-bounds.top))
    readonly property var nearby: {
        if (!feature || projectionGroup !== "continental") return []
        const middleX = (bounds.left+bounds.right)/2
        const middleY = (bounds.top+bounds.bottom)/2
        const halfWidth = width/ratio/2
        const halfHeight = height/ratio/2
        return geometry.features.filter(state => {
            if (state.id === stateId || state.id === "US-AK" || state.id === "US-HI") return false
            return state.rings.some(ring => {
                let left=Infinity,right=-Infinity,top=Infinity,bottom=-Infinity
                for (const vertex of ring) {
                    left=Math.min(left,vertex[0]); right=Math.max(right,vertex[0])
                    top=Math.min(top,vertex[1]); bottom=Math.max(bottom,vertex[1])
                }
                return right>=middleX-halfWidth && left<=middleX+halfWidth &&
                       bottom>=middleY-halfHeight && top<=middleY+halfHeight
            })
        })
    }
    function atlasPoint(row) {
        let longitude = row.longitude
        if (stateId === "US-AK" && longitude > 0) longitude -= 360
        return {x: longitude*projection[0]+projection[2], y: row.latitude*projection[1]+projection[3]}
    }
    function displayPoint(x,y) {
        return {x: width/2+(x-(bounds.left+bounds.right)/2)*ratio,
                y: height/2+(y-(bounds.top+bounds.bottom)/2)*ratio}
    }
    function point(row) {
        const projected = atlasPoint(row)
        return displayPoint(projected.x, projected.y)
    }
    function drawState(ctx, state, selected) {
        ctx.beginPath()
        for (const ring of state.rings) {
            for (let index = 0; index < ring.length; index++) {
                const p = displayPoint(ring[index][0], ring[index][1])
                if (index === 0) ctx.moveTo(p.x,p.y)
                else ctx.lineTo(p.x,p.y)
            }
            ctx.closePath()
        }
        ctx.fillStyle = selected ? "#37655b" : "#203b40"
        ctx.strokeStyle = selected ? "#bdd8bc" : "#4f6865"
        ctx.lineWidth = (selected ? 1.6 : 0.8) * markerScale
        ctx.fill()
        ctx.stroke()
    }
    onRowsChanged: {
        if (hoverId.length && !rows.some(row => row.id === hoverId)) hoverId = ""
        requestPaint()
    }
    onFeatureChanged: { hoverId = ""; requestPaint() }
    onStateIdChanged: requestPaint()
    onSelectedIdChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onTextSizeChanged: requestPaint()
    onVisibleChanged: { if (visible) requestPaint(); else hoverId = "" }
    onHoverIdChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.fillStyle = "#10282f"
        ctx.fillRect(0,0,width,height)
        if (feature) {
            for (const neighbor of nearby) drawState(ctx,neighbor,false)
            drawState(ctx,feature,true)
        }
        for (const row of rows) {
            const p = point(row)
            const selected = row.id === selectedId
            ctx.beginPath()
            ctx.arc(p.x,p.y,(selected ? 6 : 3.2)*markerScale,0,Math.PI*2)
            ctx.fillStyle = selected ? "#f4ce91" : row.economy > 5100 ? "#b9dca5" : row.economy < 4900 ? "#d39e87" : "#9dcbbf"
            ctx.fill()
            if (selected) {
                ctx.strokeStyle = "#142d31"
                ctx.lineWidth = markerScale
                ctx.stroke()
            }
            if (selected && !hoverId.length) {
                const rightSide = p.x > width/2
                ctx.font = textSize+"px sans-serif"
                ctx.textAlign = rightSide ? "right" : "left"
                ctx.strokeStyle = "#142b30"
                ctx.lineWidth = 3*markerScale
                ctx.fillStyle = "#f7ecd2"
                const x = p.x+(rightSide ? -10 : 10)*markerScale
                const y = Math.max(textSize+4,p.y-9*markerScale)
                ctx.strokeText(row.name,x,y)
                ctx.fillText(row.name,x,y)
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: map.hoverId.length ? Qt.PointingHandCursor : Qt.ArrowCursor
        function nearest(x,y) {
            let best = "", distance = 13*map.markerScale
            for (const row of map.rows) {
                const p = map.point(row)
                const d = Math.hypot(x-p.x,y-p.y)
                if (d<distance) { best = row.id; distance = d }
            }
            return best
        }
        onClicked: mouse => {
            map.hoverPosition = mapToItem(Overlay.overlay,mouse.x,mouse.y)
            map.hoverId = nearest(mouse.x,mouse.y)
            if (map.hoverId.length) map.countyPicked(map.hoverId)
        }
        onPositionChanged: mouse => {
            map.hoverPosition = mapToItem(Overlay.overlay,mouse.x,mouse.y)
            map.hoverId = nearest(mouse.x,mouse.y)
        }
        onExited: map.hoverId = ""
    }
    Text {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: map.textSize
        color: "#aacbbd"
        text: qsTr("N ↑")
        font.pixelSize: map.textSize
        visible: !!map.feature
    }
    MapHoverCard {
        objectName: "countyHoverCard"
        host: map
        visible: map.hoverId.length>0
        heading: map.hovered.name || ""
        detail: map.hovered.detail || ""
        pointer: map.hoverPosition
        textSize: map.textSize
    }
}
