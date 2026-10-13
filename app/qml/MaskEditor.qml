import QtQuick
import AstroCapture
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier
Popup {
    id:root;objectName:"maskEditor"
    parent:Overlay.overlay;anchors.centerIn:parent
    width:Math.min(1100,parent?parent.width-32:800);height:Math.min(780,parent?parent.height-32:600)
    modal:true;focus:true;padding:20
    property var sourceImage:null
    property var rasterImage:null
    property var draft:({})
    property var redoStrokes:[]
    property bool panMode:false
    property bool maskOnly:false
    property int brushValue:0
    property real brushRadius:30
    property real hardness:.7
    property real brushOpacity:1
    property string limitError:""
    signal accepted(var values)
    function edit(image,settings,raster){sourceImage=image;rasterImage=raster;draft=Object.assign({},settings);redoStrokes=[];panMode=false;maskOnly=false;limitError="";open();view.fit();brushRadius=Math.max(1,Math.min(brushRadius,Math.min(pixels.sourceWidth,pixels.sourceHeight)/2));}
    function update(key,value){draft=Object.assign({},draft,{[key]:value});}
    function undo(){const strokes=draft.maskStrokes||[];if(!strokes.length)return;redoStrokes=redoStrokes.concat([strokes[strokes.length-1]]);update("maskStrokes",strokes.slice(0,-1));limitError="";}
    function redo(){if(!redoStrokes.length)return;update("maskStrokes",(draft.maskStrokes||[]).concat([redoStrokes[redoStrokes.length-1]]));redoStrokes=redoStrokes.slice(0,-1);}
    Shortcut {sequences:[StandardKey.Undo];enabled:root.opened;onActivated:root.undo()}
    Shortcut {sequences:[StandardKey.Redo];enabled:root.opened;onActivated:root.redo()}
    background:Rectangle {color:Atelier.surface;radius:Atelier.dialogRadius;border.color:Atelier.border}
    contentItem:ColumnLayout {
        spacing:12
        RowLayout {Layout.fillWidth:true
            Label {text:qsTr("Paint the mask");font.pixelSize:20;color:Atelier.text;Layout.fillWidth:true}
            IconButton {text:qsTr("Close");ghost:true;onClicked:root.close()}
        }
        Label {text:qsTr("The preview stretch reveals faint detail without changing pixels. Red protects the image. %1 removes the last stroke.").arg(Atelier.undoShortcutName(Qt.platform.os));color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
        ImageViewport {
            id:view;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:120
            sourceWidth:pixels.sourceWidth;sourceHeight:pixels.sourceHeight
            ImagePixelsItem {id:pixels;z:0;parent:view.imageContainer;anchors.fill:parent}
            Binding {target:pixels;property:"image";value:root.sourceImage;when:root.sourceImage!==null}
            MaskOverlayItem {id:overlay;z:1;parent:view.imageContainer;anchors.fill:parent;settings:root.draft;sourceWidth:pixels.sourceWidth;sourceHeight:pixels.sourceHeight;maskOnly:root.maskOnly}
            Binding {target:overlay;property:"raster";value:root.rasterImage;when:root.rasterImage!==null}
            MouseArea {
                objectName:"maskPaintSurface";z:2;parent:view.imageContainer;anchors.fill:parent;enabled:!root.panMode;cursorShape:Qt.CrossCursor;preventStealing:true;hoverEnabled:true
                property real cursorX:0
                property real cursorY:0
                Rectangle {visible:parent.containsMouse;x:parent.cursorX-width/2;y:parent.cursorY-height/2;width:root.brushRadius*2/pixels.sourceWidth*parent.width;height:width;color:"transparent";border.color:"#eeeeee";border.width:1/view.effectiveZoom;radius:width/2}
                function point(mouse){return [Math.max(0,Math.min(1,mouse.x/width)),Math.max(0,Math.min(1,mouse.y/height))];}
                onPressed:(mouse)=>{const strokes=root.draft.maskStrokes||[];if(strokes.length>=200){root.limitError=qsTr("The mask contains 200 strokes. %1 a stroke before painting more.").arg(Atelier.undoShortcutName(Qt.platform.os));return;}root.redoStrokes=[];root.update("maskStrokes",strokes.concat([{points:[point(mouse)],radius:root.brushRadius/Math.min(pixels.sourceWidth,pixels.sourceHeight),hardness:root.hardness,opacity:root.brushOpacity,value:root.brushValue}]));}
                onPositionChanged:(mouse)=>{
                    cursorX=mouse.x;cursorY=mouse.y;if(!pressed||root.limitError.length)return;const strokes=root.draft.maskStrokes||[];if(!strokes.length)return;const stroke=strokes[strokes.length-1];const next=point(mouse),last=stroke.points[stroke.points.length-1];if(Math.hypot((next[0]-last[0])*pixels.sourceWidth,(next[1]-last[1])*pixels.sourceHeight)<Math.max(1,root.brushRadius/5))return;
                    let count=0;for(const s of strokes)count+=s.points.length;if(count>=10000){root.limitError=qsTr("The mask reached its point limit. %1 a stroke before painting more.").arg(Atelier.undoShortcutName(Qt.platform.os));return;}root.update("maskStrokes",strokes.slice(0,-1).concat([Object.assign({},stroke,{points:stroke.points.concat([next])})]));
                }
            }
        }
        RowLayout {Layout.fillWidth:true
            DarkComboBox {explanation: qsTr("Black protects the painted region from the selected treatment; white lets it act. Gray applies only part of the effect. The red overlay shows protection."); Layout.fillWidth:true;model:[qsTr("Protect · black brush"),qsTr("Reveal · white brush")];currentIndex:root.brushValue;Accessible.name:qsTr("Brush action");onActivated:root.brushValue=currentIndex}
            HelpCheckBox {explanation: qsTr("Drag to move the preview without painting. Turn this off to resume brush strokes. Zoom and pan do not alter the mask."); text:qsTr("Pan image");checked:root.panMode;onClicked:root.panMode=checked}
            HelpCheckBox {explanation: qsTr("Shows mask weights instead of the photograph: white applies, black protects, gray blends. This display choice does not change the finished image or the exported mask effect."); text:qsTr("View mask only");checked:root.maskOnly;onClicked:root.maskOnly=checked}
        }
        RowLayout {Layout.fillWidth:true;spacing:16
            ParameterControl {explanation: qsTr("Radius in original image pixels, independent of zoom. A radius of 30 px paints a 60 px-wide stroke. Increase for large areas; decrease for precise edges."); label:qsTr("Brush radius (px)");Layout.fillWidth:true;from:1;to:Math.max(1,Math.min(1000,Math.min(pixels.sourceWidth,pixels.sourceHeight)/2));decimals:0;value:root.brushRadius;onMoved:root.brushRadius=value}
            ParameterControl {explanation: qsTr("100% gives a solid brush with a sharp edge. Lower values soften the transition near the edge to hide seams between protected and processed regions."); label:qsTr("Hardness (%)");Layout.fillWidth:true;from:0;to:100;decimals:0;value:root.hardness*100;onMoved:root.hardness=value/100}
            ParameterControl {explanation: qsTr("Strength of each stroke. 100% fully paints its center; 50% moves the mask halfway toward black or white. Separate overlapping strokes accumulate; 0% has no effect."); label:qsTr("Opacity (%)");Layout.fillWidth:true;from:0;to:100;decimals:0;value:root.brushOpacity*100;onMoved:root.brushOpacity=value/100}
        }
        RowLayout {Layout.fillWidth:true
            DarkComboBox {explanation: qsTr("White starts with the treatment active everywhere: paint black to protect areas. Black starts with everything protected: paint white to target only chosen regions. Existing strokes are kept."); Layout.fillWidth:true;visible:root.draft.maskType===4;model:[qsTr("White background · apply everywhere"),qsTr("Black background · protect everywhere")];currentIndex:root.draft.maskBaseWhite===false?1:0;onActivated:root.update("maskBaseWhite",currentIndex===0)}
            ParameterControl {explanation: qsTr("Gaussian smoothing of the complete mask, in original pixels. 0 keeps the painted edges. Increase to blend the treatment gradually into nearby areas; large values spread the selection."); label:qsTr("Edge blur σ (px)");Layout.fillWidth:true;from:0;to:100;decimals:1;value:root.draft.maskBlurRadius||0;onMoved:{root.update("maskBlurRadius",value);value=Qt.binding(()=>root.draft.maskBlurRadius||0)}}
            HelpCheckBox {explanation: qsTr("Swaps protected and affected areas: black becomes white and white becomes black. Gray is inverted too. Useful for protecting an object while treating its surroundings."); text:qsTr("Invert mask");checked:root.draft.maskInvert||false;onClicked:root.update("maskInvert",checked)}
        }
        Label {visible:root.limitError.length>0||overlay.error.length>0;text:root.limitError||overlay.error;color:Atelier.danger;wrapMode:Text.Wrap;Layout.fillWidth:true}
        RowLayout {Layout.fillWidth:true
            IconButton {text:qsTr("Clear strokes");enabled:(root.draft.maskStrokes||[]).length>0;onClicked:{root.update("maskStrokes",[]);root.redoStrokes=[];root.limitError="";}}
            Item {Layout.fillWidth:true}
            IconButton {text:qsTr("Cancel");onClicked:root.close()}
            IconButton {objectName:"maskApply";text:qsTr("Apply mask");primary:true;enabled:overlay.error.length===0;onClicked:{root.accepted(root.draft);root.close();}}
        }
    }
}
