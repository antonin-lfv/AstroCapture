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
    function nativePoint(item,mouse){const p=item.mapToItem(view.imageContainer,mouse.x,mouse.y);return {x:Math.round(p.x/view.imageContainer.width*pixels.sourceWidth),y:Math.round(p.y/view.imageContainer.height*pixels.sourceHeight)};}
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
        property string explanation
        property int maximum:Atelier.maximumImageDimension
        Layout.fillWidth:true;spacing:3
        SettingLabel {text:parent.label;explanation:parent.explanation;color:Atelier.secondary;font.pixelSize:12}
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
            ImagePixelsItem {id:pixels;z:0;parent:view.imageContainer;anchors.fill:parent}
            Binding {target:pixels;property:"image";value:root.sourceImage;when:root.sourceImage!==null}
            Item {
                parent:view.imageContainer;anchors.fill:parent;z:2
                readonly property real sx:root.draft.x/pixels.sourceWidth*width
                readonly property real sy:root.draft.y/pixels.sourceHeight*height
                readonly property real sw:root.draft.width/pixels.sourceWidth*width
                readonly property real sh:root.draft.height/pixels.sourceHeight*height
                Rectangle {visible:root.draft.crop&&root.valid;x:0;y:0;width:parent.width;height:parent.sy;color:"#88000000"}
                Rectangle {visible:root.draft.crop&&root.valid;x:0;y:parent.sy;width:parent.sx;height:parent.sh;color:"#88000000"}
                Rectangle {visible:root.draft.crop&&root.valid;x:parent.sx+parent.sw;y:parent.sy;width:Math.max(0,parent.width-x);height:parent.sh;color:"#88000000"}
                Rectangle {visible:root.draft.crop&&root.valid;x:0;y:parent.sy+parent.sh;width:parent.width;height:Math.max(0,parent.height-y);color:"#88000000"}
                Rectangle {objectName:"cropSelection";visible:root.draft.crop;x:root.draft.x/pixels.sourceWidth*parent.width;y:root.draft.y/pixels.sourceHeight*parent.height;width:root.draft.width/pixels.sourceWidth*parent.width;height:root.draft.height/pixels.sourceHeight*parent.height;color:"transparent";border.color:Atelier.accent;border.width:2/view.effectiveZoom
                    MouseArea {anchors.fill:parent;enabled:!root.panMode;cursorShape:pressed?Qt.ClosedHandCursor:Qt.OpenHandCursor;preventStealing:true;property var initial;property var start
                        onPressed:(mouse)=>{initial=Object.assign({},root.draft);start=root.nativePoint(this,mouse);}
                        onPositionChanged:(mouse)=>{if(!pressed)return;const p=root.nativePoint(this,mouse);root.draft=Object.assign({},root.draft,{x:Math.max(0,Math.min(pixels.sourceWidth-initial.width,initial.x+p.x-start.x)),y:Math.max(0,Math.min(pixels.sourceHeight-initial.height,initial.y+p.y-start.y))});}
                    }
                    Repeater {model:9
                        Rectangle {required property int index;property int hx:index%3;property int hy:Math.floor(index/3);visible:index!==4;z:4;width:10/view.effectiveZoom;height:width;x:parent.width*hx/2-width/2;y:parent.height*hy/2-height/2;color:Atelier.accent;border.color:Atelier.canvas;border.width:1/view.effectiveZoom
                            MouseArea {anchors.fill:parent;enabled:!root.panMode;preventStealing:true;cursorShape:parent.hx===1?Qt.SizeVerCursor:parent.hy===1?Qt.SizeHorCursor:parent.hx===parent.hy?Qt.SizeFDiagCursor:Qt.SizeBDiagCursor;property var initial
                                onPressed:(mouse)=>initial=Object.assign({},root.draft)
                                onPositionChanged:(mouse)=>{if(!pressed)return;const p=root.nativePoint(this,mouse);let x=initial.x,y=initial.y,w=initial.width,h=initial.height;if(parent.hx===0){x=Math.max(0,Math.min(initial.x+initial.width-1,p.x));w=initial.x+initial.width-x;}else if(parent.hx===2)w=Math.max(1,Math.min(pixels.sourceWidth-x,p.x-x));if(parent.hy===0){y=Math.max(0,Math.min(initial.y+initial.height-1,p.y));h=initial.y+initial.height-y;}else if(parent.hy===2)h=Math.max(1,Math.min(pixels.sourceHeight-y,p.y-y));if(root.ratio>0){if(parent.hx===1)w=Math.max(1,Math.round(h*root.ratio));else h=Math.max(1,Math.round(w/root.ratio));w=Math.min(w,pixels.sourceWidth-x);h=Math.min(h,pixels.sourceHeight-y);if(parent.hx===0)x=initial.x+initial.width-w;if(parent.hy===0)y=initial.y+initial.height-h;}root.draft=Object.assign({},root.draft,{x:x,y:y,width:w,height:h});}
                            }
                        }
                    }
                }
                MouseArea {
                    objectName:"cropDrawSurface";anchors.fill:parent;enabled:!root.panMode;cursorShape:Qt.CrossCursor;z:-1;preventStealing:true
                    property int startX;property int startY
                    onPressed:(mouse)=>{startX=Math.max(0,Math.min(pixels.sourceWidth-1,Math.floor(mouse.x/width*pixels.sourceWidth)));startY=Math.max(0,Math.min(pixels.sourceHeight-1,Math.floor(mouse.y/height*pixels.sourceHeight)));}
                    onPositionChanged:(mouse)=>{if(!pressed)return;let endX=Math.max(0,Math.min(pixels.sourceWidth,Math.round(mouse.x/width*pixels.sourceWidth)));let endY=Math.max(0,Math.min(pixels.sourceHeight,Math.round(mouse.y/height*pixels.sourceHeight)));let w=Math.max(1,Math.abs(endX-startX)),h=Math.max(1,Math.abs(endY-startY));if(root.ratio>0){h=Math.max(1,Math.round(w/root.ratio));if(h>pixels.sourceHeight){h=pixels.sourceHeight;w=Math.round(h*root.ratio);}}let x=Math.max(0,Math.min(pixels.sourceWidth-w,endX<startX?startX-w:startX)),y=Math.max(0,Math.min(pixels.sourceHeight-h,endY<startY?startY-h:startY));root.draft=Object.assign({},root.draft,{crop:true,x:x,y:y,width:w,height:h});}
                }
            }
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            DarkComboBox {explanation: qsTr("Constrains the proportions while drawing or resizing the crop. Free allows any rectangle; Original keeps the source proportions. Numerical width and height can still be entered independently."); Layout.fillWidth:true;model:[qsTr("Free aspect ratio"),qsTr("Original aspect ratio"),"1:1","3:2","4:3","16:9"];currentIndex:root.aspect;onActivated:root.aspect=currentIndex}
            HelpCheckBox {explanation: qsTr("Drag to move the preview instead of drawing a crop. Pan and zoom only change the view, never the exported pixels."); text:qsTr("Pan image");checked:root.panMode;onClicked:root.panMode=checked}
            IconButton {text:qsTr("Full image");onClicked:root.fullImage()}
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            NumberField {explanation: qsTr("Horizontal position of the crop in source pixels: 0 is the left edge. Increasing X moves the rectangle right without changing its width."); label:qsTr("X (px)");key:"x";maximum:Math.max(0,pixels.sourceWidth-1)}
            NumberField {explanation: qsTr("Vertical position of the crop in source pixels: 0 is the top edge. Increasing Y moves the rectangle down without changing its height."); label:qsTr("Y (px)");key:"y";maximum:Math.max(0,pixels.sourceHeight-1)}
            NumberField {explanation: qsTr("Width of the region kept, in source pixels, before rotation and resizing. Pixels outside the rectangle are removed from the final image; the original stack is preserved."); label:qsTr("Width (px)");key:"width";maximum:pixels.sourceWidth}
            NumberField {explanation: qsTr("Height of the region kept, in source pixels, before rotation and resizing. Use it to remove unwanted borders; keep the rectangle inside the image."); label:qsTr("Height (px)");key:"height";maximum:pixels.sourceHeight}
        }
        RowLayout {Layout.fillWidth:true;spacing:12
            ParameterControl {explanation: qsTr("Positive angles rotate clockwise; negative angles rotate counterclockwise. 0 leaves orientation unchanged. Free rotation interpolates pixels and can add black corners."); label:qsTr("Angle (°)");Layout.fillWidth:true;from:-180;to:180;decimals:2;value:root.draft.angle;onMoved:{root.update("angle",value);value=Qt.binding(()=>root.draft.angle)}}
            ParameterControl {explanation: qsTr("100% keeps the pixel scale. 50% halves each dimension; 200% doubles each dimension and quadruples the pixel count. Enlarging does not recover missing detail."); label:qsTr("Scale (%)");Layout.fillWidth:true;from:10;to:400;decimals:1;value:root.draft.scale*100;onMoved:{root.update("scale",value/100);value=Qt.binding(()=>root.draft.scale*100)}}
            DarkComboBox {explanation: qsTr("Nearest neighbour keeps sample values but produces jagged edges. Bilinear is smoother; bicubic and Lanczos preserve sharper detail but can create rings. Area is useful for reducing an image; rotation uses linear interpolation with Area."); Layout.preferredWidth:180;model:[qsTr("Nearest neighbour"),qsTr("Bilinear"),qsTr("Bicubic"),qsTr("Area"),"Lanczos-4"];currentIndex:root.draft.interpolation;Accessible.name:qsTr("Interpolation");onActivated:root.update("interpolation",currentIndex)}
        }
        RowLayout {Layout.fillWidth:true
            HelpCheckBox {explanation: qsTr("Keeps the cropped width and height during rotation, before scaling. Rotated corners can be cut off. Disable to enlarge the canvas enough to contain the whole rotated image."); text:qsTr("Keep canvas size");checked:root.draft.keepSize;onClicked:root.update("keepSize",checked)}
            HelpCheckBox {explanation: qsTr("Limits interpolation to the minimum and maximum of each input channel, including black rotation margins. This prevents new extreme values, but does not remove every ringing artifact."); text:qsTr("Limit interpolation overshoot");checked:root.draft.clamp;onClicked:root.update("clamp",checked)}
        }
        RowLayout {Layout.fillWidth:true
            Label {text:root.valid?qsTr("Output: %1 × %2 px").arg(root.outputWidth).arg(root.outputHeight):qsTr("The crop rectangle exceeds the image boundaries.");color:root.valid?Atelier.secondary:Atelier.danger;Layout.fillWidth:true;wrapMode:Text.Wrap}
            IconButton {text:qsTr("Cancel");onClicked:root.close()}
            IconButton {objectName:"geometryApply";text:qsTr("Apply geometry");primary:true;enabled:root.valid&&root.outputWidth<=Atelier.maximumImageDimension&&root.outputHeight<=Atelier.maximumImageDimension&&root.outputWidth*root.outputHeight<=Atelier.maximumImagePixels;onClicked:{root.accepted(root.draft);root.close();}}
        }
    }
}
