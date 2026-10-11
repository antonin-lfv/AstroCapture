#pragma once
#include <QVariantMap>
#include <QStringList>
#include <optional>
#include <cmath>

namespace astro::app {
// Missing groups are valid saved choices: they may be integrated later.
inline std::optional<QVariantMap> checkedCompositionSettings(const QVariantMap& input) {
    const auto palette=input.value("palette","RGB").toString();
    if(!QStringList{"RGB","LRGB","HOO","SHO","Custom"}.contains(palette))return {};
    bool ok=false;const auto amount=input.value("luminanceAmount",1.).toDouble(&ok);
    if(!ok||!std::isfinite(amount)||amount<0||amount>1)return {};
    QVariantMap result{{"palette",palette},{"luminanceAmount",amount}};
    for(const auto& key:{"red","green","blue","luminance"}) {
        const auto value=input.value(key,"");
        if(value.metaType().id()!=QMetaType::QString||value.toString().size()>128)return {};
        result[key]=value.toString();
    }
    return result;
}
}
