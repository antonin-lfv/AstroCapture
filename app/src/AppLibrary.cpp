#include "AppLibrary.hpp"
#include "ProcessingSettings.hpp"
#include "DeepSkySettings.hpp"

#include <QCoreApplication>
#include <QUrlQuery>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <algorithm>

namespace astro::app {
namespace {
bool isSupportedImage(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "tif" || suffix == "tiff" || suffix == "png" || suffix == "jpg" || suffix == "jpeg";
}
bool isSupportedProcessingSource(const QFileInfo& file) {
    return QStringList{"ser","acraw","png","tif","tiff"}.contains(file.suffix().toLower());
}
constexpr int historyLimit = 500;
}
QUrl AppLibrary::bugReportUrl() const {
    QUrl url(QStringLiteral("mailto:antoninlefevre45@gmail.com"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("subject"), QStringLiteral("AstroCapture %1 — Bug report").arg(QCoreApplication::applicationVersion()));
    query.addQueryItem(QStringLiteral("body"), tr("Version: %1\n\nSteps to reproduce:\n\nExpected result:\n\nActual result:\n").arg(QCoreApplication::applicationVersion()));
    url.setQuery(query);
    return url;
}
void AppLibrary::reportBug() {
    if (!QDesktopServices::openUrl(bugReportUrl())) {
        status_ = tr("Could not open your email application. Contact antoninlefevre45@gmail.com.");
        emit changed();
    }
}
AppLibrary::AppLibrary(QObject* parent) : QObject(parent) {
    QSettings settings;
    exportDirectory_ = settings.value("application/exportDirectory").toString();
    language_ = settings.value("application/language", "en").toString() == "fr" ? "fr" : "en";
    historyEnabled_ = settings.value("application/historyEnabled", true).toBool();
    compactProcessing_ = settings.value("application/compactProcessing", false).toBool();
    deepSkyPanelWidth_ = std::clamp(settings.value("application/deepSkyPanelWidth", 360).toInt(), 320, 480);
    processingPanelWidth_ = std::clamp(settings.value("application/processingPanelWidth", 330).toInt(), 260, 600);
    recentProjects_ = settings.value("application/recentProjects").toStringList();
    recentProjects_.removeDuplicates();
    while(recentProjects_.size()>10)recentProjects_.removeLast();
    recentVideos_ = settings.value("application/recentVideos").toStringList();
    recentVideos_.removeDuplicates();
    while (recentVideos_.size() > 10) recentVideos_.removeLast();
    const auto deepPresets=QJsonDocument::fromJson(settings.value("application/deepSkyPresets").toByteArray()).object().toVariantMap();
    for(auto it=deepPresets.cbegin();it!=deepPresets.cend();++it)
        if(!it.key().trimmed().isEmpty() && it.key().size()<=80)
            if(auto values=checkedDeepSkySettings(it.value().toMap()))deepSkyPresets_.insert(it.key(),*values);
    const auto presets = QJsonDocument::fromJson(settings.value("application/processingPresets").toByteArray()).object().toVariantMap();
    for (auto it = presets.begin(); it != presets.end(); ++it) {
        if (auto values = checkedProcessingSettings(it.value().toMap())) processingPresets_.insert(it.key(), *values);
    }
    const auto saved = QJsonDocument::fromJson(settings.value("application/imageHistory").toByteArray()).array();
    for (const auto& value : saved) {
        auto entry = value.toObject().toVariantMap();
        const auto path = entry.value("path").toString();
        if (!QDir::isAbsolutePath(path) || !isSupportedImage(path)) continue;
        images_.append(entry);
        if (images_.size() == historyLimit) break;
    }
    if (pruneMissingImages()) save();
}
QUrl AppLibrary::exportDirectoryUrl() const {
    return exportDirectory_.isEmpty() ? QUrl{} : QUrl::fromLocalFile(exportDirectory_);
}
QVariantList AppLibrary::images() const {
    QVariantList result;
    for (const auto& value : images_) {
        auto entry = value.toMap();
        const QFileInfo file(entry.value("path").toString());
        entry["name"] = file.fileName();
        entry["url"] = QUrl::fromLocalFile(file.absoluteFilePath());
        entry["exists"] = file.isFile();
        result.append(entry);
    }
    return result;
}
void AppLibrary::save() {
    QSettings settings;
    settings.setValue("application/exportDirectory", exportDirectory_);
    settings.setValue("application/compactProcessing", compactProcessing_);
    settings.setValue("application/processingPanelWidth", processingPanelWidth_);
    settings.setValue("application/deepSkyPanelWidth", deepSkyPanelWidth_);
    settings.setValue("application/recentVideos", recentVideos_);
    settings.setValue("application/recentProjects", recentProjects_);
    settings.setValue("application/processingPresets", QJsonDocument(QJsonObject::fromVariantMap(processingPresets_)).toJson(QJsonDocument::Compact));
    settings.setValue("application/deepSkyPresets",QJsonDocument(QJsonObject::fromVariantMap(deepSkyPresets_)).toJson(QJsonDocument::Compact));
    settings.remove("application/showScrollbars");
    settings.setValue("application/language", language_);
    settings.setValue("application/historyEnabled", historyEnabled_);
    settings.setValue("application/imageHistory", QJsonDocument(QJsonArray::fromVariantList(images_)).toJson(QJsonDocument::Compact));
    settings.sync();
    if (settings.status() != QSettings::NoError) status_ = tr("Could not save application preferences.");
    emit changed();
}
QVariantList AppLibrary::recentProjects() const {
    QVariantList result;
    for(const auto& path:recentProjects_) {
        const QFileInfo file(path);
        if(!file.isFile()||!QDir::isAbsolutePath(path)||file.suffix().compare("astroproject",Qt::CaseInsensitive)!=0)continue;
        result.append(QVariantMap{{"name",file.completeBaseName()},{"path",path},{"url",QUrl::fromLocalFile(path)},{"exists",file.isFile()}});
    }
    return result;
}
void AppLibrary::removeRecentProject(const QUrl& url) {
    if(url.isLocalFile() && recentProjects_.removeAll(QFileInfo(url.toLocalFile()).absoluteFilePath()))save();
}
void AppLibrary::removeRecentVideo(const QUrl& url) {
    if(url.isLocalFile() && recentVideos_.removeAll(QFileInfo(url.toLocalFile()).absoluteFilePath()))save();
}
void AppLibrary::pruneUnavailableRecents() {
    bool changed=false;
    for(auto* list:{&recentProjects_,&recentVideos_})
        for(qsizetype i=list->size();i>0;--i)if(!QFileInfo(list->at(i-1)).isFile()){list->removeAt(i-1);changed=true;}
    if(changed)save();
}
void AppLibrary::recordProject(const QUrl& url) {
    if(!url.isLocalFile())return;
    const QFileInfo file(url.toLocalFile());
    if(!file.isFile()||file.suffix().compare("astroproject",Qt::CaseInsensitive)!=0)return;
    const auto path=file.absoluteFilePath();
    recentProjects_.removeAll(path);recentProjects_.prepend(path);
    while(recentProjects_.size()>10)recentProjects_.removeLast();
    save();emit changed();
}
QVariantList AppLibrary::recentVideos() const {
    QVariantList result;
    for (const auto& path : recentVideos_) {
        const QFileInfo file(path);
        if (!file.isFile() || !QDir::isAbsolutePath(path) || !isSupportedProcessingSource(file)) continue;
        result.append(QVariantMap{{"name", file.fileName()}, {"path", path},
                                  {"url", QUrl::fromLocalFile(path)}, {"exists", file.isFile()}});
    }
    return result;
}
QUrl AppLibrary::lastVideoDirectory() const {
    for (const auto& path : recentVideos_) {
        const QFileInfo file(path);
        if (file.isFile()) return QUrl::fromLocalFile(file.absolutePath());
    }
    return {};
}
void AppLibrary::recordVideo(const QUrl& url) {
    if (!url.isLocalFile()) return;
    const QFileInfo file(url.toLocalFile());
    if (!file.isFile() || !isSupportedProcessingSource(file)) return;
    const auto path = file.canonicalFilePath();
    recentVideos_.removeAll(path);
    recentVideos_.prepend(path);
    while (recentVideos_.size() > 10) recentVideos_.removeLast();
    save();
}
void AppLibrary::setCompactProcessing(bool value) {
    if (value == compactProcessing_) return;
    compactProcessing_ = value;
    save();
}
void AppLibrary::setDeepSkyPanelWidth(int value) {
    value=std::clamp(value,320,480);
    if(value==deepSkyPanelWidth_)return;
    deepSkyPanelWidth_=value;save();
}
void AppLibrary::setProcessingPanelWidth(int value) {
    value = std::clamp(value, 260, 600);
    if (value == processingPanelWidth_) return;
    processingPanelWidth_ = value;
    save();
}
bool AppLibrary::saveDeepSkyPreset(const QString& inputName,const QVariantMap& input,bool overwrite) {
    const auto name=inputName.trimmed();const auto values=checkedDeepSkySettings(input);
    if(name.isEmpty()||name.size()>80||!values||(deepSkyPresets_.contains(name)&&!overwrite))return false;
    deepSkyPresets_.insert(name,*values);status_=tr("Deep-sky preset saved.");save();return true;
}
QVariantMap AppLibrary::deepSkyPreset(const QString& name) const{return deepSkyPresets_.value(name).toMap();}
void AppLibrary::removeDeepSkyPreset(const QString& name){if(!deepSkyPresets_.remove(name))return;status_=tr("Deep-sky preset removed.");save();}
bool AppLibrary::saveProcessingPreset(const QString& inputName, const QVariantMap& input, bool overwrite) {
    const auto name = inputName.trimmed();
    const auto values = checkedProcessingSettings(input);
    if (name.isEmpty() || name.size() > 80 || !values) return false;
    if (processingPresets_.contains(name) && !overwrite) return false;
    processingPresets_.insert(name, *values);
    status_ = tr("Processing preset saved.");
    save();
    return true;
}
QVariantMap AppLibrary::processingPreset(const QString& name) const {
    return processingPresets_.value(name).toMap();
}
void AppLibrary::removeProcessingPreset(const QString& name) {
    if (!processingPresets_.remove(name)) return;
    status_ = tr("Processing preset removed.");
    save();
}
void AppLibrary::setLanguage(const QString& value) {
    if ((value != "en" && value != "fr") || language_ == value) return;
    language_ = value;
    status_.clear();
    save();
    emit languageChanged();
}
void AppLibrary::setHistoryEnabled(bool value) {
    if (historyEnabled_ == value) return;
    historyEnabled_ = value;
    save();
}
void AppLibrary::chooseExportDirectory(const QUrl& url) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isDir()) {
        status_ = tr("Choose an existing local folder.");
        emit changed();
        return;
    }
    exportDirectory_ = QFileInfo(url.toLocalFile()).absoluteFilePath();
    status_ = tr("Export folder saved.");
    save();
}
QUrl AppLibrary::suggestedExport(const QUrl& sourceSuggestion) const {
    if (exportDirectory_.isEmpty() || !QFileInfo(exportDirectory_).isDir()) return sourceSuggestion;
    return QUrl::fromLocalFile(QDir(exportDirectory_).filePath(QFileInfo(sourceSuggestion.toLocalFile()).fileName()));
}
QUrl AppLibrary::exportFolderFor(const QUrl& sourceSuggestion) const {
    return QUrl::fromLocalFile(QFileInfo(suggestedExport(sourceSuggestion).toLocalFile()).absolutePath());
}
bool AppLibrary::addImage(const QUrl& url) {
    if (!url.isLocalFile()) return false;
    const QFileInfo file(url.toLocalFile());
    if (!file.isFile() || !isSupportedImage(file.filePath())) return false;
    const QString path = file.canonicalFilePath();
    images_.erase(std::remove_if(images_.begin(), images_.end(), [&](const auto& value) {
        return value.toMap().value("path").toString() == path;
    }), images_.end());
    images_.prepend(QVariantMap{{"path", path}, {"date", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    if (images_.size() > historyLimit) images_.removeLast();
    return true;
}
void AppLibrary::rememberImage(const QUrl& url) {
    if (historyEnabled_ && addImage(url)) save();
}
void AppLibrary::recordExport(const QUrl& url) {
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile() || !isSupportedImage(url.toLocalFile())) return;
    exportDirectory_ = QFileInfo(url.toLocalFile()).absolutePath();
    if (historyEnabled_) addImage(url);
    status_.clear();
    save();
}
void AppLibrary::importImages(const QList<QUrl>& urls) {
    int count = 0;
    for (const auto& url : urls) if (addImage(url)) ++count;
    status_ = tr("%1 image(s) added to History.").arg(count);
    save();
}
bool AppLibrary::pruneMissingImages() {
    const auto removed = images_.removeIf([](const auto& value) {
        return !QFileInfo(value.toMap().value("path").toString()).isFile();
    });
    return removed > 0;
}
void AppLibrary::refresh() {
    // A no-op refresh must not reset the list or reload the selected preview.
    if (pruneMissingImages()) save();
}
void AppLibrary::removeImage(const QUrl& url) {
    if (!url.isLocalFile()) return;
    const QFileInfo file(url.toLocalFile());
    const auto canonical = file.canonicalFilePath();
    const auto path = canonical.isEmpty() ? file.absoluteFilePath() : canonical;
    const auto removed = images_.removeIf([&](const auto& value) {
        return value.toMap().value("path").toString() == path;
    });
    if (removed == 0) return;
    status_ = tr("Removed from History. The image file was kept.");
    save();
}
void AppLibrary::openImage(const QUrl& url) {
    if (!url.isLocalFile() || !isSupportedImage(url.toLocalFile()) || !QFileInfo(url.toLocalFile()).isFile()) {
        status_ = tr("Image not found. It may have been moved or deleted.");
    } else if (QDesktopServices::openUrl(url)) {
        status_.clear();
    } else status_ = tr("Could not open the image in your image viewer.");
    emit changed();
}
void AppLibrary::revealImage(const QUrl& url) {
    const QFileInfo file(url.toLocalFile());
    if (!url.isLocalFile() || !file.isFile()) {
        status_ = tr("Image not found. It may have been moved or deleted.");
        emit changed();
        return;
    }
#ifdef Q_OS_MACOS
    const bool opened = QProcess::startDetached("/usr/bin/open", {"-R", file.absoluteFilePath()});
#elif defined(Q_OS_WIN)
    const bool opened = QProcess::startDetached("explorer.exe", {"/select," + QDir::toNativeSeparators(file.absoluteFilePath())});
#else
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
#endif
    status_ = opened ? QString{} : tr("Could not open the image folder.");
    emit changed();
}
}
