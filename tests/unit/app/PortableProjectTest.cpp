#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include "ProjectStorage.hpp"
#include "SessionController.hpp"
#include "ImagePreview.hpp"
#include "Check.hpp"
#include <astro/recording/AstroImage.hpp>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QtEndian>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif
using namespace astro;
using namespace astro::app;
template<class T>void wait(T& c){QElapsedTimer t;t.start();while(c.busy()&&t.elapsed()<30000){QCoreApplication::processEvents();QThread::msleep(1);}ASTRO_TEST_CHECK(!c.busy());QCoreApplication::processEvents();}
QByteArray read(const QString& path){QFile f(path);ASTRO_TEST_CHECK(f.open(QIODevice::ReadOnly));return f.readAll();}
void write(const QString& path,const QByteArray& bytes){QFile f(path);ASTRO_TEST_CHECK(f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size());}
QByteArray alterManifest(const QByteArray& archive,const std::function<void(QJsonObject&)>& edit){
 const auto length=qFromLittleEndian<quint32>(archive.constData()+8);auto object=QJsonDocument::fromJson(archive.mid(44,length)).object();edit(object);
 const auto manifest=QJsonDocument(object).toJson(QJsonDocument::Compact);QByteArray result=ProjectStorage::magic();char size[4];qToLittleEndian<quint32>(manifest.size(),size);result.append(size,4);result+=QCryptographicHash::hash(manifest,QCryptographicHash::Sha256);return result+manifest+archive.mid(44+length);
}
int main(int argc,char** argv){
 QGuiApplication application(argc,argv);application.setOrganizationName("AstroCaptureTests");application.setApplicationName("PortableProjects");QTemporaryDir dir;ASTRO_TEST_CHECK(dir.isValid());
 const auto originals=dir.filePath("originals");ASTRO_TEST_CHECK(QDir().mkpath(originals));
 auto linear=std::make_shared<processing::FloatImage>(processing::FloatImage{64,48,std::vector<float>(64*48*3)});
 for(std::size_t p=0;p<linear->rgb.size();++p)linear->rgb[p]=float(p%307)/97.f-.13f;
 recording::AstroImage source;source.width=64;source.height=48;source.channels=3;source.bits=32;source.exposure=30;source.filter="RGB";source.samples=linear->rgb;
 const auto sourcePath=originals+"/light.fits";recording::writeAstroFits(sourcePath.toStdString(),source);
 const auto maskPath=originals+"/mask.png";QImage mask(64,48,QImage::Format_Grayscale8);mask.fill(128);ASTRO_TEST_CHECK(mask.save(maskPath));
 DeepSkyController controller;controller.setProjectCacheDirectory(dir.filePath("cache"));controller.addFiles({QUrl::fromLocalFile(sourcePath)});wait(controller);ASTRO_TEST_CHECK(controller.frames().size()==1);
 auto s=controller.sessionSnapshot();s.images={{"linear",linear},{"finished",linear},{"stack/R",linear},{"stack/G",linear},{"stack/B",linear}};
 const auto preview=makeImagePreview(*linear);s.previews={{"display",preview},{"result",preview},{"linear",preview},{"geometry",preview},{"maskSource",preview},{"comparison",preview}};
 auto a=defaultDeepSkySettings();a["stretch"]=false;a["maskType"]=5;a["maskFile"]=maskPath;
 auto b=a;b["red"]=1.25;b["maskType"]=4;b["maskStrokes"]=QVariantList{QVariantMap{{"points",QVariantList{QVariant::fromValue(QVariantList{.3,.4})}},{"radius",.1},{"hardness",.7},{"opacity",.6},{"value",0.}}};
 s.state["finish"]=a;s.state["options"]=QVariantMap{{"finish",a},{"versions",QVariantMap{{"A",a},{"B",b},{"active","A"}}},{"caption",QVariantMap{{"technicalText","Corrected camera and exposure"}}},{"view",QVariantMap{{"tool",4},{"zoom",2.3},{"fit",false}}}};
 s.state["displayingResult"]=true;s.state["groups"]=QStringList{"R","G","B"};s.state["counts"]=QVariantMap{{"R",1},{"G",1},{"B",1}};s.state["exposures"]=QVariantMap{{"R",30},{"G",30},{"B",30}};s.state["references"]=QVariantMap{{"RGB",sourcePath}};
 controller.restoreSession(s);ASTRO_TEST_CHECK(controller.hasResult());controller.loadMaskFile(QUrl::fromLocalFile(maskPath));wait(controller);
 const auto originalMask=read(maskPath);const auto archive=dir.filePath("portable.astroproject");controller.savePortableProject(QUrl::fromLocalFile(archive));wait(controller);
 ASTRO_TEST_CHECK(QFileInfo::exists(archive));const auto bytes=read(archive);ASTRO_TEST_CHECK(bytes.startsWith(ProjectStorage::magic())&&bytes.size()<controller.portableProjectBytes());
 // Failed or cancelled saves do not replace an existing archive or sources.
 const auto destination=dir.filePath("preserved.astroproject");write(destination,"keep existing file");bool cancelled=false;
 try{ProjectStorage::save(destination,controller.sessionSnapshot(),[]{throw std::runtime_error("cancel");});}catch(...){cancelled=true;}ASTRO_TEST_CHECK(cancelled&&read(destination)=="keep existing file");
 cancelled=false;try{ProjectStorage::save(destination,controller.sessionSnapshot(),{},[](int){throw std::runtime_error("cancel during payload");});}catch(...){cancelled=true;}ASTRO_TEST_CHECK(cancelled&&read(destination)=="keep existing file");
 const auto sourceBefore=read(sourcePath);auto changedSource=source;changedSource.samples[0]=.2f;const auto changedPath=dir.filePath("changed.fits");recording::writeAstroFits(changedPath.toStdString(),changedSource);write(sourcePath,read(changedPath));controller.savePortableProject(QUrl::fromLocalFile(destination));wait(controller);ASTRO_TEST_CHECK(read(destination)=="keep existing file"&&controller.status().contains("changed since import"));write(sourcePath,sourceBefore);
 controller.savePortableProject(QUrl::fromLocalFile(maskPath));ASTRO_TEST_CHECK(!controller.busy()&&read(maskPath)==originalMask);
 const auto moved=dir.filePath("moved.astroproject");ASTRO_TEST_CHECK(QFile::rename(archive,moved));ASTRO_TEST_CHECK(QDir(originals).removeRecursively());controller.clear();
 QVariantMap restoredView;QObject::connect(&controller,&DeepSkyController::projectRestored,[&](const QVariantMap& view){restoredView=view;});controller.loadProject(QUrl::fromLocalFile(moved));wait(controller);
 ASTRO_TEST_CHECK(controller.hasResult()&&controller.missingSources().isEmpty()&&restoredView["zoom"].toDouble()==2.3);
 auto opened=controller.sessionSnapshot();ASTRO_TEST_CHECK(opened.images["linear"]->rgb==linear->rgb&&opened.images["finished"]->rgb==linear->rgb&&opened.images["stack/R"]==opened.images["linear"]&&opened.images["stack/G"]==opened.images["linear"]);
 ASTRO_TEST_CHECK(opened.state["options"].toMap()["caption"].toMap()["technicalText"]=="Corrected camera and exposure");
 ASTRO_TEST_CHECK(opened.state["options"].toMap()["versions"].toMap()["B"].toMap()["maskStrokes"].toList()==b["maskStrokes"].toList());
 const auto openedMask=controller.finishSettings()["maskFile"].toString();ASTRO_TEST_CHECK(QFileInfo::exists(openedMask)&&read(openedMask)==originalMask&&openedMask.startsWith(dir.filePath("cache")));
 const auto openedSource=controller.frames()[0].toMap()["path"].toString();ASTRO_TEST_CHECK(QFileInfo::exists(openedSource)&&opened.state["references"].toMap()["RGB"]==openedSource);
 ASTRO_TEST_CHECK(controller.exportUrl().toLocalFile()==dir.filePath("moved-stacked.tif"));
 const auto linearExport=dir.filePath("exact.fits");controller.exportImage(QUrl::fromLocalFile(linearExport),true);wait(controller);ASTRO_TEST_CHECK(recording::readAstroImage(linearExport.toStdString()).samples==linear->rgb);
 const auto beforeArchive=read(moved);controller.exportImage(QUrl::fromLocalFile(moved),true);ASTRO_TEST_CHECK(!controller.busy()&&read(moved)==beforeArchive);
 controller.apply(controller.finishSettings());wait(controller);ASTRO_TEST_CHECK(controller.hasResult()&&controller.hasMask()); // Imported mask still usable.
 controller.selectResult("G");wait(controller);ASTRO_TEST_CHECK(controller.hasResult());controller.compose("R","G","B");wait(controller);ASTRO_TEST_CHECK(controller.hasResult());
 // Payload, manifest, traversal, duplicate, oversized and truncated archives
 // must preserve the current workspace and never leave extraction directories.
 const auto current=controller.sessionSnapshot().images["finished"];const auto cacheFolders=QDir(dir.filePath("cache")).entryList(QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot);
 QList<QByteArray> corrupt{bytes.left(bytes.size()-1),bytes+"extra"};auto payload=bytes;payload[payload.size()-1]=char(payload.back()^0x01);corrupt<<payload;auto manifest=bytes;manifest[44]=char(manifest[44]^0x01);corrupt<<manifest;
 corrupt<<alterManifest(bytes,[](QJsonObject& m){auto files=m["files"].toArray();auto row=files[0].toObject();row["name"]="../outside.fits";files[0]=row;m["files"]=files;});
 corrupt<<alterManifest(bytes,[](QJsonObject& m){auto files=m["files"].toArray();files.append(files[0]);m["files"]=files;});
 corrupt<<alterManifest(bytes,[](QJsonObject& m){auto files=m["files"].toArray();auto row=files[0].toObject();row["bytes"]="999999999999999";files[0]=row;m["files"]=files;});
 for(int i=0;i<corrupt.size();++i){const auto path=dir.filePath(QString("invalid-%1.astroproject").arg(i));write(path,corrupt[i]);controller.loadProject(QUrl::fromLocalFile(path));wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["finished"]==current);ASTRO_TEST_CHECK(QDir(dir.filePath("cache")).entryList(QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot)==cacheFolders);}
 cancelled=false;try{ProjectStorage::load(moved,dir.filePath("cache"),[]{throw std::runtime_error("cancel");});}catch(...){cancelled=true;}ASTRO_TEST_CHECK(cancelled&&QDir(dir.filePath("cache")).entryList(QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot)==cacheFolders);
 // New linked projects can find moved files by content, including masks and
 // references inside inactive A/B recipes. Homonyms with other pixels fail.
 auto linked=opened;const auto relocated=dir.filePath("relocated");ASTRO_TEST_CHECK(QDir().mkpath(relocated));
 QMap<QString,QString> relocations;for(const auto& path:ProjectStorage::sourcePaths(linked)){QString name=linked.state["sourceFingerprints"].toMap()[path].toMap()["name"].toString();for(const auto& value:linked.state["frames"].toList()){const auto row=value.toMap();if(row["path"]==path)name=row["name"].toString();}const auto newPath=relocated+'/'+name;ASTRO_TEST_CHECK(QFile::copy(path,newPath));relocations[path]=newPath;}
 controller.restoreSession(linked);const auto linkedPath=dir.filePath("linked.astroproject");controller.saveProject(QUrl::fromLocalFile(linkedPath));
 const auto correctSource=relocations[openedSource];const auto wrongFolder=dir.filePath("wrong");ASTRO_TEST_CHECK(QDir().mkpath(wrongFolder));auto wrong=source;wrong.samples[0]=.999f;recording::writeAstroFits((wrongFolder+'/'+QFileInfo(openedSource).fileName()).toStdString(),wrong);
 for(const auto& path:ProjectStorage::sourcePaths(linked))ASTRO_TEST_CHECK(QFile::remove(path));controller.loadProject(QUrl::fromLocalFile(linkedPath));ASTRO_TEST_CHECK(!controller.hasResult()&&controller.missingSources().size()==2);
 const auto oldRows=controller.frames();controller.relinkSources(QUrl::fromLocalFile(wrongFolder));wait(controller);ASTRO_TEST_CHECK(controller.frames()==oldRows&&controller.missingSources().size()==2);
 controller.restoreSession(linked);const auto cachedPixels=controller.sessionSnapshot().images["finished"];
 controller.relinkSources(QUrl::fromLocalFile(relocated));wait(controller);ASTRO_TEST_CHECK(controller.sessionSnapshot().images["finished"]==cachedPixels);
 ASTRO_TEST_CHECK(controller.missingSources().isEmpty()&&controller.frames()[0].toMap()["path"]==QFileInfo(correctSource).canonicalFilePath());
 ASTRO_TEST_CHECK(controller.projectOptions()["versions"].toMap()["A"].toMap()["maskFile"]==QFileInfo(relocations[openedMask]).canonicalFilePath());
 // Legacy files without fingerprints require explicit metadata matching and
 // refuse two indistinguishable candidates.
 auto legacy=QJsonDocument::fromJson(read(linkedPath)).object();legacy.remove("sources");auto frames=legacy["frames"].toArray();auto row=frames[0].toObject();row.remove("sourceFingerprint");row["path"]=dir.filePath("gone/light.fits");row["name"]="light.fits";frames[0]=row;legacy["frames"]=frames;legacy["options"]=QJsonObject{};
 const auto legacyPath=dir.filePath("legacy.astroproject");write(legacyPath,QJsonDocument(legacy).toJson());controller.loadProject(QUrl::fromLocalFile(legacyPath));
 const auto oldFolder=dir.filePath("old-moved");ASTRO_TEST_CHECK(QDir().mkpath(oldFolder+"/duplicate"));ASTRO_TEST_CHECK(QFile::copy(correctSource,oldFolder+"/light.fits")&&QFile::copy(correctSource,oldFolder+"/duplicate/light.fits"));
 controller.relinkSources(QUrl::fromLocalFile(oldFolder));wait(controller);ASTRO_TEST_CHECK(controller.missingSources().size()==1);
 controller.relinkSources(QUrl::fromLocalFile(oldFolder),true);wait(controller);ASTRO_TEST_CHECK(controller.missingSources().size()==1);ASTRO_TEST_CHECK(QFile::remove(oldFolder+"/duplicate/light.fits"));
 controller.relinkSources(QUrl::fromLocalFile(oldFolder),true);wait(controller);ASTRO_TEST_CHECK(controller.missingSources().isEmpty());
 // Cleanup protects the open workspace and the disk recovery separately.
 const auto cache=dir.filePath("cache");auto one=ProjectStorage::load(moved,cache);auto two=ProjectStorage::load(moved,cache);auto unused=ProjectStorage::load(moved,cache);
 DeepSkyController deep;deep.setProjectCacheDirectory(cache);deep.restoreSession(one);ProcessingController processing;MosaicController mosaic;SessionController session;session.setStorageDirectory(dir.filePath("recovery"));session.setDeepSky(&deep);session.setProcessing(&processing);session.setMosaic(&mosaic);
 SavedSession recovery;recovery.workspaces={{"deepSky",two}};SessionStorage(session.storageDirectory()).save(recovery);
 const auto unrelated=QDir(cache).filePath("unrelated");ASTRO_TEST_CHECK(QDir().mkpath(unrelated));write(unrelated+"/keep","keep");
 #ifdef Q_OS_UNIX
 const auto unusedFolder=QFileInfo(ProjectStorage::sourcePaths(unused)[0]).absoluteDir().absolutePath()+"/..";
 ASTRO_TEST_CHECK(::symlink(unrelated.toUtf8().constData(),(unusedFolder+"/external-link").toUtf8().constData())==0);
#endif
 const auto protectedCanonical=ProjectStorage::strings(one.state).unite(ProjectStorage::strings(two.state));QSet<QString> canonicalPaths;for(const auto& path:protectedCanonical)if(QFileInfo(path).isFile())canonicalPaths.insert(QFileInfo(path).canonicalFilePath());
 ASTRO_TEST_CHECK(ProjectStorage::cacheUsage(cache,canonicalPaths)["unusedBytes"].toDouble()==ProjectStorage::cacheUsage(cache,protectedCanonical)["unusedBytes"].toDouble());
 session.refreshCache();ASTRO_TEST_CHECK(session.cacheInfo()["unusedBytes"].toDouble()>0);session.clearUnusedProjectCache();wait(session);
 ASTRO_TEST_CHECK(QFileInfo::exists(ProjectStorage::sourcePaths(one)[0])&&QFileInfo::exists(ProjectStorage::sourcePaths(two)[0])&&!QFileInfo::exists(ProjectStorage::sourcePaths(unused)[0])&&read(unrelated+"/keep")=="keep");
 session.refreshCache();ASTRO_TEST_CHECK(session.cacheInfo()["unusedBytes"].toDouble()==0&&session.cacheInfo()["sessionBytes"].toDouble()>0);
 // An unreadable recovery must block deletion, not silently drop protection.
 auto another=ProjectStorage::load(moved,cache);write(dir.filePath("recovery/session.json"),"invalid");session.clearUnusedProjectCache();ASTRO_TEST_CHECK(!session.busy()&&QFileInfo::exists(ProjectStorage::sourcePaths(another)[0]));
}
