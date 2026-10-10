#include "BlindSolver.hpp"
#include <astro/recording/AstroImage.hpp>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QSaveFile>
#include <QCryptographicHash>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <signal.h>
#endif
namespace astro::app {
namespace {
void check(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Cancelled. Previous results are preserved.");}
QString enginePath(){
    const auto bundled=QCoreApplication::applicationDirPath()+"/../Helpers/astrometry/solve-field";
    if(QFileInfo(bundled).isExecutable())return QFileInfo(bundled).canonicalFilePath();
    auto path=QStandardPaths::findExecutable("solve-field");
    if(path.isEmpty())for(const auto& candidate:{QString("/opt/homebrew/bin/solve-field"),QString("/usr/local/bin/solve-field")})if(QFileInfo(candidate).isExecutable()){path=candidate;break;}
    return path;
}
QString indexDirectory(){return QSettings().value("astrometry/indexDirectory",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/astrometry/indexes").toString();}
}
QVariantMap blindSolverConfiguration(){
    const auto engine=enginePath(),directory=indexDirectory();
    const int count=QDir(directory).entryList({"index-*.fits"},QDir::Files|QDir::Readable).size();
    return {{"engine",engine},{"directory",directory},{"indexes",count},{"ready",!engine.isEmpty()&&count>0}};
}
void setBlindIndexDirectory(const QString& path){
    if(!QFileInfo(path).isDir())throw std::runtime_error("Choose an existing Astrometry.net index folder.");
    QSettings().setValue("astrometry/indexDirectory",QFileInfo(path).absoluteFilePath());
}
void installWideFieldIndex(std::stop_token stop){
    check(stop);const auto dir=indexDirectory();if(!QDir().mkpath(dir))throw std::runtime_error("Cannot create the astrometry index folder.");
    const auto path=QDir(dir).filePath("index-4109.fits");
    constexpr auto checksum="9a65a52ce04e3e75af950e5866f81b1b";
    QFile cached(path);if(cached.size()==49772160&&cached.open(QIODevice::ReadOnly)){QCryptographicHash hash(QCryptographicHash::Md5);if(hash.addData(&cached)&&hash.result().toHex()==checksum)return;}
    QCryptographicHash hash(QCryptographicHash::Md5);
    QSaveFile out(path);if(!out.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot write the astrometry index.");
    QNetworkAccessManager network;QNetworkRequest request(QUrl("https://data.astrometry.net/4100/index-4109.fits"));request.setTransferTimeout(45000);
    auto* reply=network.get(request);QEventLoop loop;QTimer timer;QElapsedTimer elapsed;elapsed.start();timer.setInterval(100);
    bool failed=false;
    QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{const auto bytes=reply->readAll();hash.addData(bytes);if(out.pos()+bytes.size()>49772160||out.write(bytes)!=bytes.size()){failed=true;reply->abort();}});
    QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(stop.stop_requested()||elapsed.elapsed()>180000)reply->abort();});
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);timer.start();loop.exec();check(stop);
    const auto bytes=reply->readAll();hash.addData(bytes);if(!bytes.isEmpty()&&out.write(bytes)!=bytes.size())failed=true;
    if(failed||reply->error()!=QNetworkReply::NoError||out.pos()!=49772160||hash.result().toHex()!=checksum)throw std::runtime_error("Index download failed. Check the connection and retry.");
    if(!out.commit())throw std::runtime_error("Cannot save the astrometry index.");
}
recording::CelestialWcs solveBlindField(const processing::FloatImage& image,const QVariantMap& options,std::stop_token stop){
    check(stop);const auto config=blindSolverConfiguration();
    if(config["engine"].toString().isEmpty())throw std::runtime_error("Astrometry.net is unavailable. Install it or use the guided Gaia solver.");
    if(!config["ready"].toBool())throw std::runtime_error("Install a wide-field index or choose your Astrometry.net index folder first.");
    const int order=options.value("sipOrder",3).toInt();
    const double low=options.value("scaleLow",0).toDouble(),high=options.value("scaleHigh",0).toDouble();
    if((order!=0&&(order<2||order>5))||!std::isfinite(low)||!std::isfinite(high)||low<0||high<0||(low>0&&(high<=low||high>100)))throw std::runtime_error("Invalid blind solving scale range or SIP order.");
    QTemporaryDir temporary(QDir::tempPath()+"/astro-blind-XXXXXX");if(!temporary.isValid())throw std::runtime_error("Cannot create astrometry working files.");
    QFile backend(temporary.filePath("backend.cfg"));if(!backend.open(QIODevice::WriteOnly)||backend.write("autoindex\ncpulimit 120\n")<0)throw std::runtime_error("Cannot create astrometry working files.");backend.close();
    recording::AstroImage input;input.width=image.width;input.height=image.height;input.channels=1;input.bits=32;input.samples.resize(std::size_t(image.width)*image.height);
    for(std::size_t p=0;p<input.samples.size();++p){if(p%65536==0)check(stop);const auto at=p*3;input.samples[p]=float(.2126*image.rgb[at]+.7152*image.rgb[at+1]+.0722*image.rgb[at+2]);}
    const auto inputPath=temporary.filePath("field.fits");recording::writeAstroFits(inputPath.toStdString(),input);check(stop);
    QStringList args{"--fits-image","--no-verify","--no-plots","--overwrite","--new-fits","none","--backend-config",backend.fileName(),"--index-dir",config["directory"].toString(),"--cpulimit","120","--dir",temporary.path(),"--out","solution","--downsample",QString::number(std::max(1,image.width/1800))};
    if(order)args<<"--tweak-order"<<QString::number(order);else args<<"--no-tweak";
    if(low>0)args<<"--scale-units"<<"arcsecperpix"<<"--scale-low"<<QString::number(low)<<"--scale-high"<<QString::number(high);
    args<<inputPath;
    QProcess process;process.setProcessChannelMode(QProcess::MergedChannels);process.setWorkingDirectory(temporary.path());
    auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",QFileInfo(config["engine"].toString()).absolutePath()+":"+env.value("PATH"));process.setProcessEnvironment(env);
#ifdef Q_OS_UNIX
    process.setChildProcessModifier([]{::setpgid(0,0);});
#endif
    process.start(config["engine"].toString(),args);
    if(!process.waitForStarted(5000))throw std::runtime_error("Cannot start Astrometry.net.");
    QElapsedTimer clock;clock.start();QByteArray log;
    while(process.state()!=QProcess::NotRunning){
        process.waitForFinished(100);log+=process.readAll();if(log.size()>32768)log=log.right(32768);
        if(stop.stop_requested()||clock.elapsed()>150000){
#ifdef Q_OS_UNIX
            if(process.processId()>0)::kill(-pid_t(process.processId()),SIGTERM);
#else
            process.terminate();
#endif
            if(!process.waitForFinished(2000)){
#ifdef Q_OS_UNIX
                if(process.processId()>0)::kill(-pid_t(process.processId()),SIGKILL);
#else
                process.kill();
#endif
                process.waitForFinished(2000);
            }
            check(stop);throw std::runtime_error("Blind solving timed out. Try a scale range or different indexes.");
        }
    }
    check(stop);QFile solved(temporary.filePath("solution.solved"));
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0||!solved.open(QIODevice::ReadOnly)||solved.read(1)!=QByteArray(1,char(1)))throw std::runtime_error("Blind solving found no reliable field. Check focus, stars and index scale coverage.");
    auto wcs=recording::readCelestialWcs(temporary.filePath("solution.wcs").toStdString());
    if(!wcs||wcs->scale()<.001||wcs->scale()>100)throw std::runtime_error("Astrometry.net returned an unsupported WCS.");
    // Detect folded/non-invertible solutions over the entire image, not just its centre.
    for(int y=0;y<=8;++y)for(int x=0;x<=8;++x){const double px=x*(image.width-1)/8.,py=y*(image.height-1)/8.;const auto sky=wcs->sky(px,py);const auto pixel=sky?wcs->pixel((*sky)[0],(*sky)[1]):std::nullopt;if(!pixel||std::hypot((*pixel)[0]-px,(*pixel)[1]-py)>.001)throw std::runtime_error("SIP verification failed. Previous coordinates are preserved.");}
    return *wcs;
}
}
