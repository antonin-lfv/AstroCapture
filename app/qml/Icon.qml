import QtQuick

// Small monochrome vector icon set drawn from basic shapes/Canvas paths,
// matching the app's existing minimal geometric style (see the concentric
// circles in the empty-state icon and the record button's dot) rather than
// pulling in an icon font or image assets.
Item {
    id: root

    property string name: ""
    property color color: "#e8eaed"

    implicitWidth: 16
    implicitHeight: 16

    Canvas {
        anchors.fill: parent
        visible: root.name === "refresh"
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
            const ctx=getContext("2d");ctx.reset();ctx.fillStyle=root.color;
            const cx=width/2,cy=height/2,r=Math.min(width,height);
            ctx.beginPath();
            for(let i=0;i<32;i++) {
                const angle=(i-.5)*Math.PI/16, radius=r*(i%4<2?.45:.34);
                const x=cx+Math.cos(angle)*radius,y=cy+Math.sin(angle)*radius;
                if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
            }
            ctx.closePath();ctx.moveTo(cx+r*.15,cy);ctx.arc(cx,cy,r*.15,0,-Math.PI*2,true);ctx.fill();
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
