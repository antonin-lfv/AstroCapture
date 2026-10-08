#include <QCoreApplication>
#include <QUrlQuery>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "AppLibrary.hpp"
#include "ProcessingSettings.hpp"
#include "Check.hpp"

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QTemporaryDir temp;
    ASTRO_TEST_CHECK(temp.isValid());
    QCoreApplication::setOrganizationName("AstroCaptureTest");
    QCoreApplication::setApplicationName("AppLibraryTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    const QString folder = temp.filePath("exports é # ?");
    ASTRO_TEST_CHECK(QDir().mkpath(folder));
    const QUrl image = QUrl::fromLocalFile(folder + "/Saturne #1.tiff");
    QFile file(image.toLocalFile());
    ASTRO_TEST_CHECK(file.open(QIODevice::WriteOnly));
    file.write("reference fixture"); file.close();
    const QUrl suggestion = QUrl::fromLocalFile(temp.filePath("video-processed.tif"));
    astro::app::AppLibrary library;
    ASTRO_TEST_CHECK(library.language() == "en" && library.historyEnabled() && library.images().isEmpty());
    QCoreApplication::setApplicationVersion("1.0.2");
    const QUrl report = library.bugReportUrl();
    ASTRO_TEST_CHECK(report.scheme() == "mailto" && report.path() == "antoninlefevre45@gmail.com");
    const QUrlQuery reportQuery(QUrl::fromEncoded(report.toEncoded()));
    ASTRO_TEST_CHECK(reportQuery.queryItemValue("subject", QUrl::FullyDecoded).contains("1.0.2"));
    ASTRO_TEST_CHECK(reportQuery.queryItemValue("body", QUrl::FullyDecoded).contains("\n\nSteps to reproduce:"));
    library.chooseExportDirectory(QUrl::fromLocalFile(folder));
    ASTRO_TEST_CHECK(library.suggestedExport(suggestion).toLocalFile() == folder + "/video-processed.tif");
    ASTRO_TEST_CHECK(library.exportFolderFor(suggestion).toLocalFile() == folder);
    library.recordExport(image);
    library.recordExport(image);
    ASTRO_TEST_CHECK(library.images().size() == 1);
    ASTRO_TEST_CHECK(library.images().front().toMap().value("url").toUrl() == QUrl::fromLocalFile(QFileInfo(image.toLocalFile()).canonicalFilePath()));
    library.recordExport(QUrl("https://example.org/image.tif"));
    library.recordExport(QUrl::fromLocalFile(temp.filePath("missing.tif")));
    ASTRO_TEST_CHECK(library.images().size() == 1 && library.exportDirectory() == folder);
    library.setLanguage("fr");
    library.setLanguage("unknown");
    ASTRO_TEST_CHECK(library.language() == "fr");
    library.setHistoryEnabled(false);
    const QUrl other = QUrl::fromLocalFile(temp.filePath("other.tif"));
    ASTRO_TEST_CHECK(QFile::copy(image.toLocalFile(), other.toLocalFile()));
    library.recordExport(other);
    ASTRO_TEST_CHECK(library.images().size() == 1 && library.exportDirectory() == temp.path());
    library.importImages({other});
    ASTRO_TEST_CHECK(library.images().size() == 2);
    {
        astro::app::AppLibrary restored;
        ASTRO_TEST_CHECK(restored.language() == "fr" && !restored.historyEnabled());
        ASTRO_TEST_CHECK(restored.exportDirectory() == temp.path() && restored.images().size() == 2);
        ASTRO_TEST_CHECK(restored.images().first().toMap().value("url").toUrl() == QUrl::fromLocalFile(QFileInfo(other.toLocalFile()).canonicalFilePath()));
        restored.setHistoryEnabled(true);
        restored.rememberImage(image); // Captures don't replace the processing export folder.
        ASTRO_TEST_CHECK(restored.exportDirectory() == temp.path());
        ASTRO_TEST_CHECK(restored.images().first().toMap().value("url").toUrl() == QUrl::fromLocalFile(QFileInfo(image.toLocalFile()).canonicalFilePath()));
        ASTRO_TEST_CHECK(QFile::remove(image.toLocalFile()));
        restored.refresh();
        ASTRO_TEST_CHECK(restored.images().size() == 1);
        ASTRO_TEST_CHECK(restored.images().first().toMap().value("url").toUrl() == QUrl::fromLocalFile(QFileInfo(other.toLocalFile()).canonicalFilePath()));
        const auto lastFolder = restored.exportDirectory();
        restored.chooseExportDirectory(QUrl("https://example.org/folder"));
        ASTRO_TEST_CHECK(restored.exportDirectory() == lastFolder);
    }
    const QUrl png = QUrl::fromLocalFile(temp.filePath("import # é.png"));
    const QUrl jpeg = QUrl::fromLocalFile(temp.filePath("import.jpg"));
    ASTRO_TEST_CHECK(QFile::copy(other.toLocalFile(), png.toLocalFile()));
    ASTRO_TEST_CHECK(QFile::copy(other.toLocalFile(), jpeg.toLocalFile()));
    astro::app::AppLibrary imported;
    imported.importImages({png, jpeg, QUrl("https://example.org/remote.png"), QUrl::fromLocalFile(temp.path())});
    ASTRO_TEST_CHECK(imported.images().size() == 3);
    ASTRO_TEST_CHECK(imported.exportDirectory() == temp.path());
    imported.setLanguage("en");
    astro::app::AppLibrary finalRestore;
    ASTRO_TEST_CHECK(finalRestore.images().size() == 3 && finalRestore.historyEnabled() && finalRestore.language() == "en");
    finalRestore.removeImage(png);
    ASTRO_TEST_CHECK(finalRestore.images().size() == 2);
    ASTRO_TEST_CHECK(QFileInfo::exists(png.toLocalFile()));
    ASTRO_TEST_CHECK(finalRestore.exportDirectory() == temp.path());
    finalRestore.removeImage(QUrl("https://example.org/remote.png"));
    finalRestore.removeImage(png); // Removing an entry twice is harmless.
    ASTRO_TEST_CHECK(finalRestore.images().size() == 2);
    astro::app::AppLibrary afterRemoval;
    ASTRO_TEST_CHECK(afterRemoval.images().size() == 2);
    ASTRO_TEST_CHECK(QFile::remove(jpeg.toLocalFile()));
    astro::app::AppLibrary afterRestart; // Missing paths are pruned at startup too.
    ASTRO_TEST_CHECK(afterRestart.images().size() == 1);
    ASTRO_TEST_CHECK(QFileInfo::exists(other.toLocalFile()));
    afterRestart.removeImage(other);
    ASTRO_TEST_CHECK(afterRestart.images().isEmpty());
    ASTRO_TEST_CHECK(QFileInfo::exists(other.toLocalFile()));
    astro::app::AppLibrary emptyRestore;
    ASTRO_TEST_CHECK(emptyRestore.images().isEmpty());
    auto values = astro::app::defaultProcessingSettings();
    values["level1"] = 1.7;
    ASTRO_TEST_CHECK(emptyRestore.saveProcessingPreset("Saturne doux", values));
    ASTRO_TEST_CHECK(!emptyRestore.saveProcessingPreset("Saturne doux", values));
    ASTRO_TEST_CHECK(!emptyRestore.saveProcessingPreset("Incomplete", {{"gamma", 1.0}}));
    values["gamma"] = 1.2;
    ASTRO_TEST_CHECK(emptyRestore.saveProcessingPreset("Saturne doux", values, true));
    emptyRestore.setCompactProcessing(true);
    emptyRestore.setProcessingPanelWidth(400);
    const auto video = QUrl::fromLocalFile(temp.filePath("Lune é #1.ser"));
    ASTRO_TEST_CHECK(QFile::copy(other.toLocalFile(), video.toLocalFile()));
    emptyRestore.recordVideo(video);
    emptyRestore.recordVideo(video);
    emptyRestore.recordVideo(QUrl("https://example.org/video.ser"));
    emptyRestore.recordVideo(png);
    astro::app::AppLibrary preferences;
    ASTRO_TEST_CHECK(preferences.processingPreset("Saturne doux") == values);
    ASTRO_TEST_CHECK(preferences.compactProcessing() && preferences.processingPanelWidth() == 400);
    ASTRO_TEST_CHECK(preferences.recentVideos().size() == 1);
    ASTRO_TEST_CHECK(preferences.lastVideoDirectory().toLocalFile() == QFileInfo(video.toLocalFile()).canonicalPath());
    preferences.removeProcessingPreset("Saturne doux");
    astro::app::AppLibrary deletedPreset;
    ASTRO_TEST_CHECK(deletedPreset.processingPresetNames().isEmpty());

}
