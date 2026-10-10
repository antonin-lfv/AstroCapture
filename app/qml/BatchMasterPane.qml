import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier
ColumnLayout {
    id:root
    required property var controller
    property var plan: ({groups:[],revision:""})
    property var selected: ({})
    property var corrections: ({})
    property alias prefix:prefixField.text
    property alias reviewed:metadataReviewed.checked
    readonly property var jobs:Array.from(plan.groups||[])
    readonly property var selectedJobs:jobs.filter(job=>selected[job.id])
    readonly property int selectedCount:selectedJobs.length
    readonly property bool needsReview:selectedJobs.some(job=>job.reviewRequired||(job.corrections||[]).some(c=>c.id===corrections[job.id]&&(c.reviewRequired||c.compatibility===1)))
    readonly property bool canCreate:selectedCount>0&&selectedCount<=64&&!controller.busy&&(!needsReview||reviewed)&&selectedJobs.every(job=>!job.blocked&&(job.role!=="Flat"||Boolean(corrections[job.id])))
    spacing:12
    function refresh() {
        const next=controller.calibrationBatchPlan()
        if(next.revision!==plan.revision) {
            const choices={},fixes={};let count=0
            for(const job of next.groups||[]) {choices[job.id]=!job.blocked&&count<64&&controller.calibrationBatchResults.length===0;if(choices[job.id])count++;fixes[job.id]=job.defaultCorrection||""}
            selected=choices;corrections=fixes;metadataReviewed.checked=false
        }
        plan=next
    }
    function choose(id,enabled) {const next=Object.assign({},selected);next[id]=enabled;selected=next}
    function correct(id,value) {const next=Object.assign({},corrections);next[id]=value;corrections=next;metadataReviewed.checked=false}
    function start() {if(canCreate)controller.createCalibrationMasters(selectedJobs.map(j=>j.id),corrections,prefix,reviewed,plan.revision)}
    function size(bytes) {return bytes>=1073741824?qsTr("%1 GiB").arg((bytes/1073741824).toFixed(2)):qsTr("%1 MiB").arg((bytes/1048576).toFixed(1))}
    function roleName(role) {return role==="DarkFlat"?qsTr("Dark flat"):role==="Bias"?qsTr("Bias"):role==="Flat"?qsTr("Flat"):qsTr("Dark")}
    component Note:Label {Layout.fillWidth:true;wrapMode:Text.Wrap;color:Atelier.secondary;font.pixelSize:12;textFormat:Text.PlainText}
    Note {text:qsTr("Review one master per configuration. Only selected raw calibration files are grouped; saved library masters may correct flats. Creating masters does not apply them to your image.")}
    RowLayout {
        Layout.fillWidth:true
        TextField {id:prefixField;objectName:"batchMasterPrefix";Layout.fillWidth:true;maximumLength:40;enabled:!root.controller.busy;placeholderText:qsTr("Optional name prefix · session or equipment");color:Atelier.text;placeholderTextColor:Atelier.muted;selectByMouse:true;Accessible.name:qsTr("Batch master name prefix");background:Rectangle {color:Atelier.field;radius:Atelier.controlRadius;border.color:prefixField.activeFocus?Atelier.accent:Atelier.border}}
        HelpIcon {text:qsTr("Names include the camera, role, exposure, filter for flats, temperature and gain when known. The prefix helps identify the observing session. Sources are never overwritten. Select up to 64 groups per batch.")}
    }
    Note {text:qsTr("Memory budget: %1 · temporary folder: %2").arg(root.size(root.plan.memoryBudget||0)).arg(root.plan.temporaryDirectory||"")}
    RowLayout {
        Layout.fillWidth:true
        IconButton {text:qsTr("Select available groups");enabled:!root.controller.busy;onClicked:{let count=0;const next={};for(const job of root.jobs){next[job.id]=!job.blocked&&count<64;if(next[job.id])count++}root.selected=next}}
        IconButton {text:qsTr("Clear selection");enabled:!root.controller.busy;onClicked:root.selected=({})}
        Item {Layout.fillWidth:true}
    }
    Note {visible:Boolean(root.plan.libraryError);color:Atelier.danger;text:root.plan.libraryError||""}
    Note {visible:root.jobs.length===0;text:qsTr("Select raw darks, biases, dark-flats or flats in Images, then reopen this plan.")}
    Note {visible:root.plan.ignoredMasters>0;text:qsTr("%1 selected saved masters are not averaged again.").arg(root.plan.ignoredMasters||0)}
    Repeater {
        model:root.jobs
        ColumnLayout {
            id:jobPanel
            required property var modelData
            property bool sourcesOpen:false
            Layout.fillWidth:true;spacing:6
            Rectangle {Layout.fillWidth:true;Layout.preferredHeight:1;color:Atelier.border}
            HelpCheckBox {objectName:"batchMasterGroup";Layout.fillWidth:true;enabled:!root.controller.busy&&!jobPanel.modelData.blocked;checked:Boolean(root.selected[jobPanel.modelData.id]);text:root.roleName(jobPanel.modelData.role)+" · "+(jobPanel.modelData.count===1?qsTr("1 source"):qsTr("%1 sources").arg(jobPanel.modelData.count))+" · "+(jobPanel.modelData.camera||qsTr("Unknown camera"));onToggled:root.choose(jobPanel.modelData.id,checked);explanation:qsTr("Grouping requires equal recorded camera, dimensions, channels, Bayer pattern, binning, gain, offset, temperature, exposure and normalization units. Flat filters are kept separate. Unknown values are not proof of identical equipment; review the files and observing log before confirming.")}
            Note {text:{const j=jobPanel.modelData;return qsTr("%1 × %2 · %3 s · gain %4 · offset %5 · %6 °C · bin %7 × %8").arg(j.width).arg(j.height).arg(j.exposure).arg(j.gain===undefined?"?":j.gain).arg(j.offset===undefined?"?":j.offset).arg(j.temperature===undefined?"?":j.temperature).arg(j.binX||"?").arg(j.binY||"?")+(j.role==="Flat"?" · "+j.group:"")}}
            Note {text:jobPanel.modelData.name}
            Note {text:qsTr("Estimated memory %1 · temporary data %2").arg(root.size(jobPanel.modelData.workingBytes)).arg(root.size(jobPanel.modelData.temporaryBytes))}
            Note {visible:jobPanel.modelData.reviewRequired;color:Atelier.warning;text:qsTr("Check missing metadata: %1").arg(jobPanel.modelData.missing||"")}
            Note {visible:jobPanel.modelData.blocked;color:Atelier.danger;text:jobPanel.modelData.error||""}
            DarkComboBox {
                objectName:"batchFlatCorrection";Layout.fillWidth:true;visible:jobPanel.modelData.role==="Flat";enabled:!root.controller.busy&&!jobPanel.modelData.blocked
                readonly property var options:Array.from(jobPanel.modelData.corrections||[])
                model:[qsTr("Choose the flat correction")].concat(options.map(c=>(c.savedMaster?qsTr("Library"):qsTr("Raw sources"))+" · "+c.label+(c.compatibility===1?" · "+qsTr("Check metadata"):"")))
                currentIndex:Math.max(0,options.findIndex(c=>c.id===root.corrections[jobPanel.modelData.id])+1)
                onActivated:root.correct(jobPanel.modelData.id,currentIndex>0?options[currentIndex-1].id:"")
                explanation:qsTr("Dark-flat must match flat exposure and sensor configuration. Bias is the fallback when no matching dark-flat is selected. A unique dark-flat is proposed first; multiple candidates require your choice. Raw correction groups are computed even if you do not save them as separate masters. Saved corrections are integrity-checked. Flat pedestal is subtracted once, then each source is normalized before robust averaging.")
            }
            IconButton {text:jobPanel.sourcesOpen?qsTr("Hide source names"):qsTr("Show source names");ghost:true;enabled:!root.controller.busy;onClicked:jobPanel.sourcesOpen=!jobPanel.sourcesOpen}
            Note {visible:jobPanel.sourcesOpen;text:Array.from(jobPanel.modelData.sources||[]).map(s=>s.name||s.path).join("\n")}
        }
    }
    HelpCheckBox {id:metadataReviewed;objectName:"batchMasterMetadataReviewed";Layout.fillWidth:true;visible:root.needsReview;enabled:!root.controller.busy;text:qsTr("I checked all missing metadata in this batch");explanation:qsTr("Confirm only after checking equipment and exposure in the files and observing log, including flat corrections. Known mismatches remain blocked. Darks are never scaled to another exposure. Creating a batch does not enable Lights already calibrated or select masters for integration.")}
    Note {visible:root.selectedJobs.some(j=>j.role==="Flat"&&!root.corrections[j.id]);color:Atelier.warning;text:qsTr("Choose a correction for every selected flat group.")}
    Note {text:qsTr("Groups run sequentially with robust median/MAD rejection from three sources onward. One source gives no noise reduction. Memory estimates are conservative, not exact measurements. Temporary disk space is checked during calculation. On failure or cancellation, completed masters remain available and unfinished groups are reported.")}
    Label {visible:root.controller.calibrationBatchResults.length>0;text:qsTr("Batch results");color:Atelier.text;font.weight:Font.Medium}
    Repeater {
        model:root.controller.calibrationBatchResults
        Note {required property var modelData;color:modelData.state==="failed"?Atelier.danger:Atelier.secondary;text:(modelData.state==="saved"?qsTr("Saved"):modelData.state==="failed"?qsTr("Failed"):modelData.state==="cancelled"?qsTr("Cancelled"):qsTr("Not processed"))+" · "+modelData.name+(modelData.error?"\n"+modelData.error:"")}
    }
}
