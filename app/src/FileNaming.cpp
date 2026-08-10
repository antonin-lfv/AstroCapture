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

QString timestampedBaseName(const QString& cameraId) {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return timestamp + QLatin1Char('_') + sanitizeForFilename(cameraId);
}

}  // namespace astro::app
