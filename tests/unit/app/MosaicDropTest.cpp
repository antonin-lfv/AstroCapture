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
#include "MosaicController.hpp"
#include "MosaicImageItem.hpp"
#include "Check.hpp"

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QTemporaryDir temp;
    ASTRO_TEST_CHECK(temp.isValid());
    QCoreApplication::setOrganizationName("AstroCaptureTest");
    QCoreApplication::setApplicationName("MosaicDropTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    qmlRegisterType<astro::app::AppLibrary>("AstroCapture", 1, 0, "AppLibrary");
    qmlRegisterType<astro::app::MosaicController>("AstroCapture", 1, 0, "MosaicController");
    qmlRegisterType<astro::app::MosaicImageItem>("AstroCapture", 1, 0, "MosaicImageItem");
    astro::app::MosaicController controller;
    astro::app::AppLibrary library;
    QQmlEngine engine;
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/MosaicWorkspace.qml"));
    ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    // Production types live in the executable QML module. Import the registered
    // test type when loading this same, unmodified workspace from the source tree.
    component.setData("import AstroCapture 1.0\n" + source.readAll(), QUrl::fromLocalFile(source.fileName()));
    if (component.isError()) qWarning() << component.errors();
    ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library", QVariant::fromValue(&library)}, {"controller", QVariant::fromValue(&controller)}}));
    ASTRO_TEST_CHECK(object != nullptr);
    auto* workspace = qobject_cast<QQuickItem*>(object.get());
    ASTRO_TEST_CHECK(workspace != nullptr);
    auto* softenEdges=workspace->findChild<QObject*>("mosaicSoftenEdges");
    ASTRO_TEST_CHECK(softenEdges && workspace->property("softenEdges").toBool());
    softenEdges->setProperty("checked",false);
    ASTRO_TEST_CHECK(!workspace->property("softenEdges").toBool());
    softenEdges->setProperty("checked",true);
    ASTRO_TEST_CHECK(workspace->property("softenEdges").toBool());
    QQuickWindow window;
    window.resize(1000, 700);
    workspace->setParentItem(window.contentItem());
    workspace->setSize(QSizeF(1000, 700));
    window.show();
    QCoreApplication::processEvents();

    QImage image(128, 128, QImage::Format_RGB32);
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
    ASTRO_TEST_CHECK(controller.files().isEmpty());
    ASTRO_TEST_CHECK(drop({png, jpeg}));
    ASTRO_TEST_CHECK(controller.files().size() == 2);
    ASTRO_TEST_CHECK(drop({png}));
    ASTRO_TEST_CHECK(controller.files().size() == 2);
    auto* viewport=workspace->findChild<QQuickItem*>("mosaicViewport");
    ASTRO_TEST_CHECK(viewport);
    QElapsedTimer timer; timer.start();
    while(timer.elapsed()<100) { QCoreApplication::processEvents(); QThread::msleep(1); }
    std::function<QQuickItem*(QQuickItem*)> findSelection=[&](QQuickItem* item)->QQuickItem* {
        if(item->objectName()=="mosaicTileSelection") return item;
        for(auto* child:item->childItems()) if(auto* result=findSelection(child)) return result;
        return nullptr;
    };
    auto* selection=findSelection(workspace); ASTRO_TEST_CHECK(selection);
    // macOS accessibility changes checked directly, without emitting toggled.
    selection->setProperty("checked",false); QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(controller.selectedCount()==1 && controller.files().size()==2);
    selection->setProperty("checked",true); QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(controller.selectedCount()==2);
    ASTRO_TEST_CHECK(viewport->width()>500);
    ASTRO_TEST_CHECK(viewport->mapToScene({viewport->width(),0}).x()<=1000);
    QList<QUrl> many;
    for(int i=0;i<30;++i) { const auto file=temp.filePath(QString("tile-%1.png").arg(i)); ASTRO_TEST_CHECK(image.save(file)); many.append(QUrl::fromLocalFile(file)); }
    controller.addImages(many);
    timer.restart(); while(timer.elapsed()<150) { QCoreApplication::processEvents(); QThread::msleep(1); }
    auto* grid=workspace->findChild<QQuickItem*>("mosaicFiles"); ASTRO_TEST_CHECK(grid);
    std::function<int(QQuickItem*)> countActive=[&](QQuickItem* item) {
        int count=item->objectName()=="mosaicThumbnailLoader" && item->property("active").toBool() ? 1 : 0;
        for(auto* child:item->childItems()) count+=countActive(child); return count;
    };
    int active=countActive(grid);
    ASTRO_TEST_CHECK(active>0 && active<12);
    grid->setProperty("contentY",1000); QCoreApplication::processEvents();
    active=countActive(grid);
    ASTRO_TEST_CHECK(active<12);
    controller.removeImage(0);
    ASTRO_TEST_CHECK(controller.files().size()==31 && QFileInfo::exists(png.toLocalFile()));
    workspace->setParentItem(nullptr);
}
