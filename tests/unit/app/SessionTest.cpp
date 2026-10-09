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
#include <iostream>
using namespace astro::app;
void wait(SessionController& c) { QElapsedTimer t;t.start();while(c.busy()&&t.elapsed()<15000){QCoreApplication::processEvents();QThread::msleep(2);}ASTRO_TEST_CHECK(!c.busy()); }
QByteArray read(const QString& p){QFile f(p);ASTRO_TEST_CHECK(f.open(QIODevice::ReadOnly));return f.readAll();}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setOrganizationName("AstroCaptureTests");app.setApplicationName("SessionTests");
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
