#pragma once
#include <QVariantMap>
#include <cmath>
#include <algorithm>
#include <optional>

namespace astro::app {
inline QVariantMap defaultDeepSkySettings() {
    return {{"pedestal",0.0},{"ghs",false},{"ghsStrength",0.0},{"ghsLocal",0.0},{"ghsSymmetry",0.0},{"ghsShadows",0.0},{"ghsHighlights",1.0},{"ghsInverse",false},{"ghsMode",0},{"maskType",0},{"maskLow",0.0},{"maskHigh",1.0},{"maskFeather",.05},{"maskX",.5},{"maskY",.5},{"maskRadius",.25},{"maskStarRadius",2.0},{"maskInvert",false},{"deconvolution",false},{"psfProfile",0},{"psfFwhm",3.0},{"psfBeta",3.0},{"rlIterations",10},{"rlDamping",.001},{"rlBlend",1.0},{"rlStopping",.0001},{"advancedDenoise",false},{"noiseScales",3},{"noiseThreshold",3.0},{"noiseAmount",.5},{"noiseChroma",.5},{"photometry",false},{"photometryReferences",QVariantList{}},{"photometryAperture",5.0},{"photometryInner",8.0},{"photometryOuter",12.0},{"photometrySaturation",1.0},{"background",false},{"stretch",true},{"black",0.0},{"white",1.0},
            {"midtones",.15},{"red",1.0},{"green",1.0},{"blue",1.0},
            {"operations",QVariantList{}},{"scnr",false},{"scnrMethod",0},{"scnrAmount",1.0},{"scnrPreserve",true},{"backgroundPoints",QVariantList{}},{"backgroundDegree",1},{"backgroundSamples",20},{"backgroundTolerance",1.0},
            {"rotation",0},{"mirrorHorizontal",false},{"mirrorVertical",false},{"denoise",0.0},{"asinh",false},{"asinhLuminance",0},{"asinhStrength",20.0},{"saturation",1.0}};
}
// Full parameter snapshots: reject missing/unknown fields and invalid input.
inline std::optional<QVariantMap> checkedDeepSkySettings(const QVariantMap& input) {
    auto result=defaultDeepSkySettings();
    const QStringList extensions{"pedestal","ghs","ghsStrength","ghsLocal","ghsSymmetry","ghsShadows","ghsHighlights","ghsInverse","ghsMode","maskType","maskLow","maskHigh","maskFeather","maskX","maskY","maskRadius","maskStarRadius","maskInvert","deconvolution","psfProfile","psfFwhm","psfBeta","rlIterations","rlDamping","rlBlend","rlStopping","advancedDenoise","noiseScales","noiseThreshold","noiseAmount","noiseChroma","photometry","photometryReferences","photometryAperture","photometryInner","photometryOuter","photometrySaturation","asinhLuminance","operations","scnr","scnrMethod","scnrAmount","scnrPreserve","backgroundPoints","backgroundDegree","backgroundSamples","backgroundTolerance","rotation","mirrorHorizontal","mirrorVertical"};
    for(auto it=input.begin();it!=input.end();++it)if(!result.contains(it.key()))return std::nullopt;
    for(auto it=result.begin();it!=result.end();++it) {
        if(!input.contains(it.key())){if(extensions.contains(it.key()))continue;return std::nullopt;}
        if(it.key()=="operations") {
            if(input[it.key()].typeId()!=QMetaType::QVariantList)return std::nullopt;
            const auto operations=input[it.key()].toList();if(operations.size()>100)return std::nullopt;QVariantList normalized;
            for(const auto& value:operations){if(value.typeId()!=QMetaType::QVariantMap)return std::nullopt;auto op=value.toMap();if(op.size()!=3||!op.contains("name")||!op.contains("enabled")||!op.contains("settings")||op["name"].typeId()!=QMetaType::QString||op["name"].toString().size()>100||op["enabled"].typeId()!=QMetaType::Bool||op["settings"].typeId()!=QMetaType::QVariantMap)return std::nullopt;const auto parameters=op["settings"].toMap();if(parameters.contains("operations"))return std::nullopt;auto checked=checkedDeepSkySettings(parameters);if(!checked)return std::nullopt;checked->remove("operations");op["settings"]=*checked;normalized<<op;}
            it.value()=normalized;continue;
        }
        if(it.key()=="photometryReferences") {
            if(input[it.key()].typeId()!=QMetaType::QVariantList)return std::nullopt;
            const auto references=input[it.key()].toList();if(references.size()>500)return std::nullopt;
            for(const auto& item:references){if(item.typeId()!=QMetaType::QVariantList)return std::nullopt;const auto row=item.toList();if(row.size()!=5)return std::nullopt;for(int i=0;i<5;++i){bool ok=false;const double v=row[i].toDouble(&ok);if(!ok||!std::isfinite(v)||v<0||(i>=2&&v<=0)||v>1e12)return std::nullopt;}}
            it.value()=references;continue;
        }
        if(it.key()=="backgroundPoints") {
            if(input[it.key()].typeId()!=QMetaType::QVariantList)return std::nullopt;
            const auto points=input[it.key()].toList();if(points.size()>2500)return std::nullopt;
            for(const auto& item:points){if(item.typeId()!=QMetaType::QVariantList)return std::nullopt;const auto point=item.toList();if(point.size()!=2)return std::nullopt;for(const auto& coord:point){bool ok=false;double v=coord.toDouble(&ok);if(!ok||!std::isfinite(v)||v<0||v>1)return std::nullopt;}}
            it.value()=points;continue;
        }
        if(it.value().typeId()==QMetaType::Bool) {
            if(input[it.key()].typeId()!=QMetaType::Bool)return std::nullopt;
            it.value()=input[it.key()];continue;
        }
        bool ok=false;const double v=input[it.key()].toDouble(&ok);
        if(!ok||!std::isfinite(v)||std::abs(v)>1e30)return std::nullopt;
        const auto k=it.key();
        if((k=="backgroundDegree"&&(v<1||v>4||v!=std::floor(v)))||(k=="backgroundSamples"&&(v<2||v>50||v!=std::floor(v)))||(k=="backgroundTolerance"&&(v<0||v>10))||(k=="rotation"&&(v<0||v>3||v!=std::floor(v))))return std::nullopt;
        if((k=="scnrMethod"&&(v<0||v>3||v!=std::floor(v)))||(k=="scnrAmount"&&(v<0||v>1)))return std::nullopt;
        if(k=="pedestal"&&(v< -1||v>1))return std::nullopt;
        if((k=="ghsStrength"&&(v<0||v>20))||(k=="ghsLocal"&&(v< -5||v>15)))return std::nullopt;
        if((QStringList{"ghsSymmetry","ghsShadows","ghsHighlights","maskLow","maskHigh","maskX","maskY","rlBlend","noiseAmount","noiseChroma"}.contains(k))&&(v<0||v>1))return std::nullopt;
        if((k=="ghsMode"||k=="psfProfile")&&(v<0||v>1||v!=std::floor(v)))return std::nullopt;
        if(k=="maskType"&&(v<0||v>3||v!=std::floor(v)))return std::nullopt;
        if(k=="maskFeather"&&(v<.001||v>1))return std::nullopt;
        if(k=="maskRadius"&&(v<.001||v>2))return std::nullopt;
        if(k=="maskStarRadius"&&(v<.5||v>6))return std::nullopt;
        if(k=="psfFwhm"&&(v<.5||v>12))return std::nullopt;
        if(k=="psfBeta"&&(v<1.5||v>10))return std::nullopt;
        if(k=="rlIterations"&&(v<1||v>100||v!=std::floor(v)))return std::nullopt;
        if(k=="rlDamping"&&(v<0||v>1))return std::nullopt;
        if(k=="rlStopping"&&(v<0||v>.1))return std::nullopt;
        if(k=="noiseScales"&&(v<1||v>5||v!=std::floor(v)))return std::nullopt;
        if(k=="noiseThreshold"&&(v<0||v>8))return std::nullopt;
        if(k=="photometryAperture"&&(v<1||v>30))return std::nullopt;
        if((k=="photometryInner"||k=="photometryOuter")&&(v<1||v>80))return std::nullopt;
        if(k=="photometrySaturation"&&(v<=0||v>1e12))return std::nullopt;
        if(k=="midtones"&&(float(v)<.001f||float(v)>.999f))return std::nullopt;
        if((k=="red"||k=="green"||k=="blue")&&(v<.1||v>3))return std::nullopt;
        if(k=="denoise"&&(v<0||v>1))return std::nullopt;
        if(k=="saturation"&&(v<0||v>3))return std::nullopt;
        if(k=="asinhLuminance"&&(v<0||v>2||v!=std::floor(v)))return std::nullopt;
        if(k=="asinhStrength"&&(v<1||v>1000))return std::nullopt;
        it.value()=k=="midtones"?std::clamp(v,.001,.999):v;
    }
    if(!(result["white"].toFloat()>result["black"].toFloat()))return std::nullopt;
    if(result["ghsShadows"].toDouble()>result["ghsSymmetry"].toDouble()||result["ghsSymmetry"].toDouble()>result["ghsHighlights"].toDouble()||result["maskLow"].toDouble()>=result["maskHigh"].toDouble())return std::nullopt;
    if(result["photometryInner"].toDouble()<=result["photometryAperture"].toDouble()||result["photometryOuter"].toDouble()<=result["photometryInner"].toDouble())return std::nullopt;
    // Scientific measurements require an un-stretched and unrotated reference image.
    bool nonlinear=false,oriented=false;
    for(const auto& value:result["operations"].toList()){const auto op=value.toMap();if(!op["enabled"].toBool())continue;const auto settings=op["settings"].toMap();if((nonlinear||oriented)&&(settings["photometry"].toBool()||settings["deconvolution"].toBool()))return std::nullopt;nonlinear|=settings["stretch"].toBool()||settings["scnr"].toBool()||settings["saturation"].toDouble()!=1;oriented|=settings["rotation"].toInt()!=0||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool();}
    if((nonlinear||oriented)&&(result["photometry"].toBool()||result["deconvolution"].toBool()))return std::nullopt;
    if(result["photometry"].toBool()&&result["maskType"].toInt()!=0)return std::nullopt;
    return result;
}
inline std::optional<QVariantMap> checkedDeepSkySnapshot(const QVariantMap& input) {
    if(input.size()!=3)return std::nullopt;
    const auto a=checkedDeepSkySettings(input.value("A").toMap());
    const auto b=checkedDeepSkySettings(input.value("B").toMap());
    const auto active=input.value("active").toString();
    if(!a||!b||(active!="A"&&active!="B"))return std::nullopt;
    return QVariantMap{{"A",*a},{"B",*b},{"active",active}};
}
}
