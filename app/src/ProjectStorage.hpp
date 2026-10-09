#pragma once
#include "WorkspaceSnapshot.hpp"
#include <QSet>
#include <QStringList>
#include <functional>

namespace astro::app {
// Portable projects contain original files and the exact floating-point workspace.
// Payloads are streamed; neither saving nor opening loads the archive into RAM.
class ProjectStorage {
public:
    using Check = std::function<void()>;
    using Progress = std::function<void(int)>;
    static QByteArray magic();
    static QString defaultCacheDirectory();
    static QStringList sourcePaths(const WorkspaceSnapshot&);
    static QVariantMap fingerprint(const QString&, const Check& = {});
    static QVariant remap(const QVariant&, const QMap<QString, QString>&);
    static double estimatedBytes(const WorkspaceSnapshot&);
    static void save(const QString&, const WorkspaceSnapshot&, const Check& = {}, const Progress& = {});
    static WorkspaceSnapshot load(const QString&, const QString& cacheDirectory, const Check& = {}, const Progress& = {}, const std::function<bool(const WorkspaceSnapshot&)>& validate = {});
    static QSet<QString> strings(const QVariant&);
    static QVariantMap cacheUsage(const QString&, const QSet<QString>& protectedPaths);
    static qint64 clearUnusedCache(const QString&, const QSet<QString>& protectedPaths, const Check& = {});
};
}
