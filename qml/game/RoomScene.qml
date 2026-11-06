import QtQuick

// Decorative schematics. All tasks and status are available in the adjacent
// labeled controls and lists. The scenes have no animated or clickable parts.
Canvas {
    id: scene
    required property string room
    Accessible.ignored: true
    onRoomChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        const colors = ({
            welcome: ["#254154", "#e2bb85"],
            office: ["#303849", "#e1b982"],
            lab: ["#1e4246", "#93d2cc"],
            operations: ["#27394a", "#93bad5"],
            civic: ["#383747", "#e2bc90"],
            society: ["#253d43", "#9dcaaa"],
            institutions: ["#403443", "#ddb0bb"],
            frontier: ["#292e4c", "#b5b9ef"],
            inbox: ["#334150", "#b2c9d8"],
            leadership: ["#3b3946", "#e1bec7"],
            objectives: ["#273c3f", "#e2c789"]
        })[room]
        if (!colors)
            return
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        const backdrop = ctx.createLinearGradient(0, 0, width, height)
        backdrop.addColorStop(0, "#1a2836")
        backdrop.addColorStop(1, colors[0])
        ctx.fillStyle = backdrop
        ctx.fillRect(0, 0, width, height)
        ctx.save()
        ctx.scale(height / 70, height / 70)
        const unitsWide = width * 70 / height
        ctx.globalAlpha = 0.22
        ctx.strokeStyle = colors[1]
        ctx.lineWidth = 0.7
        for (let x = 12; x < unitsWide; x += 20) {
            ctx.beginPath()
            ctx.moveTo(x, 63)
            ctx.lineTo(x + 13, 50)
            ctx.stroke()
        }
        for (let y = 50; y <= 64; y += 7) {
            ctx.beginPath()
            ctx.moveTo(0, y)
            ctx.lineTo(unitsWide, y)
            ctx.stroke()
        }
        ctx.translate(Math.max(12, unitsWide * 0.52 - 100), 6)
        ctx.globalAlpha = 0.9
        ctx.lineWidth = 1.3
        ctx.lineCap = "round"
        function outline(x, y, w, h) {
            ctx.fillStyle = colors[0]
            ctx.fillRect(x, y, w, h)
            ctx.strokeRect(x, y, w, h)
        }
        function line(points) {
            ctx.beginPath()
            ctx.moveTo(points[0], points[1])
            for (let i = 2; i < points.length; i += 2)
                ctx.lineTo(points[i], points[i + 1])
            ctx.stroke()
        }
        function disc(x, y, radius) {
            ctx.beginPath()
            ctx.arc(x, y, radius, 0, Math.PI * 2)
            ctx.stroke()
        }
        if (room === "office") {
            outline(75, 4, 83, 36)
            line([102, 4, 102, 40, 130, 40, 130, 4])
            line([12, 51, 128, 51, 151, 42, 35, 42, 12, 51, 12, 59])
            line([128, 51, 128, 59])
            outline(59, 28, 36, 17)
        } else if (room === "lab") {
            outline(19, 8, 68, 36)
            line([25, 32, 35, 26, 46, 30, 57, 18, 68, 20, 79, 13])
            outline(110, 10, 55, 36)
            for (let x = 120; x <= 155; x += 12)
                for (let y = 17; y <= 40; y += 8) disc(x, y, 1.8)
            line([10, 54, 175, 54])
        } else if (room === "operations") {
            for (let x = 22; x <= 165; x += 43) {
                outline(x, 7, 30, 44)
                for (let y = 15; y <= 45; y += 7) {
                    line([x + 5, y, x + 23, y])
                    disc(x + 25, y, 0.8)
                }
            }
        } else if (room === "civic") {
            line([27, 17, 97, 3, 167, 17, 27, 17])
            outline(35, 18, 124, 29)
            for (let x = 45; x < 151; x += 17) outline(x, 18, 7, 29)
            line([29, 50, 165, 50, 175, 55, 19, 55, 29, 50])
        } else if (room === "society") {
            disc(92, 27, 24)
            line([68, 27, 116, 27])
            ctx.beginPath()
            ctx.ellipse(80, 3, 24, 48)
            ctx.stroke()
            line([21, 37, 68, 27, 133, 12, 171, 38, 116, 27])
            for (const p of [[21,37], [133,12], [171,38]]) disc(p[0], p[1], 4)
        } else if (room === "institutions") {
            ctx.beginPath()
            ctx.ellipse(41, 12, 112, 32)
            ctx.stroke()
            for (const p of [[38,15],[69,7],[122,7],[157,15],[45,45],[145,45]]) disc(p[0], p[1], 4)
            line([76, 29, 90, 24, 105, 28, 119, 23])
        } else if (room === "frontier") {
            line([22, 41, 58, 41, 83, 19, 118, 19, 157, 8])
            line([58, 41, 96, 50, 131, 45, 174, 45])
            line([83, 19, 109, 35, 153, 32])
            for (const p of [[22,41],[58,41],[83,19],[118,19],[157,8],[96,50],[131,45],[174,45],[109,35],[153,32]])
                disc(p[0], p[1], 3)
        } else if (room === "inbox") {
            outline(65, 5, 83, 40)
            outline(50, 11, 83, 40)
            line([58, 19, 126, 19, 92, 35, 58, 19])
            line([58, 44, 75, 37])
            line([109, 37, 126, 44])
        } else if (room === "leadership") {
            line([22, 42, 169, 42])
            for (let x = 37; x <= 159; x += 39) {
                disc(x, 19, 6)
                line([x - 8, 38, x - 8, 33, x, 28, x + 8, 33, x + 8, 38])
            }
            line([163, 37, 169, 42, 163, 47])
        } else if (room === "objectives") {
            outline(65, 4, 75, 50)
            for (let y = 14; y <= 44; y += 10) {
                line([75, y, 78, y + 3, 84, y - 3])
                line([91, y, 128, y])
            }
        } else {
            outline(40, 22, 105, 28)
            line([30, 22, 92, 7, 155, 22, 30, 22])
            outline(84, 31, 17, 19)
            outline(52, 30, 18, 12)
            outline(116, 30, 18, 12)
            line([26, 54, 158, 54])
        }
        ctx.restore()
    }
}
