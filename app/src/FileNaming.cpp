#include "FileNaming.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

QString reserveCapturePath(const QString& directory, const QString& baseName, const QString& extension) {
    for (int suffix = 0; suffix < 10000; ++suffix) {
        const auto name = baseName + (suffix == 0 ? QString() : QStringLiteral("_%1").arg(suffix)) +
                          QLatin1Char('.') + extension;
        const auto path = QDir(directory).filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            file.close();
            return path;
        }
        if (!QFileInfo::exists(path)) return {};
    }
    return {};
}

}  // namespace astro::app
