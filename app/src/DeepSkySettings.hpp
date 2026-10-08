#pragma once
#include <QVariantMap>
#include <cmath>
#include <algorithm>
#include <optional>

namespace astro::app {
inline QVariantMap defaultDeepSkySettings() {
    return {{"background",false},{"stretch",true},{"black",0.0},{"white",1.0},
            {"midtones",.15},{"red",1.0},{"green",1.0},{"blue",1.0},
            {"denoise",0.0},{"asinh",false},{"asinhStrength",20.0},{"saturation",1.0}};
}
// Full parameter snapshots: reject missing/unknown fields and invalid input.
inline std::optional<QVariantMap> checkedDeepSkySettings(const QVariantMap& input) {
    auto result=defaultDeepSkySettings();
    if(input.size()!=result.size())return std::nullopt;
    for(auto it=result.begin();it!=result.end();++it) {
        if(!input.contains(it.key()))return std::nullopt;
        if(it.value().typeId()==QMetaType::Bool) {
            if(input[it.key()].typeId()!=QMetaType::Bool)return std::nullopt;
            it.value()=input[it.key()];continue;
        }
        bool ok=false;const double v=input[it.key()].toDouble(&ok);
        if(!ok||!std::isfinite(v)||std::abs(v)>1e30)return std::nullopt;
        const auto k=it.key();
        if(k=="midtones"&&(float(v)<.001f||float(v)>.999f))return std::nullopt;
        if((k=="red"||k=="green"||k=="blue")&&(v<.1||v>3))return std::nullopt;
        if(k=="denoise"&&(v<0||v>1))return std::nullopt;
        if(k=="saturation"&&(v<0||v>3))return std::nullopt;
        if(k=="asinhStrength"&&(v<1||v>200))return std::nullopt;
        it.value()=k=="midtones"?std::clamp(v,.001,.999):v;
    }
    if(!(result["white"].toFloat()>result["black"].toFloat()))return std::nullopt;
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
