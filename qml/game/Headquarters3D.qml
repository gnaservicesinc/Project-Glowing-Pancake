import QtQuick
import QtQuick3D

View3D {
    id: scene
    objectName:"hqPhysicalScene"
    property var headquarters: ({})
    property bool editing: false
    property int selectedItem: 0
    property real zoom: 1
    property int dragItem: 0
    property int tileX: -1
    property int tileY: -1
    readonly property int columns: headquarters.width || 3
    readonly property int rows: headquarters.depth || 3
    signal tilePicked(int x, int y, int item)
    signal itemDragged(int item, int x, int y, int target)
    function coordinates(result) {
        const direction=camera.forward
        const t=-result.scenePosition.y/direction.y
        return { x: Math.floor((result.scenePosition.x + t*direction.x + columns * 50) / 100),
                 y: Math.floor((result.scenePosition.z + t*direction.z + rows * 50) / 100),
                 item: result.objectHit ? result.objectHit.equipmentId || 0 : 0 }
    }
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#17272b"
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }
    camera: camera
    OrthographicCamera {
        id: camera
        position: Qt.vector3d(1500, 1600, 1800)
        eulerRotation: Qt.vector3d(-34, 40, 0)
        horizontalMagnification: Math.max(0.01, Math.min(scene.width / ((scene.columns + scene.rows) * 85), scene.height / (Math.max(scene.columns,scene.rows) * 110)) * scene.zoom)
        verticalMagnification: horizontalMagnification
        clipNear: 1; clipFar: 10000
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-60,-20,0); brightness: 1.25; castsShadow: true; shadowFactor: 50; shadowMapQuality: Light.ShadowMapQualityHigh }
    DirectionalLight { eulerRotation: Qt.vector3d(-25,140,0); brightness: 0.6; color: "#b7d5da" }
    component Block: Model {
        property vector3d size: Qt.vector3d(100,100,100)
        property color tint: "#a9b5ae"
        property int equipmentId: 0
        source: "#Cube"
        scale: Qt.vector3d(size.x/100,size.y/100,size.z/100)
        pickable: true
        materials: PrincipledMaterial { baseColor: parent.tint; roughness: 0.78 }
    }
    Block { size: Qt.vector3d(scene.columns*100+16,18,scene.rows*100+16); y:-12; tint:scene.headquarters.kind==="home"?"#898b83":"#c0af94" }
    Block { size: Qt.vector3d(scene.columns*100,2,scene.rows*100); y:-2; tint:scene.headquarters.kind==="home"?"#a6a99f":"#d4c7af" }
    Block { position:Qt.vector3d(0,105,-scene.rows*50);size:Qt.vector3d(scene.columns*100,210,10);tint:scene.headquarters.kind==="home"?"#a8ac9f":"#91aaa4" }
    Block { position:Qt.vector3d(-scene.columns*50,105,0);size:Qt.vector3d(10,210,scene.rows*100);tint:scene.headquarters.kind==="home"?"#a0a798":"#8da49f" }
    Node {
        visible: scene.headquarters.kind==="home"
        Block {position:Qt.vector3d(0,100,-scene.rows*50+7);size:Qt.vector3d(Math.min(240,scene.columns*80),190,4);tint:"#b7bbb0"}
        Repeater3D {
            model: 6
            Block {required property int index;position:Qt.vector3d(0,20+index*30,-scene.rows*50+10);size:Qt.vector3d(Math.min(238,scene.columns*80),2,2);tint:"#7f877d"}
        }
        Block {position:Qt.vector3d(-scene.columns*50+8,150,0);size:Qt.vector3d(4,70,110);tint:"#5d8790"}
        Block {position:Qt.vector3d(-scene.columns*50+11,150,0);size:Qt.vector3d(2,72,3);tint:"#c6c6b3"}
    }
    Repeater3D {
        model: scene.editing ? scene.columns+1 : 0
        Block {required property int index;position:Qt.vector3d(index*100-scene.columns*50,1,0);size:Qt.vector3d(0.7,0.7,scene.rows*100);tint:"#738f86"}
    }
    Repeater3D {
        model: scene.editing ? scene.rows+1 : 0
        Block {required property int index;position:Qt.vector3d(0,1,index*100-scene.rows*50);size:Qt.vector3d(scene.columns*100,0.7,0.7);tint:"#738f86"}
    }
    Repeater3D {
        model: scene.headquarters.rooms || []
        Node {
            required property var modelData
            Block {visible:parent.modelData.x>0;position:Qt.vector3d(parent.modelData.x*100-scene.columns*50,22,(parent.modelData.y+parent.modelData.depth/2)*100-scene.rows*50);size:Qt.vector3d(5,44,Math.max(0,parent.modelData.depth*100-80));tint:"#b1b8a9"}
            Block {visible:parent.modelData.y>0;position:Qt.vector3d((parent.modelData.x+parent.modelData.width/2)*100-scene.columns*50,22,parent.modelData.y*100-scene.rows*50);size:Qt.vector3d(Math.max(0,parent.modelData.width*100-80),44,5);tint:"#b1b8a9"}
        }
    }
    Repeater3D {
        model: scene.headquarters.items || []
        Node {
            id: furnishing
            required property var modelData
            readonly property var parentDesk: modelData.mount ? (scene.headquarters.items || []).find(row=>row.id===modelData.mount) : null
            readonly property real spanX: parentDesk ? parentDesk.width : modelData.width
            readonly property real spanY: parentDesk ? parentDesk.depth : modelData.depth
            visible: modelData.placed && modelData.delivered
            position: Qt.vector3d(((parentDesk ? parentDesk.x : modelData.x)+spanX/2)*100-scene.columns*50,0,((parentDesk ? parentDesk.y : modelData.y)+spanY/2)*100-scene.rows*50)
            eulerRotation.y: (parentDesk ? parentDesk.rotated : modelData.rotated) ? 90 : 0
            Block {visible:scene.selectedItem===furnishing.modelData.id;y:2;size:Qt.vector3d(furnishing.spanX*100-6,2,furnishing.spanY*100-6);tint:"#a9d9bc";equipmentId:furnishing.modelData.id}
            Node {
                visible: furnishing.modelData.kind==="desk"
                Block {position:Qt.vector3d(0,74,-32);size:Qt.vector3d(145,7,70);tint:"#be9970";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(-60,36,-32);size:Qt.vector3d(6,72,58);tint:"#34494c";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(60,36,-32);size:Qt.vector3d(6,72,58);tint:"#34494c";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,43,50);size:Qt.vector3d(44,8,44);tint:"#537b78";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,65,70);size:Qt.vector3d(44,45,7);tint:"#628e87";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,21,50);size:Qt.vector3d(7,42,7);tint:"#34494c";equipmentId:furnishing.modelData.id}
            }
            Node {
                visible: furnishing.modelData.kind==="desktop"
                Block {position:Qt.vector3d(0,103,-49);size:Qt.vector3d(57,34,5);tint:"#263d41";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,103,-45);size:Qt.vector3d(50,28,1);tint:furnishing.modelData.powered?"#83b5ae":"#243b3f";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,83,-49);size:Qt.vector3d(5,14,6);tint:"#496064";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(0,79,-14);size:Qt.vector3d(45,2,14);tint:"#d0d0b8";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(58,96,-34);size:Qt.vector3d(17,37,38);tint:"#31484b";equipmentId:furnishing.modelData.id}
            }
            Node {
                visible: furnishing.modelData.kind==="rack"
                Block {y:86;size:Qt.vector3d(65,172,80);tint:"#2b4148";equipmentId:furnishing.modelData.id}
                Repeater3D {
                    model: 7
                    Block {required property int index;position:Qt.vector3d(0,22+index*21,41);size:Qt.vector3d(56,14,2);tint:furnishing.modelData.powered?"#638e89":"#3c5258";equipmentId:furnishing.modelData.id}
                }
            }
            Node {
                visible: furnishing.modelData.kind==="bench" || furnishing.modelData.kind==="evaluation"
                Block {y:78;size:Qt.vector3d(180,10,80);tint:furnishing.modelData.kind==="evaluation"?"#adbfc0":"#a7b89a";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(-75,37,0);size:Qt.vector3d(8,74,65);tint:"#3d565a";equipmentId:furnishing.modelData.id}
                Block {position:Qt.vector3d(75,37,0);size:Qt.vector3d(8,74,65);tint:"#3d565a";equipmentId:furnishing.modelData.id}
                Block {y:99;size:Qt.vector3d(48,34,34);tint:"#516d74";equipmentId:furnishing.modelData.id}
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: mouse=>{
            const hit=scene.pick(mouse.x,mouse.y)
            if(!hit.objectHit) return
            const tile=scene.coordinates(hit)
            scene.tileX=tile.x;scene.tileY=tile.y
            scene.dragItem=scene.editing?tile.item:0
        }
        onReleased: mouse=>{
            const hit=scene.pick(mouse.x,mouse.y)
            if(hit.objectHit) {
                const tile=scene.coordinates(hit)
                if(scene.dragItem && (tile.x!==scene.tileX || tile.y!==scene.tileY)) {
                    const moved=(scene.headquarters.items || []).find(item=>item.id===scene.dragItem)
                    scene.itemDragged(scene.dragItem,moved.x+tile.x-scene.tileX,moved.y+tile.y-scene.tileY,tile.item)
                }
                else scene.tilePicked(tile.x,tile.y,tile.item)
            }
            scene.dragItem=0
        }
        onWheel: wheel=>{scene.zoom=Math.max(0.5,Math.min(2,scene.zoom+(wheel.angleDelta.y>0?0.1:-0.1)));wheel.accepted=true}
    }
}
