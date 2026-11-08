import QtQuick
import QtQuick3D

View3D {
    id: scene
    property string place: "workspace"
    property bool atHeadquarters: true
    property int peopleCount: 1
    property string officeKind:"shared"
    property var availableRooms:[]
    readonly property bool annex:place==="research_annex" || place==="evaluation_suite" || place==="collaboration"
    signal placePicked(string place)
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Color
        clearColor: "#17282c"
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }
    camera: camera
    OrthographicCamera {
        id: camera
        position: Qt.vector3d(760, 760, 900)
        eulerRotation: Qt.vector3d(-33, 40, 0)
        horizontalMagnification: Math.max(0.01, Math.min(scene.width / 1180, scene.height / 820))
        verticalMagnification: horizontalMagnification
        clipNear: 1
        clipFar: 5000
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-60,-25,0); brightness: 1.3; castsShadow: true; shadowFactor: 45; shadowMapQuality: Light.ShadowMapQualityHigh }
    DirectionalLight { eulerRotation: Qt.vector3d(-25,140,0); brightness: 0.7; color: "#b7dedb" }
    component Block: Model {
        property color tint: "#b6c0b5"
        property vector3d size: Qt.vector3d(100,100,100)
        property string destination: ""
        source: "#Cube"
        scale: Qt.vector3d(size.x/100,size.y/100,size.z/100)
        pickable: destination.length > 0
        materials: PrincipledMaterial { baseColor: parent.tint; roughness: 0.78 }
    }
    component Desk: Node {
        property string destination: "workspace"
        property color glow: "#70bcba"
        Block { position: Qt.vector3d(0,75,0); size: Qt.vector3d(138,7,66); tint: "#ba9670"; destination: parent.destination }
        Block { position: Qt.vector3d(-56,36,0); size: Qt.vector3d(6,72,54); tint: "#2c4448" }
        Block { position: Qt.vector3d(56,36,0); size: Qt.vector3d(6,72,54); tint: "#2c4448" }
        Block { position: Qt.vector3d(0,104,-12); size: Qt.vector3d(58,36,5); tint: "#24363c"; destination: parent.destination }
        Block { position: Qt.vector3d(0,104,-8); size: Qt.vector3d(51,29,1); tint: parent.glow; destination: parent.destination }
        Block { position: Qt.vector3d(0,83,-12); size: Qt.vector3d(4,15,6); tint: "#3a494b" }
        Block { position: Qt.vector3d(0,80,17); size: Qt.vector3d(38,2,14); tint: "#e3dfce" }
        Block { position: Qt.vector3d(0,41,72); size: Qt.vector3d(40,8,38); tint: "#4a7777" }
        Block { position: Qt.vector3d(0,65,90); size: Qt.vector3d(42,48,6); tint: "#4a7777" }
        Block { position: Qt.vector3d(0,20,72); size: Qt.vector3d(5,40,5); tint: "#253b42" }
    }
    component Plant: Node {
        Model { source: "#Cylinder"; y: 16; scale: Qt.vector3d(0.31,0.32,0.31); materials: PrincipledMaterial {baseColor:"#d2c7ae";roughness:0.9} }
        Model { source: "#Sphere"; y: 55; scale: Qt.vector3d(0.58,0.83,0.52); materials: PrincipledMaterial {baseColor:"#4f816c";roughness:0.9} }
    }
    Node {
        id: company
        visible: scene.atHeadquarters && !scene.annex
        Block { size: Qt.vector3d(900,24,610); position: Qt.vector3d(0,-18,0); tint: "#92785d" }
        Block { size: Qt.vector3d(880,5,590); y: -3; tint: "#dbccb0" }
        Block { size: Qt.vector3d(900,185,12); position: Qt.vector3d(0,80,-300); tint: "#a9bebb" }
        Block { size: Qt.vector3d(12,185,600); position: Qt.vector3d(-450,80,0); tint: "#9db0ae" }
        // Tall window panes and slender mullions leave the room open to light.
        Repeater3D {
            model: 4
            Block { required property int index; position: Qt.vector3d(-310+index*185,100,-290); size:Qt.vector3d(153,115,3);tint:"#4a7984" }
        }
        Block { position:Qt.vector3d(-220,0,55);size:Qt.vector3d(330,2,330);tint:scene.place==="workspace"?"#60928a":"#83a69b";destination:"workspace" }
        Block { position:Qt.vector3d(170,0,105);size:Qt.vector3d(340,2,240);tint:scene.place==="meeting"?"#c49d64":"#bcac8e";destination:"meeting" }
        Desk { position:Qt.vector3d(-250,0,10);destination:"workspace" }
        Desk { visible:scene.availableRooms.indexOf("lab")>=0;position:Qt.vector3d(-245,0,-195);destination:"lab";glow:"#b4d69d" }
        Desk { visible:scene.availableRooms.indexOf("lab")>=0 && scene.peopleCount>1;position:Qt.vector3d(-55,0,-195);destination:"lab";glow:"#b4d69d" }
        // Meeting table with chairs, shared by the small founding team.
        Node {
        visible:scene.availableRooms.indexOf("meeting")>=0
        Block { position:Qt.vector3d(165,70,100);size:Qt.vector3d(190,9,95);tint:"#eee0bf";destination:"meeting" }
        Block { position:Qt.vector3d(165,33,100);size:Qt.vector3d(14,66,14);tint:"#354d52" }
        Repeater3D {
            model:4
            Node {
                required property int index
                x:110+(index%2)*110
                z:index<2?18:185
                Block { y:42;size:Qt.vector3d(42,9,42);tint:"#a76f4f";destination:"meeting" }
                Block { y:65;z:parent.index<2?-18:18;size:Qt.vector3d(42,48,6);tint:"#a76f4f" }
                Block { y:20;size:Qt.vector3d(6,40,6);tint:"#354d52" }
            }
        }
        Block { position:Qt.vector3d(165,77,100);size:Qt.vector3d(34,3,24);tint:"#427474";destination:"meeting" }
        }
        Node {
            visible:scene.officeKind==="home"
            Block {position:Qt.vector3d(165,35,100);size:Qt.vector3d(210,45,90);tint:"#638276"}
            Block {position:Qt.vector3d(165,70,140);size:Qt.vector3d(210,65,18);tint:"#7c9c87"}
            Block {position:Qt.vector3d(165,25,-40);size:Qt.vector3d(140,8,65);tint:"#ba9670"}
            Block {position:Qt.vector3d(165,30,-40);size:Qt.vector3d(32,3,24);tint:"#dcb979"}
        }
        // Local control equipment represents access, not free compute capacity.
        Repeater3D {
            model:scene.availableRooms.indexOf("compute")>=0?2:0
            Node {
                required property int index
                x:275+index*65;z:-218
                Block { y:75;size:Qt.vector3d(52,150,65);tint:"#30474d";destination:"compute" }
                Repeater3D {
                    model:5
                    Block { required property int index;y:22+index*25;z:34;size:Qt.vector3d(42,15,2);tint:index%2?"#6a9290":"#547775";destination:"compute" }
                }
            }
        }
        Plant { position:Qt.vector3d(-390,0,220) }
        Plant { position:Qt.vector3d(400,0,-260) }
        Plant { position:Qt.vector3d(385,0,225) }
        // Stylized person; identity and assignments come from the campaign.
        Node {
            position:Qt.vector3d(-240,0,95)
            Model { source:"#Sphere";y:105;scale:Qt.vector3d(0.26,0.29,0.26);materials:PrincipledMaterial {baseColor:"#d2a37c";roughness:0.8} }
            Block { y:72;size:Qt.vector3d(34,45,25);tint:"#e4b966";destination:"workspace" }
        }
        Repeater3D {
            model:Math.min(4,Math.max(0,scene.peopleCount-1))
            Node {
                required property int index
                x:-255+(index%2)*205;z:-112+Math.floor(index/2)*260
                Model {source:"#Sphere";y:107;scale:Qt.vector3d(0.25,0.28,0.25);materials:PrincipledMaterial {baseColor:"#bd977a";roughness:0.8} }
                Block {y:73;size:Qt.vector3d(34,43,25);tint:"#638f91";destination:"lab"}
            }
        }
    }
    Node {
        visible:scene.atHeadquarters && scene.annex
        Block {size:Qt.vector3d(760,24,560);y:-18;tint:"#92785d"}
        Block {size:Qt.vector3d(745,5,545);y:-3;tint:"#d8d0b7"}
        Block {size:Qt.vector3d(760,185,12);position:Qt.vector3d(0,80,-280);tint:"#91aca6"}
        Block {size:Qt.vector3d(12,185,560);position:Qt.vector3d(-380,80,0);tint:"#91aca6"}
        Repeater3D {
            model:3
            Block {required property int index;position:Qt.vector3d(-240+index*235,100,-270);size:Qt.vector3d(180,115,3);tint:"#4a7984"}
        }
        Block {position:Qt.vector3d(0,0,0);size:Qt.vector3d(570,2,370);tint:scene.place==="evaluation_suite"?"#97adaf":scene.place==="collaboration"?"#c1ae8d":"#8eaea0"}
        Repeater3D {
            model:scene.place==="collaboration"?0:4
            Desk {required property int index;position:Qt.vector3d(-210+(index%2)*270,0,-160+Math.floor(index/2)*225);destination:scene.place;glow:scene.place==="evaluation_suite"?"#b2cddd":"#b4d69d"}
        }
        Node {
            visible:scene.place==="collaboration"
            Block {y:70;size:Qt.vector3d(360,10,130);tint:"#d5b98e";destination:"collaboration"}
            Repeater3D {
                model:4
                Node {
                    required property int index;x:-110+(index%2)*220;z:index<2?-120:120
                    Block {y:40;size:Qt.vector3d(48,8,48);tint:"#637f7d";destination:"collaboration"}
                    Block {y:65;z:parent.index<2?-22:22;size:Qt.vector3d(48,48,8);tint:"#637f7d"}
                }
            }
        }
        Plant {position:Qt.vector3d(310,0,-205)}
        Plant {position:Qt.vector3d(-310,0,210)}
        Model {source:"#Sphere";position:Qt.vector3d(-190,105,-65);scale:Qt.vector3d(0.26,0.29,0.26);materials:PrincipledMaterial {baseColor:"#d2aa82";roughness:0.8}}
        Block {position:Qt.vector3d(-190,72,-65);size:Qt.vector3d(34,43,25);tint:"#637f7d";destination:scene.place}
    }
    Node {
        visible:!scene.atHeadquarters
        Block {size:Qt.vector3d(700,20,480);y:-14;tint:"#b1b7ac"}
        Desk {position:Qt.vector3d(-60,0,30)}
        Block {position:Qt.vector3d(0,90,-240);size:Qt.vector3d(700,200,12);tint:"#779694"}
        Plant {position:Qt.vector3d(260,0,-120)}
    }
    MouseArea {
        anchors.fill:parent
        onClicked: mouse => {
            const result=scene.pick(mouse.x,mouse.y)
            if(result.objectHit && result.objectHit.destination) scene.placePicked(result.objectHit.destination)
        }
    }
}
