#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QSettings>
#include <QTranslator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <astro/recording/AstroImage.hpp>
#include <cmath>
#include "DeepSkyController.hpp"
#include "DeepSkyEdits.hpp"
#include "DeepSkyImageItem.hpp"
#include "AppLibrary.hpp"
#include "Check.hpp"
using namespace astro;
QQuickItem* visualItem(QQuickItem* parent,const QString& name) {
    if(parent->objectName()==name)return parent;
    for(auto* child:parent->childItems())if(auto* found=visualItem(child,name))return found;
    return nullptr;
}
void wait(app::DeepSkyController& controller){QElapsedTimer timer;timer.start();while(controller.busy()&&timer.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(1);}ASTRO_TEST_CHECK(!controller.busy());}
int main(int argc,char** argv){
    QGuiApplication application(argc,argv);QTemporaryDir directory;
    QTranslator translator;if(qEnvironmentVariableIsSet("ASTRO_UI_TRANSLATION_PATH") && translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))application.installTranslator(&translator);
    QCoreApplication::setOrganizationName("AstroCaptureTest");QCoreApplication::setApplicationName("DeepSkyWorkspaceTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    qmlRegisterType<app::DeepSkyEdits>("AstroCapture",1,0,"DeepSkyEdits");
    qmlRegisterType<app::DeepSkyController>("AstroCapture",1,0,"DeepSkyController");qmlRegisterType<app::DeepSkyHistogramItem>("AstroCapture",1,0,"DeepSkyHistogramItem");qmlRegisterType<app::DeepSkyImageItem>("AstroCapture",1,0,"DeepSkyImageItem");qmlRegisterType<app::AppLibrary>("AstroCapture",1,0,"AppLibrary");
    std::unique_ptr<app::DeepSkyController> screenshotController;
    app::DeepSkyController controller;app::AppLibrary library;QQmlEngine engine;QQmlComponent component(&engine);
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/DeepSkyWorkspace.qml"));ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    component.setData("import AstroCapture 1.0\n"+source.readAll(),QUrl::fromLocalFile(source.fileName()));
    if(component.isError())qWarning()<<component.errors();ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"controller",QVariant::fromValue(&controller)}}));ASTRO_TEST_CHECK(object);
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyCalibrated")->property("checked").toBool());
    QQuickWindow window;window.resize(1100,720);auto* root=qobject_cast<QQuickItem*>(object.get());root->setParentItem(window.contentItem());root->setSize({1100,720});window.show();
    QCoreApplication::processEvents();
    auto* finishTab=visualItem(root,"deepSkyFinishTab");ASTRO_TEST_CHECK(finishTab);
    ASTRO_TEST_CHECK(!finishTab->property("enabled").toBool());
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyIntegrate")->property("visible").toBool());
    auto* presetDialog=object->findChild<QObject*>("deepSkyPresetDialog");ASTRO_TEST_CHECK(presetDialog);
    auto* dialogBackground=presetDialog->property("background").value<QObject*>();ASTRO_TEST_CHECK(dialogBackground);
    ASTRO_TEST_CHECK(dialogBackground->property("color").value<QColor>()==QColor("#0b0d10"));
    recording::AstroImage data;data.width=160;data.height=128;data.exposure=60;data.filter="Red";data.samples.resize(160*128);
    const std::array<std::array<int,2>,10> stars{{{20,20},{60,16},{105,25},{140,40},{35,60},{80,55},{120,75},{25,105},{75,110},{135,105}}};
    QList<QUrl> urls;
    for(int n=0;n<3;++n){for(int y=0;y<128;++y)for(int x=0;x<160;++x){double value=.02;for(const auto& star:stars)value+=.2*std::exp(-((x-star[0]-n)*(x-star[0]-n)+(y-star[1])*(y-star[1]))/5.);data.samples[y*160+x]=float(value);}const auto path=directory.filePath(QString("light-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),data);urls<<QUrl::fromLocalFile(path);}
    auto* toast=object->findChild<QObject*>("deepSkyCompletionToast");ASTRO_TEST_CHECK(toast);
    auto* spinner=object->findChild<QObject*>("deepSkyActivitySpinner");ASTRO_TEST_CHECK(spinner);
    controller.addFiles(urls);ASTRO_TEST_CHECK(controller.busy());QCoreApplication::processEvents();if(controller.busy())ASTRO_TEST_CHECK(spinner->property("opacity").toDouble()==1);
    wait(controller);QCoreApplication::processEvents();ASTRO_TEST_CHECK(spinner->property("opacity").toDouble()==0);
    ASTRO_TEST_CHECK(toast->property("success").toBool()&&toast->property("visible").toBool());
    ASTRO_TEST_CHECK(controller.frames().size()==3&&controller.frames()[0].toMap()["group"]=="R");
    ASTRO_TEST_CHECK(controller.frames()[0].toMap()["stars"].toInt()>=8&&controller.imageWidth()==160);
    ASTRO_TEST_CHECK(controller.sourceInterpretation().contains("monochrome",Qt::CaseInsensitive));
    controller.setSourceAutoStretch(false);wait(controller);ASTRO_TEST_CHECK(!controller.sourceAutoStretch());
    controller.setSourceDebayer(false);wait(controller);ASTRO_TEST_CHECK(!controller.sourceDebayer());
    controller.setSourceAutoStretch(true);wait(controller);
    controller.addFiles(urls);wait(controller);ASTRO_TEST_CHECK(controller.frames().size()==3);
    root->setProperty("workflowStep",1);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(object->findChild<QObject*>("deepSkyIntegrate")->property("visible").toBool());
    ASTRO_TEST_CHECK(object->findChild<QObject*>("deepSkyStackDivider")->property("visible").toBool());
    object->findChild<QObject*>("deepSkyAdvanced")->setProperty("expanded",true);QCoreApplication::processEvents();
    controller.setSelected(2,false);controller.integrate(true);wait(controller);
    ASTRO_TEST_CHECK(!root->property("frameListOpen").toBool());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkySources"),"clicked"));
    ASTRO_TEST_CHECK(root->property("frameListOpen").toBool());
    if(!controller.hasResult())qWarning()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult()&&controller.displayingResult()&&controller.groups()==QStringList{"R"});
    ASTRO_TEST_CHECK(controller.hasRawComparison());
    ASTRO_TEST_CHECK(finishTab->property("enabled").toBool());
    const auto handed=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(handed.exposureCount==2&&handed.totalExposure==120&&handed.filters==QStringList{"R"}&&!handed.processingSummary.isEmpty());
    const auto integratedPixels=handed.pixels;
    const auto integratedPreview=controller.image();
    controller.showRawComparison(true);ASTRO_TEST_CHECK(controller.comparingRaw()&&controller.imageWidth()==160&&controller.processingDocument().value<app::ImageDocument>().pixels==integratedPixels);
    controller.showRawComparison(false);ASTRO_TEST_CHECK(!controller.comparingRaw()&&controller.image()==integratedPreview);
    controller.chooseRawComparison(1);wait(controller);ASTRO_TEST_CHECK(controller.rawComparisonName()=="light-1.fits");
    controller.showRawComparison(true);controller.restoreResult();ASTRO_TEST_CHECK(!controller.comparingRaw()&&controller.image()==integratedPreview);
    auto* imageViewport=root->findChild<QObject*>("deepSkyViewport");ASTRO_TEST_CHECK(imageViewport);
    imageViewport->setProperty("fitMode",false);imageViewport->setProperty("zoom",8.0);
    imageViewport->setProperty("panX",12.0);imageViewport->setProperty("panY",6.0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"compareRaw",Q_ARG(QVariant,QVariant(true))));
    ASTRO_TEST_CHECK(controller.comparingRaw() && !imageViewport->property("fitMode").toBool());
    ASTRO_TEST_CHECK(imageViewport->property("zoom").toDouble()==8.0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"compareRaw",Q_ARG(QVariant,QVariant(false))));
    ASTRO_TEST_CHECK(!controller.comparingRaw() && !imageViewport->property("fitMode").toBool());
    ASTRO_TEST_CHECK(imageViewport->property("zoom").toDouble()==8.0);
    ASTRO_TEST_CHECK(imageViewport->property("panX").toDouble()==12.0 && imageViewport->property("panY").toDouble()==6.0);
    QMetaObject::invokeMethod(imageViewport,"fit");
    auto recipe=controller.finishSettings();auto first=recipe;first.remove("operations");first["stretch"]=false;first["red"]=1.2;first["green"]=1.;first["blue"]=1.;
    recipe["operations"]=QVariantList{QVariantMap{{"name",QString("Red gain")},{"enabled",true},{"settings",first}}};recipe["rotation"]=1;
    controller.apply(recipe);wait(controller);const auto oriented=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(oriented.pixels->width==integratedPixels->height&&oriented.pixels->height==integratedPixels->width);
    ASTRO_TEST_CHECK(controller.linearHistogram()["shadowChannels"].toList().size()==3);
    controller.apply(controller.suggestedStretch());wait(controller);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(controller.finishSettings()))));wait(controller);
    const auto draftPixels=controller.processingDocument().value<app::ImageDocument>().pixels;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"commitTransformation"));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["operations"].toList().size()==1);
    const auto committedPixels=controller.processingDocument().value<app::ImageDocument>().pixels;ASTRO_TEST_CHECK(committedPixels->rgb==draftPixels->rgb);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"editOperation",Q_ARG(QVariant,QVariant(0))));wait(controller);ASTRO_TEST_CHECK(root->property("editingOperationIndex").toInt()==0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"cancelOperationEdit"));wait(controller);ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels->rgb==committedPixels->rgb);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(astro::app::defaultDeepSkySettings()))));wait(controller);
    const auto output=QUrl::fromLocalFile(directory.filePath("linear.fits"));controller.exportImage(output,true);wait(controller);
    auto result=recording::readAstroImage(output.toLocalFile().toStdString());ASTRO_TEST_CHECK(result.exposure==120&&result.channels==3&&result.width<160);
    QCoreApplication::processEvents();
    const auto viewportHeight=imageViewport->property("height").toDouble();
    const auto viewportY=imageViewport->property("y").toDouble();
    const auto layoutConnection=QObject::connect(&controller,&app::DeepSkyController::changed,root,[imageViewport,viewportHeight,viewportY]{
        QMetaObject::invokeMethod(imageViewport,[imageViewport,viewportHeight,viewportY]{
            ASTRO_TEST_CHECK(imageViewport->property("height").toDouble()==viewportHeight && imageViewport->property("y").toDouble()==viewportY);
        },Qt::QueuedConnection);
    });
    auto stretch=controller.suggestedStretch();stretch["midtones"]=.1;stretch["background"]=false;controller.apply(stretch);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(imageViewport->property("height").toDouble()==viewportHeight && imageViewport->property("y").toDouble()==viewportY);
    wait(controller);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(imageViewport->property("height").toDouble()==viewportHeight && imageViewport->property("y").toDouble()==viewportY);
    QObject::disconnect(layoutConnection);
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
    auto savedOperation=app::defaultDeepSkySettings();savedOperation.remove("operations");savedOperation["stretch"]=false;savedOperation["red"]=1.05;
    finish["operations"]=QVariantList{QVariantMap{{"name",QString("Saved color gain")},{"enabled",true},{"settings",savedOperation}}};
    controller.setProjectOptions({{"calibrated",true},{"finish",finish}});
    controller.apply(finish);wait(controller);
    const auto savedPixels=controller.processingDocument().value<app::ImageDocument>().pixels;
    auto* edits=root->findChild<app::DeepSkyEdits*>("deepSkyEdits");ASTRO_TEST_CHECK(edits);
    ASTRO_TEST_CHECK(!root->property("finishDirty").toBool());
    auto bFinish=finish;bFinish["saturation"]=.2;bFinish["midtones"]=.6;
    edits->copyToOther();
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("B"))));wait(controller);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(bFinish))));wait(controller);
    const auto bPixels=controller.processingDocument().value<app::ImageDocument>().pixels;
    ASTRO_TEST_CHECK(edits->version()=="B" && controller.finishSettings()["midtones"]==.6);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"undoFinish"));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["midtones"]==.37);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"redoFinish"));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["midtones"]==.6);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("A"))));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["midtones"]==.37);
    // With live preview off, changing a valid control marks the result dirty.
    auto pendingFinish=finish;pendingFinish["saturation"]=.8;
    edits->applyValues(pendingFinish);ASTRO_TEST_CHECK(root->property("finishDirty").toBool());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"undoFinish"));wait(controller);
    ASTRO_TEST_CHECK(!root->property("finishDirty").toBool());
    ASTRO_TEST_CHECK(library.saveDeepSkyPreset("Test finish",edits->values()));
    edits->applyValues(bFinish);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(library.deepSkyPreset("Test finish")))));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["midtones"]==.37);
    QVariant savedOptions;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"saveOptions",Q_RETURN_ARG(QVariant,savedOptions))&&savedOptions.toBool());
    QObject::connect(&controller,&app::DeepSkyController::projectSaved,&library,&app::AppLibrary::recordProject);
    QObject::connect(&controller,&app::DeepSkyController::projectOpened,&library,&app::AppLibrary::recordProject);
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
    ASTRO_TEST_CHECK(edits->version()=="A" && !edits->canUndo());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("B"))));wait(controller);
    ASTRO_TEST_CHECK(!edits->canUndo() && edits->values()["midtones"]==.6);
    const auto restoredB=controller.processingDocument().value<app::ImageDocument>().pixels;
    ASTRO_TEST_CHECK(restoredB->rgb.size()==bPixels->rgb.size());
    for(std::size_t i=0;i<restoredB->rgb.size();++i)ASTRO_TEST_CHECK(std::abs(restoredB->rgb[i]-bPixels->rgb[i])<1e-6);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("A"))));wait(controller);
    const auto previous=controller.processingDocument().value<app::ImageDocument>().pixels;
    ASTRO_TEST_CHECK(library.recentProjects().size()==1 && library.recentProjects()[0].toMap()["url"].toUrl()==project);
    QFile savedProject(project.toLocalFile());ASTRO_TEST_CHECK(savedProject.open(QIODevice::ReadOnly));
    auto corruptProject=QJsonDocument::fromJson(savedProject.readAll()).object();savedProject.close();
    auto corruptOptions=corruptProject["options"].toObject();auto corruptVersions=corruptOptions["versions"].toObject();
    auto corruptB=corruptVersions["B"].toObject();corruptB["white"]=corruptB["black"];
    corruptVersions["B"]=corruptB;corruptOptions["versions"]=corruptVersions;corruptProject["options"]=corruptOptions;
    const auto badProject=directory.filePath("invalid.astroproject");QFile badFile(badProject);ASTRO_TEST_CHECK(badFile.open(QIODevice::WriteOnly));
    badFile.write(QJsonDocument(corruptProject).toJson());badFile.close();controller.loadProject(QUrl::fromLocalFile(badProject));
    ASTRO_TEST_CHECK(library.recentProjects().size()==1);
    ASTRO_TEST_CHECK(controller.hasResult()&&!controller.status().isEmpty()&&controller.processingDocument().value<app::ImageDocument>().pixels==previous);
    const auto linearAfter=QUrl::fromLocalFile(directory.filePath("linear-after-edits.fits"));controller.exportImage(linearAfter,true);wait(controller);
    const auto scientific=recording::readAstroImage(linearAfter.toLocalFile().toStdString());
    ASTRO_TEST_CHECK(scientific.samples.size()==result.samples.size());
    for(std::size_t i=0;i<scientific.samples.size();++i)ASTRO_TEST_CHECK(std::abs(scientific.samples[i]-result.samples[i])<1e-6);
    controller.integrate(true);controller.cancel();wait(controller);QCoreApplication::processEvents();ASTRO_TEST_CHECK(!toast->property("success").toBool());ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels==previous);
    QFile original(urls[0].toLocalFile());ASTRO_TEST_CHECK(original.open(QIODevice::ReadOnly));const auto before=original.readAll();original.close();controller.exportImage(urls[0],true);ASTRO_TEST_CHECK(!controller.busy());ASTRO_TEST_CHECK(original.open(QIODevice::ReadOnly));ASTRO_TEST_CHECK(original.readAll()==before);
    app::DeepSkyController guarded;guarded.addFiles({urls[0]});wait(guarded);guarded.addFiles({urls[1]},"Dark");wait(guarded);guarded.integrate(true);wait(guarded);ASTRO_TEST_CHECK(!guarded.hasResult()&&guarded.status().contains("Calibration files are selected"));
    const auto folder=directory.filePath("structured");
    for(const auto& name:QStringList{"Brutes_180s","Darks_180s","Flats_3s","Offsets_3s"})ASTRO_TEST_CHECK(QDir().mkpath(folder+"/"+name));
    data.exposure=180;recording::writeAstroFits((folder+"/Brutes_180s/light.fit").toStdString(),data);recording::writeAstroFits((folder+"/Darks_180s/dark.fit").toStdString(),data);
    data.exposure=3;recording::writeAstroFits((folder+"/Flats_3s/flat.fit").toStdString(),data);recording::writeAstroFits((folder+"/Offsets_3s/bias.fit").toStdString(),data);recording::writeAstroFits((folder+"/resultat.fit").toStdString(),data);
    app::DeepSkyController imported;imported.addFolder(QUrl::fromLocalFile(folder));wait(imported);ASTRO_TEST_CHECK(imported.frames().size()==4);
    QStringList importedRoles;for(const auto& value:imported.frames())importedRoles<<value.toMap()["role"].toString();for(const auto& role:QStringList{"Light","Dark","Flat","DarkFlat"})ASTRO_TEST_CHECK(importedRoles.contains(role));
    imported.setRole(0,"Bias");imported.setGroup(0,"L");ASTRO_TEST_CHECK(imported.frames()[0].toMap()["role"]=="Bias"&&imported.frames()[0].toMap()["group"]=="L");
    // Raw preview switches must never change the stored sensor interpretation.
    app::DeepSkyController bayerPreview;auto mosaic=data;mosaic.bayer=domain::BayerPattern::RGGB;mosaic.filter="";
    for(int y=0;y<mosaic.height;++y)for(int x=0;x<mosaic.width;++x)mosaic.samples[y*mosaic.width+x]=(y%2==0&&x%2==0)?.1f:(y%2&&x%2)?.3f:.2f;
    const auto mosaicPath=directory.filePath("bayer-source.fits");recording::writeAstroFits(mosaicPath.toStdString(),mosaic);
    bayerPreview.addFiles({QUrl::fromLocalFile(mosaicPath)});wait(bayerPreview);ASTRO_TEST_CHECK(bayerPreview.sourceInterpretation().contains("Bayer"));
    bayerPreview.setSourceAutoStretch(false);wait(bayerPreview);const auto color=bayerPreview.image().pixelColor(50,50);ASTRO_TEST_CHECK(color.red()!=color.blue());
    bayerPreview.setSourceDebayer(false);wait(bayerPreview);const auto rawPixel=bayerPreview.image().pixelColor(50,50);ASTRO_TEST_CHECK(rawPixel.red()==rawPixel.green()&&rawPixel.green()==rawPixel.blue());
    ASTRO_TEST_CHECK(recording::readAstroImage(mosaicPath.toStdString()).bayer==domain::BayerPattern::RGGB);
    if(qEnvironmentVariableIsSet("ASTRO_UI_FITS_PATH")) {
        screenshotController=std::make_unique<app::DeepSkyController>();root->setProperty("controller",QVariant::fromValue(screenshotController.get()));
        screenshotController->addFiles({QUrl::fromLocalFile(qEnvironmentVariable("ASTRO_UI_FITS_PATH"))});wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->frames().size()==1);
        screenshotController->integrate(true);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->hasResult());
        auto settings=screenshotController->suggestedStretch();
        screenshotController->apply(settings);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->processingDocument().value<app::ImageDocument>().pixels);
        root->setProperty("workflowStep",6);root->setProperty("imageFocus",true);QCoreApplication::processEvents();QThread::msleep(50);QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH"))ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")));
        ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyViewport")->property("height").toDouble()>280);
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH")) {
            const auto path=qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH");
            root->setProperty("workflowStep",1);root->setProperty("imageFocus",false);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(path+".stack.png"));
            root->setProperty("workflowStep",6);root->setProperty("imageFocus",true);
            QMetaObject::invokeMethod(presetDialog,"open");QCoreApplication::processEvents();QThread::msleep(200);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(path+".dialog.png"));
            QMetaObject::invokeMethod(presetDialog,"close");QCoreApplication::processEvents();
            root->setProperty("workflowStep",1);QCoreApplication::processEvents();
            QQuickItem* helpTarget=nullptr;
            for(auto* item:root->findChildren<QQuickItem*>()) {
                const auto position=item->mapToScene(QPointF(20,item->height()/2));
                if(item->isVisible() && !item->property("explanation").toString().isEmpty() && position.y()>0 && position.y()<window.height()-100) {helpTarget=item;break;}
            }
            ASTRO_TEST_CHECK(helpTarget);
            const auto point=helpTarget->mapToScene(QPointF(20,helpTarget->height()/2));
            QMouseEvent hover(QEvent::MouseMove,point,point,Qt::NoButton,Qt::NoButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&hover);
            QElapsedTimer tooltipWait;tooltipWait.start();while(tooltipWait.elapsed()<500){QCoreApplication::processEvents();QThread::msleep(1);}
            bool helpVisible=false;for(auto* tip:root->findChildren<QObject*>("darkHelpTip"))helpVisible|=tip->property("visible").toBool();
            ASTRO_TEST_CHECK(helpVisible);ASTRO_TEST_CHECK(window.grabWindow().save(path+".help.png"));
            QMouseEvent leave(QEvent::MouseMove,QPointF(2,2),QPointF(2,2),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&leave);QCoreApplication::processEvents();
        }
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH")) {
            auto gradient=screenshotController->finishSettings();gradient["background"]=true;gradient["backgroundDegree"]=2;
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(gradient))));wait(*screenshotController);
            root->setProperty("workflowStep",6);root->setProperty("finishingTool",1);root->setProperty("editingBackgroundSamples",true);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(!screenshotController->backgroundSamples().empty());
            for(int repaint=0;repaint<8;++repaint){QCoreApplication::processEvents();QThread::msleep(16);}
            ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkySampleOverlay")->property("visible").toBool());
            auto* overlay=qobject_cast<QQuickItem*>(root->findChild<QObject*>("deepSkySampleOverlay"));
            const auto sky=screenshotController->backgroundSamples().front().toList();
            const auto position=overlay->mapToItem(window.contentItem(),QPointF(sky[0].toDouble()*overlay->width(),sky[1].toDouble()*overlay->height()));
            QMouseEvent samplePress(QEvent::MouseButtonPress,position,position,Qt::RightButton,Qt::RightButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&samplePress);
            QMouseEvent sampleRelease(QEvent::MouseButtonRelease,position,position,Qt::RightButton,Qt::NoButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&sampleRelease);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(root->findChild<app::DeepSkyEdits*>("deepSkyEdits")->values()["backgroundPoints"].toList().size()==screenshotController->backgroundSamples().size()-1);
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"applyStoredFinish"));wait(*screenshotController);
            const auto preservedResult=screenshotController->processingDocument().value<app::ImageDocument>().pixels;
            screenshotController->setShowingBackgroundModel(true);ASTRO_TEST_CHECK(screenshotController->showingBackgroundModel());
            ASTRO_TEST_CHECK(screenshotController->processingDocument().value<app::ImageDocument>().pixels==preservedResult);
            screenshotController->setShowingBackgroundModel(false);
            for(int repaint=0;repaint<4;++repaint){QCoreApplication::processEvents();QThread::msleep(16);}

            ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")+"-gradient.png"));
            root->setProperty("editingBackgroundSamples",false);root->setProperty("finishingTool",0);
        }
        const auto beforeAB=screenshotController->processingDocument().value<app::ImageDocument>().pixels;
        auto realB=root->findChild<app::DeepSkyEdits*>("deepSkyEdits")->values();realB["midtones"]=.35;realB["asinh"]=false;
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("B"))));wait(*screenshotController);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(realB))));wait(*screenshotController);
        QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH"))ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")+"-B.png"));
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"switchVersion",Q_ARG(QVariant,QVariant("A"))));wait(*screenshotController);
        const auto afterAB=screenshotController->processingDocument().value<app::ImageDocument>().pixels;
        ASTRO_TEST_CHECK(beforeAB->rgb.size()==afterAB->rgb.size());
        for(std::size_t i=0;i<afterAB->rgb.size();i+=997)ASTRO_TEST_CHECK(std::abs(beforeAB->rgb[i]-afterAB->rgb[i])<1e-6);
    }
    std::cout<<"OK\n";
}
