#include "AppLibrary.hpp"
#include "DeepSkyController.hpp"
#include "DeepSkyEdits.hpp"
#include "DeepSkyImageItem.hpp"
#include "ExportSizeEstimator.hpp"
#include "HistogramItem.hpp"
#include "ImagePixelsItem.hpp"
#include "LivePreviewItem.hpp"
#include "MaskOverlayItem.hpp"
#include "MosaicController.hpp"
#include "MosaicImageItem.hpp"
#include "ObservationController.hpp"
#include "ObservationPlot.hpp"
#include "PreviewController.hpp"
#include "ProcessingEdits.hpp"
#include "ProcessingImageItem.hpp"
#include "QuitGuard.hpp"
#include "SessionController.hpp"
#include "ZoomLoupeItem.hpp"
#include "Check.hpp"
#include <astro/recording/AstroImage.hpp>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTranslator>
#include <algorithm>
#include <cstring>
#include <iostream>
using namespace astro;
using namespace astro::app;

static QByteArray bytes(const QString& path) {
    QFile f(path); ASTRO_TEST_CHECK(f.open(QIODevice::ReadOnly)); return f.readAll();
}
static void write(const QString& path,const QByteArray& data) {
    QFile f(path); ASTRO_TEST_CHECK(f.open(QIODevice::WriteOnly));
    ASTRO_TEST_CHECK(f.write(data)==data.size());
}
static QString hash(const processing::FloatImage& image) {
    return QString::fromLatin1(QCryptographicHash::hash(
        QByteArrayView(reinterpret_cast<const char*>(image.rgb.data()),image.rgb.size()*4),
        QCryptographicHash::Sha256).toHex());
}
template<class Predicate> static void until(Predicate ready) {
    QElapsedTimer timer;timer.start();
    while(!ready()&&timer.elapsed()<45000){QCoreApplication::processEvents();QThread::msleep(2);}
    ASTRO_TEST_CHECK(ready());QCoreApplication::processEvents();
}
template<class Controller> static void wait(Controller* controller) {
    until([&]{return !controller->busy();});
}
static QVariant evaluate(QObject* scope,const QString& expression) {
    QQmlExpression e(qmlContext(scope),scope,expression);auto value=e.evaluate();
    if(e.hasError())qWarning()<<e.error();ASTRO_TEST_CHECK(!e.hasError());return value;
}
static void click(QObject* scope,const char* name) {
    auto* control=scope->findChild<QObject*>(name);ASTRO_TEST_CHECK(control);
    ASTRO_TEST_CHECK(control->property("enabled").toBool());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(control,"clicked"));QCoreApplication::processEvents();
}
static void registerTypes() {
#define REGISTER(Type) qmlRegisterType<Type>("AstroCapture",1,0,#Type)
    REGISTER(AppLibrary);REGISTER(DeepSkyController);REGISTER(DeepSkyEdits);
    REGISTER(DeepSkyImageItem);REGISTER(DeepSkyHistogramItem);REGISTER(ExportSizeEstimator);
    REGISTER(HistogramItem);REGISTER(ImagePixelsItem);REGISTER(LivePreviewItem);
    REGISTER(MaskOverlayItem);REGISTER(MosaicController);REGISTER(MosaicImageItem);
    REGISTER(PreviewController);REGISTER(ProcessingController);REGISTER(ProcessingEdits);
    REGISTER(ProcessingImageItem);REGISTER(SessionController);REGISTER(ZoomLoupeItem);
    REGISTER(ObservationController);REGISTER(ObservationPlot);
#undef REGISTER
}
static recording::AstroImage sourceImage() {
    recording::AstroImage image;
    const auto path=qEnvironmentVariable("ASTRO_PERSONAL_FITS_PATH");
    if(!path.isEmpty()) {
        const auto original=recording::readAstroImage(path.toStdString());
        ASTRO_TEST_CHECK(original.channels==3&&original.width>=1024&&original.height>=768);
        image=original;image.width=1024;image.height=768;image.samples.resize(1024*768*3);
        const int ox=(original.width-1024)/2,oy=(original.height-768)/2;
        for(int y=0;y<768;++y)std::copy_n(original.samples.begin()+((y+oy)*original.width+ox)*3,
            1024*3,image.samples.begin()+y*1024*3);
        image.wcs.reset();
    } else {
        image.width=128;image.height=96;image.channels=3;image.bits=32;image.exposure=30;
        image.samples.resize(128*96*3);
        for(std::size_t i=0;i<image.samples.size();++i)image.samples[i]=float(i%127)/400.f-.015f;
    }
    return image;
}
static void capture(QQuickWindow* window,const QString& name) {
    const auto directory=qEnvironmentVariable("ASTRO_PERSONAL_QA_DIR");if(directory.isEmpty())return;
    ASTRO_TEST_CHECK(QDir().mkpath(directory));
    for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(30);}
    ASTRO_TEST_CHECK(window->grabWindow().save(directory+'/'+name+".png"));
}
static int workspace(const QString& directory,bool restore,bool faults=false) {
    QCoreApplication::setOrganizationName("AstroCaptureTests");
    QCoreApplication::setApplicationName("PersonalUse");
    QCoreApplication::setApplicationVersion(ASTRO_APP_VERSION);
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory);
    QSettings().setValue("calibration/libraryDirectory",directory+"/masters");
    QSettings().setValue("application/integrationTemporaryDirectory",directory);
    QTranslator translator;ASTRO_TEST_CHECK(translator.load(ASTRO_TRANSLATION_PATH));
    QCoreApplication::installTranslator(&translator);
    if(qEnvironmentVariableIsSet("ASTRO_OBSERVATION_WEATHER_WINDOW")) {
        QSettings().setValue("observation/weatherCache",QVariantMap{{"data",bytes(ASTRO_SOURCE_DIR "/tests/unit/app/observation-weather.json")},{"latitude",48.8566},{"longitude",2.3522},{"receivedUtc",QDateTime::currentSecsSinceEpoch()}});
    }
    registerTypes();QQmlEngine engine;
    engine.rootContext()->setContextProperty("validationDirectory",directory);
    const QDir sourceDirectory(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml"));
    const auto qmlDirectory=directory+"/qml";ASTRO_TEST_CHECK(QDir().mkpath(qmlDirectory));
    // The executable module normally supplies an implicit self-import. Make
    // that import explicit when loading these same files from the filesystem.
    for(const auto& name:sourceDirectory.entryList({"*.qml","*.js"},QDir::Files)) {
        auto content=bytes(sourceDirectory.filePath(name));
        if(name.endsWith(".qml"))content="import AstroCapture 1.0\n"+content;
        write(qmlDirectory+'/'+name,content);
    }
    ASTRO_TEST_CHECK(QDir().mkpath(qmlDirectory+"/assets"));
    write(qmlDirectory+"/assets/logo.png",bytes(sourceDirectory.filePath("assets/logo.png")));
    auto qml=bytes(qmlDirectory+"/Main.qml");
    // Only storage destinations are redirected; test the delivered full window.
    qml.replace("id:sessionController;objectName:",
        "storageDirectory:validationDirectory+'/recovery';id:sessionController;objectName:");
    qml.replace("id: deepSkyController;objectName:",
        "projectCacheDirectory:validationDirectory+'/projects';id: deepSkyController;objectName:");
    QQmlComponent component(&engine);component.setData(qml,QUrl::fromLocalFile(qmlDirectory+"/Main.qml"));
    if(component.isError())qWarning()<<component.errors();ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.create());
    if(!object)qWarning()<<component.errors();ASTRO_TEST_CHECK(object);
    auto* window=qobject_cast<QQuickWindow*>(object.get());ASTRO_TEST_CHECK(window);
    auto* deep=window->findChild<DeepSkyController*>("deepSkyController");
    auto* processing=window->findChild<ProcessingController*>("processingController");
    auto* caption=window->findChild<ProcessingController*>("captionController");
    auto* session=window->findChild<SessionController*>("sessionController");
    auto* library=window->findChild<AppLibrary*>("appLibrary");
    ASTRO_TEST_CHECK(deep&&processing&&caption&&session&&library);
    ASTRO_TEST_CHECK(session->storageDirectory()==directory+"/recovery");
    ASTRO_TEST_CHECK(deep->projectCacheDirectory()==directory+"/projects");
    QuitGuard guard(window);QCoreApplication::instance()->installEventFilter(&guard);
    auto* quit=window->findChild<QObject*>("quitConfirmation");ASTRO_TEST_CHECK(quit);

    if(qEnvironmentVariableIsSet("ASTRO_OBSERVATION_WEATHER_WINDOW")) {
        auto* observation=window->findChild<ObservationController*>("observationController");ASTRO_TEST_CHECK(observation);
        auto settings=observation->sites()[0].toMap();settings["date"]="2026-10-12";settings["minimum"]=30;settings["sunLimit"]=-18;ASTRO_TEST_CHECK(observation->calculate(settings));
        ASTRO_TEST_CHECK(observation->selectTarget("NGC1976"));observation->setFraction(.5);
        window->setProperty("currentTab","observation");
        auto* workspace=window->findChild<QObject*>("observationWorkspace");ASTRO_TEST_CHECK(workspace);workspace->setProperty("viewMode","clouds");
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-clouds-%1").arg(size.width()));
        }
        workspace->setProperty("viewMode","suggestions");capture(window,"observation-clouds-suggestions-900");
        settings=observation->settings();settings["longitude"]=-71.25;settings["latitude"]=-29.9;settings["name"]="La Serena";ASTRO_TEST_CHECK(observation->calculate(settings));
        ASTRO_TEST_CHECK(!observation->weather()["available"].toBool());workspace->setProperty("viewMode","clouds");capture(window,"observation-clouds-other-place-900");
        settings=observation->sites()[0].toMap();settings["date"]="2026-12-12";settings["minimum"]=30;settings["sunLimit"]=-18;ASTRO_TEST_CHECK(observation->calculate(settings));
        capture(window,"observation-clouds-unavailable-date-900");
        window->setProperty("quitApproved",true);return 0;
    }

    if(qEnvironmentVariableIsSet("ASTRO_OBSERVATION_WINDOW")) {
        auto* observation=window->findChild<ObservationController*>("observationController");ASTRO_TEST_CHECK(observation);
        auto settings=observation->settings();settings["date"]="2026-12-12";ASTRO_TEST_CHECK(observation->calculate(settings));
        ASTRO_TEST_CHECK(observation->selectTarget("NGC1976"));
        window->setProperty("currentTab","observation");
        auto* workspace=window->findChild<QObject*>("observationWorkspace");ASTRO_TEST_CHECK(workspace);
        ASTRO_TEST_CHECK(workspace->property("viewMode")=="prepare");
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-welcome-%1").arg(size.width()));
        }
        workspace->setProperty("viewMode","night");
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-night-%1").arg(size.width()));
        }
        if(!observation->targetFavorite())observation->toggleFavoriteTarget();ASTRO_TEST_CHECK(observation->saveSite("Mon jardin · Paris"));
        workspace->setProperty("viewMode","suggestions");
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-suggestions-%1").arg(size.width()));
        }
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(observation,"updateReachability",Qt::DirectConnection,Q_ARG(int,4)));
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(observation,"updateReachability",Qt::DirectConnection,Q_ARG(int,1)));
        capture(window,"observation-connection-lost-900");
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(observation,"updateReachability",Qt::DirectConnection,Q_ARG(int,4)));
        workspace->setProperty("viewMode","world");
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-world-%1").arg(size.width()));
        }
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(workspace,"openObjectChooser"));
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("observation-object-chooser-%1").arg(size.width()));
        }
        auto* chooser=workspace->findChild<QObject*>("observationObjectChooser");ASTRO_TEST_CHECK(chooser);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(chooser,"close"));
        auto* date=workspace->findChild<QObject*>("observationDate");ASTRO_TEST_CHECK(date);date->setProperty("text",QDate::currentDate().addDays(-7).toString(Qt::ISODate));
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(workspace,"calculate"));capture(window,"observation-past-night-900");
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(workspace,"currentNight"));
        settings=observation->settings();settings["date"]="2026-12-23";ASTRO_TEST_CHECK(observation->calculate(settings));
        ASTRO_TEST_CHECK(observation->selectTarget("moon"));observation->setFraction(.6);workspace->setProperty("viewMode","sky");capture(window,"observation-moon-horizon-900");
        window->setProperty("quitApproved",true);return 0;
    }

    if(restore) {
        ASTRO_TEST_CHECK(session->recoveryAvailable()&&!deep->hasResult());
        click(window,"homeRestoreSession");wait(session);wait(deep);wait(processing);
        ASTRO_TEST_CHECK(deep->hasResult()&&processing->hasResult());
        const auto expected=QJsonDocument::fromJson(bytes(directory+"/expected.json")).object();
        ASTRO_TEST_CHECK(hash(*deep->sessionSnapshot().images["linear"])==expected["linear"].toString());
        ASTRO_TEST_CHECK(hash(*deep->sessionSnapshot().images["finished"])==expected["finished"].toString());
        ASTRO_TEST_CHECK(hash(*processing->sessionSnapshot().images["result"])==expected["processing"].toString());
        ASTRO_TEST_CHECK(processing->captionTechnicalText()=="Métadonnées corrigées · contrôle de reprise");
        ASTRO_TEST_CHECK(window->property("currentTab").toString()=="deepSky");
        deep->exportImage(QUrl::fromLocalFile(directory+"/restored.fits"),true);wait(deep);
        ASTRO_TEST_CHECK(recording::readAstroImage((directory+"/restored.fits").toStdString()).samples==deep->sessionSnapshot().images["linear"]->rgb);
        capture(window,"restored-window");
        window->close();ASTRO_TEST_CHECK(window->isVisible()&&quit->property("visible").toBool());
        click(quit,"quitCancel");ASTRO_TEST_CHECK(window->isVisible());
        if(faults) {
            const auto volume=qEnvironmentVariable("ASTRO_PERSONAL_FAULT_DIRECTORY");
            ASTRO_TEST_CHECK(!volume.isEmpty());
            const auto oldManifest=bytes(volume+"/recovery/session.json");
            const auto oldPng=bytes(volume+"/preserve.png");
            const auto oldFits=bytes(volume+"/preserve.fits");
            const auto oldProject=bytes(volume+"/preserve.astroproject");
            int exported=0,projects=0;
            QObject::connect(processing,&ProcessingController::imageExported,[&]{++exported;});
            QObject::connect(deep,&DeepSkyController::imageExported,[&]{++exported;});
            QObject::connect(deep,&DeepSkyController::projectSaved,[&]{++projects;});
            processing->exportImage(QUrl::fromLocalFile(volume+"/preserve.png"));wait(processing);
            deep->exportImage(QUrl::fromLocalFile(volume+"/preserve.fits"),true);wait(deep);
            deep->savePortableProject(QUrl::fromLocalFile(volume+"/preserve.astroproject"));wait(deep);
            ASTRO_TEST_CHECK(exported==0&&projects==0);
            ASTRO_TEST_CHECK(bytes(volume+"/preserve.png")==oldPng&&bytes(volume+"/preserve.fits")==oldFits);
            ASTRO_TEST_CHECK(bytes(volume+"/preserve.astroproject")==oldProject);
            session->setStorageDirectory(volume+"/recovery");
            auto annotation=processing->exportAnnotation();annotation["title"]="Échec disque contrôlé";
            processing->setExportAnnotation(annotation);
            bool succeeded=true;QObject::connect(session,&SessionController::saveFinished,[&](bool ok){succeeded=ok;});
            window->close();click(quit,"quitSaveSession");wait(session);
            ASTRO_TEST_CHECK(!succeeded&&window->isVisible()&&!window->property("quitApproved").toBool());
            ASTRO_TEST_CHECK(quit->property("visible").toBool()&&!quit->property("saveStatus").toString().isEmpty());
            ASTRO_TEST_CHECK(bytes(volume+"/recovery/session.json")==oldManifest);
            ASTRO_TEST_CHECK(hash(*deep->sessionSnapshot().images["linear"])==expected["linear"].toString());
            capture(window,"disk-full-quit-preserved");click(quit,"quitCancel");
            std::cout<<"Full filesystem: existing PNG/FITS/project and recovery manifest preserved; failed save does not quit\n";
        }
        std::cout<<"Cross-process recovery: float pixels, edited caption, active workspace and cached FITS export passed\n";
    } else {
        auto* preview=window->findChild<PreviewController*>();ASTRO_TEST_CHECK(preview);
        preview->setSaveDirectory(directory);preview->setVideoFormat("SER");
        preview->connectMock();ASTRO_TEST_CHECK(preview->connected());
        until([&]{return preview->frameCount()>=3;});window->setProperty("currentTab","capture");
        preview->setRecordingTargetFrameCount(0);preview->startRecording();
        until([&]{return preview->recordingFrameCount()>=8;});
        window->close();ASTRO_TEST_CHECK(window->isVisible()&&quit->property("visible").toBool());
        ASTRO_TEST_CHECK(!quit->findChild<QObject*>("quitDiscard")->property("enabled").toBool());
        ASTRO_TEST_CHECK(!quit->findChild<QObject*>("quitSaveSession")->property("enabled").toBool());
        evaluate(window,"navigate('deepSky')");ASTRO_TEST_CHECK(window->property("currentTab").toString()=="capture");
        click(quit,"quitCancel");preview->stopRecording();const auto video=preview->recordingPath();
        ASTRO_TEST_CHECK(preview->recordingDroppedFrames()==0);preview->disconnectCamera();
        processing->openPath(video);wait(processing);processing->analyze();wait(processing);
        const auto sourcePreview=processing->image();
        processing->stack(processing->frameCount());wait(processing);
        // Mock color bars have no reliable planetary landmarks. Rejecting their
        // alignment is correct; importing one frame still exercises SER pixels.
        ASTRO_TEST_CHECK(!processing->hasResult()&&processing->image()==sourcePreview);
        processing->stack(1);wait(processing);
        ASTRO_TEST_CHECK(processing->hasResult());
        const auto image=sourceImage();const auto source=directory+"/source.fits";
        recording::writeAstroFits(source.toStdString(),image);const auto original=bytes(source);
        window->setProperty("currentTab","deepSky");deep->addFiles({QUrl::fromLocalFile(source)});wait(deep);
        deep->integrate(true,0,false,false);wait(deep);ASTRO_TEST_CHECK(deep->hasResult());
        const auto linear=deep->sessionSnapshot().images["linear"];
        ASTRO_TEST_CHECK(linear->rgb==image.samples);
        evaluate(window,"deepSkyWorkspace.setFinish(Object.assign({},deepSkyWorkspace.options(),{stretch:true,black:0,white:1,midtones:0.22,saturation:1.15}))");
        until([&]{return !deep->busy()&&!evaluate(window,"deepSkyWorkspace.finishDirty").toBool();});
        const auto finished=deep->sessionSnapshot().images["finished"];
        ASTRO_TEST_CHECK(finished->rgb!=linear->rgb);
        deep->exportImage(QUrl::fromLocalFile(directory+"/linear.fits"),true);wait(deep);
        ASTRO_TEST_CHECK(recording::readAstroImage((directory+"/linear.fits").toStdString()).samples==linear->rgb);
        ASTRO_TEST_CHECK(processing->openDocument(deep->processingDocument()));
        auto annotation=processing->exportAnnotation();annotation["enabled"]=true;
        annotation["title"]="Orion · validation personnelle";
        annotation["technicalText"]="Métadonnées corrigées · contrôle de reprise";
        processing->setExportAnnotation(annotation);
        for(const auto* ext:{"png","tif","jpg"}) {
            const auto destination=directory+"/caption."+ext;
            processing->exportImage(QUrl::fromLocalFile(destination));ASTRO_TEST_CHECK(processing->exporting());wait(processing);
            ASTRO_TEST_CHECK(!QImageReader(destination).read().isNull());
        }
        const auto png=QImageReader(directory+"/caption.png").read();
        const auto tiff=QImageReader(directory+"/caption.tif").read();
        ASTRO_TEST_CHECK(png.depth()==64&&tiff.depth()==64&&png.size()==tiff.size());
        const auto png64=png.convertToFormat(QImage::Format_RGBA64),tiff64=tiff.convertToFormat(QImage::Format_RGBA64);
        for(int y=0;y<png.height();++y)ASTRO_TEST_CHECK(std::memcmp(png64.constScanLine(y),tiff64.constScanLine(y),png.width()*8)==0);
        ASTRO_TEST_CHECK(library->images().size()==3);
        const auto unchanged=processing->sessionSnapshot().images["result"];
        write(directory+"/preserve.png","existing destination");
        processing->exportImage(QUrl::fromLocalFile(directory+"/preserve.png"));processing->cancel();wait(processing);
        ASTRO_TEST_CHECK(bytes(directory+"/preserve.png")=="existing destination");
        processing->exportImage(QUrl::fromLocalFile(directory+"/absent/result.png"));wait(processing);
        ASTRO_TEST_CHECK(processing->sessionSnapshot().images["result"]==unchanged);
        deep->integrate(true,3,false,false);deep->cancel();wait(deep);
        ASTRO_TEST_CHECK(deep->sessionSnapshot().images["linear"]==linear);
        evaluate(window,"deepSkyWorkspace.saveOptions()");
        deep->savePortableProject(QUrl::fromLocalFile(directory+"/portable.astroproject"));wait(deep);
        ASTRO_TEST_CHECK(QFileInfo::exists(directory+"/portable.astroproject"));
        write(directory+"/expected.json",QJsonDocument(QJsonObject{{"linear",hash(*linear)},
            {"finished",hash(*finished)},{"processing",hash(*unchanged)}}).toJson());
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window->resize(size);capture(window,QString("window-%1x%2").arg(size.width()).arg(size.height()));
            window->close();ASTRO_TEST_CHECK(window->isVisible()&&quit->property("visible").toBool());
            capture(window,QString("quit-%1x%2").arg(size.width()).arg(size.height()));
            click(quit,"quitCancel");ASTRO_TEST_CHECK(window->isVisible());
        }
        ASTRO_TEST_CHECK(bytes(source)==original);
        const auto targets=quit->property("exportTargets").toList();
        ASTRO_TEST_CHECK(targets.size()==2&&targets[0].toMap()["name"]=="Ciel profond"
            &&targets[1].toMap()["name"]=="Traitement");
        window->close();click(quit,"quitSaveSession");wait(session);
        ASTRO_TEST_CHECK(!window->isVisible()&&QFileInfo::exists(directory+"/recovery/session.json"));
        std::cout<<"Full window: integration, finishing, lossless caption exports, cancellation, portable project and save-and-quit passed\n";
    }
    window->setProperty("quitApproved",true);window->close();
    QCoreApplication::instance()->removeEventFilter(&guard);return 0;
}
int main(int argc,char** argv) {
    QGuiApplication application(argc,argv);application.setQuitOnLastWindowClosed(false);
    const auto args=application.arguments();
    if(args.size()==2&&args[1]=="--paths") {
        application.setOrganizationName("AstroCapture");application.setApplicationName("AstroCapture");
        std::cout<<QSettings().fileName().toStdString()<<'\n'
                 <<QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()<<'\n'
                 <<QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString()<<'\n';
        return 0;
    }
    if(args.size()==3)return workspace(args[2],args[1]!="--save",args[1]=="--faults");
    QTemporaryDir directory;ASTRO_TEST_CHECK(directory.isValid());
    for(const auto* stage:{"--save","--restore"}) {
        QProcess child;child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(application.applicationFilePath(),{stage,directory.path()});
        ASTRO_TEST_CHECK(child.waitForStarted());
        ASTRO_TEST_CHECK(child.waitForFinished(100000));
        std::cout<<child.readAll().toStdString();
        ASTRO_TEST_CHECK(child.exitStatus()==QProcess::NormalExit&&child.exitCode()==0);
    }
}
