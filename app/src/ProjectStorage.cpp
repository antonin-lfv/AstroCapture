#include "ProjectStorage.hpp"
#include "SessionStorage.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtEndian>
#include <set>

namespace astro::app {
namespace {
constexpr qint64 maxManifest = 64 * 1024 * 1024;
constexpr qint64 maxArchive = 128LL * 1024 * 1024 * 1024;
constexpr qint64 chunkSize = 1024 * 1024;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void checkpoint(const ProjectStorage::Check& check) { if (check) check(); }
QByteArray digest(const QString& path, const ProjectStorage::Check& check) {
    QFile input(path); require(input.open(QIODevice::ReadOnly), "A project source file is missing or unreadable.");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!input.atEnd()) { checkpoint(check); const auto block = input.read(chunkSize); require(!block.isEmpty(), "Cannot read a project source file."); hash.addData(block); }
    return hash.result().toHex();
}
bool validEntry(const QString& name) {
    static const QRegularExpression pattern("^(inputs/[0-9]{6}\\.[a-z0-9]{1,12}|state/session\\.json|state/[0-9a-f-]{36}\\.(pixels|plane|png))$");
    return pattern.match(name).hasMatch();
}
struct Entry { QString name, source; qint64 bytes; QByteArray hash; };
qint64 sizeValue(const QJsonValue& value) {
    bool ok = false; const auto size = value.toString().toLongLong(&ok);
    require(ok && size >= 0 && size <= maxArchive, "Invalid portable project file size."); return size;
}
QString token(const QString& entry) { return "astroproject:" + entry; }
void spaceFor(const QString& folder, qint64 bytes) {
    QStorageInfo disk(folder);
    require(!disk.isValid() || !disk.isReady() || disk.bytesAvailable() < 0 || disk.bytesAvailable() >= bytes,
            "Not enough free space for this portable project.");
}
QList<QString> cacheFolders(const QString& root) {
    QList<QString> folders;
    static const QRegularExpression uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    for (const auto& info : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        if (!uuid.match(info.fileName()).hasMatch()) continue;
        QFile marker(QDir(info.filePath()).filePath(".astrocache"));
        if (QFileInfo(marker).isSymLink() || !marker.open(QIODevice::ReadOnly) || marker.size() > 1024) continue;
        if (QJsonDocument::fromJson(marker.readAll()).object()["version"].toInt() == 1) folders << info.absoluteFilePath();
    }
    return folders;
}
bool inUse(const QString& folder, const QSet<QString>& paths) {
    const auto prefix = QDir::cleanPath(folder) + '/';
    const auto canonicalFolder = QFileInfo(folder).canonicalFilePath();
    const auto canonicalPrefix = canonicalFolder + '/';
    for (auto path : paths) {
        if (path.startsWith("file:")) path = QUrl(path).toLocalFile();
        if(!QDir::isAbsolutePath(path))continue;
        path=QDir::cleanPath(path);
        if(path.startsWith(prefix))return true;
        const QFileInfo info(path);auto canonical=info.canonicalFilePath();
        if(canonical.isEmpty()){const auto parent=info.absoluteDir().canonicalPath();if(!parent.isEmpty())canonical=QDir(parent).filePath(info.fileName());}
        if(!canonicalFolder.isEmpty()&&canonical.startsWith(canonicalPrefix))return true;
    }
    return false;
}
qint64 folderBytes(const QString& folder) {
    qint64 bytes = 0; QDirIterator it(folder, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); bytes += it.fileInfo().size(); } return bytes;
}
}
QByteArray ProjectStorage::magic() { return QByteArray("ACPJ0002", 8); }
QString ProjectStorage::defaultCacheDirectory() { return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath("projects"); }
QStringList ProjectStorage::sourcePaths(const WorkspaceSnapshot& snapshot) {
    QStringList paths;
    for (const auto& row : snapshot.state["frames"].toList()) paths << row.toMap()["path"].toString();
    const auto sourceRowPath=snapshot.state["sourceRow"].toMap()["path"].toString();if(!sourceRowPath.isEmpty())paths<<sourceRowPath;
    const auto references=snapshot.state["references"].toMap();for(const auto& path:references)if(!path.toString().isEmpty())paths<<path.toString();
    const auto add = [&](const QVariantMap& finish) {
        if (!finish["maskFile"].toString().isEmpty()) paths << finish["maskFile"].toString();
        for (const auto& op : finish["operations"].toList()) {
            const auto path = op.toMap()["settings"].toMap()["maskFile"].toString(); if (!path.isEmpty()) paths << path;
        }
    };
    add(snapshot.state["finish"].toMap());
    const auto options = snapshot.state["options"].toMap(); add(options["finish"].toMap());
    const auto versions = options["versions"].toMap(); add(versions["A"].toMap()); add(versions["B"].toMap());
    paths.removeAll(QString()); paths.removeDuplicates(); return paths;
}
QVariantMap ProjectStorage::fingerprint(const QString& path, const Check& check) {
    const QFileInfo before(path); require(before.isFile(), "A project source file is missing or unreadable.");
    const auto hash = digest(path, check); const QFileInfo after(path);
    require(before.size() == after.size() && before.lastModified() == after.lastModified(), "A source file changed while saving. Try again.");
    return {{"name", before.fileName()}, {"bytes", QString::number(before.size())}, {"sha256", QString::fromLatin1(hash)}};
}
QVariant ProjectStorage::remap(const QVariant& value, const QMap<QString, QString>& paths) {
    if (value.metaType().id() == QMetaType::QVariantMap) {
        auto map = value.toMap(); for (auto it = map.begin(); it != map.end(); ++it) it.value() = remap(it.value(), paths);
        // Fingerprint dictionaries use paths as keys as well as values.
        QVariantMap renamed; for (auto it = map.cbegin(); it != map.cend(); ++it) renamed[paths.value(it.key(), it.key())] = it.value(); return renamed;
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        auto list = value.toList(); for (auto& item : list) item = remap(item, paths); return list;
    }
    if (value.metaType().id() == QMetaType::QStringList) {
        auto list = value.toStringList(); for (auto& item : list) item = paths.value(item, item); return list;
    }
    if (value.metaType().id() == QMetaType::QString) return paths.value(value.toString(), value.toString());
    return value;
}
double ProjectStorage::estimatedBytes(const WorkspaceSnapshot& s) {
    double bytes = 65536; std::set<const processing::FloatImage*> images; std::set<const std::vector<float>*> planes; QSet<qint64> previews;
    QSet<QString> files;for (const auto& path : sourcePaths(s)) { const QFileInfo info(path); if (!info.isFile()) return -1;if(!files.contains(info.canonicalFilePath())){files.insert(info.canonicalFilePath());bytes += info.size();} }
    for (const auto& image : s.images) if (images.insert(image.get()).second) bytes += 20 + image->rgb.size() * 4.;
    for (const auto& plane : s.planes) if (planes.insert(plane.get()).second) bytes += 20 + plane->size() * 4.;
    // PNG compression depends on the image. Report a conservative estimate.
    for (const auto& preview : s.previews) if (!previews.contains(preview.cacheKey())) { previews.insert(preview.cacheKey()); bytes += preview.sizeInBytes() * 1.01 + 1024; }
    return bytes;
}
void ProjectStorage::save(const QString& path, const WorkspaceSnapshot& snapshot, const Check& check, const Progress& progress) {
    const auto sources = sourcePaths(snapshot); require(sources.size() <= 4096, "Too many source files in this project.");
    spaceFor(QFileInfo(path).absolutePath(), qint64(std::max(0., estimatedBytes(snapshot))));
    QTemporaryDir stage(QFileInfo(path).dir().filePath(".astrocapture-project-XXXXXX")); require(stage.isValid(), "Cannot create temporary project files.");
    QMap<QString, QString> paths; QVariantMap fingerprints; QList<Entry> entries; QJsonArray inputs;
    for (int i = 0; i < sources.size(); ++i) {
        checkpoint(check); const auto canonical=QFileInfo(sources[i]).canonicalFilePath();
        require(!canonical.isEmpty(), "A project source file is missing or unreadable.");
        if(paths.contains(canonical)){paths[sources[i]]=paths[canonical];fingerprints[sources[i]]=fingerprints[canonical];continue;}
        auto suffix = QFileInfo(sources[i]).suffix().toLower(); if (!QRegularExpression("^[a-z0-9]{1,12}$").match(suffix).hasMatch()) suffix = "dat";
        const auto name = QString("inputs/%1.%2").arg(i, 6, 10, QChar('0')).arg(suffix); auto fp = fingerprint(sources[i], check);
        auto originalName=snapshot.state["sourceFingerprints"].toMap()[sources[i]].toMap()["name"].toString();
        auto expected=snapshot.state["sourceFingerprints"].toMap()[sources[i]].toMap()["sha256"].toString();
        for(const auto& value:snapshot.state["frames"].toList()){const auto row=value.toMap();if(row["path"].toString()==sources[i]){expected=row["sourceFingerprint"].toMap()["sha256"].toString();originalName=row["name"].toString();}}
        require(expected.isEmpty()||expected==fp["sha256"].toString(), "A source file changed since import. Reimport it before saving a portable project.");
        if(!originalName.isEmpty())fp["name"]=originalName;fingerprints[sources[i]]=fp;fingerprints[canonical]=fp;
        entries << Entry{name, sources[i], fp["bytes"].toString().toLongLong(), fp["sha256"].toString().toLatin1()}; paths[sources[i]] = token(name);paths[canonical]=token(name); inputs << name;
    }
    auto portable = snapshot; portable.state["sourceFingerprints"]=fingerprints;
    auto rows=portable.state["frames"].toList();for(auto& value:rows){auto row=value.toMap();row["sourceFingerprint"]=fingerprints[row["path"].toString()];value=row;}portable.state["frames"]=rows;
    portable.state = remap(portable.state, paths).toMap();
    SessionStorage storage(stage.filePath("state")); storage.save({{}, {{"deepSky", portable}}}, check);
    for (const auto& info : QDir(stage.filePath("state")).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        const auto name = "state/" + info.fileName(); require(validEntry(name), "Invalid portable project asset.");
        entries << Entry{name, info.filePath(), info.size(), digest(info.filePath(), check)};
    }
    qint64 total = 0; QJsonArray files;
    for (const auto& entry : entries) { require(total <= maxArchive - entry.bytes, "Portable project exceeds 128 GiB."); total += entry.bytes;
        files << QJsonObject{{"name", entry.name}, {"bytes", QString::number(entry.bytes)}, {"sha256", QString::fromLatin1(entry.hash)}};
    }
    const auto manifest = QJsonDocument(QJsonObject{{"version", 2}, {"inputs", inputs}, {"files", files}}).toJson(QJsonDocument::Compact);
    require(manifest.size() <= maxManifest, "Portable project manifest is too large."); spaceFor(QFileInfo(path).absolutePath(), total + manifest.size() + 44);
    QSaveFile output(path); require(output.open(QIODevice::WriteOnly), "Cannot save portable project. Check disk space.");
    QByteArray header = magic(); char size[4]; qToLittleEndian<quint32>(manifest.size(), size); header.append(size, 4); header.append(QCryptographicHash::hash(manifest, QCryptographicHash::Sha256)); header += manifest;
    require(output.write(header) == header.size(), "Cannot save portable project. Check disk space.");
    qint64 done = 0; int lastProgress = -1;
    for (const auto& entry : entries) {
        QFile input(entry.source); require(input.open(QIODevice::ReadOnly) && input.size() == entry.bytes, "A source file changed while saving. Try again.");
        QCryptographicHash hash(QCryptographicHash::Sha256); qint64 remaining = entry.bytes;
        while (remaining) { checkpoint(check); const auto block = input.read(std::min(chunkSize, remaining)); require(!block.isEmpty() && output.write(block) == block.size(), "Cannot save portable project. Check disk space."); hash.addData(block); remaining -= block.size(); done += block.size(); const int percent = total ? int(done * 100 / total) : 100; if (progress && percent != lastProgress) { lastProgress = percent; progress(percent); } }
        require(hash.result().toHex() == entry.hash && input.atEnd(), "A source file changed while saving. Try again.");
    }
    checkpoint(check); require(output.commit(), "Cannot finish saving portable project.");
}
WorkspaceSnapshot ProjectStorage::load(const QString& path, const QString& cache, const Check& check, const Progress& progress, const std::function<bool(const WorkspaceSnapshot&)>& validate) {
    QFile input(path); require(input.open(QIODevice::ReadOnly) && input.read(8) == magic(), "Invalid portable project.");
    const auto length = input.read(4); require(length.size() == 4, "Incomplete portable project."); const auto size = qFromLittleEndian<quint32>(length.constData());
    require(size > 0 && size <= maxManifest, "Invalid portable project manifest size.");
    const auto hash = input.read(32); const auto bytes = input.read(size); require(bytes.size() == size && hash == QCryptographicHash::hash(bytes, QCryptographicHash::Sha256), "Portable project manifest is damaged.");
    QJsonParseError error; const auto object = QJsonDocument::fromJson(bytes, &error).object();
    require(error.error == QJsonParseError::NoError && object["version"].toInt() == 2 && object["files"].isArray() && object["inputs"].isArray(), "Invalid portable project manifest.");
    const auto files = object["files"].toArray(); require(files.size() > 0 && files.size() <= 4500, "Too many portable project assets.");
    QList<Entry> entries; QSet<QString> names; qint64 total = 0, stateBytes = 0;
    for (const auto& file : files) {
        const auto row = file.toObject(); const auto name = row["name"].toString(); const auto count = sizeValue(row["bytes"]); const auto digest = row["sha256"].toString().toLatin1();
        require(validEntry(name) && !names.contains(name) && QRegularExpression("^[0-9a-f]{64}$").match(QString::fromLatin1(digest)).hasMatch() && total <= maxArchive - count, "Invalid portable project asset.");
        if (name.startsWith("state/")) { stateBytes += count; require(stateBytes <= 4LL * 1024 * 1024 * 1024 + maxManifest, "Portable project results exceed 4 GiB."); }
        total += count; names.insert(name); entries << Entry{name, {}, count, digest};
    }
    require(names.contains("state/session.json") && input.size() - input.pos() == total, "Incomplete portable project.");
    QSet<QString> inputs; for (const auto& item : object["inputs"].toArray()) { const auto name = item.toString(); require(name.startsWith("inputs/") && names.contains(name) && !inputs.contains(name), "Invalid portable project source list."); inputs.insert(name); }
    for (const auto& name : names) require(!name.startsWith("inputs/") || inputs.contains(name), "Invalid portable project source list.");
    require(QDir().mkpath(cache) && !QFileInfo(cache).isSymLink(), "Cannot create the project cache."); spaceFor(cache, total);
    QTemporaryDir stage(QDir(cache).filePath(".opening-XXXXXX")); require(stage.isValid() && QDir().mkpath(stage.filePath("state")) && QDir().mkpath(stage.filePath("inputs")), "Cannot create the project cache.");
    qint64 done = 0; int lastProgress = -1;
    for (const auto& entry : entries) {
        QFile output(stage.filePath(entry.name)); require(output.open(QIODevice::WriteOnly), "Cannot open portable project. Check cache disk space.");
        QCryptographicHash digest(QCryptographicHash::Sha256); qint64 remaining = entry.bytes;
        while (remaining) { checkpoint(check); const auto block = input.read(std::min(chunkSize, remaining)); require(!block.isEmpty() && output.write(block) == block.size(), "Cannot open portable project. Check cache disk space."); digest.addData(block); remaining -= block.size(); done += block.size(); const int percent = total ? int(done * 100 / total) : 100; if (progress && percent != lastProgress) { lastProgress = percent; progress(percent); } }
        require(digest.result().toHex() == entry.hash, "Portable project data is damaged.");
    }
    const auto session = SessionStorage(stage.filePath("state")).load(check); require(session.workspaces.size() == 1 && session.workspaces.contains("deepSky"), "Invalid portable project workspace.");
    auto snapshot = session.workspaces["deepSky"];
    require(!validate || validate(snapshot), "Invalid portable project settings or results.");
    for (const auto& source : sourcePaths(snapshot)) require(source.startsWith("astroproject:") && inputs.contains(source.mid(13)), "Invalid portable project source reference.");
    const auto destination = QDir(cache).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); QMap<QString, QString> paths;
    for (const auto& name : inputs) paths[token(name)] = QDir(destination).filePath(name);
    snapshot.state = remap(snapshot.state, paths).toMap();
    QFile marker(stage.filePath(".astrocache")); require(marker.open(QIODevice::WriteOnly) && marker.write("{\"version\":1}") == 13, "Cannot finish opening the portable project."); marker.close();
    checkpoint(check); require(QDir().rename(stage.path(), destination), "Cannot finish opening the portable project."); stage.setAutoRemove(false); return snapshot;
}
QSet<QString> ProjectStorage::strings(const QVariant& value) {
    QSet<QString> result;
    if (value.metaType().id() == QMetaType::QVariantMap) { const auto map = value.toMap(); for (auto it = map.cbegin(); it != map.cend(); ++it) { result.insert(it.key()); result.unite(strings(it.value())); } }
    else if (value.metaType().id() == QMetaType::QVariantList || value.metaType().id() == QMetaType::QStringList) { for (const auto& item : value.toList()) result.unite(strings(item)); }
    else if (value.canConvert<QString>()) result.insert(value.toString()); return result;
}
QVariantMap ProjectStorage::cacheUsage(const QString& root, const QSet<QString>& paths) {
    qint64 total = 0, unused = 0; int count = 0;
    for (const auto& folder : cacheFolders(root)) { const auto bytes = folderBytes(folder); total += bytes; if (!inUse(folder, paths)) unused += bytes; ++count; }
    return {{"bytes", double(total)}, {"unusedBytes", double(unused)}, {"projects", count}};
}
qint64 ProjectStorage::clearUnusedCache(const QString& root, const QSet<QString>& paths, const Check& check) {
    qint64 removed = 0;
    for (const auto& folder : cacheFolders(root)) { checkpoint(check); if (inUse(folder, paths)) continue; const auto bytes = folderBytes(folder); require(QDir(folder).removeRecursively(), "Cannot remove an unused project cache."); removed += bytes; }
    return removed;
}
}
