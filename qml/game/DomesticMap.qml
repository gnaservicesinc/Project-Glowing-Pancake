import QtQuick

Canvas {
    id: map
    required property var geometry
    required property string selectedId
    signal statePicked(string id)
    Accessible.name: qsTr("Domestic map. Use the adjacent state list for keyboard selection.")
    Accessible.role: Accessible.Graphic
    onGeometryChanged: requestPaint()
    onSelectedIdChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    readonly property real ratio: geometry.width > 0 ? Math.min(width / geometry.width, height / geometry.height) : 1
    readonly property real insetX: (width - (geometry.width || 0) * ratio) / 2
    readonly property real insetY: (height - (geometry.height || 0) * ratio) / 2

    function drawState(ctx, state, selected) {
        ctx.beginPath()
        for (const ring of state.rings) {
            ctx.moveTo(ring[0][0], ring[0][1])
            for (let i = 1; i < ring.length; ++i)
                ctx.lineTo(ring[i][0], ring[i][1])
            ctx.closePath()
        }
        ctx.fillStyle = selected ? "#d29a57" : "#405d75"
        ctx.strokeStyle = selected ? "#ffe0a3" : "#a2bbce"
        ctx.lineWidth = (selected ? 1.6 : 0.7) / ratio
        ctx.fill()
        ctx.stroke()
    }
    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (!geometry.features || ratio <= 0)
            return
        ctx.fillStyle = "#101d28"
        ctx.fillRect(0, 0, width, height)
        ctx.save()
        ctx.translate(insetX, insetY)
        ctx.scale(ratio, ratio)
        let selected = null
        let district = null
        for (const state of geometry.features) {
            if (state.id === selectedId)
                selected = state
            else
                drawState(ctx, state, false)
            if (state.callout)
                district = state
        }
        if (selected)
            drawState(ctx, selected, true)
        if (district) {
            ctx.beginPath()
            ctx.arc(district.label[0], district.label[1], 5 / ratio, 0, 2 * Math.PI)
            ctx.fillStyle = selectedId === district.id ? "#ffd897" : "#e0eef7"
            ctx.fill()
            ctx.lineWidth = 1.2 / ratio
            ctx.strokeStyle = "#14283c"
            ctx.stroke()
        }
        ctx.restore()
    }
    function containsPoint(ring, x, y) {
        let inside = false
        for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
            const a = ring[i]
            const b = ring[j]
            if ((a[1] > y) !== (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
                inside = !inside
        }
        return inside
    }
    function pick(px, py) {
        if (!geometry.features || ratio <= 0)
            return
        const x = (px - insetX) / ratio
        const y = (py - insetY) / ratio
        for (const state of geometry.features) {
            if (state.callout && Math.hypot(state.label[0] - x, state.label[1] - y) * ratio <= 8) {
                statePicked(state.id)
                return
            }
        }
        for (const state of geometry.features) {
            let inside = false
            for (const ring of state.rings)
                if (containsPoint(ring, x, y))
                    inside = !inside
            if (inside) {
                statePicked(state.id)
                return
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => map.pick(mouse.x, mouse.y)
    }
}
