.pragma library
// Original procedural object illustrations, shared by storage and the floor plan.
function draw(c,kind,x,y,w,h,powered) {
    c.save();c.translate(x,y);c.scale(w/100,h/100)
    function box(x,y,w,h,color){c.fillStyle=color;c.fillRect(x,y,w,h)}
    function oval(x,y,w,h,color){c.fillStyle=color;c.beginPath();c.ellipse(x,y,w,h);c.fill()}
    function screen(x,y,w,h){box(x,y,w,h,"#223d48");box(x+3,y+3,w-6,h-6,powered?"#91d7ce":"#52747e");box(x+w/2-2,y+h,4,9,"#d6d9c9")}
    if(kind==="home") {
        box(19,42,62,48,"#d8bf94");box(43,64,15,26,"#42656b");box(25,51,13,15,"#a8d7cc");box(63,51,12,15,"#a8d7cc")
        c.fillStyle="#a46f59";c.beginPath();c.moveTo(8,43);c.lineTo(50,8);c.lineTo(92,43);c.closePath();c.fill()
    } else if(kind==="sofa") {
        box(8,30,84,48,"#426c79");box(16,38,68,32,"#7ea7a9");box(39,38,2,31,"#426c79");box(63,38,2,31,"#426c79")
        box(8,42,10,40,"#a2c2bb");box(82,42,10,40,"#a2c2bb");box(18,82,5,9,"#bca77e");box(77,82,5,9,"#bca77e")
    } else if(kind==="painting") {
        box(8,10,84,78,"#b79a68");box(14,16,72,66,"#e4d4ae");box(20,22,60,54,"#8ca9b2");box(20,51,60,25,"#567d71")
        oval(56,28,13,13,"#ead59a");c.fillStyle="#b3c4ac";c.beginPath();c.moveTo(20,59);c.lineTo(43,35);c.lineTo(68,76);c.lineTo(20,76);c.closePath();c.fill()
    } else if(kind==="sculpture") {
        box(25,78,50,12,"#c4c0ad");box(31,68,38,10,"#9faea4")
        c.strokeStyle="#bd9762";c.lineWidth=9;c.beginPath();c.ellipse(30,10,40,56);c.stroke();c.lineWidth=5;c.beginPath();c.ellipse(19,26,62,25);c.stroke()
    } else if(kind==="desk" || kind==="table") {
        box(10,32,80,38,"#c89762");box(16,70,5,20,"#38555b");box(79,70,5,20,"#38555b")
        if(kind==="desk"){box(36,73,28,16,"#567f84");box(36,87,28,7,"#91afa5")}
        else {box(12,11,23,16,"#698b80");box(65,11,23,16,"#698b80");box(12,77,23,16,"#698b80");box(65,77,23,16,"#698b80")}
    } else if(kind==="laptop" || kind==="research_laptop") {
        screen(17,18,66,45);box(10,72,80,9,"#c8d4cb");box(28,75,44,2,"#58787e")
        if(kind==="research_laptop")box(35,33,30,10,"#e0bd85")
    } else if(kind==="desktop" || kind==="thin_client" || kind==="workstation") {
        if(kind==="workstation"){screen(1,29,30,32);screen(35,23,30,38);screen(69,29,30,32)}
        else {screen(12,22,62,43);if(kind==="desktop")box(79,31,17,46,"#769095")}
        box(20,79,51,6,"#c9d6cd")
        if(kind==="thin_client")box(79,64,16,10,"#c3a274")
    } else if(kind==="plant") {
        box(34,66,32,26,"#c99470");box(48,29,4,40,"#799b63")
        oval(18,18,33,28,"#80b580");oval(49,10,31,27,"#a1c98c");oval(50,37,35,23,"#6c9f75");oval(21,43,28,21,"#9ec58a")
    } else if(kind==="aquarium") {
        box(9,23,82,47,"#b6d5ce");box(14,28,72,36,"#4a969b");box(9,70,82,8,"#b39770");box(15,78,6,15,"#38555b");box(79,78,6,15,"#38555b")
        oval(29,39,19,9,"#f0b16e");oval(58,48,15,8,"#ebdc9a");box(20,46,3,18,"#83bd83")
    } else if(kind==="water") {
        box(31,32,38,57,"#d5dfd4");box(35,8,30,29,"#8fc6d8");box(37,48,26,20,"#416a73");box(42,48,4,5,"#e78c7e");box(55,48,4,5,"#8abbe2")
    } else if(kind==="microwave") {
        box(14,31,72,40,"#ccd3c4");box(19,36,47,29,"#365a62");box(72,39,7,7,"#e9c587");box(72,51,7,10,"#80978a");box(11,73,78,6,"#b79972")
    } else if(kind==="cabinet") {
        box(20,12,60,79,"#c3b18f");box(49,17,2,69,"#786e58");box(41,46,3,15,"#eae2c8");box(56,46,3,15,"#eae2c8")
    } else if(kind==="vending") {
        box(23,5,57,91,"#739a8f");box(28,12,33,61,"#284b55");box(66,24,9,16,"#c9d7aa");box(33,80,34,8,"#274750")
        for(let i=0;i<3;i++)for(let j=0;j<3;j++)box(32+j*9,18+i*17,6,10,["#dcb877","#a8cba6","#df9a84"][j])
    } else if(kind==="rack") {
        box(24,5,52,91,"#2b4652");for(let i=0;i<6;i++){box(30,12+i*13,40,9,"#60838b");box(62,14+i*13,4,4,powered?"#ade5a5":"#38505a")}
    } else {
        box(8,54,84,8,"#b9cbb6");box(16,62,5,28,"#567278");box(79,62,5,28,"#567278");box(33,23,36,31,"#678d98");box(39,29,24,18,"#a9d2ca")
    }
    c.restore()
}
