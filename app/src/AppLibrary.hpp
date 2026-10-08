#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace astro::app {
// Local application preferences and image references; never owns the image files.
class AppLibrary : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString exportDirectory READ exportDirectory NOTIFY changed)
    Q_PROPERTY(QUrl exportDirectoryUrl READ exportDirectoryUrl NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool historyEnabled READ historyEnabled WRITE setHistoryEnabled NOTIFY changed)
    Q_PROPERTY(QVariantList images READ images NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList recentVideos READ recentVideos NOTIFY changed)
    Q_PROPERTY(QUrl lastVideoDirectory READ lastVideoDirectory NOTIFY changed)
    Q_PROPERTY(QStringList processingPresetNames READ processingPresetNames NOTIFY changed)
    Q_PROPERTY(bool compactProcessing READ compactProcessing WRITE setCompactProcessing NOTIFY changed)
    Q_PROPERTY(int processingPanelWidth READ processingPanelWidth WRITE setProcessingPanelWidth NOTIFY changed)
public:
    explicit AppLibrary(QObject* parent = nullptr);
    QString exportDirectory() const { return exportDirectory_; }
    QUrl exportDirectoryUrl() const;
    QString language() const { return language_; }
    bool historyEnabled() const { return historyEnabled_; }
    QVariantList images() const;
    QString status() const { return status_; }
    QVariantList recentVideos() const;
    QUrl lastVideoDirectory() const;
    QStringList processingPresetNames() const { return processingPresets_.keys(); }
    bool compactProcessing() const { return compactProcessing_; }
    int processingPanelWidth() const { return processingPanelWidth_; }
    void setCompactProcessing(bool value);
    void setProcessingPanelWidth(int value);
    Q_INVOKABLE void recordVideo(const QUrl& url);
    Q_INVOKABLE bool saveProcessingPreset(const QString& name, const QVariantMap& values, bool overwrite = false);
    Q_INVOKABLE QVariantMap processingPreset(const QString& name) const;
    Q_INVOKABLE void removeProcessingPreset(const QString& name);
    void setLanguage(const QString& value);
    void setHistoryEnabled(bool value);
    Q_INVOKABLE void chooseExportDirectory(const QUrl& url);
    Q_INVOKABLE QUrl suggestedExport(const QUrl& sourceSuggestion) const;
    Q_INVOKABLE QUrl exportFolderFor(const QUrl& sourceSuggestion) const;
    Q_INVOKABLE void recordExport(const QUrl& url);
    Q_INVOKABLE void rememberImage(const QUrl& url);
    Q_INVOKABLE void importImages(const QList<QUrl>& urls);
    Q_INVOKABLE QUrl bugReportUrl() const;
    Q_INVOKABLE void reportBug();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void removeImage(const QUrl& url);
    Q_INVOKABLE void openImage(const QUrl& url);
    Q_INVOKABLE void revealImage(const QUrl& url);
signals:
    void changed();
    void languageChanged();
private:
    bool addImage(const QUrl& url);
    bool pruneMissingImages();
    void save();
    QString exportDirectory_, status_;
    QString language_{"en"};
    bool historyEnabled_{true};
    QVariantList images_;
    QStringList recentVideos_;
    QVariantMap processingPresets_;
    bool compactProcessing_{false};
    int processingPanelWidth_{330};
};
}
