import QtQuick
import "EquipmentArt.js" as Art
import "FloorMaterials.js" as Floors

Item {
    id: plan
    property var headquarters: ({width:3,depth:3,rooms:[],items:[]})
    property int selectedItem: 0
    property int hoverItem: 0
    onHoverItemChanged:drawing.requestPaint()
    onEditingChanged:drawing.requestPaint()
    property int highlightedRoom: 0
    property bool editing: false
    property bool inspectionEnabled: false
    readonly property int columns: headquarters.width || 3
    readonly property int rows: headquarters.depth || 3
    readonly property real cell: Math.max(1,Math.min((width-32)/columns,(height-32)/rows))
    readonly property real leftEdge: (width-columns*cell)/2
    readonly property real topEdge: (height-rows*cell)/2
    property bool placing:false
    property var preview:({})
    signal hovered(int x,int y,int target)
    signal cancelRequested()
    signal rotateRequested()
    onPreviewChanged:drawing.requestPaint()
    onPlacingChanged:drawing.requestPaint()
    property int pressedItem: 0
    property int pressedX: -1
    property int pressedY: -1
    signal tilePicked(int x,int y,int item)
    signal itemDragged(int item,int x,int y,int target)
    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Headquarters floor plan, %1 by %2 metres").arg(columns).arg(rows)
    function hit(x,y) {
        let found=0
        for(const item of headquarters.items || [])
            if(item.placed && !item.mount && x>=item.x && y>=item.y && x<item.x+item.width && y<item.y+item.depth) found=item.id
        return found
    }
    onHeadquartersChanged: drawing.requestPaint()
    onSelectedItemChanged: drawing.requestPaint()
    onHighlightedRoomChanged: drawing.requestPaint()
    onWidthChanged: drawing.requestPaint()
    onHeightChanged: drawing.requestPaint()
    Canvas {
        id: drawing
        anchors.fill: parent
        Component.onCompleted: ["concrete","carpet","oak"].forEach(kind=>loadImage(Floors.source(kind)))
        onImageLoaded:requestPaint()
        function surface(c,kind,x,y,w,h,unit) {
            c.fillStyle=Floors.base(kind);c.fillRect(x,y,w,h)
            const image=Floors.source(kind),tile=unit*2
            if(!isImageLoaded(image))return
            c.save();c.beginPath();c.rect(x,y,w,h);c.clip()
            for(let yy=y;yy<y+h;yy+=tile)for(let xx=x;xx<x+w;xx+=tile)c.drawImage(image,xx,yy,tile,tile)
            c.restore()
        }
        onPaint: {
            const c=getContext("2d");c.reset();c.fillStyle="#1a2d3c";c.fillRect(0,0,width,height)
            const u=plan.cell,x0=plan.leftEdge,y0=plan.topEdge,w=plan.columns*u,h=plan.rows*u
            c.fillStyle="#0e202c";c.fillRect(x0-7,y0-7,w+14,h+20)
            surface(c,plan.headquarters.flooring || "concrete",x0,y0,w,h,u)
            for(const room of plan.headquarters.rooms || []) surface(c,room.flooring || plan.headquarters.flooring || "concrete",x0+room.x*u,y0+room.y*u,room.width*u,room.depth*u,u)
            if(plan.editing) {
                c.strokeStyle="#50696d";c.lineWidth=0.6
                for(let x=0;x<=plan.columns;++x){c.beginPath();c.moveTo(x0+x*u,y0);c.lineTo(x0+x*u,y0+h);c.stroke()}
                for(let y=0;y<=plan.rows;++y){c.beginPath();c.moveTo(x0,y0+y*u);c.lineTo(x0+w,y0+y*u);c.stroke()}
            }
            c.strokeStyle="#e1d9bf";c.lineWidth=5
            for(const room of plan.headquarters.rooms || []) {
                if (plan.highlightedRoom && room.id === plan.highlightedRoom) {
                    c.fillStyle = "rgba(239,195,137,0.15)"
                    c.fillRect(x0+room.x*u,y0+room.y*u,room.width*u,room.depth*u)
                    c.strokeStyle = "#ffd897"
                } else c.strokeStyle = "#d9d7bb"
                c.strokeRect(x0+room.x*u,y0+room.y*u,room.width*u,room.depth*u)
                if(!plan.editing && room.label && room.width*u>100) {
                    c.font=Math.max(10,Math.min(14,u*0.24))+"px sans-serif";c.textAlign="left";c.textBaseline="top"
                    let title=room.label
                    while(title.length>1 && c.measureText(title).width>room.width*u-28)title=title.slice(0,-1)
                    if(title!==room.label)title=title.slice(0,-1)+"…"
                    c.fillStyle="#ecf0e8";c.fillRect(x0+room.x*u+7,y0+room.y*u+7,c.measureText(title).width+10,22)
                    c.fillStyle="#233641";c.fillText(title,x0+room.x*u+12,y0+room.y*u+11)
                }
            }
            c.font=Math.max(9,Math.min(14,u*0.22))+"px sans-serif";c.textAlign="center";c.textBaseline="middle"
            for(const item of plan.headquarters.items || []) {
                if(!item.placed || item.mount) continue
                const ix=x0+item.x*u,iy=y0+item.y*u,iw=item.width*u,ih=item.depth*u
                c.fillStyle=item.kind==="desk"?"#b79670":item.kind==="rack"?"#314f58":item.kind==="evaluation"?"#a4bdc0":"#8baf94"
                c.fillRect(ix+3,iy+3,iw-6,ih-6);c.strokeStyle=item.id===plan.selectedItem?"#ffe2a9":item.id===plan.hoverItem?"#bce6ed":"#d7d2b8";c.lineWidth=item.id===plan.selectedItem || item.id===plan.hoverItem?3:1;c.strokeRect(ix+3,iy+3,iw-6,ih-6)
                Art.draw(c,item.kind,ix+4,iy+4,iw-8,ih-8,item.powered)
                const computer=(plan.headquarters.items || []).find(row=>row.mount===item.id && row.placed)
                if(computer)Art.draw(c,computer.kind,ix+iw*0.2,iy+ih*0.06,iw*0.6,ih*0.53,computer.powered)
            }
            if(plan.placing && plan.preview.width) {
                const p=plan.preview,item=(plan.headquarters.items || []).find(row=>row.id===plan.selectedItem)
                c.fillStyle=p.valid?"#7777d6a2":"#99e76f67";c.strokeStyle=p.valid?"#b9f5d3":"#ffc7b4";c.lineWidth=2
                for(let y=0;y<p.depth;y++)for(let x=0;x<p.width;x++) {
                    const gx=x0+(p.x+x)*u,gy=y0+(p.y+y)*u
                    c.fillRect(gx+1,gy+1,u-2,u-2);c.strokeRect(gx+1,gy+1,u-2,u-2)
                }
                if(item){c.globalAlpha=0.55;Art.draw(c,item.kind,x0+p.x*u,y0+p.y*u,p.width*u,p.depth*u,item.powered);c.globalAlpha=1}
            }
            if(!plan.highlightedRoom && !plan.placing && !(plan.headquarters.items || []).some(item=>item.placed)) {
                c.fillStyle="#283d48";c.font=Math.max(11,Math.min(18,u*0.3))+"px sans-serif";c.fillText(qsTr("Empty premises"),x0+w/2,y0+h/2)
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: plan.editing || plan.inspectionEnabled
        hoverEnabled:true
        acceptedButtons:Qt.LeftButton | Qt.RightButton
        cursorShape:plan.placing?Qt.ClosedHandCursor:plan.hoverItem?Qt.PointingHandCursor:Qt.ArrowCursor
        onPositionChanged:mouse=>{const x=Math.floor((mouse.x-plan.leftEdge)/plan.cell),y=Math.floor((mouse.y-plan.topEdge)/plan.cell);plan.hovered(x,y,plan.hit(x,y))}
        onExited:plan.hovered(-100,-100,0)
        onPressed: mouse=>{if(mouse.button===Qt.RightButton){plan.cancelRequested();return}plan.pressedX=Math.floor((mouse.x-plan.leftEdge)/plan.cell);plan.pressedY=Math.floor((mouse.y-plan.topEdge)/plan.cell);plan.pressedItem=plan.editing?plan.hit(plan.pressedX,plan.pressedY):0}
        onReleased: mouse=>{
            if(mouse.button!==Qt.LeftButton)return
            const x=Math.floor((mouse.x-plan.leftEdge)/plan.cell),y=Math.floor((mouse.y-plan.topEdge)/plan.cell),target=plan.hit(x,y)
            if(!plan.placing && plan.pressedItem && (x!==plan.pressedX || y!==plan.pressedY)) {
                const moved=(plan.headquarters.items || []).find(item=>item.id===plan.pressedItem)
                plan.itemDragged(plan.pressedItem,moved.x+x-plan.pressedX,moved.y+y-plan.pressedY,target)
            }
            else plan.tilePicked(x,y,target)
            plan.pressedItem=0
        }
    }
}
