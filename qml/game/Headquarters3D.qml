import QtQuick
import QtQuick3D

View3D {
    id: scene
    objectName:"hqPhysicalScene"
    property var headquarters: ({})
    property bool editing: false
    property int selectedItem: 0
    property int hoverItem: 0
    property bool placing:false
    property var preview:({})
    signal hovered(int x,int y,int target)
    signal cancelRequested()
    signal rotateRequested()
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
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }
    camera: camera
    OrthographicCamera {
        id: camera
        position: Qt.vector3d(1500, 1670, 1800)
        eulerRotation: Qt.vector3d(-34, 40, 0)
        horizontalMagnification: Math.max(0.01, Math.min((scene.width-48) / (scene.columns*77 + scene.rows*65), (scene.height-48) / (scene.columns*36 + scene.rows*43 + 190)) * scene.zoom)
        verticalMagnification: horizontalMagnification
        clipNear: 1; clipFar: 10000
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-60,-20,0); brightness: 1.5; color:"#ffe4c0"; castsShadow: true; shadowFactor: 65; shadowMapQuality: Light.ShadowMapQualityHigh }
    DirectionalLight { eulerRotation: Qt.vector3d(-25,140,0); brightness: 0.75; color: "#bbd9ed" }
    component Block: Model {
        property vector3d size: Qt.vector3d(100,100,100)
        property color tint: "#a9b5ae"
        property int equipmentId: 0
        property real glow: 0
        source: "#Cube"
        scale: Qt.vector3d(size.x/100,size.y/100,size.z/100)
        pickable: equipmentId>=0
        materials: PrincipledMaterial { baseColor: parent.tint; roughness: 0.72; emissiveFactor: Qt.vector3d(parent.glow,parent.glow,parent.glow) }
    }
    // Architectural surfaces do not represent purchasable furniture. Only the
    // floor is pickable; trim and glazing never intercept equipment selection.
    Block { size:Qt.vector3d(scene.columns*100+28,24,scene.rows*100+28);y:-18;tint:"#283f4a";equipmentId:-1 }
    Model {
        source:"#Cube"; y:-4; scale:Qt.vector3d(scene.columns,0.08,scene.rows)
        pickable:true
        property int equipmentId:0
        materials:PrincipledMaterial {
            roughness:0.85
            baseColorMap:Texture {
                source:"qrc:/pgp/assets/ui/hq-oak-floor.svg"
                scaleU:scene.columns/2;scaleV:scene.rows/2
                tilingModeHorizontal:Texture.Repeat;tilingModeVertical:Texture.Repeat
                generateMipmaps:true; mipFilter:Texture.Linear
            }
        }
    }
    Block {position:Qt.vector3d(0,105,-scene.rows*50);size:Qt.vector3d(scene.columns*100,210,10);tint:"#c5c6b8";equipmentId:-1}
    Block {position:Qt.vector3d(-scene.columns*50,105,0);size:Qt.vector3d(10,210,scene.rows*100);tint:"#839b99";equipmentId:-1}
    // Baseboards, a warm lower wall and continuous top rail frame the cutaway.
    Block {position:Qt.vector3d(0,28,-scene.rows*50+6);size:Qt.vector3d(scene.columns*100,56,3);tint:"#80938a";equipmentId:-1}
    Block {position:Qt.vector3d(0,58,-scene.rows*50+8);size:Qt.vector3d(scene.columns*100,4,5);tint:"#d6d5bd";equipmentId:-1}
    Block {position:Qt.vector3d(0,6,-scene.rows*50+8);size:Qt.vector3d(scene.columns*100,12,6);tint:"#e2d7bd";equipmentId:-1}
    Block {position:Qt.vector3d(-scene.columns*50+8,6,0);size:Qt.vector3d(6,12,scene.rows*100);tint:"#e2d7bd";equipmentId:-1}
    Block {position:Qt.vector3d(0,211,-scene.rows*50);size:Qt.vector3d(scene.columns*100+10,6,16);tint:"#e2d7bd";equipmentId:-1}
    Block {position:Qt.vector3d(-scene.columns*50,211,0);size:Qt.vector3d(16,6,scene.rows*100);tint:"#c4ccbb";equipmentId:-1}
    Repeater3D {
        model:Math.max(1,Math.floor(scene.columns/3))
        Node {
            required property int index
            x:(index+0.5)*scene.columns*100/Math.max(1,Math.floor(scene.columns/3))-scene.columns*50
            z:-scene.rows*50+10
            Block {y:139;size:Qt.vector3d(132,96,8);tint:"#ede1c5";equipmentId:-1}
            Block {position:Qt.vector3d(0,139,5);size:Qt.vector3d(119,83,3);tint:"#759cae";glow:0.12;equipmentId:-1}
            Block {position:Qt.vector3d(0,117,7);size:Qt.vector3d(119,35,1);tint:"#adc9ca";equipmentId:-1}
            Block {position:Qt.vector3d(0,139,9);size:Qt.vector3d(4,83,3);tint:"#e9dec5";equipmentId:-1}
            Block {position:Qt.vector3d(0,139,9);size:Qt.vector3d(119,4,3);tint:"#e9dec5";equipmentId:-1}
            Block {position:Qt.vector3d(0,89,10);size:Qt.vector3d(144,6,24);tint:"#f0e2c4";equipmentId:-1}
            // A quiet pool of daylight on the floor, with no continuous animation.
            Block {position:Qt.vector3d(30,1.2,100);size:Qt.vector3d(115,0.4,170);tint:"#f6dbaa";opacity:0.15;equipmentId:-1}
        }
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
            Block {visible:parent.modelData.x>0;position:Qt.vector3d(parent.modelData.x*100-scene.columns*50,22,(parent.modelData.y+parent.modelData.depth/2)*100-scene.rows*50);size:Qt.vector3d(5,44,Math.max(0,parent.modelData.depth*100-80));tint:"#8a9d92";equipmentId:-1}
            Block {visible:parent.modelData.y>0;position:Qt.vector3d((parent.modelData.x+parent.modelData.width/2)*100-scene.columns*50,22,parent.modelData.y*100-scene.rows*50);size:Qt.vector3d(Math.max(0,parent.modelData.width*100-80),44,5);tint:"#8a9d92";equipmentId:-1}
        }
    }
    component Furnishing: Node {
        id: furnishing
        required property var modelData
        readonly property var parentDesk: modelData.mount ? (scene.headquarters.items || []).find(row=>row.id===modelData.mount) : null
        readonly property real spanX: parentDesk ? parentDesk.width : modelData.width
        readonly property real spanY: parentDesk ? parentDesk.depth : modelData.depth
        opacity:modelData.id===-1?0.48:1
        visible: modelData.placed && modelData.delivered
        position: Qt.vector3d(((parentDesk ? parentDesk.x : modelData.x)+spanX/2)*100-scene.columns*50,0,((parentDesk ? parentDesk.y : modelData.y)+spanY/2)*100-scene.rows*50)
        eulerRotation.y: (parentDesk ? parentDesk.rotated : modelData.rotated) ? 90 : 0
        Block {visible:scene.selectedItem===furnishing.modelData.id || scene.hoverItem===furnishing.modelData.id;y:2;size:Qt.vector3d(furnishing.spanX*100-6,2,furnishing.spanY*100-6);tint:scene.selectedItem===furnishing.modelData.id?"#efc389":"#98c9cc";equipmentId:furnishing.modelData.id}
        Node {
            visible: furnishing.modelData.kind==="desk"
            Block {position:Qt.vector3d(-60,3,-32);size:Qt.vector3d(14,6,65);tint:"#243b46";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(60,3,-32);size:Qt.vector3d(14,6,65);tint:"#243b46";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(46,48,-32);size:Qt.vector3d(27,43,52);tint:"#a27d59";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(46,57,-5);size:Qt.vector3d(12,2,2);tint:"#dbd5bd";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(-48,79,-22);size:Qt.vector3d(22,3,29);tint:"#e5d8b9";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(-48,81,-22);size:Qt.vector3d(19,1,26);tint:"#466c7d";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,3,50);size:Qt.vector3d(53,5,7);tint:"#34494c";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,3,50);size:Qt.vector3d(7,5,53);tint:"#34494c";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,74,-32);size:Qt.vector3d(145,7,70);tint:"#be9970";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(-60,36,-32);size:Qt.vector3d(6,72,58);tint:"#34494c";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(60,36,-32);size:Qt.vector3d(6,72,58);tint:"#34494c";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,43,50);size:Qt.vector3d(44,8,44);tint:"#537b78";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,65,70);size:Qt.vector3d(44,45,7);tint:"#628e87";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,21,50);size:Qt.vector3d(7,42,7);tint:"#34494c";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible: ["desktop","workstation","thin_client","laptop","research_laptop"].includes(furnishing.modelData.kind)
            Block {position:Qt.vector3d(0,103,-49);size:Qt.vector3d(57,34,5);tint:"#263d41";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,103,-45);size:Qt.vector3d(50,28,1);tint:furnishing.modelData.powered?"#74aebc":"#243b3f";glow:furnishing.modelData.powered?0.16:0;equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,83,-49);size:Qt.vector3d(5,14,6);tint:"#496064";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(0,79,-14);size:Qt.vector3d(45,2,14);tint:"#d0d0b8";equipmentId:furnishing.modelData.id}
            Block {visible:furnishing.modelData.kind==="desktop";position:Qt.vector3d(58,96,-34);size:Qt.vector3d(17,37,38);tint:"#31484b";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible:furnishing.modelData.kind==="workstation"
            Block {position:Qt.vector3d(-56,103,-42);size:Qt.vector3d(49,34,5);tint:"#7ab4ad";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(56,103,-42);size:Qt.vector3d(49,34,5);tint:"#7ab4ad";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible:["laptop","research_laptop"].includes(furnishing.modelData.kind)
            Block {position:Qt.vector3d(0,80,-28);size:Qt.vector3d(59,3,38);tint:"#c3cec5";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible:furnishing.modelData.kind==="plant"
            Block {y:23;size:Qt.vector3d(42,46,42);tint:"#b37d61";equipmentId:furnishing.modelData.id}
            Block {y:70;size:Qt.vector3d(7,70,7);tint:"#728c60";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(-15,85,0);size:Qt.vector3d(42,32,48);tint:"#79a877";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(14,110,0);size:Qt.vector3d(43,39,45);tint:"#95be88";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible:furnishing.modelData.kind==="aquarium" || furnishing.modelData.kind==="table"
            Block {y:70;size:Qt.vector3d(furnishing.modelData.kind==="table"?230:150,8,70);tint:"#b89a74";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(-60,32,0);size:Qt.vector3d(7,64,60);tint:"#4d6866";equipmentId:furnishing.modelData.id}
            Block {position:Qt.vector3d(60,32,0);size:Qt.vector3d(7,64,60);tint:"#4d6866";equipmentId:furnishing.modelData.id}
            Block {visible:furnishing.modelData.kind==="aquarium";y:105;size:Qt.vector3d(140,60,60);tint:"#69b8bc";equipmentId:furnishing.modelData.id}
            Block {visible:furnishing.modelData.kind==="aquarium";position:Qt.vector3d(24,105,31);size:Qt.vector3d(20,10,3);tint:"#ebbd7a";equipmentId:furnishing.modelData.id}
        }
        Node {
            visible:["water","microwave","cabinet","vending"].includes(furnishing.modelData.kind)
            Block {y:60;size:Qt.vector3d(65,120,65);tint:furnishing.modelData.kind==="cabinet"?"#bfad8b":"#a9c7be";equipmentId:furnishing.modelData.id}
            Block {visible:furnishing.modelData.kind!=="cabinet";position:Qt.vector3d(0,80,34);size:Qt.vector3d(48,48,3);tint:"#386b78";equipmentId:furnishing.modelData.id}
            Block {visible:furnishing.modelData.kind==="water";y:137;size:Qt.vector3d(37,34,37);tint:"#91c9d6";equipmentId:furnishing.modelData.id}
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
    Repeater3D {
        model: scene.headquarters.items || []
        Furnishing {}
    }
    // Moving a preview must never rebuild the real, pickable equipment.
    Furnishing {
        modelData: Object.assign({}, (scene.headquarters.items || []).find(row=>row.id===scene.selectedItem) || {},
                                 {id:-1,placed:scene.placing,delivered:true,x:scene.preview.x || 0,y:scene.preview.y || 0,
                                  width:scene.preview.width || 1,depth:scene.preview.depth || 1,mount:scene.preview.mount || 0,rotated:scene.preview.rotated || false})
    }
    Repeater3D {
        model:scene.placing && scene.preview.width?scene.preview.width*scene.preview.depth:0
        Block {
            required property int index
            equipmentId:-1
            position:Qt.vector3d((scene.preview.x+index%scene.preview.width+0.5)*100-scene.columns*50,3,(scene.preview.y+Math.floor(index/scene.preview.width)+0.5)*100-scene.rows*50)
            size:Qt.vector3d(96,3,96);opacity:0.65;tint:scene.preview.valid?"#76d3a1":"#e77d71"
        }
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons:Qt.LeftButton | Qt.RightButton
        cursorShape:scene.placing || scene.dragItem?Qt.ClosedHandCursor:scene.hoverItem?Qt.PointingHandCursor:Qt.ArrowCursor
        onPositionChanged:mouse=>{const hit=scene.pick(mouse.x,mouse.y);if(hit.objectHit){const tile=scene.coordinates(hit);scene.hovered(tile.x,tile.y,tile.item)}else scene.hovered(-100,-100,0)}
        onExited:scene.hovered(-100,-100,0)
        onPressed: mouse=>{
            if(mouse.button===Qt.RightButton){scene.cancelRequested();return}
            const hit=scene.pick(mouse.x,mouse.y)
            if(!hit.objectHit) return
            const tile=scene.coordinates(hit)
            scene.tileX=tile.x;scene.tileY=tile.y
            scene.dragItem=scene.editing?tile.item:0
        }
        onReleased: mouse=>{
            if(mouse.button!==Qt.LeftButton)return
            const hit=scene.pick(mouse.x,mouse.y)
            if(hit.objectHit) {
                const tile=scene.coordinates(hit)
                if(!scene.placing && scene.dragItem && (tile.x!==scene.tileX || tile.y!==scene.tileY)) {
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
