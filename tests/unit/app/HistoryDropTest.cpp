#include <QGuiApplication>
#include <QDragMoveEvent>
#include <memory>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QElapsedTimer>
#include <QThread>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include "AppLibrary.hpp"
#include "Check.hpp"

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QTemporaryDir temp;
    ASTRO_TEST_CHECK(temp.isValid());
    QCoreApplication::setOrganizationName("AstroCaptureTest");
    QCoreApplication::setApplicationName("HistoryDropTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    qmlRegisterType<astro::app::AppLibrary>("AstroCapture", 1, 0, "AppLibrary");
    astro::app::AppLibrary library;
    QQmlEngine engine;
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/HistoryWorkspace.qml"));
    ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    // Production types live in the executable QML module. Import the registered
    // test type when loading this same, unmodified workspace from the source tree.
    component.setData("import AstroCapture 1.0\n" + source.readAll(), QUrl::fromLocalFile(source.fileName()));
    if (component.isError()) qWarning() << component.errors();
    ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library", QVariant::fromValue(&library)}}));
    ASTRO_TEST_CHECK(object != nullptr);
    auto* workspace = qobject_cast<QQuickItem*>(object.get());
    ASTRO_TEST_CHECK(workspace != nullptr);
    QQuickWindow window;
    window.resize(1000, 700);
    workspace->setParentItem(window.contentItem());
    workspace->setSize(QSizeF(1000, 700));
    window.show();
    QCoreApplication::processEvents();

    QImage image(32, 32, QImage::Format_RGB32);
    image.fill(Qt::blue);
    const auto png = QUrl::fromLocalFile(temp.filePath("planète #1.png"));
    const auto jpeg = QUrl::fromLocalFile(temp.filePath("planète 2.jpg"));
    ASTRO_TEST_CHECK(image.save(png.toLocalFile()));
    ASTRO_TEST_CHECK(image.save(jpeg.toLocalFile()));
    auto drop = [&](const QList<QUrl>& urls) {
        QMimeData mime;
        mime.setUrls(urls);
        QDragEnterEvent enter(QPoint(500, 350), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &enter);
        // Qt Quick negotiates the active DropArea on drag move.
        QDragMoveEvent move(QPoint(500, 350), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &move);
        QDropEvent event(QPointF(500, 350), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
        QCoreApplication::processEvents();
        return event.isAccepted();
    };
    ASTRO_TEST_CHECK(!drop({QUrl("https://example.org/remote.png")}));
    ASTRO_TEST_CHECK(!drop({QUrl::fromLocalFile(temp.filePath("video.ser"))}));
    ASTRO_TEST_CHECK(library.images().isEmpty());
    ASTRO_TEST_CHECK(drop({png, jpeg}));
    ASTRO_TEST_CHECK(library.images().size() == 2);
    ASTRO_TEST_CHECK(drop({png}));
    ASTRO_TEST_CHECK(library.images().size() == 2); // A repeated drop must not duplicate the image.
    ASTRO_TEST_CHECK(library.exportDirectory().isEmpty()); // Importing is not a Processing export.
    const QString selectedPng = QFileInfo(png.toLocalFile()).canonicalFilePath();
    workspace->setProperty("selectedPath", selectedPng);
    auto* viewport = workspace->findChild<QObject*>("historyViewport");
    ASTRO_TEST_CHECK(viewport);
    const auto waitForPreview = [&] {
        QElapsedTimer timer; timer.start();
        while (viewport->property("sourceWidth").toInt() != image.width() && timer.elapsed() < 5000) {
            QCoreApplication::processEvents(); QThread::msleep(1);
        }
        ASTRO_TEST_CHECK(viewport->property("sourceWidth").toInt() == image.width());
        ASTRO_TEST_CHECK(viewport->property("sourceHeight").toInt() == image.height());
    };
    waitForPreview();
    if (QImageWriter::supportedImageFormats().contains("tiff")) {
        image = QImage(48, 40, QImage::Format_RGBA64);
        image.fill(Qt::red);
        const auto tiff = QUrl::fromLocalFile(temp.filePath("planète #3.tiff"));
        ASTRO_TEST_CHECK(image.save(tiff.toLocalFile()));
        ASTRO_TEST_CHECK(drop({tiff}));
        waitForPreview();
        library.removeImage(tiff);
        workspace->setProperty("selectedPath", selectedPng);
    }
    library.refresh();
    ASTRO_TEST_CHECK(workspace->property("selectedPath").toString() == selectedPng);
    // Removing another entry automatically must not jump away from this preview.
    ASTRO_TEST_CHECK(QFile::remove(jpeg.toLocalFile()));
    library.refresh();
    ASTRO_TEST_CHECK(library.images().size() == 1);
    ASTRO_TEST_CHECK(workspace->property("selectedPath").toString() == selectedPng);
    auto* removeButton = workspace->findChild<QObject*>("removeHistoryEntry");
    ASTRO_TEST_CHECK(removeButton && removeButton->property("enabled").toBool());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(removeButton, "clicked"));
    ASTRO_TEST_CHECK(library.images().isEmpty());
    ASTRO_TEST_CHECK(QFileInfo::exists(png.toLocalFile()));
    ASTRO_TEST_CHECK(!removeButton->property("enabled").toBool());
    ASTRO_TEST_CHECK(workspace->property("selectedPath").toString().isEmpty());
    workspace->setParentItem(nullptr);
}
