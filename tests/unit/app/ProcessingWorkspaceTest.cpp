#include <QImageReader>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QWheelEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTranslator>
#include <QtEndian>
#include <astro/recording/SerRecorder.hpp>
#include <cmath>
#include "ProcessingController.hpp"
#include "ProcessingEdits.hpp"
#include "ProcessingImageItem.hpp"
#include "AppLibrary.hpp"
#include "ExportSizeEstimator.hpp"
#include "Check.hpp"

static void until(const std::function<bool()>& ready) {
    QElapsedTimer timer; timer.start();
    while (!ready() && timer.elapsed() < 10000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    ASTRO_TEST_CHECK(ready());
}
int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QTranslator translator;if(translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))application.installTranslator(&translator);
    QTemporaryDir temp;
    QCoreApplication::setOrganizationName("AstroCaptureTest");
    QCoreApplication::setApplicationName("ProcessingWorkspaceTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    qmlRegisterType<astro::app::ProcessingController>("AstroCapture", 1, 0, "ProcessingController");
    qmlRegisterType<astro::app::ProcessingEdits>("AstroCapture", 1, 0, "ProcessingEdits");
    qmlRegisterType<astro::app::ProcessingImageItem>("AstroCapture", 1, 0, "ProcessingImageItem");
    qmlRegisterType<astro::app::AppLibrary>("AstroCapture", 1, 0, "AppLibrary");
    qmlRegisterType<astro::app::ExportSizeEstimator>("AstroCapture",1,0,"ExportSizeEstimator");
    astro::app::AppLibrary library;
    astro::app::ProcessingController controller;
    QQmlEngine engine;
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/ProcessingWorkspace.qml"));
    ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);
    component.setData("import AstroCapture 1.0\n" + source.readAll(), QUrl::fromLocalFile(source.fileName()));
    if (component.isError()) qWarning() << component.errors();
    ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library", QVariant::fromValue(&library)}, {"controller", QVariant::fromValue(&controller)}}));
    ASTRO_TEST_CHECK(object != nullptr);
    auto* root = qobject_cast<QQuickItem*>(object.get());
    QQuickWindow window;
    window.resize(1100, 720); root->setParentItem(window.contentItem()); root->setSize({1100, 720}); window.show();
    auto* controls=root->findChild<QQuickItem*>("processingControls");ASTRO_TEST_CHECK(controls && !controls->isVisible());
    const QString path = temp.filePath("test.ser");
    astro::domain::FrameDescriptor descriptor;
    descriptor.width = 96; descriptor.height = 64;
    descriptor.storage = astro::domain::PixelStorage::Mono8; descriptor.validBits = 8;
    astro::recording::SerRecorder recorder;
    ASTRO_TEST_CHECK(recorder.open(path.toStdString(), descriptor, 20));
    for (int n = 0; n < 8; ++n) {
        astro::domain::FrameBuffer frame; frame.descriptor = descriptor; frame.metadata.sequence = n;
        std::vector<std::byte> bytes(96 * 64);
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 96; ++x)
            bytes[y * 96 + x] = std::byte(int(100 + 50 * std::sin((x - n % 3) * .16) * std::cos((y - n % 2) * .21)));
        frame.storage = std::make_shared<const astro::domain::BufferStorage>(std::move(bytes));
        ASTRO_TEST_CHECK(recorder.writeFrame(frame));
    }
    ASTRO_TEST_CHECK(recorder.close());
    controller.openPath(path); until([&] { return !controller.busy(); });
    ASTRO_TEST_CHECK(library.recentVideos().size() == 1);
    controller.analyze(); until([&] { return !controller.busy(); });
    // Drag the quality cutoff and verify the frame count follows immediately.
    auto* cutoff=root->findChild<QQuickItem*>("qualitySelectionHandle");
    auto* selectedCount=root->findChild<QObject*>("framesToKeep");
    ASTRO_TEST_CHECK(cutoff && selectedCount);
    until([&] { return cutoff->isVisible() && cutoff->width() > 100 && cutoff->mapToScene({0,0}).y() > 0; });
    // Let the layout settle after analysis reveals the graph.
    QElapsedTimer layoutTimer; layoutTimer.start();
    while (layoutTimer.elapsed() < 100) { QCoreApplication::processEvents(); QThread::msleep(1); }
    const auto dragPoint=cutoff->mapToScene({cutoff->width()*.25,cutoff->height()/2});
    QMouseEvent press(QEvent::MouseButtonPress,dragPoint,window.mapToGlobal(dragPoint.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&press);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(selectedCount->property("value").toInt()==2);
    QMouseEvent release(QEvent::MouseButtonRelease,dragPoint,window.mapToGlobal(dragPoint.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&window,&release);
    controller.stack(6); until([&] { return !controller.busy(); });
    ASTRO_TEST_CHECK(controller.showingResult());
    const auto before = controller.image();
    auto* edits = root->findChild<astro::app::ProcessingEdits*>("processingEdits");
    ASTRO_TEST_CHECK(edits);
    edits->beginEdit(); edits->setValue("level1", 1); edits->setValue("level1", 2); edits->endEdit();
    until([&] { return !root->property("adjustmentsDirty").toBool() && !controller.busy(); });
    ASTRO_TEST_CHECK(controller.image() != before);
    const auto after = controller.image();
    auto* paintItem = root->findChild<astro::app::ProcessingImageItem*>("processingPreviewImage");
    ASTRO_TEST_CHECK(paintItem);
    paintItem->setWidth(after.width()); paintItem->setHeight(after.height());
    root->setProperty("holdBefore", true);
    QImage painted(after.size(), QImage::Format_RGB888);
    { QPainter painter(&painted); paintItem->paint(&painter); }
    ASTRO_TEST_CHECK(painted == before && controller.image() == after);
    root->setProperty("holdBefore", false); root->setProperty("compareImages", true);
    root->setProperty("compareSplit", 0.5);
    { QPainter painter(&painted); paintItem->paint(&painter); }
    ASTRO_TEST_CHECK(painted.pixel(2, 20) == before.pixel(2, 20));
    ASTRO_TEST_CHECK(painted.pixel(after.width() - 2, 20) == after.pixel(after.width() - 2, 20));
    ASTRO_TEST_CHECK(controller.image() == after);
    // Export captions must use the same overlay as the preview, preserve all
    // unannotated 16-bit samples, and disappear without changing the result.
    auto exported = [&](const QString& name) {
        const QString destination = temp.filePath(name);
        controller.exportImage(QUrl::fromLocalFile(destination));
        until([&] { return !controller.busy(); });
        QFile file(destination); ASTRO_TEST_CHECK(file.open(QIODevice::ReadOnly));
        return file.readAll();
    };
    const auto plainTiff = exported("plain.tif");
    astro::app::ExportSizeEstimator sizeEstimate;
    sizeEstimate.setInput(controller.exportSizeInput());sizeEstimate.setActive(true);
    auto checkExportSize=[&](const QString& format,const QString& name) {
        sizeEstimate.setFormat(format);
        until([&]{return sizeEstimate.bytes()>=0 && !sizeEstimate.calculating();});
        ASTRO_TEST_CHECK(sizeEstimate.exact());
        ASTRO_TEST_CHECK(sizeEstimate.bytes()==QFileInfo(temp.filePath(name)).size());
    };
    checkExportSize("tif","plain.tif");
    exported("plain.png");
    checkExportSize("png","plain.png");
    QImageReader pngReader(temp.filePath("plain.png"));
    const auto pngImage=pngReader.read();
    const auto tiffImage=QImageReader(temp.filePath("plain.tif")).read();
    ASTRO_TEST_CHECK(!pngImage.isNull() && pngImage.depth()==64);
    auto samePixels=[](QImage a,QImage b) {
      a=a.convertToFormat(QImage::Format_RGBA64);b=b.convertToFormat(QImage::Format_RGBA64);
      if(a.size()!=b.size())return false;
      for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)
        if(reinterpret_cast<const QRgba64*>(a.constScanLine(y))[x]!=reinterpret_cast<const QRgba64*>(b.constScanLine(y))[x])return false;
      return true;
    };
    ASTRO_TEST_CHECK(samePixels(pngImage,tiffImage));
    exported("plain.jpg");
    checkExportSize("jpg","plain.jpg");
    ASTRO_TEST_CHECK(!QImageReader(temp.filePath("plain.jpg")).read().isNull());
    auto* captionTitle = root->findChild<QObject*>("annotationTitle");
    auto* captionEnabled = root->findChild<QObject*>("annotationEnabled");
    ASTRO_TEST_CHECK(captionTitle && captionEnabled);
    captionTitle->setProperty("text", "Titre ");
    ASTRO_TEST_CHECK(controller.exportAnnotation().value("title") == "Titre ");
    captionTitle->setProperty("text", "Titre accessible");
    captionEnabled->setProperty("checked", true);
    ASTRO_TEST_CHECK(controller.exportAnnotation().value("title") == "Titre accessible");
    ASTRO_TEST_CHECK(controller.exportAnnotation().value("enabled").toBool());
    ASTRO_TEST_CHECK(!controller.annotationImage().isNull());
    controller.setExportAnnotation({{"enabled", true}, {"technical", false}, {"title", "Lune"}, {"top", "30 mai · été"}, {"bottom", "NexImage 5"}});
    ASTRO_TEST_CHECK(!controller.annotationImage().isNull());
    root->setProperty("compareImages", false);
    { QPainter painter(&painted); paintItem->paint(&painter); }
    ASTRO_TEST_CHECK(painted != after && controller.image() == after);
    const auto captionedTiff = exported("caption.tif");
    exported("caption.png");
    sizeEstimate.setInput(controller.exportSizeInput());
    checkExportSize("png","caption.png");
    ASTRO_TEST_CHECK(samePixels(QImageReader(temp.filePath("caption.png")).read(),QImageReader(temp.filePath("caption.tif")).read()));
    ASTRO_TEST_CHECK(captionedTiff.size() == plainTiff.size() && captionedTiff != plainTiff);
    const auto u16 = [](const QByteArray& bytes, int offset) { return qFromLittleEndian<quint16>(bytes.constData() + offset); };
    const auto u32 = [](const QByteArray& bytes, int offset) { return qFromLittleEndian<quint32>(bytes.constData() + offset); };
    const int ifd = int(u32(plainTiff, 4));
    int pixels = 0;
    for (int i = 0; i < u16(plainTiff, ifd); ++i) {
        const int entry = ifd + 2 + i * 12;
        if (u16(plainTiff, entry) == 273) pixels = int(u32(plainTiff, entry + 8));
        if (u16(plainTiff, entry) == 258) ASTRO_TEST_CHECK(u16(captionedTiff, int(u32(captionedTiff, entry + 8))) == 16);
    }
    ASTRO_TEST_CHECK(pixels > 0);
    int untouched = 0, marked = 0;
    const auto overlay = controller.annotationImage();
    for (int y = 0; y < overlay.height(); ++y) for (int x = 0; x < overlay.width(); ++x) {
        if (qAlpha(overlay.pixel(x, y)) == 0) {
            const int offset = pixels + (y * overlay.width() + x) * 6;
            ASTRO_TEST_CHECK(plainTiff.mid(offset, 6) == captionedTiff.mid(offset, 6));
            ++untouched;
        } else ++marked;
    }
    ASTRO_TEST_CHECK(untouched > 0 && marked > 0);
    controller.setExportAnnotation({{"enabled", false}, {"top", "Lune"}});
    ASTRO_TEST_CHECK(exported("plain-again.tif") == plainTiff);
    { QPainter painter(&painted); paintItem->paint(&painter); }
    ASTRO_TEST_CHECK(painted == after);
    // Extended gains survive preset persistence and reach the processing engine.
    auto strong = edits->values(); strong["level1"] = 50.0;
    ASTRO_TEST_CHECK(library.saveProcessingPreset("Strong", strong, true));
    { astro::app::AppLibrary restored; ASTRO_TEST_CHECK(restored.processingPreset("Strong")["level1"].toDouble() == 50); }
    controller.apply({10, 0, 0, 0, 0, 0}, 0, 1, 1, 1, 1);
    until([&] { return !controller.busy(); });
    ASTRO_TEST_CHECK(controller.status().contains(QCoreApplication::translate("astro::app::ProcessingController","Adjustments applied. Compare before / after, then export.")));
    edits->undo(); until([&] { return !root->property("adjustmentsDirty").toBool() && !controller.busy(); });
    ASTRO_TEST_CHECK(controller.image() == before);
    edits->redo(); until([&] { return !root->property("adjustmentsDirty").toBool() && !controller.busy(); });
    ASTRO_TEST_CHECK(controller.image() == after);
    auto* viewport = root->findChild<QObject*>("processingViewport");
    ASTRO_TEST_CHECK(viewport);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(viewport, "actualSize"));
    ASTRO_TEST_CHECK(!viewport->property("fitMode").toBool() && viewport->property("effectiveZoom").toDouble() == 1.0);
    ASTRO_TEST_CHECK(std::abs(viewport->property("renderedWidth").toDouble() * viewport->property("pixelRatio").toDouble() - after.width()) < 0.01);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(viewport, "fit"));
    ASTRO_TEST_CHECK(viewport->property("fitMode").toBool());

    // Exercise the event handlers, not only the zoom helper functions.
    root->setProperty("compareImages", false);
    viewport->setProperty("sourceWidth", 2000);
    viewport->setProperty("sourceHeight", 1500);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(viewport, "actualSize"));
    auto* surface = viewport->findChild<QQuickItem*>("viewportSurface");
    ASTRO_TEST_CHECK(surface);
    const QPointF center = surface->mapToScene({surface->width() / 2, surface->height() / 2});
    QWheelEvent wheel(center, window.mapToGlobal(center.toPoint()), {}, {0, 120}, Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&window, &wheel);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(viewport->property("effectiveZoom").toDouble() > 1.1);
    ASTRO_TEST_CHECK(std::isfinite(viewport->property("panX").toDouble()));
    const auto mouse = [&](QEvent::Type type, QPointF point, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, window.mapToGlobal(point.toPoint()), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
        QCoreApplication::processEvents();
    };
    const double previousPan = viewport->property("panX").toDouble();
    mouse(QEvent::MouseButtonPress, center, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, center + QPointF(70, 20), Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, center + QPointF(70, 20), Qt::LeftButton, Qt::NoButton);
    ASTRO_TEST_CHECK(viewport->property("panX").toDouble() > previousPan + 50);
    auto* resize = root->findChild<QQuickItem*>("processingResizeHandle");
    ASTRO_TEST_CHECK(resize);
    const QPointF handle = resize->mapToScene({resize->width() / 2, resize->height() / 2});
    const int previousWidth = library.processingPanelWidth();
    mouse(QEvent::MouseButtonPress, handle, Qt::LeftButton, Qt::LeftButton);
    mouse(QEvent::MouseMove, handle - QPointF(50, 0), Qt::NoButton, Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, handle - QPointF(50, 0), Qt::LeftButton, Qt::NoButton);
    ASTRO_TEST_CHECK(library.processingPanelWidth() == previousWidth + 50);
    controller.seek(2); until([&] { return !controller.busy(); });
    ASTRO_TEST_CHECK(!controller.showingResult()); // Comparison never replaces the source video.
    controller.openPath(path); until([&] { return !controller.busy(); });
    ASTRO_TEST_CHECK(controller.exportAnnotation().value("top").toString().isEmpty());
    ASTRO_TEST_CHECK(controller.annotationImage().isNull());
    auto pixelsImage=std::make_shared<astro::processing::FloatImage>();
    *pixelsImage={96,64,std::vector<float>(96*64*3,.123456f)};
    const auto doc=astro::app::ImageDocument{pixelsImage,QImage(96,64,QImage::Format_RGB32),{{"title","Mosaïque lunaire"},{"enabled",true}},"Mosaic",QUrl::fromLocalFile(temp.filePath("mosaic-processed.tif")),{QFileInfo(path).canonicalFilePath()}};
    ASTRO_TEST_CHECK(controller.openDocument(QVariant::fromValue(doc)));
    ASTRO_TEST_CHECK(controller.stillImage() && !controller.loaded() && controller.hasResult() && !controller.ranked());
    ASTRO_TEST_CHECK(controller.stackSummary()==QCoreApplication::translate("astro::app::ProcessingController","Still image · no stacking needed"));
    ASTRO_TEST_CHECK(controller.exportAnnotation().value("title")=="Mosaïque lunaire");
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!cutoff->isVisible());
    controller.apply({0,0,0,0,0,0},0,1,1,1,1); until([&]{return !controller.busy();});
    controller.setExportAnnotation({{"enabled",false}});
    const auto stillTiff=exported("still.tif");
    const int stillIfd=int(u32(stillTiff,4)); int stillPixels=0;
    for(int i=0;i<u16(stillTiff,stillIfd);++i) { const int e=stillIfd+2+i*12; if(u16(stillTiff,e)==273)stillPixels=int(u32(stillTiff,e+8)); }
    ASTRO_TEST_CHECK(stillPixels>0 && u16(stillTiff,stillPixels)==std::lround(.123456f*65535));
    auto protectedDoc=doc;auto edgeMask=std::make_shared<std::vector<float>>(96*64,1.f);(*edgeMask)[0]=0;protectedDoc.edgeMask=edgeMask;
    protectedDoc.mosaicImageCount=3;
    ASTRO_TEST_CHECK(controller.openDocument(QVariant::fromValue(protectedDoc)));
    ASTRO_TEST_CHECK(controller.stackSummary()==QCoreApplication::translate("astro::app::ProcessingController","Mosaic · %1 images assembled").arg(3));
    controller.setExportAnnotation({{"enabled",false}});
    controller.apply({0,0,0,0,0,0},0,2,1,1,1);until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(controller.stackSummary()==QCoreApplication::translate("astro::app::ProcessingController","Mosaic · %1 images assembled").arg(3));
    const auto protectedTiff=exported("protected-edges.tif");
    ASTRO_TEST_CHECK(u16(protectedTiff,stillPixels)==std::lround(.123456f*65535));
    ASTRO_TEST_CHECK(u16(protectedTiff,stillPixels+6)==std::lround(.246912f*65535));
    ASTRO_TEST_CHECK(!controller.openDocument(QVariant{}));
    controller.openPath(path); until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(!controller.stillImage() && controller.loaded());
    ASTRO_TEST_CHECK(controller.stackSummary().isEmpty());
    // Direct imports bypass video analysis and retain 16-bit sample precision.
    controller.openPath(temp.filePath("still.tif")); until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(controller.stillImage() && controller.hasResult() && !controller.loaded());
    ASTRO_TEST_CHECK(controller.sourceName().contains("still.tif") && controller.frameCount()==0);
    ASTRO_TEST_CHECK(controller.imageLabel()==QCoreApplication::translate("astro::app::ProcessingController","Image · before adjustments"));
    QCoreApplication::processEvents();ASTRO_TEST_CHECK(controls->isVisible() && controls->width()>=260);
    const auto reopened=exported("reopened.tif");
    ASTRO_TEST_CHECK(u16(reopened,stillPixels)==std::lround(.123456f*65535));
    const auto originalBytes=stillTiff;
    controller.exportImage(QUrl::fromLocalFile(temp.filePath("still.tif")));
    ASTRO_TEST_CHECK(controller.status().contains(QCoreApplication::translate("astro::app::ProcessingController","An original image cannot be overwritten.")));
    QFile originalFile(temp.filePath("still.tif")); ASTRO_TEST_CHECK(originalFile.open(QIODevice::ReadOnly));
    ASTRO_TEST_CHECK(originalFile.readAll()==originalBytes);
    QImage png(96,64,QImage::Format_RGB32); png.fill(qRgb(91,102,113));
    const auto pngPath=temp.filePath("Lune été #1.PNG"); ASTRO_TEST_CHECK(png.save(pngPath));
    auto drop=[&](const QList<QUrl>& urls) {
        QMimeData mime; mime.setUrls(urls);
        QDragEnterEvent enter(QPoint(350,250),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&window,&enter);
        QDragMoveEvent move(QPoint(350,250),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&window,&move);
        QDropEvent event(QPointF(350,250),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(&window,&event); QCoreApplication::processEvents();
        return event.isAccepted();
    };
    ASTRO_TEST_CHECK(!drop({QUrl("https://example.org/image.png")}));
    ASTRO_TEST_CHECK(!drop({QUrl::fromLocalFile(pngPath),QUrl::fromLocalFile(temp.filePath("still.tif"))}));
    ASTRO_TEST_CHECK(drop({QUrl::fromLocalFile(pngPath)})); until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(controller.sourceName().contains("Lune été #1.PNG"));
    ASTRO_TEST_CHECK(!cutoff->isVisible());
    controller.apply({0,0,0,0,0,0},0,1,1,1,1); until([&]{return !controller.busy();});
    const auto pngTiff=exported("png-processed.tif");
    ASTRO_TEST_CHECK(u16(pngTiff,stillPixels)==91*257);
    QImage precisePng(96,64,QImage::Format_RGBA64);
    precisePng.fill(QColor::fromRgba64(12345,23456,34567,65535));
    const auto precisePath=temp.filePath("precise.png"); ASTRO_TEST_CHECK(precisePng.save(precisePath));
    controller.openPath(precisePath); until([&]{return !controller.busy();});
    const auto preciseTiff=exported("precise-processed.tif");
    ASTRO_TEST_CHECK(u16(preciseTiff,stillPixels)==12345 && u16(preciseTiff,stillPixels+2)==23456);
    const auto goodPreview=controller.image();
    QFile broken(temp.filePath("broken.png")); ASTRO_TEST_CHECK(broken.open(QIODevice::WriteOnly)); broken.write("invalid"); broken.close();
    controller.openPath(broken.fileName()); until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(controller.image()==goodPreview && controller.sourceName().contains("precise.png"));
    auto* caption=root->findChild<QObject*>("annotationPopup");ASTRO_TEST_CHECK(caption);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(caption,"open"));
    QElapsedTimer captionWait;captionWait.start();while(captionWait.elapsed()<200){QCoreApplication::processEvents();QThread::msleep(1);}
    auto* captionPreview=root->findChild<QQuickItem*>("captionPreview");ASTRO_TEST_CHECK(captionPreview && captionPreview->width()>100 && captionPreview->height()>100);
    auto* captionSize=caption->findChild<astro::app::ExportSizeEstimator*>();ASTRO_TEST_CHECK(captionSize);
    until([&]{return captionSize->bytes()>=0 && !captionSize->calculating();});
    if(qEnvironmentVariableIsSet("ASTRO_CAPTION_SNAPSHOT"))ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_CAPTION_SNAPSHOT")));
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(caption,"close"));
    controller.openPath(path); until([&]{return !controller.busy();});
    ASTRO_TEST_CHECK(!controller.stillImage() && controller.loaded() && !controller.hasResult());
    root->setParentItem(nullptr);
    // Load every guide topic at the minimum supported window size.
    QQmlComponent guideComponent(&engine,QUrl::fromLocalFile(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/GuideWorkspace.qml")));
    if(guideComponent.isError()) qWarning()<<guideComponent.errors();
    ASTRO_TEST_CHECK(guideComponent.isReady());
    std::unique_ptr<QObject> guideObject(guideComponent.create());
    auto* guide=qobject_cast<QQuickItem*>(guideObject.get()); ASTRO_TEST_CHECK(guide);
    guide->setParentItem(window.contentItem()); guide->setSize({720,480});
    for(int topic=0;topic<5;++topic) {
        guide->setProperty("topic",topic);
        QElapsedTimer layoutTimer;layoutTimer.start();
        while(layoutTimer.elapsed()<50){QCoreApplication::processEvents();QThread::msleep(1);}
        auto* scroll=guide->findChild<QQuickItem*>("guideLessonScroll");ASTRO_TEST_CHECK(scroll);
        ASTRO_TEST_CHECK(scroll->width()>0 && scroll->width()<=720 && scroll->height()>100);
        ASTRO_TEST_CHECK(scroll->property("contentHeight").toDouble()>scroll->height());
    }
    guide->setParentItem(nullptr);
}
