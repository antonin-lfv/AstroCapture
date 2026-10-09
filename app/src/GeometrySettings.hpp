#pragma once
#include <QVariantMap>
#include <optional>
#include <cmath>
namespace astro::app {
inline QVariantMap defaultGeometry(){return {{"crop",false},{"x",0},{"y",0},{"width",1},{"height",1},{"angle",0.0},{"scale",1.0},{"interpolation",4},{"clamp",true},{"keepSize",false}};}
inline std::optional<QVariantMap> checkedGeometry(const QVariantMap& values){
    auto result=defaultGeometry();if(values.size()!=result.size())return {};
    for(auto i=result.begin();i!=result.end();++i){if(!values.contains(i.key()))return {};const auto v=values[i.key()];if(i.value().typeId()==QMetaType::Bool){if(v.typeId()!=QMetaType::Bool)return {};i.value()=v;continue;}bool ok=false;const double n=v.toDouble(&ok);if(!ok||!std::isfinite(n))return {};const auto k=i.key();if((k=="angle"&&std::abs(n)>180)||(k=="scale"&&(n<.1||n>4))||(k=="interpolation"&&(n<0||n>4||n!=std::floor(n)))||(QStringList{"x","y","width","height"}.contains(k)&&(n!=std::floor(n)||n<(k=="x"||k=="y"?0:1)||n>32000000)))return {};i.value()=n;}
    return result;
}
inline bool geometryActive(const QVariantMap& v){return v["crop"].toBool()||v["angle"].toDouble()!=0||v.value("scale",1).toDouble()!=1;}
}
