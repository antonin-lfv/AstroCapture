#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QSettings>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <astro/recording/AstroImage.hpp>
#include <cmath>
#include "DeepSkyController.hpp"
#include "DeepSkyImageItem.hpp"
#include "AppLibrary.hpp"
#include "Check.hpp"
using namespace astro;
void wait(app::DeepSkyController& controller){QElapsedTimer timer;timer.start();while(controller.busy()&&timer.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(1);}ASTRO_TEST_CHECK(!controller.busy());}
int main(int argc,char** argv){
    QGuiApplication application(argc,argv);QTemporaryDir directory;
    QTranslator translator;if(qEnvironmentVariableIsSet("ASTRO_UI_TRANSLATION_PATH") && translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))application.installTranslator(&translator);
    QCoreApplication::setOrganizationName("AstroCaptureTest");QCoreApplication::setApplicationName("DeepSkyWorkspaceTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    qmlRegisterType<app::DeepSkyController>("AstroCapture",1,0,"DeepSkyController");qmlRegisterType<app::DeepSkyHistogramItem>("AstroCapture",1,0,"DeepSkyHistogramItem");qmlRegisterType<app::DeepSkyImageItem>("AstroCapture",1,0,"DeepSkyImageItem");qmlRegisterType<app::AppLibrary>("AstroCapture",1,0,"AppLibrary");
    std::unique_ptr<app::DeepSkyController> screenshotController;
    app::DeepSkyController controller;app::AppLibrary library;QQmlEngine engine;QQmlComponent component(&engine);
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/DeepSkyWorkspace.qml"));ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    component.setData("import AstroCapture 1.0\n"+source.readAll(),QUrl::fromLocalFile(source.fileName()));
    if(component.isError())qWarning()<<component.errors();ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"controller",QVariant::fromValue(&controller)}}));ASTRO_TEST_CHECK(object);
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyCalibrated")->property("checked").toBool());
    QQuickWindow window;window.resize(1100,720);auto* root=qobject_cast<QQuickItem*>(object.get());root->setParentItem(window.contentItem());root->setSize({1100,720});window.show();
    recording::AstroImage data;data.width=160;data.height=128;data.exposure=60;data.filter="Red";data.samples.resize(160*128);
    const std::array<std::array<int,2>,10> stars{{{20,20},{60,16},{105,25},{140,40},{35,60},{80,55},{120,75},{25,105},{75,110},{135,105}}};
    QList<QUrl> urls;
    for(int n=0;n<3;++n){for(int y=0;y<128;++y)for(int x=0;x<160;++x){double value=.02;for(const auto& star:stars)value+=.2*std::exp(-((x-star[0]-n)*(x-star[0]-n)+(y-star[1])*(y-star[1]))/5.);data.samples[y*160+x]=float(value);}const auto path=directory.filePath(QString("light-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),data);urls<<QUrl::fromLocalFile(path);}
    controller.addFiles(urls);wait(controller);ASTRO_TEST_CHECK(controller.frames().size()==3&&controller.frames()[0].toMap()["group"]=="R");
    ASTRO_TEST_CHECK(controller.frames()[0].toMap()["stars"].toInt()>=8&&controller.imageWidth()==160);
    controller.addFiles(urls);wait(controller);ASTRO_TEST_CHECK(controller.frames().size()==3);
    controller.setSelected(2,false);controller.integrate(true);wait(controller);
    if(!controller.hasResult())qWarning()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult()&&controller.displayingResult()&&controller.groups()==QStringList{"R"});
    ASTRO_TEST_CHECK(controller.hasRawComparison());
    const auto handed=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(handed.exposureCount==2&&handed.totalExposure==120&&handed.filters==QStringList{"R"}&&!handed.processingSummary.isEmpty());
    const auto integratedPixels=handed.pixels;
    const auto integratedPreview=controller.image();
    controller.showRawComparison(true);ASTRO_TEST_CHECK(controller.comparingRaw()&&controller.imageWidth()==160&&controller.processingDocument().value<app::ImageDocument>().pixels==integratedPixels);
    controller.showRawComparison(false);ASTRO_TEST_CHECK(!controller.comparingRaw()&&controller.image()==integratedPreview);
    controller.chooseRawComparison(1);wait(controller);ASTRO_TEST_CHECK(controller.rawComparisonName()=="light-1.fits");
    controller.showRawComparison(true);controller.restoreResult();ASTRO_TEST_CHECK(!controller.comparingRaw()&&controller.image()==integratedPreview);
    const auto output=QUrl::fromLocalFile(directory.filePath("linear.fits"));controller.exportImage(output,true);wait(controller);
    auto result=recording::readAstroImage(output.toLocalFile().toStdString());ASTRO_TEST_CHECK(result.exposure==120&&result.channels==3&&result.width<160);
    auto stretch=controller.suggestedStretch();stretch["midtones"]=.1;stretch["background"]=false;controller.apply(stretch);wait(controller);
    ASTRO_TEST_CHECK(controller.linearHistogram()["channels"].toList().size()==3);
    auto pending=stretch;pending["midtones"]=.3;controller.apply(pending);ASTRO_TEST_CHECK(controller.adjusting());pending["midtones"]=.7;controller.apply(pending);wait(controller);
    processing::Finish expectedSettings;expectedSettings.black=stretch["black"].toFloat();expectedSettings.white=stretch["white"].toFloat();expectedSettings.midtones=.7;
    expectedSettings.color={stretch["red"].toFloat(),stretch["green"].toFloat(),stretch["blue"].toFloat()};
    const auto expected=processing::finishDeepSky({result.width,result.height,result.samples},expectedSettings);
    const auto last=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(std::abs(last.pixels->rgb[0]-expected.rgb[0])<1e-6);
    const auto document=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(document.pixels&&document.preview.size()==QSize(document.pixels->width,document.pixels->height));
    const auto tiff=QUrl::fromLocalFile(directory.filePath("finished.tif"));controller.exportImage(tiff,false);wait(controller);ASTRO_TEST_CHECK(QFileInfo::exists(tiff.toLocalFile()));
    controller.inspect(0);wait(controller);ASTRO_TEST_CHECK(!controller.displayingResult());controller.restoreResult();ASTRO_TEST_CHECK(controller.displayingResult()&&controller.image().size()==document.preview.size());const auto handoff=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(handoff.preview.size()==QSize(handoff.pixels->width,handoff.pixels->height));
    QCoreApplication::processEvents();if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH"))window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH"));
    // Persist custom controls, restore them, and verify the recreated pixels,
    // including with live preview disabled (restoration must still apply).
    ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyLivePreview")->setProperty("checked",false));
    QVariant customFinish;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"options",Q_RETURN_ARG(QVariant,customFinish)));
    auto finish=customFinish.toMap();
    finish["black"]=.01;finish["white"]=.6;finish["midtones"]=.37;
    finish["red"]=1.2;finish["green"]=.9;finish["blue"]=1.1;
    finish["background"]=false;finish["denoise"]=.4;
    finish["asinh"]=true;finish["asinhStrength"]=12.;finish["saturation"]=1.3;
    controller.setProjectOptions({{"calibrated",true},{"finish",finish}});
    controller.apply(finish);wait(controller);
    const auto savedPixels=controller.processingDocument().value<app::ImageDocument>().pixels;
    const auto project=QUrl::fromLocalFile(directory.filePath("session.astroproject"));controller.saveProject(project);controller.clear();root->setProperty("workflowStep",0);controller.loadProject(project);ASTRO_TEST_CHECK(controller.frames().size()==3&&!controller.frames()[2].toMap()["selected"].toBool()&&!controller.hasResult());
    controller.integrate(true);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());
    ASTRO_TEST_CHECK(root->property("workflowStep").toInt()==6);
    QVariant restoredFinish;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"options",Q_RETURN_ARG(QVariant,restoredFinish)));
    const auto restored=restoredFinish.toMap();
    for(auto it=finish.cbegin();it!=finish.cend();++it) {
        if(it.value().typeId()==QMetaType::Bool)ASTRO_TEST_CHECK(restored[it.key()].toBool()==it.value().toBool());
        else ASTRO_TEST_CHECK(std::abs(restored[it.key()].toDouble()-it.value().toDouble())<1e-5);
    }
    const auto recreated=controller.processingDocument().value<app::ImageDocument>().pixels;
    ASTRO_TEST_CHECK(recreated->rgb.size()==savedPixels->rgb.size());
    for(std::size_t i=0;i<recreated->rgb.size();++i)ASTRO_TEST_CHECK(std::abs(recreated->rgb[i]-savedPixels->rgb[i])<1e-6);
    ASTRO_TEST_CHECK(!root->property("pendingProjectFinish").isValid() || root->property("pendingProjectFinish").isNull());
    const auto previous=controller.processingDocument().value<app::ImageDocument>().pixels;
    controller.integrate(true);controller.cancel();wait(controller);ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels==previous);
    QFile original(urls[0].toLocalFile());ASTRO_TEST_CHECK(original.open(QIODevice::ReadOnly));const auto before=original.readAll();original.close();controller.exportImage(urls[0],true);ASTRO_TEST_CHECK(!controller.busy());ASTRO_TEST_CHECK(original.open(QIODevice::ReadOnly));ASTRO_TEST_CHECK(original.readAll()==before);
    app::DeepSkyController guarded;guarded.addFiles({urls[0]});wait(guarded);guarded.addFiles({urls[1]},"Dark");wait(guarded);guarded.integrate(true);wait(guarded);ASTRO_TEST_CHECK(!guarded.hasResult()&&guarded.status().contains("Calibration files are selected"));
    const auto folder=directory.filePath("structured");
    for(const auto& name:QStringList{"Brutes_180s","Darks_180s","Flats_3s","Offsets_3s"})ASTRO_TEST_CHECK(QDir().mkpath(folder+"/"+name));
    data.exposure=180;recording::writeAstroFits((folder+"/Brutes_180s/light.fit").toStdString(),data);recording::writeAstroFits((folder+"/Darks_180s/dark.fit").toStdString(),data);
    data.exposure=3;recording::writeAstroFits((folder+"/Flats_3s/flat.fit").toStdString(),data);recording::writeAstroFits((folder+"/Offsets_3s/bias.fit").toStdString(),data);recording::writeAstroFits((folder+"/resultat.fit").toStdString(),data);
    app::DeepSkyController imported;imported.addFolder(QUrl::fromLocalFile(folder));wait(imported);ASTRO_TEST_CHECK(imported.frames().size()==4);
    QStringList importedRoles;for(const auto& value:imported.frames())importedRoles<<value.toMap()["role"].toString();for(const auto& role:QStringList{"Light","Dark","Flat","DarkFlat"})ASTRO_TEST_CHECK(importedRoles.contains(role));
    imported.setRole(0,"Bias");imported.setGroup(0,"L");ASTRO_TEST_CHECK(imported.frames()[0].toMap()["role"]=="Bias"&&imported.frames()[0].toMap()["group"]=="L");
    if(qEnvironmentVariableIsSet("ASTRO_UI_FITS_PATH")) {
        screenshotController=std::make_unique<app::DeepSkyController>();root->setProperty("controller",QVariant::fromValue(screenshotController.get()));
        screenshotController->addFiles({QUrl::fromLocalFile(qEnvironmentVariable("ASTRO_UI_FITS_PATH"))});wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->frames().size()==1);
        screenshotController->integrate(true);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->hasResult());
        auto settings=screenshotController->suggestedStretch();
        screenshotController->apply(settings);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->processingDocument().value<app::ImageDocument>().pixels);
        root->setProperty("workflowStep",6);QCoreApplication::processEvents();QThread::msleep(50);QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH"))ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")));
    }
    std::cout<<"OK\n";
}
