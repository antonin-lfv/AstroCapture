import QtQuick
import AstroCapture
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier
Popup {
    id:root
    objectName:"geometryEditor"
    parent:Overlay.overlay;anchors.centerIn:parent
    width:Math.min(1100,parent?parent.width-32:800);height:Math.min(780,parent?parent.height-32:600)
    modal:true;focus:true;padding:20
    property var sourceImage:null
    property var draft:({crop:false,x:0,y:0,width:1,height:1,angle:0,scale:1,interpolation:4,clamp:true,keepSize:false})
    property bool panMode:false
    property int aspect:0
    readonly property real ratio:aspect===1?pixels.sourceWidth/pixels.sourceHeight:aspect===2?1:aspect===3?3/2:aspect===4?4/3:aspect===5?16/9:0
    signal accepted(var values)
    function edit(image,values){sourceImage=image;draft=Object.assign({},values);panMode=false;aspect=0;open();view.fit();}
    function update(key,value){const next=Object.assign({},draft);next[key]=value;draft=next;}
    function fullImage(){draft=Object.assign({},draft,{crop:false,x:0,y:0,width:pixels.sourceWidth,height:pixels.sourceHeight});}
    readonly property bool valid:!draft.crop||(draft.x>=0&&draft.y>=0&&draft.width>0&&draft.height>0&&draft.x+draft.width<=pixels.sourceWidth&&draft.y+draft.height<=pixels.sourceHeight)
    readonly property int outputWidth:Math.max(1,Math.round((draft.keepSize?baseWidth:Math.ceil(baseWidth*Math.abs(Math.cos(draft.angle*Math.PI/180))+baseHeight*Math.abs(Math.sin(draft.angle*Math.PI/180))-1e-10))*draft.scale))
    readonly property int outputHeight:Math.max(1,Math.round((draft.keepSize?baseHeight:Math.ceil(baseWidth*Math.abs(Math.sin(draft.angle*Math.PI/180))+baseHeight*Math.abs(Math.cos(draft.angle*Math.PI/180))-1e-10))*draft.scale))
    readonly property int baseWidth:draft.crop?draft.width:pixels.sourceWidth
    readonly property int baseHeight:draft.crop?draft.height:pixels.sourceHeight
    background:Rectangle {color:Atelier.surface;radius:Atelier.dialogRadius;border.color:Atelier.border}
    component NumberField:ColumnLayout {
        property string label
        property string key
        property int maximum:32000000
        Layout.fillWidth:true;spacing:3
        Label {text:parent.label;color:Atelier.secondary;font.pixelSize:12}
        DarkSpinBox {Layout.fillWidth:true;from:parent.key==="x"||parent.key==="y"?0:1;to:parent.maximum;value:root.draft[parent.key]||0;editable:true;Accessible.name:parent.label;onValueModified:{root.update(parent.key,value);root.update("crop",true);}}
    }
    contentItem:ColumnLayout {
        spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Crop, rotate and resize");font.pixelSize:20;color:Atelier.text;Layout.fillWidth:true}
            IconButton {text:qsTr("Close");ghost:true;onClicked:root.close()}
        }
        Label {text:qsTr("Drag a rectangle on the image. Coordinates start at the top left, in original pixels.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
        ImageViewport {
            id:view;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:180
            sourceWidth:pixels.sourceWidth;sourceHeight:pixels.sourceHeight
            ImagePixelsItem {id:pixels;parent:view.imageContainer;anchors.fill:parent}
            Binding {target:pixels;property:"image";value:root.sourceImage;when:root.sourceImage!==null}
            Item {
                parent:view.imageContainer;anchors.fill:parent
                Rectangle {visible:root.draft.crop;x:root.draft.x/pixels.sourceWidth*parent.width;y:root.draft.y/pixels.sourceHeight*parent.height;width:root.draft.width/pixels.sourceWidth*parent.width;height:root.draft.height/pixels.sourceHeight*parent.height;color:"transparent";border.color:Atelier.accent;border.width:2/view.effectiveZoom}
                MouseArea {
                    anchors.fill:parent;enabled:!root.panMode;cursorShape:Qt.CrossCursor;preventStealing:true
                    property int startX;property int startY
                    onPressed:(mouse)=>{startX=Math.max(0,Math.min(pixels.sourceWidth-1,Math.floor(mouse.x/width*pixels.sourceWidth)));startY=Math.max(0,Math.min(pixels.sourceHeight-1,Math.floor(mouse.y/height*pixels.sourceHeight)));}
                    onPositionChanged:(mouse)=>{if(!pressed)return;let endX=Math.max(0,Math.min(pixels.sourceWidth,Math.round(mouse.x/width*pixels.sourceWidth)));let endY=Math.max(0,Math.min(pixels.sourceHeight,Math.round(mouse.y/height*pixels.sourceHeight)));let w=Math.max(1,Math.abs(endX-startX)),h=Math.max(1,Math.abs(endY-startY));if(root.ratio>0){h=Math.max(1,Math.round(w/root.ratio));if(h>pixels.sourceHeight){h=pixels.sourceHeight;w=Math.round(h*root.ratio);}}let x=Math.max(0,Math.min(pixels.sourceWidth-w,endX<startX?startX-w:startX)),y=Math.max(0,Math.min(pixels.sourceHeight-h,endY<startY?startY-h:startY));root.draft=Object.assign({},root.draft,{crop:true,x:x,y:y,width:w,height:h});}
                }
            }
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            DarkComboBox {Layout.fillWidth:true;model:[qsTr("Free aspect ratio"),qsTr("Original aspect ratio"),"1:1","3:2","4:3","16:9"];currentIndex:root.aspect;onActivated:root.aspect=currentIndex}
            CheckBox {text:qsTr("Pan image");checked:root.panMode;onClicked:root.panMode=checked}
            IconButton {text:qsTr("Full image");onClicked:root.fullImage()}
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            NumberField {label:qsTr("X (px)");key:"x";maximum:Math.max(0,pixels.sourceWidth-1)}
            NumberField {label:qsTr("Y (px)");key:"y";maximum:Math.max(0,pixels.sourceHeight-1)}
            NumberField {label:qsTr("Width (px)");key:"width";maximum:pixels.sourceWidth}
            NumberField {label:qsTr("Height (px)");key:"height";maximum:pixels.sourceHeight}
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            ParameterControl {label:qsTr("Angle (°)");Layout.fillWidth:true;from:-180;to:180;decimals:2;value:root.draft.angle;onMoved:root.update("angle",value)}
            ParameterControl {label:qsTr("Scale (%)");Layout.fillWidth:true;from:10;to:400;decimals:1;value:root.draft.scale*100;onMoved:root.update("scale",value/100)}
            DarkComboBox {Layout.preferredWidth:150;model:[qsTr("Nearest neighbour"),qsTr("Bilinear"),qsTr("Bicubic"),qsTr("Area"),"Lanczos-4"];currentIndex:root.draft.interpolation;Accessible.name:qsTr("Interpolation");onActivated:root.update("interpolation",currentIndex)}
        }
        RowLayout {Layout.fillWidth:true
            CheckBox {text:qsTr("Keep canvas size");checked:root.draft.keepSize;onClicked:root.update("keepSize",checked)}
            CheckBox {text:qsTr("Limit interpolation overshoot");checked:root.draft.clamp;onClicked:root.update("clamp",checked)}
        }
        RowLayout {Layout.fillWidth:true
            Label {text:root.valid?qsTr("Output: %1 × %2 px").arg(root.outputWidth).arg(root.outputHeight):qsTr("The crop rectangle exceeds the image boundaries.");color:root.valid?Atelier.secondary:Atelier.danger;Layout.fillWidth:true;wrapMode:Text.Wrap}
            IconButton {text:qsTr("Cancel");onClicked:root.close()}
            IconButton {objectName:"geometryApply";text:qsTr("Apply geometry");primary:true;enabled:root.valid&&root.outputWidth*root.outputHeight<=32000000;onClicked:{root.accepted(root.draft);root.close();}}
        }
    }
}
