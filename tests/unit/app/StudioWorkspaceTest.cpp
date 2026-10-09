#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QThread>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QTranslator>
#include "AppLibrary.hpp"
#include "Check.hpp"
#include "QuitGuard.hpp"
#include <QEvent>

static QQuickItem* item(QQuickItem* root,const QString& name) {
    if(root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* found=item(child,name))return found;
    return nullptr;
}
static std::unique_ptr<QObject> create(QQmlEngine& engine,const QString& name,const QVariantMap& properties={}) {
    QFile file(QStringLiteral(ASTRO_SOURCE_DIR "/app/qml/")+name+".qml");
    ASTRO_TEST_CHECK(file.open(QIODevice::ReadOnly));
    QQmlComponent component(&engine);component.setData("import AstroCapture 1.0\n"+file.readAll(),QUrl::fromLocalFile(file.fileName()));
    if(component.isError())qWarning()<<component.errors();ASTRO_TEST_CHECK(component.isReady());
    auto result=std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
    if(!result)qWarning()<<component.errors();ASTRO_TEST_CHECK(result);return result;
}
class GuardedWindow : public QWindow {
public:
    bool permitClose=false;
    int closeAttempts=0;
protected:
    bool event(QEvent* event) override {
        if(event->type()==QEvent::Close){++closeAttempts;if(!permitClose){event->ignore();return true;}}
        return QWindow::event(event);
    }
};
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);QTemporaryDir directory;ASTRO_TEST_CHECK(directory.isValid());
    QCoreApplication::setOrganizationName("AstroCaptureTest");QCoreApplication::setApplicationName("StudioWorkspaceTest");
    QCoreApplication::setApplicationVersion("1.2.0");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    QTranslator translator;if(translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))app.installTranslator(&translator);
    qmlRegisterType<astro::app::AppLibrary>("AstroCapture",1,0,"AppLibrary");
    // Native Quit must respect a rejected close, then allow a confirmed close.
    GuardedWindow nativeWindow;nativeWindow.show();
    astro::app::QuitGuard quitGuard(&nativeWindow);
    QObject quitSource;quitSource.installEventFilter(&quitGuard);
    QEvent firstQuit(QEvent::Quit);QCoreApplication::sendEvent(&quitSource,&firstQuit);
    ASTRO_TEST_CHECK(nativeWindow.isVisible() && nativeWindow.closeAttempts==1);
    nativeWindow.permitClose=true;
    QEvent confirmedQuit(QEvent::Quit);QCoreApplication::sendEvent(&quitSource,&confirmedQuit);
    ASTRO_TEST_CHECK(!nativeWindow.isVisible() && nativeWindow.closeAttempts==2);
    astro::app::AppLibrary library;QQmlEngine engine;
    QQuickWindow window;window.resize(1280,820);window.show();
    auto quitDialog=create(engine,"QuitConfirmation",{{"hasProject",true},{"hasProcessing",true},{"exportTargets",QVariantList{QVariantMap{{"id","deepSky"},{"name","Deep sky"},{"ready",true}}}}});
    quitDialog->setProperty("parent",QVariant::fromValue(window.contentItem()));
    QQmlExpression quitHook(engine.rootContext(),quitDialog.get(),"saveProjectRequested.connect(function(){saveStatus='project';});saveSettingsRequested.connect(function(){saveStatus='settings';});exportRequested.connect(function(target){saveStatus=target;});quitRequested.connect(function(){saveStatus='quit';})");
    quitHook.evaluate();ASTRO_TEST_CHECK(!quitHook.hasError());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitDialog.get(),"open"));QCoreApplication::processEvents();
    auto* quitSave=quitDialog->findChild<QObject*>("quitSaveProject");
    auto* quitExport=quitDialog->findChild<QObject*>("quitExportTiff");
    auto* quitSettings=quitDialog->findChild<QObject*>("quitSaveSettings");
    auto* quitDiscard=quitDialog->findChild<QObject*>("quitDiscard");
    ASTRO_TEST_CHECK(quitSave && quitExport && quitSettings && quitDiscard);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitSave,"clicked"));ASTRO_TEST_CHECK(quitDialog->property("saveStatus").toString()=="project");
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitExport,"clicked"));ASTRO_TEST_CHECK(quitDialog->property("saveStatus").toString()=="deepSky");
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitSettings,"clicked"));ASTRO_TEST_CHECK(quitDialog->property("saveStatus").toString()=="settings");
    quitDialog->setProperty("busy",true);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!quitDiscard->property("enabled").toBool() && !quitSave->property("enabled").toBool());
    quitDialog->setProperty("busy",false);quitDialog->setProperty("recording",true);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!quitDiscard->property("enabled").toBool());
    quitDialog->setProperty("recording",false);
    if(!qEnvironmentVariableIsEmpty("ASTRO_QUIT_SCREENSHOT")){QThread::msleep(200);QCoreApplication::processEvents();window.grabWindow().save(qEnvironmentVariable("ASTRO_QUIT_SCREENSHOT"));}
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitDialog->findChild<QObject*>("quitCancel"),"clicked"));QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!quitDialog->property("visible").toBool() && quitDialog->property("saveStatus").toString()!="quit");
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(quitDiscard,"clicked"));ASTRO_TEST_CHECK(quitDialog->property("saveStatus").toString()=="quit");
    auto navigation=create(engine,"StudioNavigation");auto* nav=qobject_cast<QQuickItem*>(navigation.get());
    nav->setParentItem(window.contentItem());nav->setSize({1280,104});QCoreApplication::processEvents();
    QQmlExpression navigationHook(engine.rootContext(),nav,"navigate.connect(function(tab){currentTab=tab;})");navigationHook.evaluate();ASTRO_TEST_CHECK(!navigationHook.hasError());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(nav,"studioTab_capture"),"clicked"));
    ASTRO_TEST_CHECK(nav->property("currentTab").toString()=="capture");
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(nav,"studioSettings"),"clicked"));
    ASTRO_TEST_CHECK(nav->property("currentTab").toString()=="settings");
    nav->setProperty("currentTab","home");
    ASTRO_TEST_CHECK(item(nav,"studioTab_home"));
    ASTRO_TEST_CHECK(item(nav,"studioTab_home")->property("selected").toBool());
    nav->setProperty("currentTab","deepSky");QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(nav,"studioTab_deepSky")->property("selected").toBool());
    nav->setProperty("recording",true);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(nav,"studioTab_home")->isEnabled());
    ASTRO_TEST_CHECK(item(nav,"studioTab_capture")->isEnabled());
    ASTRO_TEST_CHECK(item(nav,"studioTab_guide")->isEnabled());
    ASTRO_TEST_CHECK(!item(nav,"studioTab_processing")->isEnabled());
    ASTRO_TEST_CHECK(!item(nav,"studioSettings")->isEnabled());
    nav->setProperty("recording",false);nav->setSize({900,104});QCoreApplication::processEvents();
    for(auto* child:nav->childItems())ASTRO_TEST_CHECK(child->width()<=900);
    auto home=create(engine,"HomeWorkspace",{{"library",QVariant::fromValue(&library)}});
    auto* content=qobject_cast<QQuickItem*>(home.get());content->setParentItem(window.contentItem());content->setY(104);content->setSize({1280,716});QCoreApplication::processEvents();
    QQmlExpression homeHook(engine.rootContext(),content,"navigate.connect(function(tab){processingName=tab;});chooseProject.connect(function(){processingName='projectDialog';})");homeHook.evaluate();ASTRO_TEST_CHECK(!homeHook.hasError());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(content,"homeDeepSky"),"clicked"));
    ASTRO_TEST_CHECK(content->property("processingName").toString()=="deepSky");
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(content,"homeOpenProject"),"clicked"));
    ASTRO_TEST_CHECK(content->property("processingName").toString()=="projectDialog");
    ASTRO_TEST_CHECK(!item(content,"homeResumeDeepSky")->isVisible());
    content->setProperty("deepSkyOpen",true);content->setProperty("deepSkyCount",12);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(content,"homeResumeDeepSky")->isVisible());
    content->setProperty("recording",true);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(content,"homeCapture")->isEnabled());
    ASTRO_TEST_CHECK(!item(content,"homeProcessing")->isEnabled());
    ASTRO_TEST_CHECK(!item(content,"homeDeepSky")->isEnabled());
    ASTRO_TEST_CHECK(!item(content,"homeOpenProject")->isEnabled());
    content->setProperty("recording",false);content->setProperty("projectBusy",true);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!item(content,"homeOpenProject")->isEnabled());
    content->setProperty("projectBusy",false);
    const auto project=QUrl::fromLocalFile(directory.filePath("M8-M20.astroproject"));
    QFile projectFile(project.toLocalFile());ASTRO_TEST_CHECK(projectFile.open(QIODevice::WriteOnly));projectFile.write("{}");projectFile.close();
    library.recordProject(project);library.recordProject(project);ASTRO_TEST_CHECK(library.recentProjects().size()==1);
    astro::app::AppLibrary reopened;ASTRO_TEST_CHECK(reopened.recentProjects().size()==1);
    ASTRO_TEST_CHECK(reopened.recentProjects()[0].toMap()["exists"].toBool());
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(content,"homeRemoveProject"));
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(content,"homeRemoveProject"),"clicked"));
    ASTRO_TEST_CHECK(library.recentProjects().isEmpty() && QFileInfo::exists(project.toLocalFile()));
    astro::app::AppLibrary removed;ASTRO_TEST_CHECK(removed.recentProjects().isEmpty());
    library.recordProject(project);
    const auto video=QUrl::fromLocalFile(directory.filePath("recent.ser"));QFile videoFile(video.toLocalFile());ASTRO_TEST_CHECK(videoFile.open(QIODevice::WriteOnly));videoFile.write("test");videoFile.close();
    library.recordVideo(video);QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(content,"homeRemoveVideo"));
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(content,"homeRemoveVideo"),"clicked"));
    ASTRO_TEST_CHECK(library.recentVideos().isEmpty() && QFileInfo::exists(video.toLocalFile()));
    library.recordVideo(video);
    ASTRO_TEST_CHECK(projectFile.remove() && videoFile.remove());library.pruneUnavailableRecents();
    ASTRO_TEST_CHECK(library.recentProjects().isEmpty() && library.recentVideos().isEmpty());
    astro::app::AppLibrary pruned;ASTRO_TEST_CHECK(pruned.recentProjects().isEmpty() && pruned.recentVideos().isEmpty());
    nav->setProperty("currentTab","home");
    for(const auto size:{QSize(1280,820),QSize(900,640)}) {
        window.resize(size);nav->setSize({double(size.width()),104});content->setSize({double(size.width()),double(size.height()-104)});QCoreApplication::processEvents();QThread::msleep(100);QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(item(content,"homeCapture")->width()>0);
        if(!qEnvironmentVariableIsEmpty("ASTRO_STUDIO_SCREENSHOT"))window.grabWindow().save(qEnvironmentVariable("ASTRO_STUDIO_SCREENSHOT")+QString("-%1.png").arg(size.width()));
    }
}
