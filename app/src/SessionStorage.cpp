#include "SessionStorage.hpp"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QRegularExpression>
#include <QUuid>
#include <QtEndian>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>

namespace astro::app {
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
QJsonObject readManifest(const QString& folder) {
    QFile file(QDir(folder).filePath("session.json"));
    require(file.open(QIODevice::ReadOnly)&&file.size()<=64*1024*1024,"Cannot read the saved session.");
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    require(error.error==QJsonParseError::NoError&&doc.isObject()&&doc.object()["version"].toInt()==1,"Invalid saved session.");
    return doc.object();
}
QByteArray fileHash(const QString& path,const std::function<void()>& check) {
    QFile file(path);require(file.open(QIODevice::ReadOnly),"A saved session file is missing.");QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd()){if(check)check();const auto block=file.read(1024*1024);require(!block.isEmpty()||file.atEnd(),"Cannot read a saved session file.");hash.addData(block);}
    return hash.result();
}
QString assetPath(const QString& folder,const QString& file) {
    static const QRegularExpression name("^[0-9a-f-]{36}\\.(pixels|plane|png)$");
    const auto path=QDir(folder).filePath(file);const QFileInfo info(path);
    require(name.match(file).hasMatch()&&!info.isSymLink(),"Invalid saved session file name.");return path;
}
void writePixels(const QString& path,int width,int height,int channels,std::span<const float> pixels,const std::function<void()>& check) {
    require(width>0&&height>0&&std::uint64_t(width)*height<=32000000&&pixels.size()==std::size_t(width)*height*channels,"Invalid saved session image.");
    QSaveFile file(path);require(file.open(QIODevice::WriteOnly),"Cannot save the session. Check free disk space.");QDataStream stream(&file);stream.setByteOrder(QDataStream::LittleEndian);
    stream<<quint32(0x41534350)<<quint32(1)<<quint32(width)<<quint32(height)<<quint32(channels);
    for(std::size_t start=0;start<pixels.size();start+=262144){if(check)check();const auto block=pixels.subspan(start,std::min(std::size_t(262144),pixels.size()-start));
        if constexpr(std::endian::native==std::endian::little)require(file.write(reinterpret_cast<const char*>(block.data()),qint64(block.size_bytes()))==qint64(block.size_bytes()),"Cannot finish saving the session.");
        else {QByteArray bytes(qsizetype(block.size_bytes()),Qt::Uninitialized);for(std::size_t i=0;i<block.size();++i)qToLittleEndian(std::bit_cast<quint32>(block[i]),bytes.data()+i*4);require(file.write(bytes)==bytes.size(),"Cannot finish saving the session.");}
    }
    require(stream.status()==QDataStream::Ok&&file.commit(),"Cannot finish saving the session.");
}
struct Pixels {int width{},height{},channels{};std::vector<float> samples;};
Pixels readPixels(const QString& path,int channels,const std::function<void()>& check) {
    QFile file(path);require(file.open(QIODevice::ReadOnly),"A saved session image is missing.");QDataStream stream(&file);stream.setByteOrder(QDataStream::LittleEndian);quint32 magic,version,w,h,c;stream>>magic>>version>>w>>h>>c;
    require(stream.status()==QDataStream::Ok&&magic==0x41534350&&version==1&&w>0&&h>0&&std::uint64_t(w)*h<=32000000&&c==quint32(channels)&&file.size()==20+qint64(w)*h*c*4,"Invalid saved session image.");
    Pixels p{int(w),int(h),int(c),std::vector<float>(std::size_t(w)*h*c)};
    for(std::size_t start=0;start<p.samples.size();start+=262144){if(check)check();const auto count=std::min(std::size_t(262144),p.samples.size()-start);auto* output=p.samples.data()+start;require(file.read(reinterpret_cast<char*>(output),qint64(count*4))==qint64(count*4),"Incomplete saved session image.");
        if constexpr(std::endian::native!=std::endian::little)for(std::size_t i=0;i<count;++i)output[i]=std::bit_cast<float>(qFromLittleEndian<quint32>(reinterpret_cast<const char*>(output+i)));
        for(std::size_t i=0;i<count;++i)require(std::isfinite(output[i])&&(channels==3||(output[i]>=0&&output[i]<=1)),"Invalid values in a saved session image.");
    }
    return p;
}
QJsonObject assetObject(const QString& file,const QByteArray& hash){return {{"file",file},{"sha256",QString::fromLatin1(hash.toHex())}};}
QString newName(const char* suffix){return QUuid::createUuid().toString(QUuid::WithoutBraces)+suffix;}
}
bool SessionStorage::available() const{return QFileInfo::exists(QDir(directory_).filePath("session.json"));}
QString SessionStorage::savedAt() const{try{return readManifest(directory_)["savedAt"].toString();}catch(...){return {};}}
void SessionStorage::save(const SavedSession& session,const std::function<void()>& check) {
    require(QDir().mkpath(directory_),"Cannot create the session folder.");QJsonObject spaces;QSet<QString> retained;
    for(auto it=session.workspaces.cbegin();it!=session.workspaces.cend();++it){const auto& s=it.value();QJsonObject imageRefs,planeRefs,previewRefs;
        for(auto p=s.images.cbegin();p!=s.images.cend();++p){require(bool(p.value()),"Invalid saved session image.");auto& cached=images_[p.value().get()];
            if(cached.asset.hash.size()!=32||cached.image.lock()!=p.value()||!QFileInfo::exists(QDir(directory_).filePath(cached.asset.file))){cached.image=p.value();cached.asset.hash.clear();cached.asset.file=newName(".pixels");const auto path=QDir(directory_).filePath(cached.asset.file);writePixels(path,p.value()->width,p.value()->height,3,p.value()->rgb,check);cached.asset.hash=fileHash(path,check);}
            retained.insert(cached.asset.file);imageRefs[p.key()]=assetObject(cached.asset.file,cached.asset.hash);
        }
        for(auto p=s.planes.cbegin();p!=s.planes.cend();++p){require(bool(p.value()),"Invalid saved mask.");const auto base=s.images.value("stack",s.images.value("result"));require(base&&p.value()->size()==std::size_t(base->width)*base->height,"Invalid saved mask size.");auto& cached=planes_[p.value().get()];
            if(cached.asset.hash.size()!=32||cached.plane.lock()!=p.value()||!QFileInfo::exists(QDir(directory_).filePath(cached.asset.file))){cached.plane=p.value();cached.asset.hash.clear();cached.asset.file=newName(".plane");const auto path=QDir(directory_).filePath(cached.asset.file);writePixels(path,base->width,base->height,1,*p.value(),check);cached.asset.hash=fileHash(path,check);}
            retained.insert(cached.asset.file);planeRefs[p.key()]=assetObject(cached.asset.file,cached.asset.hash);
        }
        for(auto p=s.previews.cbegin();p!=s.previews.cend();++p){if(check)check();auto& cached=previews_[p.value().cacheKey()];
            if(cached.hash.size()!=32||cached.file.isEmpty()||!QFileInfo::exists(QDir(directory_).filePath(cached.file))){cached.hash.clear();cached.file=newName(".png");QSaveFile file(QDir(directory_).filePath(cached.file));require(file.open(QIODevice::WriteOnly)&&p.value().save(&file,"PNG")&&file.commit(),"Cannot save the session preview.");cached.hash=fileHash(file.fileName(),check);}
            retained.insert(cached.file);previewRefs[p.key()]=assetObject(cached.file,cached.hash);
        }
        spaces[it.key()]=QJsonObject{{"state",QJsonObject::fromVariantMap(s.state)},{"images",imageRefs},{"planes",planeRefs},{"previews",previewRefs}};
    }
    qint64 totalBytes=0;for(const auto& file:retained)totalBytes+=QFileInfo(QDir(directory_).filePath(file)).size();require(totalBytes<=qint64(4)*1024*1024*1024,"The saved session exceeds the 4 GiB recovery limit.");
    if(check)check();const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"ui",QJsonObject::fromVariantMap(session.ui)},{"workspaces",spaces}}).toJson(QJsonDocument::Compact);
    require(bytes.size()<=64*1024*1024,"The saved session settings are too large.");QSaveFile manifest(QDir(directory_).filePath("session.json"));require(manifest.open(QIODevice::WriteOnly)&&manifest.write(bytes)==bytes.size()&&manifest.commit(),"Cannot save the session settings.");
    // Only collect our own obsolete cache files after the manifest commits.
    for(const auto& file:QDir(directory_).entryList({"*.pixels","*.plane","*.png"},QDir::Files))if(!retained.contains(file)&&QRegularExpression("^[0-9a-f-]{36}\\.").match(file).hasMatch())QFile::remove(QDir(directory_).filePath(file));
    for(auto i=previews_.begin();i!=previews_.end();)i=!retained.contains(i->second.file)?previews_.erase(i):std::next(i);
    for(auto i=images_.begin();i!=images_.end();)i=i->second.image.expired()?images_.erase(i):std::next(i);
    for(auto i=planes_.begin();i!=planes_.end();)i=i->second.plane.expired()?planes_.erase(i):std::next(i);
}
SavedSession SessionStorage::load(const std::function<void()>& check) {
    const auto manifest=readManifest(directory_);SavedSession session;session.ui=manifest["ui"].toObject().toVariantMap();const auto spaces=manifest["workspaces"].toObject();require(spaces.size()<=3,"Invalid session workspaces.");
    QMap<QString,QByteArray> verified; qint64 totalBytes=0;
    auto verify=[&](const QString& file,const QJsonObject& ref){const auto path=assetPath(directory_,file);const auto expected=ref["sha256"].toString().toLatin1();require(expected.size()==64,"Invalid saved session checksum.");if(!verified.contains(file)){totalBytes+=QFileInfo(path).size();require(totalBytes<=qint64(4)*1024*1024*1024,"The saved session exceeds the 4 GiB recovery limit.");verified[file]=fileHash(path,check).toHex();}require(verified[file]==expected,"A saved session file is damaged.");return path;};
    QMap<QString,std::shared_ptr<const processing::FloatImage>> loadedImages;QMap<QString,std::shared_ptr<const std::vector<float>>> loadedPlanes;QMap<QString,QImage> loadedPreviews;
    for(auto it=spaces.begin();it!=spaces.end();++it){require(QStringList{"processing","deepSky","mosaic"}.contains(it.key()),"Invalid session workspace.");const auto space=it.value().toObject();WorkspaceSnapshot s;s.state=space["state"].toObject().toVariantMap();
        auto refs=space["images"].toObject();require(refs.size()<=64,"Too many saved session images.");for(auto p=refs.begin();p!=refs.end();++p){const auto ref=p.value().toObject();const auto file=ref["file"].toString();const auto path=verify(file,ref);
            if(!loadedImages.contains(file)){auto data=readPixels(path,3,check);auto image=std::make_shared<processing::FloatImage>(processing::FloatImage{data.width,data.height,std::move(data.samples)});loadedImages[file]=image;images_[image.get()]={image,{file,QByteArray::fromHex(ref["sha256"].toString().toLatin1())}};}
            s.images[p.key()]=loadedImages[file];
        }
        refs=space["planes"].toObject();require(refs.size()<=8,"Too many saved session masks.");for(auto p=refs.begin();p!=refs.end();++p){const auto ref=p.value().toObject();const auto file=ref["file"].toString();const auto path=verify(file,ref);
            if(!loadedPlanes.contains(file)){auto data=readPixels(path,1,check);auto values=std::make_shared<std::vector<float>>(std::move(data.samples));loadedPlanes[file]=values;planes_[values.get()]={values,{file,QByteArray::fromHex(ref["sha256"].toString().toLatin1())}};}
            s.planes[p.key()]=loadedPlanes[file];
        }
        refs=space["previews"].toObject();require(refs.size()<=16,"Too many session previews.");for(auto p=refs.begin();p!=refs.end();++p){const auto ref=p.value().toObject();const auto file=ref["file"].toString();const auto path=verify(file,ref);
            if(!loadedPreviews.contains(file)){QImageReader reader(path);const auto size=reader.size();require(size.width()>0&&size.height()>0&&std::uint64_t(size.width())*size.height()<=32000000,"Invalid session preview dimensions.");auto image=reader.read();require(!image.isNull(),"Cannot read the session preview.");loadedPreviews[file]=image;previews_[image.cacheKey()]={file,QByteArray::fromHex(ref["sha256"].toString().toLatin1())};}
            s.previews[p.key()]=loadedPreviews[file];
        }
        session.workspaces[it.key()]=std::move(s);
    }
    return session;
}
void SessionStorage::discard() {
    const auto manifestPath=QDir(directory_).filePath("session.json");require(!QFileInfo::exists(manifestPath)||QFile::remove(manifestPath),"Cannot remove the saved session.");
    for(const auto& file:QDir(directory_).entryList({"*.pixels","*.plane","*.png"},QDir::Files))if(QRegularExpression("^[0-9a-f-]{36}\\.").match(file).hasMatch())QFile::remove(QDir(directory_).filePath(file));
    images_.clear();planes_.clear();previews_.clear();
}
}
