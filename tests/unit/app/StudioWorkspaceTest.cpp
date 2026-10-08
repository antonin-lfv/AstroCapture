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
#include <QTranslator>
#include "AppLibrary.hpp"
#include "Check.hpp"

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
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);QTemporaryDir directory;ASTRO_TEST_CHECK(directory.isValid());
    QCoreApplication::setOrganizationName("AstroCaptureTest");QCoreApplication::setApplicationName("StudioWorkspaceTest");
    QCoreApplication::setApplicationVersion("1.2.0");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    QTranslator translator;if(translator.load(qEnvironmentVariable("ASTRO_UI_TRANSLATION_PATH")))app.installTranslator(&translator);
    qmlRegisterType<astro::app::AppLibrary>("AstroCapture",1,0,"AppLibrary");
    astro::app::AppLibrary library;QQmlEngine engine;
    QQuickWindow window;window.resize(1280,820);window.show();
    auto navigation=create(engine,"StudioNavigation");auto* nav=qobject_cast<QQuickItem*>(navigation.get());
    nav->setParentItem(window.contentItem());nav->setSize({1280,104});QCoreApplication::processEvents();
    QQmlExpression navigationHook(engine.rootContext(),nav,"navigate.connect(function(tab){currentTab=tab;})");navigationHook.evaluate();ASTRO_TEST_CHECK(!navigationHook.hasError());
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(nav,"studioTab_capture"),"clicked"));
    ASTRO_TEST_CHECK(nav->property("currentTab").toString()=="capture");
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
    ASTRO_TEST_CHECK(projectFile.remove());ASTRO_TEST_CHECK(!reopened.recentProjects()[0].toMap()["exists"].toBool());
    nav->setProperty("currentTab","home");
    for(const auto size:{QSize(1280,820),QSize(900,640)}) {
        window.resize(size);nav->setSize({double(size.width()),104});content->setSize({double(size.width()),double(size.height()-104)});QCoreApplication::processEvents();QThread::msleep(100);QCoreApplication::processEvents();
        ASTRO_TEST_CHECK(item(content,"homeCapture")->width()>0);
        if(!qEnvironmentVariableIsEmpty("ASTRO_STUDIO_SCREENSHOT"))window.grabWindow().save(qEnvironmentVariable("ASTRO_STUDIO_SCREENSHOT")+QString("-%1.png").arg(size.width()));
    }
}
