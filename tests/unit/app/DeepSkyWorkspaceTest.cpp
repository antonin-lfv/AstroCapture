#include "MasterLibrary.hpp"
#include "SessionStorage.hpp"
#include <QImageReader>
#include "BlindSolver.hpp"
#include "MaskOverlayItem.hpp"
#include "ImagePixelsItem.hpp"
#include "EquipmentProfile.hpp"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include "ExportSizeEstimator.hpp"
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <astro/recording/AstroImage.hpp>
#include <cmath>
#include <limits>
#include "DeepSkyController.hpp"
#include "DeepSkyEdits.hpp"
#include "DeepSkyImageItem.hpp"
#include "AppLibrary.hpp"
#include "AstrometrySettings.hpp"
#include "Check.hpp"
using namespace astro;
QQuickItem* visualItem(QQuickItem* parent,const QString& name) {
    if(parent->objectName()==name)return parent;
    for(auto* child:parent->childItems())if(auto* found=visualItem(child,name))return found;
    return nullptr;
}
void wait(app::DeepSkyController& controller){QElapsedTimer timer;timer.start();while(controller.busy()&&timer.elapsed()<(qEnvironmentVariableIsSet("ASTRO_UI_LARGE_QA")?240000:20000)){QCoreApplication::processEvents();QThread::msleep(1);}ASTRO_TEST_CHECK(!controller.busy());}
int main(int argc,char** argv){
    QGuiApplication application(argc,argv);QTemporaryDir directory;
    QTranslator translator;if(qEnvironmentVariableIsSet("ASTRO_UI_TRANSLATION_PATH") && translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))application.installTranslator(&translator);
    QCoreApplication::setOrganizationName("AstroCaptureTest");QCoreApplication::setApplicationName("DeepSkyWorkspaceTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    qmlRegisterType<astro::app::ImagePixelsItem>("AstroCapture",1,0,"ImagePixelsItem");
    qmlRegisterType<app::MaskOverlayItem>("AstroCapture",1,0,"MaskOverlayItem");
    qmlRegisterType<app::DeepSkyEdits>("AstroCapture",1,0,"DeepSkyEdits");
    qmlRegisterType<app::ExportSizeEstimator>("AstroCapture",1,0,"ExportSizeEstimator");
    qmlRegisterType<app::DeepSkyController>("AstroCapture",1,0,"DeepSkyController");qmlRegisterType<app::DeepSkyHistogramItem>("AstroCapture",1,0,"DeepSkyHistogramItem");qmlRegisterType<app::DeepSkyImageItem>("AstroCapture",1,0,"DeepSkyImageItem");qmlRegisterType<app::AppLibrary>("AstroCapture",1,0,"AppLibrary");
    std::unique_ptr<app::DeepSkyController> screenshotController;
    app::DeepSkyController controller;app::AppLibrary library;QQmlEngine engine;QQmlComponent component(&engine);
    QFile source(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/DeepSkyWorkspace.qml"));ASTRO_TEST_CHECK(source.open(QIODevice::ReadOnly));
    component.setData("import AstroCapture 1.0\n"+source.readAll(),QUrl::fromLocalFile(source.fileName()));
    if(component.isError())qWarning()<<component.errors();ASTRO_TEST_CHECK(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"library",QVariant::fromValue(&library)},{"controller",QVariant::fromValue(&controller)}}));ASTRO_TEST_CHECK(object);
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyCalibrated")->property("checked").toBool());
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyWcsProjection")->property("checked").toBool());
    QQuickWindow window;window.resize(1100,720);auto* root=qobject_cast<QQuickItem*>(object.get());root->setParentItem(window.contentItem());root->setSize({1100,720});window.show();
    QCoreApplication::processEvents();
    // Complete palette workflow, including missing layers and saved choices.
    if(!qEnvironmentVariableIsSet("ASTRO_UI_LARGE_QA")&&!qEnvironmentVariableIsSet("ASTRO_UI_BATCH_QA")&&!qEnvironmentVariableIsSet("ASTRO_UI_RESOURCE_QA")&&!qEnvironmentVariableIsSet("ASTRO_UI_DRIZZLE_QA")&&!qEnvironmentVariableIsSet("ASTRO_UI_BAYER_QA")) {
        const bool visual=qEnvironmentVariableIsSet("ASTRO_UI_COMPOSITION_QA");
        const QStringList filters{"R","G","B","L","HA","OIII"};
        recording::AstroImage real;
        if(visual)real=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());
        for(int n=0;n<filters.size();++n) {
            recording::AstroImage frame;frame.width=visual?1024:160;frame.height=visual?768:140;frame.channels=1;frame.exposure=60;frame.filter=filters[n].toStdString();
            frame.samples.resize(std::size_t(frame.width)*frame.height);
            for(int y=0;y<frame.height;++y)for(int x=0;x<frame.width;++x) {
                double value=.01;
                if(visual){const auto at=(std::size_t(y+(real.height-frame.height)/2)*real.width+x+(real.width-frame.width)/2)*real.channels;value=n==3?(real.samples[at]+real.samples[at+1]+real.samples[at+2])/3:real.samples[at+(n==0||n==4?0:n==1||n==5?1:2)];}
                else for(const auto& xy:std::vector<std::pair<int,int>>{{20,21},{50,20},{85,25},{120,20},{140,55},{30,55},{65,62},{100,60},{125,85},{20,110},{60,110},{100,115}}){const auto dx=x-xy.first,dy=y-xy.second;value+=.3*std::exp(-(dx*dx+dy*dy)/4.5);}
                frame.samples[std::size_t(y)*frame.width+x]=float(value);
            }
            const auto path=directory.filePath("composition-"+filters[n]+".fits");recording::writeAstroFits(path.toStdString(),frame);
            controller.addFiles({QUrl::fromLocalFile(path)},"Light","Auto");wait(controller);
        }
        controller.integrate(true,0,false,false);wait(controller);ASTRO_TEST_CHECK(controller.hasResult()&&controller.groups().size()==6);
        auto* panel=root->findChild<QObject*>("deepSkyComposition");auto* palette=root->findChild<QObject*>("compositionPalette");auto* apply=root->findChild<QObject*>("compositionApply");ASTRO_TEST_CHECK(panel&&palette&&apply);
        root->setProperty("workflowStep",6);root->setProperty("expandedFamilies",QVariantList{2});root->setProperty("finishingTool",2);root->setProperty("frameListOpen",false);
        auto choose=[&](int index){ASTRO_TEST_CHECK(QMetaObject::invokeMethod(panel,"choosePalette",Q_ARG(QVariant,QVariant(index))));QCoreApplication::processEvents();};
        for(const auto& name:{"RGB","LRGB","HOO"}){const auto plan=controller.compositionPlan({{"palette",name}});ASTRO_TEST_CHECK(plan["ready"].toBool());const auto settings=plan["settings"].toMap();if(QString(name)=="HOO")ASTRO_TEST_CHECK(settings["red"]=="HA"&&settings["green"]=="OIII"&&settings["blue"]=="OIII");if(QString(name)=="LRGB")ASTRO_TEST_CHECK(settings["luminance"]=="L");}
        const auto prior=controller.sessionSnapshot().images["linear"];
        choose(3);ASTRO_TEST_CHECK(!apply->property("enabled").toBool());ASTRO_TEST_CHECK(controller.compositionPlan({{"palette","SHO"}})["missing"].toStringList()==QStringList{"SII"});
        controller.composeChannels({{"palette","SHO"}});ASTRO_TEST_CHECK(!controller.busy()&&controller.sessionSnapshot().images["linear"]==prior);
        auto capture=[&](const QString& name){const auto folder=qEnvironmentVariable("ASTRO_UI_COMPOSITION_QA");QDir().mkpath(folder);QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");for(int i=0;i<8;++i){QCoreApplication::processEvents();QThread::msleep(30);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};
        if(visual)for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();scroll->setProperty("contentY",0);capture(QString("composition-missing-%1x%2.png").arg(size.width()).arg(size.height()));}
        auto sii=recording::readAstroImage(directory.filePath("composition-HA.fits").toStdString());sii.filter="SII";for(auto& v:sii.samples)v*=.8f;
        const auto siiPath=directory.filePath("composition-SII.fits");recording::writeAstroFits(siiPath.toStdString(),sii);controller.addFiles({QUrl::fromLocalFile(siiPath)},"Light","Auto");wait(controller);controller.integrate(true,0,false,false);wait(controller);
        ASTRO_TEST_CHECK(controller.compositionPlan({{"palette","SHO"}})["ready"].toBool());ASTRO_TEST_CHECK(apply->property("enabled").toBool());ASTRO_TEST_CHECK(QMetaObject::invokeMethod(apply,"clicked"));wait(controller);ASTRO_TEST_CHECK(controller.compositionState()["palette"]=="SHO");
        choose(1);ASTRO_TEST_CHECK(apply->property("enabled").toBool());
        auto* amount=root->findChild<QObject*>("compositionLuminanceAmount");ASTRO_TEST_CHECK(amount);amount->setProperty("value",50);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(amount,"moved"));
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(apply,"clicked"));wait(controller);
        ASTRO_TEST_CHECK(controller.compositionState()["palette"]=="LRGB"&&controller.compositionState()["luminanceAmount"].toDouble()==.5);
        ASTRO_TEST_CHECK(!panel->property("pending").toBool());ASTRO_TEST_CHECK(app::DeepSkyController::validSession(controller.sessionSnapshot()));
        if(visual)for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();scroll->setProperty("contentY",0);capture(QString("composition-lrgb-%1x%2.png").arg(size.width()).arg(size.height()));}
        const auto savedChoices=root->property("compositionSettings").toMap();root->setProperty("compositionSettings",QVariantMap{{"palette","LRGB"},{"luminanceAmount",.25}});QCoreApplication::processEvents();ASTRO_TEST_CHECK(amount->property("value").toDouble()==25.);root->setProperty("compositionSettings",savedChoices);QCoreApplication::processEvents();ASTRO_TEST_CHECK(amount->property("value").toDouble()==50.);
        const auto composed=controller.sessionSnapshot().images["linear"];
        choose(2);ASTRO_TEST_CHECK(panel->property("pending").toBool()&&controller.sessionSnapshot().images["linear"]==composed);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(apply,"clicked"));wait(controller);ASTRO_TEST_CHECK(controller.compositionState()["palette"]=="HOO");
        const auto beforeCancel=controller.sessionSnapshot().images["linear"];controller.composeChannels({{"palette","LRGB"},{"luminanceAmount",.25}});controller.cancel();wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==beforeCancel&&controller.compositionState()["palette"]=="HOO");
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(panel,"chooseChannel",Q_ARG(QVariant,QVariant("green")),Q_ARG(QVariant,QVariant("B"))));
        ASTRO_TEST_CHECK(root->property("compositionSettings").toMap()["palette"]=="Custom");ASTRO_TEST_CHECK(panel->property("pending").toBool());
        auto* manualGreen=visualItem(root,"compositionChannel_green");ASTRO_TEST_CHECK(manualGreen&&manualGreen->property("visible").toBool());
        if(visual)for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();scroll->setProperty("contentY",0);capture(QString("composition-custom-%1x%2.png").arg(size.width()).arg(size.height()));}

        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"saveOptions"));const auto saved=controller.sessionSnapshot();
        app::SessionStorage storage(directory.filePath("composition-session"));app::SavedSession serialized;serialized.workspaces["deepSky"]=saved;storage.save(serialized);const auto loaded=storage.load();
        app::DeepSkyController recovered;recovered.restoreSession(loaded.workspaces["deepSky"]);ASTRO_TEST_CHECK(recovered.compositionState()==controller.compositionState()&&recovered.projectOptions()["composition"]==controller.projectOptions()["composition"]);
        const auto path=QUrl::fromLocalFile(directory.filePath("composition-linked.astroproject"));controller.saveProject(path);wait(controller);app::DeepSkyController linked;linked.loadProject(path);ASTRO_TEST_CHECK(linked.projectOptions()["composition"]==controller.projectOptions()["composition"]);
        auto invalid=saved;auto options=invalid.state["options"].toMap();options["composition"]=QVariantMap{{"palette","LRGB"},{"luminanceAmount",2.}};invalid.state["options"]=options;ASTRO_TEST_CHECK(!app::DeepSkyController::validSession(invalid));
        const auto result=controller.sessionSnapshot().images["linear"];controller.composeChannels({{"palette","Custom"},{"red","R"},{"green","G"},{"blue","B"},{"luminanceAmount",-1.}});ASTRO_TEST_CHECK(!controller.busy()&&controller.sessionSnapshot().images["linear"]==result);
        auto* layer=root->findChild<QObject*>("compositionInspectLayer");ASTRO_TEST_CHECK(layer);layer->setProperty("currentIndex",controller.groups().indexOf("R"));ASTRO_TEST_CHECK(QMetaObject::invokeMethod(layer,"activated",Q_ARG(int,controller.groups().indexOf("R"))));wait(controller);ASTRO_TEST_CHECK(controller.compositionState().isEmpty());
        qInfo()<<"Composition palettes, missing filters, L contribution, pending state and project/recovery checks passed";
        if(visual)return 0;
        controller.clear();QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,QVariant(QVariantMap{}))));QCoreApplication::processEvents();
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_LARGE_QA")) {
        // Two identical private copies qualify resources and exact pixels;
        // they intentionally provide no additional photons/subpixel diversity.
        const auto path=qEnvironmentVariable("ASTRO_UI_FITS_PATH");
        const auto folder=qEnvironmentVariable("ASTRO_UI_LARGE_QA");QDir().mkpath(folder);
        const auto second=directory.filePath("Orion-duplicate.fits");ASTRO_TEST_CHECK(QFile::copy(path,second));
        controller.addFiles({QUrl::fromLocalFile(path),QUrl::fromLocalFile(second)},"Light","RGB");wait(controller);
        ASTRO_TEST_CHECK(controller.frames().size()==2);
        const auto plan=controller.integrationResources({{"calibrated",true},{"drizzle",true},{"scale",2.0}});
        qInfo()<<"Large-image preflight"<<plan;ASTRO_TEST_CHECK(!plan["blocked"].toBool());
        root->setProperty("workflowStep",1);root->findChild<QObject*>("deepSkyAdvanced")->setProperty("expanded",true);
        root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);root->findChild<QObject*>("deepSkyDrizzle")->setProperty("checked",true);root->findChild<QObject*>("deepSkyDrizzleScale")->setProperty("currentIndex",2);
        auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();
        auto capture=[&](const QString& name){QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");for(int n=0;n<8;++n){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);scroll->setProperty("contentY",450);capture(QString("large-plan-%1x%2.png").arg(size.width()).arg(size.height()));}
        QElapsedTimer elapsed;elapsed.start();controller.integrate(true,0,false,false,false,true,2,1);wait(controller);
        qInfo()<<"Large integration ms"<<elapsed.elapsed()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult());
        auto result=controller.sessionSnapshot().images["linear"];
        ASTRO_TEST_CHECK(std::uint64_t(result->width)*result->height>100000000);
        ASTRO_TEST_CHECK(app::DeepSkyController::validSession(controller.sessionSnapshot()));
        // Exact identity-grid drizzle must keep every input intensity.
        {const auto source=recording::readAstroImage(path.toStdString());
            ASTRO_TEST_CHECK(result->width==source.width*2 && result->height==source.height*2);
            for(int y=0;y<result->height;++y)for(int x=0;x<result->width;++x)for(int c=0;c<3;++c)
                ASTRO_TEST_CHECK(std::abs(result->rgb[(std::size_t(y)*result->width+x)*3+c]-source.samples[(std::size_t(y/2)*source.width+x/2)*3+c])<2e-6);
        }
        const auto hash=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(result->rgb.data()),result->rgb.size()*4),QCryptographicHash::Sha256);
        qInfo()<<"Large dimensions"<<result->width<<result->height<<"linear SHA256"<<hash.toHex();
        root->setProperty("workflowStep",6);root->setProperty("frameListOpen",false);
        auto finish=app::defaultDeepSkySettings();const auto suggested=controller.suggestedStretch();for(const auto& key:{"black","white","midtones","red","green","blue"})finish[key]=suggested[key];
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(finish))));wait(controller);
        qInfo()<<"Large finish"<<controller.status();ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==result);
        const auto finished=controller.sessionSnapshot().images["finished"];ASTRO_TEST_CHECK(finished&&finished->width==result->width);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);scroll->setProperty("contentY",0);capture(QString("large-finish-%1x%2.png").arg(size.width()).arg(size.height()));}
        const auto fits=directory.filePath("large-linear.fits");controller.exportImage(QUrl::fromLocalFile(fits),true);wait(controller);ASTRO_TEST_CHECK(QFileInfo(fits).size()>1200000000);
        {const auto restored=recording::readAstroImage(fits.toStdString());ASTRO_TEST_CHECK(restored.samples==result->rgb);}
        const auto tiff=QFileInfo(folder+"/large-finished.tif").absoluteFilePath();controller.exportImage(QUrl::fromLocalFile(tiff),false);wait(controller);ASTRO_TEST_CHECK(QFileInfo(tiff).size()>600000000);
        {QImageReader::setAllocationLimit(1024);QImageReader reader(tiff);ASTRO_TEST_CHECK(reader.size()==QSize(finished->width,finished->height));const auto decoded=reader.read().convertToFormat(QImage::Format_RGBA64);ASTRO_TEST_CHECK(!decoded.isNull());for(int y:{0,decoded.height()/2,decoded.height()-1}){const auto* row=reinterpret_cast<const QRgba64*>(decoded.constScanLine(y));for(int x:{0,decoded.width()/2,decoded.width()-1})for(int c=0;c<3;++c){const auto v=c==0?row[x].red():c==1?row[x].green():row[x].blue();ASTRO_TEST_CHECK(v==std::uint16_t(std::lround(std::clamp(finished->rgb[(std::size_t(y)*decoded.width()+x)*3+c],0.f,1.f)*65535)));}}}
        qInfo()<<"Large FITS and TIFF verified";
        app::SessionStorage storage(directory.filePath("large-session"));app::SavedSession saved;saved.workspaces["deepSky"]=controller.sessionSnapshot();storage.save(saved);
        {const auto restored=storage.load();ASTRO_TEST_CHECK(app::DeepSkyController::validSession(restored.workspaces["deepSky"]));ASTRO_TEST_CHECK(restored.workspaces["deepSky"].images["linear"]->rgb==result->rgb);ASTRO_TEST_CHECK(restored.workspaces["deepSky"].images["finished"]->rgb==finished->rgb);}
        // A heavy combination is rejected before changing scientific results.
        controller.apply({{"starSeparation",true},{"advancedDenoise",true},{"deconvolution",true}});ASTRO_TEST_CHECK(!controller.busy());ASTRO_TEST_CHECK(controller.sessionSnapshot().images["finished"]==finished);qInfo()<<"Large preflight rejection"<<controller.status();
        qInfo()<<"Large image integration / finish / FITS / TIFF / recovery passed";return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_BATCH_QA")) {
        QSettings().setValue("calibration/libraryDirectory",directory.filePath("masters"));
        auto original=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());auto image=original;image.width=1024;image.height=768;image.samples.resize(1024*768*3);const int ox=(original.width-1024)/2,oy=(original.height-768)/2;for(int y=0;y<768;++y)std::copy_n(original.samples.begin()+(std::size_t(y+oy)*original.width+ox)*3,1024*3,image.samples.begin()+std::size_t(y)*1024*3);original.samples.clear();original.samples.shrink_to_fit();image.wcs.reset();image.camera="ZWO ASI071MC - controle interface";image.binX=image.binY=1;image.hasGain=image.hasOffset=image.hasTemperature=true;image.gain=90;image.offset=10;image.temperature=-10;image.exposure=30;image.filter="RGB";
        const auto light=directory.filePath("Orion-batch-interface.fits");recording::writeAstroFits(light.toStdString(),image);controller.addFiles({QUrl::fromLocalFile(light)});wait(controller);controller.integrate(true,0,false,false);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());const auto result=controller.sessionSnapshot().images["linear"];
        auto add=[&](QString name,QString role,double exposure,float value){auto source=image;source.exposure=exposure;source.samples.assign(image.samples.size(),value);const auto path=directory.filePath(name+".fits");recording::writeAstroFits(path.toStdString(),source);controller.addFiles({QUrl::fromLocalFile(path)},role);wait(controller);};
        add("dark-30-a","Dark",30,.02f);add("dark-30-b","Dark",30,.025f);add("dark-60","Dark",60,.04f);add("bias","Bias",.001,.001f);add("dark-flat","DarkFlat",1,.01f);add("flat-RGB","Flat",1,.51f);
        root->setProperty("workflowStep",6);auto* dialog=root->findChild<QObject*>("masterLibraryDialog");ASTRO_TEST_CHECK(dialog);dialog->setProperty("mode",1);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(dialog,"open"));
        const auto folder=qEnvironmentVariable("ASTRO_UI_BATCH_QA");QDir().mkpath(folder);auto* scroll=qobject_cast<QQuickItem*>(dialog->findChild<QObject*>("batchMasterScroll"));ASTRO_TEST_CHECK(scroll);auto* pane=dialog->findChild<QObject*>("batchMasterPane");
        auto settle=[&]{for(int i=0;i<12;++i){QCoreApplication::processEvents();QThread::msleep(40);}};auto capture=[&](QString name){settle();ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){
            window.resize(size);root->setSize(size);scroll->setProperty("contentY",0);capture(QString("batch-plan-%1x%2.png").arg(size.width()).arg(size.height()));
            auto* action=qobject_cast<QQuickItem*>(dialog->findChild<QObject*>("createBatchMasters"));ASTRO_TEST_CHECK(action&&action->isEnabled());auto box=action->mapRectToScene(QRectF(0,0,action->width(),action->height()));ASTRO_TEST_CHECK(box.left()>=0&&box.top()>=0&&box.right()<=root->width()&&box.bottom()<=root->height());
            auto* group=visualItem(window.contentItem(),"batchMasterGroup");auto* help=visualItem(group,"helpIcon");ASTRO_TEST_CHECK(help);help->forceActiveFocus();capture(QString("batch-help-%1x%2.png").arg(size.width()).arg(size.height()));auto* tip=help->findChild<QObject*>("darkHelpTip");ASTRO_TEST_CHECK(tip&&tip->property("visible").toBool());auto* text=qobject_cast<QQuickItem*>(tip->property("contentItem").value<QObject*>());box=text->mapRectToScene(QRectF(0,0,text->width(),text->height()));ASTRO_TEST_CHECK(box.left()>=0&&box.top()>=0&&box.right()<=root->width()&&box.bottom()<=root->height());help->setFocus(false);
            scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->height()));capture(QString("batch-flat-%1x%2.png").arg(size.width()).arg(size.height()));
        }
        ASTRO_TEST_CHECK(pane->property("canCreate").toBool());ASTRO_TEST_CHECK(QMetaObject::invokeMethod(pane,"start"));ASTRO_TEST_CHECK(dialog->findChild<QObject*>("createBatchMasters")->property("busy").toBool());wait(controller);ASTRO_TEST_CHECK(controller.calibrationMasters().size()==5&&controller.sessionSnapshot().images["linear"]==result);ASTRO_TEST_CHECK(controller.calibrationBatchResults().size()==5);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->height()));capture(QString("batch-results-%1x%2.png").arg(size.width()).arg(size.height()));dialog->setProperty("mode",0);capture(QString("master-library-%1x%2.png").arg(size.width()).arg(size.height()));auto* search=dialog->findChild<QObject*>("masterLibrarySearch");search->setProperty("text","60");settle();ASTRO_TEST_CHECK(dialog->findChild<QObject*>("masterLibraryChoice")->property("count").toInt()==1);ASTRO_TEST_CHECK(!dialog->findChild<QObject*>("useLibraryMaster")->property("enabled").toBool());search->setProperty("text","");dialog->findChild<QObject*>("masterLibraryMatchingOnly")->setProperty("checked",true);settle();ASTRO_TEST_CHECK(dialog->findChild<QObject*>("masterLibraryChoice")->property("count").toInt()==4);dialog->findChild<QObject*>("masterLibraryMatchingOnly")->setProperty("checked",false);dialog->setProperty("mode",1);}
        QMetaObject::invokeMethod(dialog,"close");root->setVisible(false);
        QQmlComponent guideComponent(&engine);guideComponent.loadUrl(QUrl::fromLocalFile(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/GuideWorkspace.qml")));ASTRO_TEST_CHECK(guideComponent.isReady());std::unique_ptr<QObject> guideObject(guideComponent.create());auto* guide=qobject_cast<QQuickItem*>(guideObject.get());ASTRO_TEST_CHECK(guide);guide->setParentItem(window.contentItem());guide->setProperty("topic",3);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);guide->setSize(size);settle();auto* lesson=guide->findChild<QObject*>("guideLessonScroll")->property("contentItem").value<QObject*>();auto* card=visualItem(guide,"guideBatchMastersCard");ASTRO_TEST_CHECK(card);lesson->setProperty("contentY",card->y());capture(QString("batch-guide-%1x%2.png").arg(size.width()).arg(size.height()));}
        return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_RESOURCE_QA")) {
        const bool full=qEnvironmentVariableIsSet("ASTRO_RESOURCE_FULL_QA");
        auto image=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());ASTRO_TEST_CHECK(image.channels==3);
        if(!full){auto original=std::move(image);image=original;image.width=1024;image.height=768;image.samples.resize(std::size_t(image.width)*image.height*3);const int ox=(original.width-image.width)/2,oy=(original.height-image.height)/2;for(int y=0;y<image.height;++y)std::copy_n(original.samples.begin()+(std::size_t(y+oy)*original.width+ox)*3,image.width*3,image.samples.begin()+std::size_t(y)*image.width*3);if(image.wcs)image.wcs=image.wcs->remap({1,0,-double(ox),0,1,-double(oy)});}
        const auto expected=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(image.samples.data()),image.samples.size()*4),QCryptographicHash::Sha256);
        const auto input=directory.filePath("Orion-resource-check.fits");recording::writeAstroFits(input.toStdString(),image);image.samples.clear();image.samples.shrink_to_fit();
        controller.addFiles({QUrl::fromLocalFile(input)});wait(controller);root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);
        const auto folder=qEnvironmentVariable("ASTRO_UI_RESOURCE_QA");QDir().mkpath(folder);
        auto capture=[&](QString name){QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");for(int i=0;i<12;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};
        auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){
            window.resize(size);root->setSize(size);QCoreApplication::processEvents();scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));capture(QString("resources-%1x%2.png").arg(size.width()).arg(size.height()));
            auto* label=qobject_cast<QQuickItem*>(root->findChild<QObject*>("deepSkyResourceMemory"));ASTRO_TEST_CHECK(label && label->width()>0);const auto box=label->mapRectToScene(QRectF(0,0,label->width(),label->height()));ASTRO_TEST_CHECK(box.left()>=0 && box.right()<=root->width() && box.top()>=0 && box.bottom()<=root->height());
            auto* control=qobject_cast<QQuickItem*>(root->findChild<QObject*>("deepSkyResourcesHelp"));auto* help=visualItem(control,"helpIcon");ASTRO_TEST_CHECK(help);help->forceActiveFocus();capture(QString("resources-help-%1x%2.png").arg(size.width()).arg(size.height()));auto* tip=help->findChild<QObject*>("darkHelpTip");ASTRO_TEST_CHECK(tip && tip->property("visible").toBool());auto* text=qobject_cast<QQuickItem*>(tip->property("contentItem").value<QObject*>());const auto helpBox=text->mapRectToScene(QRectF(0,0,text->width(),text->height()));ASTRO_TEST_CHECK(helpBox.left()>=0 && helpBox.top()>=0 && helpBox.right()<=root->width() && helpBox.bottom()<=root->height());help->setFocus(false);
            QSettings().setValue("application/integrationTemporaryDirectory",directory.filePath("missing"));QMetaObject::invokeMethod(&controller,"changed");capture(QString("resources-error-%1x%2.png").arg(size.width()).arg(size.height()));ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyIntegrate")->property("enabled").toBool());controller.chooseIntegrationTemporaryDirectory({});
        }
        qInfo()<<"Resource plan before stack:"<<controller.integrationResources({{"calibrated",true}});
        QElapsedTimer timing;timing.start();controller.integrate(true,3,false,false);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());
        const auto result=controller.sessionSnapshot().images["linear"];const auto actual=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char*>(result->rgb.data()),result->rgb.size()*4),QCryptographicHash::Sha256);
        ASTRO_TEST_CHECK(actual==expected);qInfo()<<"Resource integration:"<<result->width<<result->height<<timing.elapsed()<<"ms; Float32 identical";
        if(!full){
            window.resize(900,700);root->setSize({900,700});root->setProperty("workflowStep",1);capture("resources-result-900x700.png");
            QQmlComponent guideComponent(&engine);guideComponent.loadUrl(QUrl::fromLocalFile(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/GuideWorkspace.qml")));ASTRO_TEST_CHECK(guideComponent.isReady());std::unique_ptr<QObject> guideObject(guideComponent.create());auto* guide=qobject_cast<QQuickItem*>(guideObject.get());ASTRO_TEST_CHECK(guide);guide->setParentItem(window.contentItem());guide->setProperty("topic",3);root->setVisible(false);
            for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);guide->setSize(size);for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}auto* lesson=guide->findChild<QObject*>("guideLessonScroll")->property("contentItem").value<QObject*>();lesson->setProperty("contentY",500);for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+QString("/resources-guide-%1x%2.png").arg(size.width()).arg(size.height())));}
        }
        return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_WCS_QA")) {
        auto original=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());ASTRO_TEST_CHECK(original.channels==3);
        auto crop=original;crop.width=1024;crop.height=768;crop.samples.resize(std::size_t(crop.width)*crop.height*3);const int ox=(original.width-crop.width)/2,oy=(original.height-crop.height)/2;
        for(int y=0;y<crop.height;++y)std::copy_n(original.samples.begin()+(std::size_t(y+oy)*original.width+ox)*3,crop.width*3,crop.samples.begin()+std::size_t(y)*crop.width*3);original.samples.clear();original.samples.shrink_to_fit();
        // Real Orion data with declared, simulated distortion. This fixture
        // checks composition and registration, not an acquired SIP calibration.
        recording::CelestialWcs plane;plane.crval={83.82,-5.39};plane.crpix={512.5,384.5};plane.cd={-.0003,0,0,.0003};crop.wcs=plane;crop.filter="RGB";
        QList<QUrl> urls;for(int n=0;n<3;++n){auto frame=crop;if(n==1){frame.wcs->sipOrder=2;frame.wcs->sipA[12]=.00003;frame.wcs->sipB[2]=-.00002;frame.wcs->crpix[0]+=.4;
            for(int y=0;y<frame.height;++y)for(int x=0;x<frame.width;++x){const auto sky=frame.wcs->sky(x,y),p=plane.pixel((*sky)[0],(*sky)[1]);const double sx=(*p)[0],sy=(*p)[1];const int xx=int(std::floor(sx)),yy=int(std::floor(sy));for(int c=0;c<3;++c){double value=0;if(xx>=0&&yy>=0&&xx+1<crop.width&&yy+1<crop.height){const double dx=sx-xx,dy=sy-yy;value=(1-dx)*(1-dy)*crop.samples[(yy*crop.width+xx)*3+c]+dx*(1-dy)*crop.samples[(yy*crop.width+xx+1)*3+c]+(1-dx)*dy*crop.samples[((yy+1)*crop.width+xx)*3+c]+dx*dy*crop.samples[((yy+1)*crop.width+xx+1)*3+c];}frame.samples[(y*frame.width+x)*3+c]=float(value);}}
        }if(n==2)frame.wcs->crpix[0]+=15;const auto path=directory.filePath(QString("M42-WCS-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),frame);urls<<QUrl::fromLocalFile(path);}
        controller.addFiles(urls);wait(controller);root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);root->findChild<QObject*>("deepSkyAdvanced")->setProperty("expanded",true);root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);root->findChild<QObject*>("deepSkyWcsProjection")->setProperty("checked",true);root->findChild<QObject*>("deepSkyDrizzle")->setProperty("checked",true);root->findChild<QObject*>("deepSkyDrizzleScale")->setProperty("currentIndex",0);
        const auto folder=qEnvironmentVariable("ASTRO_UI_WCS_QA");QDir().mkpath(folder);auto settle=[&]{for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}};auto capture=[&](QString name){QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);scroll->setProperty("contentY",0);capture(QString("wcs-settings-%1x%2.png").arg(size.width()).arg(size.height()));auto* control=qobject_cast<QQuickItem*>(root->findChild<QObject*>("deepSkyWcsProjection"));auto* help=visualItem(control,"helpIcon");ASTRO_TEST_CHECK(help);help->forceActiveFocus();settle();capture(QString("wcs-help-%1x%2.png").arg(size.width()).arg(size.height()));auto* tip=help->findChild<QObject*>("darkHelpTip");ASTRO_TEST_CHECK(tip&&tip->property("visible").toBool());auto* text=qobject_cast<QQuickItem*>(tip->property("contentItem").value<QObject*>());ASTRO_TEST_CHECK(text);const auto box=text->mapRectToScene(QRectF(0,0,text->width(),text->height()));ASTRO_TEST_CHECK(box.left()>=0&&box.top()>=0&&box.right()<=root->width()&&box.bottom()<=root->height());help->setFocus(false);}
        QElapsedTimer timing;timing.start();ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyIntegrate"),"clicked"));wait(controller);qInfo()<<"Orion simulated SIP projection:"<<timing.elapsed()<<"ms"<<controller.status()<<controller.integrationReport();ASTRO_TEST_CHECK(controller.hasResult()&&controller.integrationReport().front().toMap()["wcsProjection"].toBool());
        const auto out=QUrl::fromLocalFile(folder+"/orion-wcs.fits");controller.exportImage(out,true);wait(controller);const auto exported=recording::readAstroImage(out.toLocalFile().toStdString());ASTRO_TEST_CHECK(exported.wcs&&exported.wcs->sipOrder==0);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);settle();scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));capture(QString("wcs-report-%1x%2.png").arg(size.width()).arg(size.height()));auto* details=visualItem(window.contentItem(),"deepSkyWcsSourceChecks");ASTRO_TEST_CHECK(details);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(details,"clicked"));settle();scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));capture(QString("wcs-sources-%1x%2.png").arg(size.width()).arg(size.height()));ASTRO_TEST_CHECK(QMetaObject::invokeMethod(details,"clicked"));root->setProperty("workflowStep",6);scroll->setProperty("contentY",0);capture(QString("wcs-result-%1x%2.png").arg(size.width()).arg(size.height()));}return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_HDR_QA")) {
        auto original=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());ASTRO_TEST_CHECK(original.channels==3);
        auto crop=original;crop.width=1024;crop.height=768;crop.samples.resize(std::size_t(crop.width)*crop.height*3);const int ox=(original.width-crop.width)/2,oy=(original.height-crop.height)/2;
        for(int y=0;y<crop.height;++y)std::copy_n(original.samples.begin()+(std::size_t(y+oy)*original.width+ox)*3,crop.width*3,crop.samples.begin()+std::size_t(y)*crop.width*3);if(crop.wcs)crop.wcs=crop.wcs->remap({1,0,-double(ox),0,1,-double(oy)});original.samples.clear();original.samples.shrink_to_fit();
        QList<QUrl> inputs;for(int n=0;n<3;++n){auto frame=crop;frame.exposure=std::array<double,3>{120,30,3}[n];frame.filter="RGB";for(auto& v:frame.samples)v=std::min(1.f,float(v*4*frame.exposure/120));const auto path=directory.filePath(QString("M42-%1s.fits").arg(frame.exposure));recording::writeAstroFits(path.toStdString(),frame);inputs<<QUrl::fromLocalFile(path);}controller.addFiles(inputs);wait(controller);
        const auto folder=qEnvironmentVariable("ASTRO_UI_HDR_QA");QDir().mkpath(folder);auto capture=[&](const QString& name){QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(folder+"/"+name));};
        controller.integrate(true,3,false,false,false,false,1,1,false,{},true);wait(controller);ASTRO_TEST_CHECK(controller.diagnosticMaps().size()==3);
        root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("openIntegrationMaps"),"clicked"));wait(controller);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);capture(QString("maps-%1x%2.png").arg(size.width()).arg(size.height()));}
        QMetaObject::invokeMethod(root->findChild<QObject*>("integrationMapsDialog"),"close");ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("openHdr"),"clicked"));
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);capture(QString("hdr-settings-%1x%2.png").arg(size.width()).arg(size.height()));}
        root->findChild<QObject*>("hdrReviewed")->setProperty("checked",true);QCoreApplication::processEvents();QElapsedTimer time;time.start();ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("hdrMerge"),"clicked"));wait(controller);qInfo()<<"Orion HDR interface fixture:"<<time.elapsed()<<"ms"<<controller.hdrReport();ASTRO_TEST_CHECK(controller.hdrReport()["recovered"].toULongLong()>0);root->setProperty("workflowStep",6);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);capture(QString("hdr-result-%1x%2.png").arg(size.width()).arg(size.height()));}
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("openIntegrationMaps"),"clicked"));wait(controller);
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);capture(QString("hdr-map-%1x%2.png").arg(size.width()).arg(size.height()));}
        controller.exportImage(QUrl::fromLocalFile(folder+"/orion-hdr.fits"),true);wait(controller);return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_FEATURES_QA")) {
        app::setBlindIndexDirectory(QStringLiteral(ASTRO_SOURCE_DIR "/build/astrometry-indexes"));
        QSettings().setValue("calibration/libraryDirectory",directory.filePath("masters"));
        recording::AstroImage master;master.width=master.height=64;master.bits=32;master.samples.assign(4096,.001f);master.camera="ASI071MC · séance de référence";master.masterRole="Dark";master.masterCount=30;master.exposure=30;master.hasTemperature=true;master.temperature=-10;master.binX=master.binY=1;
        app::storeCalibrationMaster(master,"ASI071MC · 30 s · −10 °C",{});
        controller.addFiles({QUrl::fromLocalFile(qEnvironmentVariable("ASTRO_UI_FITS_PATH"))});wait(controller);controller.integrate(true);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());
        auto settings=controller.finishSettings();settings["starSeparation"]=true;settings["starsGain"]=.8;ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(settings))));wait(controller);ASTRO_TEST_CHECK(controller.hasStarLayers());
        qInfo()<<"Real Orion star fits"<<controller.advancedReport()["starsAccepted"]<<controller.advancedReport()["starsDetected"]<<controller.advancedReport()["starsFitRms"];
        root->setProperty("workflowStep",6);root->setProperty("imageFocus",false);root->setProperty("expandedFamilies",QVariantList{5});root->setProperty("finishingTool",5);
        const auto folder=qEnvironmentVariable("ASTRO_UI_FEATURES_QA");QDir().mkpath(folder);
        auto capture=[&](const QString& path){for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(path));};
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window.resize(size);root->setSize(size);QMetaObject::invokeMethod(root,"openFamily",Q_ARG(QVariant,QVariant(5)));QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");controller.setStarLayerView(0);const auto suffix=QString("-%1x%2.png").arg(size.width()).arg(size.height());capture(folder+"/stars"+suffix);
            controller.setStarLayerView(2);capture(folder+"/residual"+suffix);controller.setStarLayerView(0);
            auto* masterDialog=root->findChild<QObject*>("masterLibraryDialog");ASTRO_TEST_CHECK(masterDialog);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(masterDialog,"open"));capture(folder+"/masters"+suffix);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(masterDialog,"close"));
            auto* astrometry=root->findChild<QObject*>("astrometryDialog");ASTRO_TEST_CHECK(astrometry);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(astrometry,"open"));root->findChild<QObject*>("astrometrySolveMode")->setProperty("currentIndex",1);capture(folder+"/blind"+suffix);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(astrometry,"close"));
        }
        return 0;
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_DRIZZLE_QA")||qEnvironmentVariableIsSet("ASTRO_UI_BAYER_QA")) {
        const bool bayerQa=qEnvironmentVariableIsSet("ASTRO_UI_BAYER_QA");
        auto original=recording::readAstroImage(qEnvironmentVariable("ASTRO_UI_FITS_PATH").toStdString());
        recording::AstroImage crop=original;crop.width=1024;crop.height=768;crop.samples.resize(std::size_t(crop.width)*crop.height*crop.channels);
        const int ox=(original.width-crop.width)/2,oy=(original.height-crop.height)/2;
        for(int y=0;y<crop.height;++y)std::copy_n(original.samples.begin()+(std::size_t(y+oy)*original.width+ox)*crop.channels,crop.width*crop.channels,crop.samples.begin()+std::size_t(y)*crop.width*crop.channels);
        if(crop.wcs)crop.wcs=crop.wcs->remap({1,0,-double(ox),0,1,-double(oy)});
        original.samples.clear();original.samples.shrink_to_fit();
        // QA mosaics sample the measured channel from the real RGB scene at
        // four controlled integer dither phases. This is an interface fixture,
        // not a claim that the user's original exposures contain this dithering.
        QList<QUrl> urls;for(int n=0;n<(bayerQa?8:2);++n){auto frame=crop;if(bayerQa){frame.width-=2;frame.height-=2;frame.channels=1;frame.bayer=domain::BayerPattern::RGGB;frame.samples.resize(std::size_t(frame.width)*frame.height);const int dx=n%2,dy=(n/2)%2;for(int y=0;y<frame.height;++y)for(int x=0;x<frame.width;++x)frame.samples[y*frame.width+x]=crop.samples[((y+dy)*crop.width+x+dx)*3+domain::bayerChannelAt(frame.bayer,x,y)];}const auto path=directory.filePath(QString("orion-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),frame);urls<<QUrl::fromLocalFile(path);}
        controller.addFiles(urls);wait(controller);
        root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);
        root->findChild<QObject*>("deepSkyAdvanced")->setProperty("expanded",true);
        root->findChild<QObject*>("deepSkyDrizzle")->setProperty("checked",true);
        root->findChild<QObject*>("deepSkyDrizzlePixelFraction")->setProperty("value",.8);
        if(bayerQa){auto* native=root->findChild<QObject*>("deepSkyDrizzleNativeBayer");native->setProperty("checked",true);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(native,"clicked"));}
        root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);
        const auto folder=qEnvironmentVariable(bayerQa?"ASTRO_UI_BAYER_QA":"ASTRO_UI_DRIZZLE_QA");QDir().mkpath(folder);
        auto capture=[&](const QString& path){for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}ASTRO_TEST_CHECK(window.grabWindow().save(path));};
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}) {
            window.resize(size);root->setSize(size);root->setProperty("workflowStep",1);for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}
            auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();
            scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));
            QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");capture(folder+QString("/drizzle-settings-%1x%2.png").arg(size.width()).arg(size.height()));
        }
        QElapsedTimer timing;timing.start();ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyIntegrate"),"clicked"));wait(controller);
        ASTRO_TEST_CHECK(controller.hasResult());qInfo()<<"Real Orion crop drizzle:"<<timing.elapsed()<<"ms"<<controller.integrationReport();
        ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels->width>(bayerQa?1000:2000));
        const auto result=QUrl::fromLocalFile(folder+"/orion-drizzle.fits");controller.exportImage(result,true);wait(controller);ASTRO_TEST_CHECK(QFileInfo::exists(result.toLocalFile()));
        for(const auto size:{QSize(1440,960),QSize(1100,800),QSize(900,700)}){window.resize(size);root->setSize(size);root->setProperty("workflowStep",6);QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyCompletionToast"),"close");capture(folder+QString("/drizzle-result-%1x%2.png").arg(size.width()).arg(size.height()));}
        window.resize(900,700);root->setSize({900,700});root->setProperty("workflowStep",1);root->setProperty("frameListOpen",false);for(int i=0;i<5;++i){QCoreApplication::processEvents();QThread::msleep(40);}
        auto* scroll=root->findChild<QObject*>("deepSkyInspector")->property("contentItem").value<QObject*>();
        scroll->setProperty("contentY",std::max(0.,scroll->property("contentHeight").toDouble()-scroll->property("height").toDouble()));capture(folder+"/drizzle-report-900x700.png");
        return 0;
    }
    // Opening the dialog restores stored limits; a user changing methods must
    // still get the appropriate default after that assignment breaks the binding.
    auto* calibrationMethod=root->findChild<QObject*>("catalogCalibrationMethod");
    auto* calibrationLimit=root->findChild<QObject*>("catalogCalibrationLimitField");
    ASTRO_TEST_CHECK(calibrationMethod && calibrationLimit);
    calibrationLimit->setProperty("text","24");
    calibrationMethod->setProperty("currentIndex",1);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(calibrationMethod,"activated",Q_ARG(int,1)));
    ASTRO_TEST_CHECK(calibrationLimit->property("text").toString()=="60");
    calibrationMethod->setProperty("currentIndex",0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(calibrationMethod,"activated",Q_ARG(int,0)));
    ASTRO_TEST_CHECK(calibrationLimit->property("text").toString()=="150");
    auto* finishTab=visualItem(root,"deepSkyFinishTab");ASTRO_TEST_CHECK(finishTab);
    ASTRO_TEST_CHECK(!finishTab->property("enabled").toBool());
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyIntegrate")->property("visible").toBool());
    auto* presetDialog=object->findChild<QObject*>("deepSkyPresetDialog");ASTRO_TEST_CHECK(presetDialog);
    auto* dialogBackground=presetDialog->property("background").value<QObject*>();ASTRO_TEST_CHECK(dialogBackground);
    ASTRO_TEST_CHECK(dialogBackground->property("color").value<QColor>()==QColor("#1c1e22"));
    auto* gear=visualItem(root,"deepSkyProjectMenu");ASTRO_TEST_CHECK(gear);
    const auto gearPoint=gear->mapToScene(QPointF(gear->width()/2,gear->height()/2));
    QMouseEvent gearPress(QEvent::MouseButtonPress,gearPoint,gearPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&gearPress);
    QMouseEvent gearRelease(QEvent::MouseButtonRelease,gearPoint,gearPoint,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QGuiApplication::sendEvent(&window,&gearRelease);QCoreApplication::processEvents();
    auto* projectOptions=root->findChild<QObject*>("deepSkyProjectOptions");ASTRO_TEST_CHECK(projectOptions && projectOptions->property("visible").toBool());
    ASTRO_TEST_CHECK(projectOptions->property("width").toDouble()>=200.);
    if(qEnvironmentVariableIsSet("ASTRO_UI_MENU_SNAPSHOT")) {
        QThread::msleep(200);QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_MENU_SNAPSHOT")));
    }
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(projectOptions,"close"));QCoreApplication::processEvents();
    const double tabsY=finishTab->mapToScene(QPointF()).y();
    auto* inspector=root->findChild<QObject*>("deepSkyInspector");auto* flickable=inspector->property("contentItem").value<QObject*>();ASTRO_TEST_CHECK(flickable);
    flickable->setProperty("contentY",100.);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(finishTab->mapToScene(QPointF()).y()==tabsY);
    flickable->setProperty("contentY",0.);
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
    {
        const auto previous=controller.sessionSnapshot();
        const auto plan=controller.integrationResources({{"calibrated",true}});
        ASTRO_TEST_CHECK(!plan["blocked"].toBool() && plan["temporaryBytes"].toULongLong()==160ULL*128*2*12);
        const auto work=directory.filePath("temporary space é # folder");ASTRO_TEST_CHECK(QDir().mkpath(work));
        controller.chooseIntegrationTemporaryDirectory(QUrl::fromLocalFile(work));
        ASTRO_TEST_CHECK(controller.integrationTemporaryDirectoryUrl().toLocalFile()==QFileInfo(work).canonicalFilePath());
        app::DeepSkyController other;ASTRO_TEST_CHECK(other.integrationTemporaryDirectory()==controller.integrationTemporaryDirectory());
        const auto sentinel=work+"/keep-user-file.txt";QFile marker(sentinel);ASTRO_TEST_CHECK(marker.open(QIODevice::WriteOnly));marker.write("keep");marker.close();
        controller.integrate(true);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());
        ASTRO_TEST_CHECK(QDir(work).entryList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot)==QStringList{"keep-user-file.txt"});
        const auto stable=controller.sessionSnapshot();
        QSettings().setValue("application/integrationTemporaryDirectory",work+"/missing");QMetaObject::invokeMethod(&controller,"changed");QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(controller.integrationResources({{"calibrated",true}})["blocked"].toBool());
        ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyIntegrate")->property("enabled").toBool());
        controller.integrate(true);ASTRO_TEST_CHECK(!controller.busy() && controller.sessionSnapshot().images["linear"]->rgb==stable.images["linear"]->rgb);
        controller.chooseIntegrationTemporaryDirectory({});controller.restoreSession(previous);
        auto excessive=previous;QVariantList groups;
        for(int n=0;n<63;++n){auto row=previous.state["frames"].toList().front().toMap();row["selected"]=true;row["group"]=QString("filter-%1").arg(n);groups<<row;}
        excessive.state["frames"]=groups;app::DeepSkyController grouped;grouped.restoreSession(excessive);
        const auto recovery=grouped.integrationResources({{"calibrated",true}});
        ASTRO_TEST_CHECK(recovery["blocked"].toBool() && recovery["error"].toString().contains("recovery",Qt::CaseInsensitive));
    }
    {
        // The actual QML action persists drizzle controls; portable results
        // and failed operations preserve the previous usable calculation.
        const auto original=controller.sessionSnapshot();const auto view=root->property("sessionState");
        auto* drizzle=root->findChild<QObject*>("deepSkyDrizzle");auto* fraction=root->findChild<QObject*>("deepSkyDrizzlePixelFraction");
        drizzle->setProperty("checked",true);root->findChild<QObject*>("deepSkyDiagnosticMaps")->setProperty("checked",true);fraction->setProperty("value",.75);QCoreApplication::processEvents();
        controller.setSelected(1,false);QCoreApplication::processEvents();ASTRO_TEST_CHECK(root->property("drizzleInvalid").toBool());
        ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyIntegrate")->property("enabled").toBool());controller.setSelected(1,true);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyIntegrate"),"clicked"));wait(controller);
        const auto result=controller.sessionSnapshot();ASTRO_TEST_CHECK(controller.integrationReport().size()==1&&controller.integrationReport().front().toMap()["drizzle"].toBool());ASTRO_TEST_CHECK(result.images["linear"]->width>300&&result.images["linear"]->height>250);
        ASTRO_TEST_CHECK(controller.projectOptions()["drizzle"].toBool()&&controller.projectOptions()["pixelFraction"].toDouble()==.75);
        ASTRO_TEST_CHECK(controller.diagnosticMaps().size()==3&&controller.projectOptions()["diagnosticMaps"].toBool());
        const auto mapKey=controller.diagnosticMaps().front().toMap()["key"].toString();controller.inspectDiagnostic(mapKey,0);wait(controller);ASTRO_TEST_CHECK(!controller.diagnosticImage().isNull()&&controller.diagnosticInfo()["key"]==mapKey);
        const auto mapPath=QUrl::fromLocalFile(directory.filePath("weights.fits"));controller.exportDiagnostic(mapPath,mapKey);wait(controller);const auto mapFits=recording::readAstroImage(mapPath.toLocalFile().toStdString());ASTRO_TEST_CHECK(mapFits.samples==result.images["diagnostic/"+mapKey]->rgb&&mapFits.fitsBitpix==-32);
        auto invalid=result;auto wrong=std::make_shared<processing::FloatImage>(*invalid.images["diagnostic/"+mapKey]);wrong->rgb[0]=-1;invalid.images["diagnostic/"+mapKey]=wrong;ASTRO_TEST_CHECK(!app::DeepSkyController::validSession(invalid));

        ASTRO_TEST_CHECK(app::DeepSkyController::validSession(result)&&app::DeepSkyController::validSession(controller.sessionSnapshot()));const auto path=QUrl::fromLocalFile(directory.filePath("drizzle.astroproject"));controller.savePortableProject(path);wait(controller);if(!QFileInfo::exists(path.toLocalFile()))qWarning()<<controller.status();
        app::DeepSkyController reopened;reopened.setProjectCacheDirectory(directory.filePath("drizzle-cache"));reopened.loadProject(path);wait(reopened);if(!reopened.hasResult())qWarning()<<reopened.status();
        ASTRO_TEST_CHECK(reopened.hasResult()&&reopened.integrationReport()==controller.integrationReport()&&reopened.projectOptions()["drizzle"].toBool()&&reopened.sessionSnapshot().images["linear"]->rgb==result.images["linear"]->rgb);ASTRO_TEST_CHECK(reopened.diagnosticMaps()==controller.diagnosticMaps()&&reopened.sessionSnapshot().images["diagnostic/"+mapKey]->rgb==result.images["diagnostic/"+mapKey]->rgb);
        controller.integrate(true,3,true,true,false,true,0,.8);wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==result.images["linear"]);
        auto* native=root->findChild<QObject*>("deepSkyDrizzleNativeBayer");ASTRO_TEST_CHECK(native&&!native->property("checked").toBool());
        native->setProperty("checked",true);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(native,"clicked"));QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(root->property("drizzleInvalid").toBool()&&fraction->property("value").toDouble()==1&&root->findChild<QObject*>("deepSkyDrizzleScale")->property("currentIndex").toInt()==0);
        controller.integrate(true,3,true,true,false,true,1,1,true);wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==result.images["linear"]);
        controller.restoreSession(original);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,view)));QCoreApplication::processEvents();ASTRO_TEST_CHECK(!drizzle->property("checked").toBool());
        ASTRO_TEST_CHECK(!native->property("checked").toBool());
    }

    {
        const auto original=controller.sessionSnapshot();const auto view=root->property("sessionState");controller.clear();
        recording::AstroImage hdr=data;recording::CelestialWcs hdrWcs;hdrWcs.crval={83.8,-5.4};hdrWcs.crpix={80.5,64.5};hdrWcs.cd={-.001,0,0,.001};hdrWcs.sipOrder=3;hdrWcs.sipA[12]=1e-5;hdrWcs.sipB[2]=-2e-5;hdr.wcs=hdrWcs;hdr.channels=3;hdr.filter="RGB";hdr.bits=32;hdr.samples.resize(std::size_t(hdr.width)*hdr.height*3);QList<QUrl> hdrUrls;
        for(int n=0;n<2;++n){hdr.exposure=n?6:60;for(int y=0;y<hdr.height;++y)for(int x=0;x<hdr.width;++x)for(int c=0;c<3;++c){double v=.02;for(const auto& star:stars)v+=.2*std::exp(-((x-star[0])*(x-star[0])+(y-star[1])*(y-star[1]))/5.);v+=2.5*std::exp(-((x-78)*(x-78)+(y-82)*(y-82))/40.);hdr.samples[(y*hdr.width+x)*3+c]=float(std::min(1.,v*(1-c*.2)*hdr.exposure/60));}const auto path=directory.filePath(QString("hdr-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),hdr);hdrUrls<<QUrl::fromLocalFile(path);}
        controller.addFiles(hdrUrls);wait(controller);root->setProperty("workflowStep",1);
        const QVariantList inputs{QVariantMap{{"index",0},{"exposure",60},{"saturation",1}},QVariantMap{{"index",1},{"exposure",6},{"saturation",1}}};
        controller.mergeHdr(inputs,.1,false);wait(controller);ASTRO_TEST_CHECK(!controller.hasResult());
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("openHdr"),"clicked"));QCoreApplication::processEvents();
        auto* threshold=visualItem(window.contentItem(),"hdrSaturation0");ASTRO_TEST_CHECK(threshold);const auto entered=QLocale().toString(.95,'g',8);threshold->setProperty("text",entered);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(threshold,"textEdited"));QCoreApplication::processEvents();ASTRO_TEST_CHECK(threshold->property("text").toString()==entered);
        ASTRO_TEST_CHECK(!root->findChild<QObject*>("hdrMerge")->property("enabled").toBool());root->findChild<QObject*>("hdrReviewed")->setProperty("checked",true);QCoreApplication::processEvents();ASTRO_TEST_CHECK(root->findChild<QObject*>("hdrMerge")->property("enabled").toBool());
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("hdrMerge"),"clicked"));wait(controller);if(!controller.hasResult())qWarning()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult()&&controller.hdrReport()["recovered"].toULongLong()>0&&controller.hdrReport()["clipped"].toULongLong()==0&&controller.diagnosticMaps().size()==2);
        const auto state=controller.sessionSnapshot();const auto file=QUrl::fromLocalFile(directory.filePath("hdr.astroproject"));controller.savePortableProject(file);wait(controller);app::DeepSkyController reopened;reopened.setProjectCacheDirectory(directory.filePath("hdr-cache"));reopened.loadProject(file);wait(reopened);ASTRO_TEST_CHECK(reopened.hdrReport()["imageKey"]==controller.hdrReport()["imageKey"]&&reopened.hdrReport()["recovered"]==controller.hdrReport()["recovered"]&&reopened.sessionSnapshot().images["linear"]->rgb==state.images["linear"]->rgb&&reopened.diagnosticMaps()==controller.diagnosticMaps());for(const auto& source:reopened.hdrReport()["sources"].toList())ASTRO_TEST_CHECK(QFileInfo::exists(source.toMap()["path"].toString()));
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"saveOptions"));ASTRO_TEST_CHECK(controller.projectOptions()["hdrOptions"].toMap()["sources"].toList().size()==2);
        const auto linked=QUrl::fromLocalFile(directory.filePath("hdr-linked.astroproject"));controller.saveProject(linked);wait(controller);app::DeepSkyController linkedController;linkedController.loadProject(linked);wait(linkedController);ASTRO_TEST_CHECK(linkedController.projectOptions()["hdrOptions"].toMap()==controller.projectOptions()["hdrOptions"].toMap());
        auto bad=inputs;auto same=bad[1].toMap();same["exposure"]=60;bad[1]=same;controller.mergeHdr(bad,.1,true);wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==state.images["linear"]&&controller.hdrReport()==state.state["hdrReport"]);
        controller.mergeHdr(inputs,.1,true);controller.cancel();wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==state.images["linear"]);
        controller.exportImage(QUrl::fromLocalFile(directory.filePath("hdr-result.fits")),true);wait(controller);const auto exported=recording::readAstroImage(directory.filePath("hdr-result.fits").toStdString());ASTRO_TEST_CHECK(exported.samples==state.images["linear"]->rgb&&exported.exposure==60);ASTRO_TEST_CHECK(exported.wcs&&exported.wcs->crpix==hdrWcs.crpix&&exported.wcs->sipA==hdrWcs.sipA);
        const auto diagnostic=QUrl::fromLocalFile(directory.filePath("hdr-diagnostic.fits"));controller.exportDiagnostic(diagnostic,"HDR/recovered");wait(controller);const auto diagnosticFits=recording::readAstroImage(diagnostic.toLocalFile().toStdString());ASTRO_TEST_CHECK(diagnosticFits.wcs&&diagnosticFits.wcs->sipB==hdrWcs.sipB&&diagnosticFits.samples==state.images["diagnostic/HDR/recovered"]->rgb);

        controller.clear();controller.restoreSession(original);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,view)));wait(controller);
    }

    {
        const auto original=controller.sessionSnapshot();const auto view=root->property("sessionState");controller.clear();wait(controller);
        auto cfa=data;cfa.bayer=domain::BayerPattern::RGGB;cfa.filter="RGB";cfa.bits=32;QList<QUrl> cfaUrls;
        for(int n=0;n<8;++n){const int dx=n%2,dy=(n/2)%2;for(int y=0;y<cfa.height;++y)for(int x=0;x<cfa.width;++x){double v=.02;for(const auto& star:stars)v+=.2*std::exp(-((x-star[0]-dx)*(x-star[0]-dx)+(y-star[1]-dy)*(y-star[1]-dy))/5.);cfa.samples[y*cfa.width+x]=float(v*(1+domain::bayerChannelAt(cfa.bayer,x,y)));}const auto path=directory.filePath(QString("cfa-%1.fits").arg(n));auto written=cfa;if(n%2==0)written.bayer=domain::BayerPattern::None;recording::writeAstroFits(path.toStdString(),written);cfaUrls<<QUrl::fromLocalFile(path);}
        controller.addFiles(cfaUrls);wait(controller);root->setProperty("workflowStep",1);QCoreApplication::processEvents();
        auto* native=root->findChild<QObject*>("deepSkyDrizzleNativeBayer");root->findChild<QObject*>("deepSkyDrizzle")->setProperty("checked",true);native->setProperty("checked",true);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(native,"clicked"));QCoreApplication::processEvents();ASTRO_TEST_CHECK(root->property("drizzleInvalid").toBool());root->findChild<QObject*>("deepSkyDrizzleBayerPattern")->setProperty("currentIndex",1);QCoreApplication::processEvents();ASTRO_TEST_CHECK(!root->property("drizzleInvalid").toBool());
        root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyIntegrate"),"clicked"));wait(controller);if(!controller.hasResult())qWarning()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult());
        const auto cfaResult=controller.sessionSnapshot();ASTRO_TEST_CHECK(controller.projectOptions()["nativeBayer"].toBool()&&controller.projectOptions()["bayerPattern"].toString()=="RGGB"&&controller.integrationReport().front().toMap()["nativeBayer"].toBool());
        const auto path=QUrl::fromLocalFile(directory.filePath("bayer-drizzle.astroproject"));controller.savePortableProject(path);wait(controller);
        app::DeepSkyController reopened;reopened.setProjectCacheDirectory(directory.filePath("bayer-cache"));reopened.loadProject(path);wait(reopened);ASTRO_TEST_CHECK(reopened.hasResult()&&reopened.projectOptions()["nativeBayer"].toBool()&&reopened.projectOptions()["bayerPattern"].toString()=="RGGB"&&reopened.integrationReport()==controller.integrationReport()&&reopened.sessionSnapshot().images["linear"]->rgb==cfaResult.images["linear"]->rgb);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,root->property("sessionState"))));QCoreApplication::processEvents();ASTRO_TEST_CHECK(native->property("checked").toBool()&&root->findChild<QObject*>("deepSkyDrizzleScale")->property("currentIndex").toInt()==0);
        // Same Bayer phase leaves colors uncovered and keeps the previous result.
        for(int n=0;n<8;++n)controller.setSelected(n,n==0||n==4);controller.integrate(true,0,false,true,false,true,1,1,true,"RGGB");wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==cfaResult.images["linear"]);
        controller.setSelected(1,true);controller.integrate(true,0,false,true,false,true,1,1,true,"BGGR");wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]==cfaResult.images["linear"]&&controller.status().contains("conflict",Qt::CaseInsensitive));
        controller.restoreSession(original);ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,view)));QCoreApplication::processEvents();
    }

    {
        const auto original=controller.sessionSnapshot();auto verified=original;
        const recording::CelestialWcs wcs{{83.82,-5.39},{80.5,64.5},{-.001,0,0,.001}};
        std::vector<processing::CatalogStar> catalog;
        for(int i=0;i<10;++i){const auto p=wcs.sky(stars[i][0],stars[i][1]);catalog.push_back({std::to_string(i+1),(*p)[0],(*p)[1],12.,5778.,true});}
        verified.state["astrometry"]=QVariantMap{{"verified",true},{"wcs",app::wcsValues(wcs)},{"catalog",app::catalogValues(catalog)},{"imageKey",QString::fromLatin1(app::linearImageKey(*original.images["linear"]))}};
        ASTRO_TEST_CHECK(controller.canRestoreSession(verified));controller.restoreSession(verified);QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(controller.hasCelestialCoordinates());
        auto* astroDialog=root->findChild<QObject*>("astrometryDialog");ASTRO_TEST_CHECK(astroDialog);
        const auto previousWcs=controller.astrometry();
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(astroDialog,"open"));
        controller.targetResolved({{"name","M 42"},{"source","CDS fixture"},{"ra",83.8201},{"dec",-5.3876}});
        controller.operationFinished(true,"Target lookup finished");QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(std::abs(astroDialog->findChild<QObject*>("astrometryRaField")->property("text").toDouble()-83.8201)<1e-7);
        ASTRO_TEST_CHECK(controller.astrometry()==previousWcs);
        ASTRO_TEST_CHECK(!toast->property("opened").toBool());
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(astroDialog,"close"));
        astroDialog->setProperty("targetQuery","M42");
        const auto expected=*wcs.sky(35,60);const auto p=controller.skyCoordinates(35,60);
        ASTRO_TEST_CHECK(std::abs(p["ra"].toDouble()-expected[0])<1e-10&&std::abs(p["dec"].toDouble()-expected[1])<1e-10);
        ASTRO_TEST_CHECK(controller.skyCoordinates(-1,0).isEmpty()&&controller.skyCoordinates(160,0).isEmpty());
        auto transformed=controller.finishSettings();transformed["rotation"]=1;transformed["mirrorHorizontal"]=true;
        transformed["geometry"]=QVariantMap{{"crop",true},{"x",6.},{"y",7.},{"width",100.},{"height",120.},{"angle",0.},{"scale",2.},{"interpolation",1},{"clamp",true},{"keepSize",false}};
        controller.apply(transformed);wait(controller);
        // Quarter rotation followed by a horizontal mirror maps (x,y) to (y,x).
        // Crop and resize map centres to 2*(coordinate-origin)+0.5.
        const auto moved=controller.skyCoordinates(2*(60-6)+.5,2*(35-7)+.5);
        ASTRO_TEST_CHECK(std::abs(moved["ra"].toDouble()-expected[0])<1e-10&&std::abs(moved["dec"].toDouble()-expected[1])<1e-10);
        controller.showRawComparison(true);ASTRO_TEST_CHECK(!controller.hasCelestialCoordinates()&&controller.skyCoordinates(10,10).isEmpty());controller.showRawComparison(false);
        const auto responsePath=directory.filePath("synthetic-response.csv");QFile responseFile(responsePath);ASTRO_TEST_CHECK(responseFile.open(QIODevice::WriteOnly));responseFile.write("wavelength_nm,red,green,blue\n336,0,0,0\n550,0.6,0.8,0.4\n1020,0,0,0\n");responseFile.close();
        ASTRO_TEST_CHECK(controller.importColorResponse(QUrl::fromLocalFile(responsePath)));
        const auto curve=controller.colorResponse();const auto previousSettings=controller.finishSettings();
        ASTRO_TEST_CHECK(library.saveEquipmentProfile("Synthetic QA response",curve,{{"camera","Fixture sensor"},{"source","Synthetic test, not camera measurement"}}));
        ASTRO_TEST_CHECK(controller.useEquipmentProfile(library.equipmentProfile("Synthetic QA response")));
        ASTRO_TEST_CHECK(controller.finishSettings()==previousSettings&&controller.astrometry()["responseCamera"]=="Fixture sensor");
        library.removeEquipmentProfile("Synthetic QA response");ASTRO_TEST_CHECK(controller.colorResponse()["response"].toList()==curve["response"].toList());
        const auto retained=controller.colorResponse();auto damaged=retained;damaged["dataHash"]=QString(64,'f');
        ASTRO_TEST_CHECK(!controller.useEquipmentProfile(damaged)&&controller.colorResponse()==retained);
        // A syntactically valid passband can still integrate to zero. Reject it
        // before publishing state that the session validator would refuse.
        auto underflow=retained;
        const QVariantList narrow{QVariant(QVariantList{550.,0.,0.,0.}),
            QVariant(QVariantList{550.+1e-10,std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::denorm_min()}),
            QVariant(QVariantList{550.+2e-10,0.,0.,0.})};
        underflow["response"]=narrow;underflow["dataHash"]=QString::fromLatin1(app::equipmentDataHash(narrow));
        ASTRO_TEST_CHECK(app::checkedEquipmentProfile(underflow).has_value());
        ASTRO_TEST_CHECK(!controller.useEquipmentProfile(underflow)&&controller.colorResponse()==retained);
        const auto portableState=controller.sessionSnapshot();controller.clear();controller.restoreSession(portableState);
        ASTRO_TEST_CHECK(controller.colorResponse()==retained&&controller.hasCelestialCoordinates());
        controller.restoreSession(original);QCoreApplication::processEvents();
    }
    ASTRO_TEST_CHECK(finishTab->property("enabled").toBool());
    const auto handed=controller.processingDocument().value<app::ImageDocument>();ASTRO_TEST_CHECK(handed.exposureCount==2&&handed.totalExposure==120&&handed.filters==QStringList{"R"}&&!handed.processingSummary.isEmpty());
    const auto integratedPixels=handed.pixels;
    const auto integratedPreview=controller.image();
    ASTRO_TEST_CHECK(controller.hasFinishComparison());
    const auto beforeFinish=controller.comparisonImage();
    ASTRO_TEST_CHECK(beforeFinish.size()==integratedPreview.size());
    for(const QString mode:{QString("before"),QString("split"),QString("result")}) {
        auto* comparisonButton=visualItem(root,"deepSkyCompare_"+mode);ASTRO_TEST_CHECK(comparisonButton);
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(comparisonButton,"clicked"));
        ASTRO_TEST_CHECK(root->property("comparisonMode").toString()==mode);
        ASTRO_TEST_CHECK(controller.image()==integratedPreview);
        ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels==integratedPixels);
    }
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
    ASTRO_TEST_CHECK(controller.comparisonImage()==beforeFinish.transformed(QTransform().rotate(90)));
    ASTRO_TEST_CHECK(controller.linearHistogram()["shadowChannels"].toList().size()==3);
    controller.apply(controller.suggestedStretch());wait(controller);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(controller.finishSettings()))));wait(controller);
    ASTRO_TEST_CHECK(!object->findChild<QObject*>("deepSkyVersionA"));
    ASTRO_TEST_CHECK(object->findChild<QObject*>("deepSkyExportButton"));
    // Disclosure families can coexist, close completely, and survive session restore.
    const auto disclosureSettings=controller.finishSettings();
    const auto disclosurePixels=controller.processingDocument().value<app::ImageDocument>().pixels;
    auto openFamilies=[&] {
        auto value=root->property("expandedFamilies");
        if(value.metaType()==QMetaType::fromType<QJSValue>())value=value.value<QJSValue>().toVariant();
        return value.toList();
    };
    auto toggleFamily=[&](int index) {
        auto* header=root->findChild<QObject*>(QString("deepSkyFamily%1Header").arg(index));
        ASTRO_TEST_CHECK(header && QMetaObject::invokeMethod(header,"clicked"));
        QCoreApplication::processEvents();
    };
    toggleFamily(1);
    ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyFamily0")->property("expanded").toBool());
    ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyFamily1")->property("expanded").toBool());
    toggleFamily(0);toggleFamily(1);
    ASTRO_TEST_CHECK(openFamilies().isEmpty());
    ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyResetFamily")->property("enabled").toBool());
    const auto closedState=root->property("sessionState");
    toggleFamily(2);toggleFamily(3);
    ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyFamily2")->property("expanded").toBool());
    ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyFamily3")->property("expanded").toBool());
    const auto openState=root->property("sessionState");
    const auto storedClosed=QJsonDocument::fromJson(QJsonDocument(QJsonObject::fromVariantMap(closedState.toMap())).toJson()).object().toVariantMap();
    const auto storedOpen=QJsonDocument::fromJson(QJsonDocument(QJsonObject::fromVariantMap(openState.toMap())).toJson()).object().toVariantMap();
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,QVariant(storedClosed))));
    ASTRO_TEST_CHECK(openFamilies().isEmpty());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,QVariant(storedOpen))));
    ASTRO_TEST_CHECK(openFamilies()==QVariantList({2,3}));
    auto legacyView=openState.toMap();legacyView.remove("expandedFamilies");legacyView["tool"]=2;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"restoreSessionState",Q_ARG(QVariant,QVariant(legacyView))));
    ASTRO_TEST_CHECK(openFamilies()==QVariantList{QVariant(2)});
    ASTRO_TEST_CHECK(controller.finishSettings()==disclosureSettings);
    ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels==disclosurePixels);
    root->setProperty("expandedFamilies",QVariantList{0});
    root->setProperty("finishingTool",0);
    auto beforeFamily=controller.finishSettings();beforeFamily["red"]=1.4;beforeFamily["rotation"]=1;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(beforeFamily))));wait(controller);
    root->setProperty("finishingTool",2);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"resetFamily"));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["red"].toDouble()==1. && controller.finishSettings()["rotation"].toInt()==1);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"undoFinish"));wait(controller);
    ASTRO_TEST_CHECK(controller.finishSettings()["red"].toDouble()==1.4);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(astro::app::defaultDeepSkySettings()))));wait(controller);
    root->setProperty("finishingTool",0);
    ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyCommitTransformation"));
    ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyUndo"));
    ASTRO_TEST_CHECK(!root->findChild<QObject*>("deepSkyRedo"));
    const auto output=QUrl::fromLocalFile(directory.filePath("linear.fits"));controller.exportImage(output,true);
    ASTRO_TEST_CHECK(controller.exporting()&&root->findChild<QObject*>("deepSkyExportButton")->property("busy").toBool());
    wait(controller);
    ASTRO_TEST_CHECK(!controller.exporting()&&!root->findChild<QObject*>("deepSkyExportButton")->property("busy").toBool());
    controller.exportImage(QUrl::fromLocalFile(directory.filePath("invalid.extension")),true);wait(controller);
    ASTRO_TEST_CHECK(!controller.exporting()&&!QFileInfo::exists(directory.filePath("invalid.extension")));
    ASTRO_TEST_CHECK(controller.linearExportBytes()==QFileInfo(output.toLocalFile()).size());
    auto* fitsSize=root->findChild<QObject*>("exportSizeHint");ASTRO_TEST_CHECK(fitsSize && fitsSize->property("estimatedBytes").toDouble()==controller.linearExportBytes());
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
    // Restoration applies automatically, just like changes from controls.
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
    const QString correctedCaption="M8 / M20 · 2 poses de 60 s\nFiltre Hα · observatoire";
    root->setProperty("captionOptions",QVariantMap{{"enabled",true},{"title",QString("M8 / M20")},{"top",QString("Poses Siril")},{"technicalText",correctedCaption}});
    QVariant savedOptions;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"saveOptions",Q_RETURN_ARG(QVariant,savedOptions))&&savedOptions.toBool());
    QObject::connect(&controller,&app::DeepSkyController::projectSaved,&library,&app::AppLibrary::recordProject);
    QObject::connect(&controller,&app::DeepSkyController::projectOpened,&library,&app::AppLibrary::recordProject);
    const auto project=QUrl::fromLocalFile(directory.filePath("session.astroproject"));controller.saveProject(project);controller.clear();root->setProperty("workflowStep",0);controller.loadProject(project);ASTRO_TEST_CHECK(controller.frames().size()==3&&!controller.frames()[2].toMap()["selected"].toBool()&&!controller.hasResult());
    ASTRO_TEST_CHECK(root->property("captionOptions").toMap()["title"].toString()=="M8 / M20");
    ASTRO_TEST_CHECK(root->property("captionOptions").toMap()["technicalText"].toString()==correctedCaption);
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
    // Advanced controls use the same history/project values as the normal workshop.
    auto advanced=app::defaultDeepSkySettings();advanced["ghs"]=true;advanced["ghsStrength"]=2.;advanced["ghsSymmetry"]=.2;advanced["maskType"]=2;advanced["advancedDenoise"]=true;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(advanced))));wait(controller);
    ASTRO_TEST_CHECK(controller.hasMask()&&controller.psfKernel().size()>0&&controller.advancedReport().contains("noiseBefore"));
    const auto maskedOutput=controller.processingDocument().value<app::ImageDocument>().pixels;
    controller.setShowingMask(true);ASTRO_TEST_CHECK(controller.showingMask()&&controller.processingDocument().value<app::ImageDocument>().pixels==maskedOutput);
    controller.restoreResult();ASTRO_TEST_CHECK(!controller.showingMask());
    const auto curve=controller.ghsCurve(advanced);ASTRO_TEST_CHECK(curve.size()==513&&curve.front().toDouble()==0&&curve.back().toDouble()==1);
    auto* advancedPopup=root->findChild<QObject*>("deepSkyAdvancedTools");ASTRO_TEST_CHECK(advancedPopup);
    root->setProperty("finishingTool",4);QCoreApplication::processEvents();
    for(int index=0;index<5;++index){advancedPopup->setProperty("toolIndex",index);QCoreApplication::processEvents();ASTRO_TEST_CHECK(advancedPopup->property("visible").toBool());if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH")){QThread::msleep(50);QCoreApplication::processEvents();ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")+QString(".advanced-%1.png").arg(index)));}}
    root->setProperty("finishingTool",0);
    const auto catalogPath=directory.filePath("references.csv");QFile catalog(catalogPath);ASTRO_TEST_CHECK(catalog.open(QIODevice::WriteOnly));catalog.write("x,y,red,green,blue\n25,25,1,1,1\n60,60,1,1,1\n100,100,1,1,1\n");catalog.close();
    const auto references=controller.importPhotometryCatalog(QUrl::fromLocalFile(catalogPath));ASTRO_TEST_CHECK(references.size()==3);
    auto badPhotometry=app::defaultDeepSkySettings();badPhotometry["photometry"]=true;badPhotometry["photometryReferences"]=QVariantList{};controller.apply(badPhotometry);wait(controller);ASTRO_TEST_CHECK(controller.processingDocument().value<app::ImageDocument>().pixels==maskedOutput);
    advanced["photometryReferences"]=references;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(advanced))));wait(controller);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"saveOptions",Q_RETURN_ARG(QVariant,savedOptions)));ASTRO_TEST_CHECK(savedOptions.toBool());
    const auto advancedProject=QUrl::fromLocalFile(directory.filePath("advanced.astroproject"));controller.saveProject(advancedProject);
    app::DeepSkyController reopened;reopened.loadProject(advancedProject);const auto advancedSaved=reopened.projectOptions()["finish"].toMap();ASTRO_TEST_CHECK(advancedSaved["ghs"].toBool()&&advancedSaved["maskType"].toInt()==2&&advancedSaved["photometryReferences"].toList()==references);
    auto hdrSettings=app::defaultDeepSkySettings();hdrSettings["stretch"]=false;hdrSettings["pedestal"]=1.;controller.apply(hdrSettings);wait(controller);
    const auto hdrPixels=controller.processingDocument().value<app::ImageDocument>().pixels;ASTRO_TEST_CHECK(*std::max_element(hdrPixels->rgb.begin(),hdrPixels->rgb.end())>1);
    const auto hdrPath=directory.filePath("processed-float.fits");controller.exportImage(QUrl::fromLocalFile(hdrPath),false);wait(controller);const auto hdrRead=recording::readAstroImage(hdrPath.toStdString());ASTRO_TEST_CHECK(hdrRead.samples==hdrPixels->rgb);
    {
    // A portable project saved before integration may have finishing settings
    // without A/B (legacy/API). Restore those controls and apply after stacking.
    controller.clear();controller.setProjectCacheDirectory(directory.filePath("project-cache"));controller.addFiles({urls[0]});wait(controller);
    auto pending=controller.sessionSnapshot();auto pendingFinish=app::defaultDeepSkySettings();pendingFinish["stretch"]=false;pendingFinish["red"]=1.37;
    pending.state["options"]=QVariantMap{{"finish",pendingFinish}};controller.restoreSession(pending);
    const auto pendingProject=QUrl::fromLocalFile(directory.filePath("before-integration.astroproject"));controller.savePortableProject(pendingProject);wait(controller);controller.clear();
    controller.loadProject(pendingProject);wait(controller);ASTRO_TEST_CHECK(!controller.hasResult());
    QVariant pendingValues;ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"options",Q_RETURN_ARG(QVariant,pendingValues)));
    ASTRO_TEST_CHECK(pendingValues.toMap()["red"].toDouble()==1.37&&!pendingValues.toMap()["stretch"].toBool());
    controller.integrate(true);wait(controller);ASTRO_TEST_CHECK(controller.hasResult()&&controller.finishSettings()["red"].toDouble()==1.37&&!controller.finishSettings()["stretch"].toBool()&&!root->property("finishDirty").toBool());
    }
    if(qEnvironmentVariableIsSet("ASTRO_UI_FITS_PATH")) {
        screenshotController=std::make_unique<app::DeepSkyController>();root->setProperty("controller",QVariant::fromValue(screenshotController.get()));
        screenshotController->addFiles({QUrl::fromLocalFile(qEnvironmentVariable("ASTRO_UI_FITS_PATH"))});wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->frames().size()==1);
        screenshotController->integrate(true);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->hasResult());
        auto settings=screenshotController->suggestedStretch();
        screenshotController->apply(settings);wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->processingDocument().value<app::ImageDocument>().pixels);
        root->setProperty("workflowStep",6);root->setProperty("imageFocus",true);QCoreApplication::processEvents();QThread::msleep(50);QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH")) {
            const auto snapshot=qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH");
            ASTRO_TEST_CHECK(window.grabWindow().save(snapshot));
            auto* chooser=root->findChild<QObject*>("deepSkyFinishFamily");ASTRO_TEST_CHECK(chooser);
            auto* popup=chooser->property("popup").value<QObject*>();ASTRO_TEST_CHECK(popup);
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(popup,"open"));
            QCoreApplication::processEvents();QThread::msleep(100);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(popup->property("contentItem").value<QObject*>()->property("count").toInt()==6);
            ASTRO_TEST_CHECK(window.grabWindow().save(snapshot+".dropdown.png"));
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(popup,"close"));QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyProjectMenu"),"clicked"));QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyProjectOptions")->property("visible").toBool());
            QThread::msleep(200);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(snapshot+".project-menu.png"));
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyProjectOptions"),"close"));QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyExportButton"),"clicked"));QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(snapshot+".export.png"));
            ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyExportPanel"),"close"));QCoreApplication::processEvents();
        }
        ASTRO_TEST_CHECK(root->findChild<QObject*>("deepSkyViewport")->property("height").toDouble()>280);
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH")) {
            const auto path=qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH");
            root->setProperty("workflowStep",1);root->setProperty("imageFocus",false);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(path+".stack.png"));
            root->setProperty("workflowStep",6);root->setProperty("imageFocus",true);
            QMetaObject::invokeMethod(presetDialog,"open");QCoreApplication::processEvents();QThread::msleep(200);QCoreApplication::processEvents();
            ASTRO_TEST_CHECK(window.grabWindow().save(path+".dialog.png"));
            QMetaObject::invokeMethod(presetDialog,"close");QCoreApplication::processEvents();
            root->setProperty("workflowStep",1);QCoreApplication::processEvents();QThread::msleep(250);QCoreApplication::processEvents();
            QQuickItem* helpTarget=nullptr;
            for(auto* item:root->findChildren<QQuickItem*>()) {
                const auto position=item->mapToScene(QPointF(item->width()/2,item->height()/2));
                if(item->isVisible() && item->objectName()=="helpIcon" && item->property("text").toString().contains("Siril") && position.y()>140 && position.y()<window.height()-100) {helpTarget=item;break;}
            }
            ASTRO_TEST_CHECK(helpTarget);
            const auto point=helpTarget->mapToScene(QPointF(helpTarget->width()/2,helpTarget->height()/2));
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
    if(screenshotController && qEnvironmentVariableIsSet("ASTRO_UI_ADVANCED_QA")) {
        auto advanced=screenshotController->finishSettings();advanced["background"]=false;advanced["operations"]=QVariantList{};advanced["ghs"]=true;advanced["ghsStrength"]=3.;advanced["ghsLocal"]=2.;advanced["ghsSymmetry"]=.02;advanced["ghsHighlights"]=.8;advanced["advancedDenoise"]=true;advanced["noiseScales"]=2;advanced["maskType"]=2;
        QElapsedTimer elapsed;elapsed.start();ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(advanced))));wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->finishSettings()["ghs"].toBool()&&screenshotController->hasMask());
        std::cout<<"Advanced real-data preview: "<<elapsed.elapsed()<<" ms\n";
        auto* popup=root->findChild<QObject*>("deepSkyAdvancedTools");root->setProperty("finishingTool",4);QCoreApplication::processEvents();QThread::msleep(200);QCoreApplication::processEvents();
        if(qEnvironmentVariableIsSet("ASTRO_UI_SNAPSHOT_PATH"))for(int i=0;i<5;++i){popup->setProperty("toolIndex",i);QCoreApplication::processEvents();QThread::msleep(50);QCoreApplication::processEvents();ASTRO_TEST_CHECK(window.grabWindow().save(qEnvironmentVariable("ASTRO_UI_SNAPSHOT_PATH")+QString(".advanced-%1.png").arg(i)));}
        root->setProperty("finishingTool",0);
        screenshotController->estimatePsf();wait(*screenshotController);if(!screenshotController->advancedReport().contains("estimatedFwhm"))std::cerr<<"PSF status: "<<screenshotController->status().toStdString()<<"\n";ASTRO_TEST_CHECK(screenshotController->advancedReport().contains("estimatedFwhm"));
        std::cout<<"Measured PSF: "<<screenshotController->advancedReport()["estimatedFwhm"].toDouble()<<" px, "<<screenshotController->advancedReport()["estimatedStars"].toInt()<<" stars\n";
        advanced["maskType"]=0;advanced["deconvolution"]=true;advanced["rlIterations"]=2;advanced["rlStopping"]=0.;advanced["rlDamping"]=.002;advanced["advancedDenoise"]=false;elapsed.restart();
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root,"setFinish",Q_ARG(QVariant,QVariant(advanced))));wait(*screenshotController);ASTRO_TEST_CHECK(screenshotController->advancedReport()["rlIterations"].toInt()==2);
        std::cout<<"Real-data two-iteration restoration: "<<elapsed.elapsed()<<" ms\n";
    }
    {
        const auto original=controller.sessionSnapshot();const auto view=root->property("workflowStep");controller.clear();
        recording::AstroImage frame;frame.width=frame.height=160;frame.channels=3;frame.exposure=60;frame.filter="RGB";frame.samples.resize(160*160*3);recording::CelestialWcs plane;plane.crval={83.82,-5.39};plane.crpix={80.5,80.5};plane.cd={-.001,0,0,.001};QList<QUrl> urls;
        for(int n=0;n<3;++n){frame.wcs=plane;frame.wcs->sipOrder=2;frame.wcs->sipA[12]=(n?2:-1)*.0001;frame.wcs->crpix[0]+=.6*n;for(int y=0;y<160;++y)for(int x=0;x<160;++x){const auto sky=frame.wcs->sky(x,y),p=plane.pixel((*sky)[0],(*sky)[1]);double v=.02;for(int iy=0;iy<4;++iy)for(int ix=0;ix<4;++ix)v+=(.16+.03*ix)*std::exp(-(std::pow((*p)[0]-20-ix*36-(iy%2)*3,2)+std::pow((*p)[1]-20-iy*36,2))/6.);for(int c=0;c<3;++c)frame.samples[(y*160+x)*3+c]=float(v*(1-c*.3));}if(n==2)frame.wcs->crpix[0]+=15;const auto path=directory.filePath(QString("wcs-%1.fits").arg(n));recording::writeAstroFits(path.toStdString(),frame);urls<<QUrl::fromLocalFile(path);}
        controller.addFiles(urls);wait(controller);root->setProperty("workflowStep",1);root->findChild<QObject*>("deepSkyDrizzle")->setProperty("checked",false);root->findChild<QObject*>("deepSkyWcsProjection")->setProperty("checked",true);root->findChild<QObject*>("deepSkyCalibrated")->setProperty("checked",true);controller.setSelected(1,false);controller.setSelected(2,false);QCoreApplication::processEvents();ASTRO_TEST_CHECK(root->property("wcsProjectionInvalid").toBool());controller.setSelected(1,true);controller.setSelected(2,true);QCoreApplication::processEvents();ASTRO_TEST_CHECK(!root->property("wcsProjectionInvalid").toBool());
        ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root->findChild<QObject*>("deepSkyIntegrate"),"clicked"));wait(controller);if(!controller.hasResult())qWarning()<<controller.status();ASTRO_TEST_CHECK(controller.hasResult()&&controller.projectOptions()["wcsProjection"].toBool());const auto snapshot=controller.sessionSnapshot();ASTRO_TEST_CHECK(app::DeepSkyController::validSession(snapshot));const auto report=controller.integrationReport().front().toMap();ASTRO_TEST_CHECK(report["wcsProjection"].toBool()&&report["wcsChecks"].toList().size()==2&&report["rejectedFrames"].toList().size()==1&&!report["rejectedFrames"].toList().front().toMap()["reason"].toString().isEmpty()&&!controller.astrometry()["verified"].toBool());const auto wcs=app::readWcs(controller.astrometry()["wcs"]);ASTRO_TEST_CHECK(wcs&&wcs->sipOrder==0);
        const auto path=QUrl::fromLocalFile(directory.filePath("wcs.astroproject"));controller.savePortableProject(path);wait(controller);app::DeepSkyController reopened;reopened.setProjectCacheDirectory(directory.filePath("wcs-cache"));reopened.loadProject(path);wait(reopened);ASTRO_TEST_CHECK(reopened.hasResult()&&reopened.integrationReport()==controller.integrationReport()&&reopened.projectOptions()["wcsProjection"].toBool()&&reopened.sessionSnapshot().images["linear"]->rgb==snapshot.images["linear"]->rgb);
        auto broken=snapshot;auto brokenReport=report;auto checks=report["wcsChecks"].toList();auto bad=checks[1].toMap();bad["rms"]=10;checks[1]=bad;brokenReport["wcsChecks"]=checks;broken.state["integrationReport"]=QVariantList{brokenReport};ASTRO_TEST_CHECK(!app::DeepSkyController::validSession(broken));
        const auto output=QUrl::fromLocalFile(directory.filePath("wcs-output.fits"));controller.exportImage(output,true);wait(controller);const auto exported=recording::readAstroImage(output.toLocalFile().toStdString());ASTRO_TEST_CHECK(exported.wcs&&exported.wcs->sipOrder==0&&exported.samples==snapshot.images["linear"]->rgb);
        frame.wcs->crpix[0]+=15;QFile::remove(urls[1].toLocalFile());recording::writeAstroFits(urls[1].toLocalFile().toStdString(),frame);controller.integrate(true,0,false,false,false,false,1,1,false,{},false,true);wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["linear"]->rgb==snapshot.images["linear"]->rgb&&controller.integrationReport()==snapshot.state["integrationReport"].toList());
        controller.restoreSession(original);root->findChild<QObject*>("deepSkyWcsProjection")->setProperty("checked",false);root->setProperty("workflowStep",view);QCoreApplication::processEvents();
    }
    std::cout<<"OK\n";
}
