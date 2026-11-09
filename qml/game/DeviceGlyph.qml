import QtQuick

Canvas {
    id: glyph
    property string app: ""
    property color ink: "#eff2da"
    onAppChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const scale = Math.min(width,height)/32
        ctx.translate((width-32*scale)/2,(height-32*scale)/2)
        ctx.scale(scale,scale)
        ctx.strokeStyle = ink
        ctx.lineWidth = 1.8
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        function line(points) {
            ctx.beginPath()
            ctx.moveTo(points[0],points[1])
            for (let i=2;i<points.length;i+=2) ctx.lineTo(points[i],points[i+1])
            ctx.stroke()
        }
        function circle(x,y,r) {
            ctx.beginPath()
            ctx.arc(x,y,r,0,Math.PI*2)
            ctx.stroke()
        }
        if (app === "lab" || app === "frontier") {
            line([8,8,16,16,25,8]);line([16,16,16,26])
            circle(8,8,3);circle(25,8,3);circle(16,26,3)
        } else if (app === "people" || app === "leadership") {
            circle(12,10,4);circle(23,12,3)
            line([4,25,6,20,12,18,18,20,20,25]);line([21,20,25,19,29,23])
        } else if (app === "office" || app === "civic" || app === "institutions") {
            line([5,13,16,6,27,13,5,13]);line([8,25,8,14]);line([16,25,16,14]);line([24,25,24,14]);line([5,26,27,26])
        } else if (app === "inbox") {
            ctx.strokeRect(5,8,22,16);line([6,9,16,17,26,9])
        } else if (app === "facilities") {
            line([5,15,16,6,27,15,27,25,5,25,5,15]);ctx.strokeRect(11,15,10,10)
        } else if (app === "operations" || app === "devices") {
            ctx.strokeRect(6,6,20,8);ctx.strokeRect(6,18,20,8);line([10,10,13,10]);line([10,22,13,22])
        } else if (app === "society") {
            circle(16,16,11)
            line([6,16,26,16]);line([16,5,12,11,11,16,12,21,16,27]);line([16,5,20,11,21,16,20,21,16,27])
        } else if (app === "objectives") {
            circle(16,16,11);circle(16,16,6);circle(16,16,1.5)
        } else {
            for (let y=7;y<26;y+=8) for (let x=7;x<26;x+=8) ctx.strokeRect(x,y,3,3)
        }
    }
}
