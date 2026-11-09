import QtQuick

Item {
    id: plan
    property var headquarters: ({width:3,depth:3,rooms:[],items:[]})
    property int selectedItem: 0
    property bool editing: false
    readonly property int columns: headquarters.width || 3
    readonly property int rows: headquarters.depth || 3
    readonly property real cell: Math.max(1,Math.min((width-32)/columns,(height-32)/rows))
    readonly property real leftEdge: (width-columns*cell)/2
    readonly property real topEdge: (height-rows*cell)/2
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
    onWidthChanged: drawing.requestPaint()
    onHeightChanged: drawing.requestPaint()
    Canvas {
        id: drawing
        anchors.fill: parent
        onPaint: {
            const c=getContext("2d");c.reset();c.fillStyle="#17292d";c.fillRect(0,0,width,height)
            const u=plan.cell,x0=plan.leftEdge,y0=plan.topEdge,w=plan.columns*u,h=plan.rows*u
            c.fillStyle=plan.headquarters.kind==="home"?"#69766c":"#a2a58f";c.fillRect(x0,y0,w,h)
            c.strokeStyle="#748f83";c.lineWidth=0.6
            for(let x=0;x<=plan.columns;++x){c.beginPath();c.moveTo(x0+x*u,y0);c.lineTo(x0+x*u,y0+h);c.stroke()}
            for(let y=0;y<=plan.rows;++y){c.beginPath();c.moveTo(x0,y0+y*u);c.lineTo(x0+w,y0+y*u);c.stroke()}
            c.strokeStyle="#d9d7bb";c.lineWidth=3
            for(const room of plan.headquarters.rooms || []) c.strokeRect(x0+room.x*u,y0+room.y*u,room.width*u,room.depth*u)
            c.font=Math.max(9,Math.min(14,u*0.22))+"px sans-serif";c.textAlign="center";c.textBaseline="middle"
            for(const item of plan.headquarters.items || []) {
                if(!item.placed || item.mount) continue
                const ix=x0+item.x*u,iy=y0+item.y*u,iw=item.width*u,ih=item.depth*u
                c.fillStyle=item.kind==="desk"?"#b79670":item.kind==="rack"?"#314f58":item.kind==="evaluation"?"#a4bdc0":"#8baf94"
                c.fillRect(ix+3,iy+3,iw-6,ih-6);c.strokeStyle=item.id===plan.selectedItem?"#f0d59b":"#d7d2b8";c.lineWidth=item.id===plan.selectedItem?3:1;c.strokeRect(ix+3,iy+3,iw-6,ih-6)
                c.fillStyle="#f7efdb";c.fillText(item.kind==="desk"?qsTr("Desk"):item.kind==="rack"?qsTr("Rack"):qsTr("Equipment"),ix+iw/2,iy+ih*0.65)
                const computer=(plan.headquarters.items || []).find(row=>row.mount===item.id && row.placed)
                if(computer){c.fillStyle=computer.powered?"#99ccbd":"#29434b";c.fillRect(ix+iw*0.3,iy+ih*0.2,iw*0.4,ih*0.2)}
            }
            if(!(plan.headquarters.items || []).some(item=>item.placed)) {
                c.fillStyle="#f1ead5";c.font=Math.max(11,Math.min(18,u*0.3))+"px sans-serif";c.fillText(qsTr("Empty premises"),x0+w/2,y0+h/2)
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: plan.editing
        onPressed: mouse=>{plan.pressedX=Math.floor((mouse.x-plan.leftEdge)/plan.cell);plan.pressedY=Math.floor((mouse.y-plan.topEdge)/plan.cell);plan.pressedItem=plan.hit(plan.pressedX,plan.pressedY)}
        onReleased: mouse=>{
            const x=Math.floor((mouse.x-plan.leftEdge)/plan.cell),y=Math.floor((mouse.y-plan.topEdge)/plan.cell),target=plan.hit(x,y)
            if(plan.pressedItem && (x!==plan.pressedX || y!==plan.pressedY)) {
                const moved=(plan.headquarters.items || []).find(item=>item.id===plan.pressedItem)
                plan.itemDragged(plan.pressedItem,moved.x+x-plan.pressedX,moved.y+y-plan.pressedY,target)
            }
            else plan.tilePicked(x,y,target)
            plan.pressedItem=0
        }
    }
}
