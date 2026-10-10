import "Atelier.js" as Atelier
import QtQuick

// Small monochrome vector icon set drawn from basic shapes/Canvas paths,
// matching the app's existing minimal geometric style (see the concentric
// circles in the empty-state icon and the record button's dot) rather than
// pulling in an icon font or image assets.
Item {
    id: root

    property string name: ""
    property color color: Atelier.text

    implicitWidth: 16
    implicitHeight: 16

    Canvas {
        id:studioIcon;anchors.fill:parent
        visible:["home","processing","stars","mosaic","history","book","export","loading","copy"].indexOf(root.name)>=0
        onWidthChanged:requestPaint();onHeightChanged:requestPaint()
        Connections {target:root;function onColorChanged(){studioIcon.requestPaint();}function onNameChanged(){studioIcon.requestPaint();}}
        onPaint: {
            const c=getContext("2d");c.reset();c.scale(width/24,height/24);
            c.strokeStyle=root.color;c.fillStyle=root.color;c.lineWidth=1.6;c.lineCap="round";c.lineJoin="round";
            c.beginPath();
            if(root.name==="export") {c.moveTo(3,3);c.lineTo(17,3);c.lineTo(21,7);c.lineTo(21,21);c.lineTo(3,21);c.closePath();c.rect(7,3,9,6);c.moveTo(13,5);c.lineTo(13,7);c.rect(7,13,10,8);}
            else if(root.name==="loading") {c.arc(12,12,8,-Math.PI/2,Math.PI);}
            else if(root.name==="copy") {c.rect(8,8,12,13);c.moveTo(5,16);c.lineTo(3,16);c.lineTo(3,3);c.lineTo(15,3);c.lineTo(15,5);}
            else if(root.name==="home") {c.moveTo(3,11);c.lineTo(12,3);c.lineTo(21,11);c.moveTo(6,10);c.lineTo(6,21);c.lineTo(10,21);c.lineTo(10,15);c.lineTo(14,15);c.lineTo(14,21);c.lineTo(18,21);c.lineTo(18,10);}
            else if(root.name==="processing") {c.arc(12,12,9,0,Math.PI*2);c.moveTo(17,12);c.arc(12,12,5,0,Math.PI*2);c.moveTo(3,12);c.lineTo(7,12);c.moveTo(17,12);c.lineTo(21,12);c.moveTo(12,3);c.lineTo(12,7);c.moveTo(12,17);c.lineTo(12,21);}
            else if(root.name==="stars") {c.moveTo(10,2);c.lineTo(12,8);c.lineTo(18,10);c.lineTo(12,12);c.lineTo(10,18);c.lineTo(8,12);c.lineTo(2,10);c.lineTo(8,8);c.closePath();c.moveTo(19,15);c.lineTo(19,22);c.moveTo(15.5,18.5);c.lineTo(22.5,18.5);}
            else if(root.name==="mosaic") {for(let y=4;y<20;y+=9)for(let x=4;x<20;x+=9)c.rect(x,y,6,6);}
            else if(root.name==="history") {c.arc(12,12,9,-Math.PI*.8,Math.PI*1.2);c.moveTo(12,6);c.lineTo(12,12);c.lineTo(16,14);c.moveTo(3,3);c.lineTo(3,8);c.lineTo(8,8);}
            else if(root.name==="book") {c.moveTo(12,6);c.quadraticCurveTo(7,2,3,4);c.lineTo(3,19);c.quadraticCurveTo(7,17,12,21);c.quadraticCurveTo(17,17,21,19);c.lineTo(21,4);c.quadraticCurveTo(17,2,12,6);c.lineTo(12,21);}
            c.stroke();
        }
        Component.onCompleted:requestPaint()
    }

    Canvas {
        id: refreshIcon
        anchors.fill: parent
        visible: root.name === "refresh"
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onColorChanged() { refreshIcon.requestPaint(); } }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = root.color;
            ctx.fillStyle = root.color;
            ctx.lineWidth = Math.max(1.3, width * 0.1);
            ctx.lineCap = "round";

            const cx = width / 2;
            const cy = height / 2;
            const r = width * 0.32;
            const endAngle = 2.05;

            ctx.beginPath();
            ctx.arc(cx, cy, r, -2.6, endAngle, false);
            ctx.stroke();

            const ex = cx + r * Math.cos(endAngle);
            const ey = cy + r * Math.sin(endAngle);
            ctx.beginPath();
            ctx.moveTo(ex - width * 0.17, ey - height * 0.03);
            ctx.lineTo(ex + width * 0.06, ey + height * 0.15);
            ctx.lineTo(ex + width * 0.2, ey - width * 0.05);
            ctx.closePath();
            ctx.fill();
        }
        onVisibleChanged: if (visible) requestPaint()
        Component.onCompleted: requestPaint()
    }

    Item {
        anchors.fill: parent
        visible: root.name === "camera"
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.84
            height: parent.height * 0.64
            radius: 2
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.36
            height: width
            radius: width / 2
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
        Rectangle {
            x: parent.width * 0.26
            y: parent.height * 0.1
            width: parent.width * 0.24
            height: parent.height * 0.12
            radius: 0.5
            color: root.color
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.name === "stop"
        width: parent.width * 0.5
        height: width
        radius: 1
        color: root.color
    }

    Canvas {
        id:gearCanvas;anchors.fill:parent;visible:root.name==="gear"
        onWidthChanged:requestPaint();onHeightChanged:requestPaint()
        Connections {target:root;function onColorChanged(){gearCanvas.requestPaint();}}
        onPaint: {
            const ctx=getContext("2d");ctx.reset();ctx.strokeStyle=root.color;ctx.lineWidth=Math.max(1.2,Math.min(width,height)/15);ctx.lineJoin="round";
            const cx=width/2,cy=height/2,r=Math.min(width,height);
            ctx.beginPath();
            for(let i=0;i<32;i++) {
                const angle=(i-.5)*Math.PI/16, radius=r*(i%4<2?.45:.34);
                const x=cx+Math.cos(angle)*radius,y=cy+Math.sin(angle)*radius;
                if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
            }
            ctx.closePath();ctx.stroke();ctx.beginPath();ctx.arc(cx,cy,r*.15,0,Math.PI*2);ctx.stroke();
        }
        Component.onCompleted:requestPaint()
    }

    Item {
        anchors.fill: parent
        visible: root.name === "settings"
        Repeater {
            model: 3
            delegate: Item {
                required property int index
                width: parent.width
                height: parent.height / 3
                y: index * height
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 1.3
                    color: root.color
                    opacity: 0.85
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: [2, 8, 5][index]
                    width: 3.2
                    height: 3.2
                    radius: 1.6
                    color: root.color
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.name === "folder"
        Rectangle {
            x: 0
            y: parent.height * 0.3
            width: parent.width
            height: parent.height * 0.56
            radius: 1.5
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
        Rectangle {
            x: 0
            y: parent.height * 0.18
            width: parent.width * 0.44
            height: parent.height * 0.16
            radius: 1
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
    }

    Item {
        anchors.fill: parent
        visible: root.name === "close"
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.7
            height: 1.5
            color: root.color
            rotation: 45
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.7
            height: 1.5
            color: root.color
            rotation: -45
        }
    }

    Item {
        anchors.fill: parent
        visible: root.name === "plus"
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.68
            height: 1.5
            color: root.color
        }
        Rectangle {
            anchors.centerIn: parent
            width: 1.5
            height: parent.height * 0.68
            color: root.color
        }
    }

    Item {
        anchors.fill: parent
        visible: root.name === "trash"
        Rectangle {
            x: parent.width * 0.22
            y: parent.height * 0.3
            width: parent.width * 0.56
            height: parent.height * 0.58
            radius: 1
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
        Rectangle {
            x: parent.width * 0.1
            y: parent.height * 0.22
            width: parent.width * 0.8
            height: 1.4
            color: root.color
        }
        Rectangle {
            x: parent.width * 0.38
            y: parent.height * 0.08
            width: parent.width * 0.24
            height: parent.height * 0.14
            radius: 1
            color: "transparent"
            border.color: root.color
            border.width: 1.3
        }
    }

    Canvas {
        anchors.fill: parent
        visible: root.name === "check"
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = root.color;
            ctx.lineWidth = Math.max(1.4, width * 0.13);
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            ctx.beginPath();
            ctx.moveTo(width * 0.2, height * 0.52);
            ctx.lineTo(width * 0.42, height * 0.74);
            ctx.lineTo(width * 0.82, height * 0.26);
            ctx.stroke();
        }
        onVisibleChanged: if (visible) requestPaint()
        Component.onCompleted: requestPaint()
    }
}
