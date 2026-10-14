#include "SessionController.hpp"
#include "Check.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QProcess>
#include <cstdlib>
#include <iostream>
using namespace astro::app;
void wait(SessionController& c) { QElapsedTimer t;t.start();while(c.busy()&&t.elapsed()<15000){QCoreApplication::processEvents();QThread::msleep(2);}ASTRO_TEST_CHECK(!c.busy()); }
QByteArray read(const QString& p){QFile f(p);ASTRO_TEST_CHECK(f.open(QIODevice::ReadOnly));return f.readAll();}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setOrganizationName("AstroCaptureTests");app.setApplicationName("SessionTests");
    if(app.arguments().size()==4 && app.arguments()[1]=="--interrupt-save") {
        SessionStorage storage(app.arguments()[2]);auto saved=storage.load();
        auto changed=std::make_shared<astro::processing::FloatImage>(*saved.workspaces["processing"].images["result"]);
        changed->rgb[0]=33.f;saved.workspaces["processing"].images["result"]=changed;
        const int stopAt=app.arguments()[3].toInt();int checkpoints=0;
        storage.save(saved,[&]{if(++checkpoints==stopAt)std::_Exit(86);});
        return 0;
    }
    QTemporaryDir temp;ASTRO_TEST_CHECK(temp.isValid());
    ProcessingController processing;DeepSkyController deep;MosaicController mosaic;
    SessionController session;session.setStorageDirectory(temp.filePath("recovery"));session.setProcessing(&processing);session.setDeepSky(&deep);session.setMosaic(&mosaic);
    auto image=std::make_shared<astro::processing::FloatImage>();image->width=12;image->height=8;image->rgb.resize(12*8*3);
    for(std::size_t i=0;i<image->rgb.size();++i)image->rgb[i]=float(i)*.0091f-.13f; // Preserve negative and HDR float samples.
    ImageDocument doc;doc.pixels=image;doc.preview=QImage(12,8,QImage::Format_RGB32);doc.preview.fill(Qt::gray);doc.name="Missing source";doc.protectedPaths={temp.filePath("gone.tif")};doc.annotation={{"technicalText","Custom metadata"}};doc.edgeMask=std::make_shared<const std::vector<float>>(96,.75f);
    ASTRO_TEST_CHECK(processing.openDocument(QVariant::fromValue(doc)));
    auto incomplete=processing.sessionSnapshot();incomplete.previews.remove("after");ASTRO_TEST_CHECK(!processing.canRestoreSession(incomplete));
    session.setUiState({{"tab","processing"},{"custom",.12345}});
    ASTRO_TEST_CHECK(session.saveNow());wait(session);ASTRO_TEST_CHECK(session.recoveryAvailable());
    SessionStorage storage(temp.filePath("recovery"));auto saved=storage.load();
    ASTRO_TEST_CHECK(saved.workspaces["processing"].images["result"]->rgb==image->rgb);
    ASTRO_TEST_CHECK(saved.workspaces["processing"].images["stack"]==saved.workspaces["processing"].images["result"]);
    ASTRO_TEST_CHECK(*saved.workspaces["processing"].planes["edges"]==*doc.edgeMask);
    const auto assetNames=QDir(temp.filePath("recovery")).entryList({"*.pixels","*.plane","*.png"},QDir::Files);
    saved.ui["custom"]=9;storage.save(saved);
    ASTRO_TEST_CHECK(QDir(temp.filePath("recovery")).entryList({"*.pixels","*.plane","*.png"},QDir::Files)==assetNames);
    const auto originalManifest=read(temp.filePath("recovery/session.json"));
    const auto originalFiles=QDir(temp.filePath("recovery")).entryList(QDir::Files|QDir::Hidden);
    for(int kind=0;kind<4;++kind) {
        auto invalid=saved;auto& space=invalid.workspaces["processing"];
        if(kind==0)for(int i=0;i<65;++i)space.images[QString::number(i)]=image;
        if(kind==1)for(int i=0;i<9;++i)space.planes[QString::number(i)]=doc.edgeMask;
        if(kind==2)for(int i=0;i<17;++i)space.previews[QString::number(i)]=doc.preview;
        if(kind==3)invalid.workspaces["unknown"]=space;
        bool rejected=false;try{storage.save(invalid);}catch(const std::exception&){rejected=true;}
        ASTRO_TEST_CHECK(rejected&&read(temp.filePath("recovery/session.json"))==originalManifest);
        ASTRO_TEST_CHECK(QDir(temp.filePath("recovery")).entryList(QDir::Files|QDir::Hidden)==originalFiles);
    }
    // Exit a separate process at every save checkpoint, without destructors.
    // Until publication, the previous manifest and all float pixels survive.
    const auto crashFolder=temp.filePath("interrupted");SessionStorage crashStorage(crashFolder);crashStorage.save(saved);
    const auto crashManifest=read(crashFolder+"/session.json");bool published=false;int interruptions=0;
    for(int checkpoint=1;checkpoint<=40;++checkpoint) {
        QProcess child;child.start(app.applicationFilePath(),{"--interrupt-save",crashFolder,QString::number(checkpoint)});
        ASTRO_TEST_CHECK(child.waitForStarted(5000)&&child.waitForFinished(15000));
        ASTRO_TEST_CHECK(child.exitStatus()==QProcess::NormalExit);
        if(child.exitCode()==0){published=true;break;}
        ASTRO_TEST_CHECK(child.exitCode()==86);++interruptions;
        ASTRO_TEST_CHECK(read(crashFolder+"/session.json")==crashManifest);
        ASTRO_TEST_CHECK(SessionStorage(crashFolder).load().workspaces["processing"].images["result"]->rgb==image->rgb);
    }
    ASTRO_TEST_CHECK(published&&interruptions>=2);
    ASTRO_TEST_CHECK(SessionStorage(crashFolder).load().workspaces["processing"].images["result"]->rgb[0]==33.f);
    std::cout<<"Abrupt process exit: "<<interruptions<<" save checkpoints preserve the previous recovery\n";
    auto changed=std::make_shared<astro::processing::FloatImage>(*image);changed->rgb[0]=33.f;saved.workspaces["processing"].images["result"]=changed;
    bool cancelled=false;try{storage.save(saved,[]{throw std::runtime_error("cancel");});}catch(...){cancelled=true;}
    ASTRO_TEST_CHECK(cancelled && read(temp.filePath("recovery/session.json"))==originalManifest);
    ProcessingController p2;DeepSkyController d2;MosaicController m2;SessionController resumed;resumed.setStorageDirectory(temp.filePath("recovery"));resumed.setProcessing(&p2);resumed.setDeepSky(&d2);resumed.setMosaic(&m2);
    bool restored=false;QObject::connect(&resumed,&SessionController::sessionRestored,[&]{restored=true;});
    ASTRO_TEST_CHECK(resumed.restoreLast());wait(resumed);
    ASTRO_TEST_CHECK(restored&&p2.stillImage()&&p2.sessionSnapshot().images["result"]->rgb==image->rgb);
    ASTRO_TEST_CHECK(p2.captionTechnicalText()=="Custom metadata" && resumed.status().contains("unavailable"));
    ASTRO_TEST_CHECK(!resumed.restoreLast()); // Never replace current work.
    p2.exportImage(QUrl::fromLocalFile(temp.filePath("cached.png")));
    QElapsedTimer t;t.start();while(p2.busy()&&t.elapsed()<15000){QCoreApplication::processEvents();QThread::msleep(2);}ASTRO_TEST_CHECK(QFileInfo::exists(temp.filePath("cached.png")));
    auto manifest=QJsonDocument::fromJson(originalManifest).object();auto spaces=manifest["workspaces"].toObject();auto ps=spaces["processing"].toObject();auto refs=ps["images"].toObject();const auto pixelName=refs["result"].toObject()["file"].toString();
    QFile corrupt(temp.filePath("recovery/"+pixelName));ASTRO_TEST_CHECK(corrupt.open(QIODevice::ReadWrite));corrupt.seek(20);corrupt.write("BAD!",4);corrupt.close();
    ProcessingController p3;DeepSkyController d3;MosaicController m3;SessionController rejected;rejected.setStorageDirectory(temp.filePath("recovery"));rejected.setProcessing(&p3);rejected.setDeepSky(&d3);rejected.setMosaic(&m3);
    ASTRO_TEST_CHECK(rejected.restoreLast());wait(rejected);ASTRO_TEST_CHECK(!p3.stillImage()&&!d3.hasResult()&&!m3.hasResult()&&rejected.status().contains("damaged"));
    QFile unrelated(temp.filePath("recovery/original.png"));ASTRO_TEST_CHECK(unrelated.open(QIODevice::WriteOnly));unrelated.write("keep");unrelated.close();rejected.discardRecovery();ASTRO_TEST_CHECK(QFileInfo::exists(unrelated.fileName())&&!storage.available());
    std::cout<<"Atomic recovery, exact float pixels, cached export, integrity checks and asset reuse passed\n";
}
