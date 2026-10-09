#pragma once
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <QVariantMap>
namespace astro::app {
inline processing::MaskOptions maskOptions(const QVariantMap& s){
    processing::MaskOptions o{s["maskType"].toInt(),s["maskLow"].toDouble(),s["maskHigh"].toDouble(),s["maskFeather"].toDouble(),s["maskX"].toDouble(),s["maskY"].toDouble(),s["maskRadius"].toDouble(),s["maskStarRadius"].toDouble(),s["maskInvert"].toBool()};
    o.baseWhite=s.value("maskBaseWhite",true).toBool();o.blurRadius=s["maskBlurRadius"].toDouble();
    for(const auto& row:s["maskStrokes"].toList()){const auto s=row.toMap();processing::MaskStroke stroke;stroke.radius=s["radius"].toDouble();stroke.hardness=s["hardness"].toDouble();stroke.opacity=s["opacity"].toDouble();stroke.value=s["value"].toDouble();for(const auto& p:s["points"].toList()){const auto point=p.toList();stroke.points.push_back({point.value(0).toDouble(),point.value(1).toDouble()});}o.strokes.push_back(std::move(stroke));}return o;
}
}
