#include "FileNaming.hpp"

#include <QDateTime>

namespace astro::app {

QString sanitizeForFilename(const QString& value) {
    QString result = value;
    for (QChar& ch : result) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('-') && ch != QLatin1Char('_')) {
            ch = QLatin1Char('_');
        }
    }
    return result;
}

QString timestampedBaseName(const QString& cameraId, const QString& targetName) {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString trimmedTarget = targetName.trimmed();
    if (trimmedTarget.isEmpty()) {
        return timestamp + QLatin1Char('_') + sanitizeForFilename(cameraId);
    }
    return timestamp + QLatin1Char('_') + sanitizeForFilename(trimmedTarget) + QLatin1Char('_') +
           sanitizeForFilename(cameraId);
}

}  // namespace astro::app
