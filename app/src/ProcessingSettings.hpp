#pragma once
#include <QVariantMap>
#include <cmath>
#include <optional>

namespace astro::app {
inline QVariantMap defaultProcessingSettings() {
    QVariantMap values{{"denoise", 0.0}, {"red", 1.0}, {"green", 1.0}, {"blue", 1.0}, {"gamma", 1.0}};
    for (int i = 1; i <= 6; ++i) values.insert(QString("level%1").arg(i), 0.0);
    return values;
}
inline std::optional<QVariantMap> checkedProcessingSettings(const QVariantMap& input) {
    auto result = defaultProcessingSettings();
    if (input.size() != result.size()) return std::nullopt;
    for (auto it = result.begin(); it != result.end(); ++it) {
        bool valid = false;
        const double value = input.value(it.key()).toDouble(&valid);
        const bool level = it.key().startsWith("level");
        const bool denoise = it.key() == "denoise";
        const double lower = level || denoise ? 0.0 : 0.5;
        const double upper = level ? (it.key() == "level1" ? 50.0 : it.key().mid(5).toInt() <= 3 ? 20.0 : 10.0) : (denoise ? 0.05 : 2.0);
        if (!valid || !std::isfinite(value) || value < lower || value > upper) return std::nullopt;
        it.value() = value;
    }
    return result;
}
}
