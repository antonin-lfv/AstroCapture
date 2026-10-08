#include "DeepSkyController.hpp"
#include "ImagePreview.hpp"
#include "DeepSkySettings.hpp"
#include <astro/recording/AstroImage.hpp>
#include <astro/recording/TiffWriter.hpp>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QImageWriter>
#include <QColorSpace>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>
#include <set>
namespace astro::app {
namespace {
struct Cancelled{};
void check(std::stop_token stop){if(stop.stop_requested())throw Cancelled{};}
std::filesystem::path nativePath(const QString& p){return std::filesystem::path(p.toStdString());}
QString groupName(QString text) {
    text=text.trimmed().toUpper();
    if(text=="RED")return "R";if(text=="GREEN")return "G";if(text=="BLUE")return "B";
    if(text=="LUMINANCE"||text=="LUM")return "L";
    if(text=="H-ALPHA"||text=="HALPHA"||text=="H_ALPHA")return "HA";
    if(text=="OIII"||text=="O3")return "OIII";if(text=="SII"||text=="S2")return "SII";
    return text;
}
recording::AstroImage readFrame(const QVariantMap& row) {
    const auto path=row["path"].toString();
    const auto suffix=QFileInfo(path).suffix().toLower();
    if(QStringList{"png","tif","tiff"}.contains(suffix)) {
        QImageReader reader(path);
        const auto size=reader.size();
        if(size.width()<=0||size.height()<=0||std::uint64_t(size.width())*size.height()>32000000||reader.imageCount()>1)
            throw std::runtime_error("Choose a single image of at most 32 megapixels.");
        auto image=reader.read().convertToFormat(QImage::Format_RGBA64);
        if(image.isNull())throw std::runtime_error("Cannot read this PNG/TIFF image.");
        recording::AstroImage out;out.width=image.width();out.height=image.height();out.channels=3;out.bits=16;out.samples.resize(std::size_t(out.width)*out.height*3);
        for(int y=0;y<out.height;++y){auto* pixels=reinterpret_cast<const QRgba64*>(image.constScanLine(y));for(int x=0;x<out.width;++x){const auto at=(std::size_t(y)*out.width+x)*3;out.samples[at]=pixels[x].red()/65535.f;out.samples[at+1]=pixels[x].green()/65535.f;out.samples[at+2]=pixels[x].blue()/65535.f;}}
        return out;
    }
    return recording::readAstroImage(nativePath(path),row["scale"].toDouble());
}
QVariantMap suggested(const processing::FloatImage& image) {
    std::array<std::vector<float>,3> channels;
    const auto pixels=image.rgb.size()/3,step=std::max(std::size_t(1),pixels/100000);
    std::array<float,3> medians{},gains{1,1,1};
    for(std::size_t p=0;p<pixels;p+=step)for(int c=0;c<3;++c)if(std::isfinite(image.rgb[p*3+c]))channels[c].push_back(image.rgb[p*3+c]);
    for(int c=0;c<3;++c){if(channels[c].empty())throw std::runtime_error("No finite samples in this image.");std::sort(channels[c].begin(),channels[c].end());medians[c]=channels[c][channels[c].size()/2];}
    const float target=*std::min_element(medians.begin(),medians.end());
    if(target>0)for(int c=0;c<3;++c)if(medians[c]>0)gains[c]=std::clamp(target/medians[c],.1f,3.f);
    std::vector<float> sample;
    for(int c=0;c<3;++c)for(float value:channels[c])sample.push_back(value*gains[c]);
    std::sort(sample.begin(),sample.end());
    const float black=sample[sample.size()/1000];
    float white=black+1e-6f;
    for(std::size_t p=0;p<pixels;++p)for(int c=0;c<3;++c)if(std::isfinite(image.rgb[p*3+c]))white=std::max(white,image.rgb[p*3+c]*gains[c]);
    const float median=std::clamp((sample[sample.size()/2]-black)/(white-black),1e-6f,.99f);
    const float midtones=std::clamp(median*.82f/(median+.18f-2*median*.18f),.001f,.999f);
    return {{"black",black},{"white",white},{"midtones",midtones},{"red",gains[0]},{"green",gains[1]},{"blue",gains[2]}};
}
QImage previewImage(const processing::FloatImage& image,bool stretch,const processing::CheckCancel& check={}) {
    if(stretch){const auto settings=suggested(image);processing::Finish f;f.black=settings["black"].toFloat();f.white=settings["white"].toFloat();f.midtones=settings["midtones"].toFloat();f.color={settings["red"].toFloat(),settings["green"].toFloat(),settings["blue"].toFloat()};return makeImagePreview(processing::finishDeepSky(image,f,check),check);}
    return makeImagePreview(image,check);
}
QVariantMap makeLinearHistogram(const processing::FloatImage& image,const std::array<float,3>& gains,float black,float white) {
    std::array<std::vector<float>,3> samples;std::vector<float> combined;
    const auto step=std::max(std::size_t(1),image.rgb.size()/3/100000);
    for(std::size_t p=0;p<image.rgb.size()/3;p+=step)for(int c=0;c<3;++c){const auto value=image.rgb[p*3+c]*gains[c];if(std::isfinite(value)){samples[c].push_back(value);combined.push_back(value);}}
    if(combined.empty())return {};
    std::sort(combined.begin(),combined.end());const float low=combined.front(),high=std::max(low+1e-6f,combined[combined.size()*999/1000]);
    QVariantList channels;unsigned below=0,above=0;
    for(const auto& channel:samples){std::array<unsigned,1024> bins{};for(float value:channel){below+=value<=black;above+=value>=white;if(value>=low&&value<=high)++bins[std::clamp(int((value-low)/(high-low)*1023),0,1023)];}QVariantList counts;for(unsigned value:bins)counts<<value;channels<<QVariant::fromValue(counts);}
    return {{"channels",channels},{"minimum",low},{"maximum",high},{"blackLoss",100.*below/combined.size()},{"whiteLoss",100.*above/combined.size()}};
}

struct Display {
    std::shared_ptr<const processing::FloatImage> pixels;
    QImage preview;
    QVariantMap stretch;
};
Display prepare(const processing::FloatImage& image) {
    const auto settings=suggested(image);processing::Finish f;
    f.black=settings["black"].toFloat();f.white=settings["white"].toFloat();f.midtones=settings["midtones"].toFloat();f.color={settings["red"].toFloat(),settings["green"].toFloat(),settings["blue"].toFloat()};
    auto preview=allocateImagePreview(image);const auto target=previewTarget(preview);
    auto pixels=std::make_shared<processing::FloatImage>(processing::finishDeepSky(image,f,{},nullptr,&target));
    auto complete=settings;complete["histogram"]=makeLinearHistogram(image,f.color,f.black,f.white);
    return {pixels,preview,complete};
}
void atomicCopy(const QString& from,const QString& to,std::stop_token stop) {
    QFile input(from);QSaveFile output(to);
    if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open export destination.");
    while(!input.atEnd()){check(stop);const auto block=input.read(1024*1024);if(block.isEmpty()||output.write(block)!=block.size())throw std::runtime_error("Image export failed. Check disk space.");}
    check(stop);if(!output.commit())throw std::runtime_error("Cannot commit exported image.");
}
}
DeepSkyController::~DeepSkyController(){worker_.request_stop();if(worker_.joinable())worker_.join();}
void DeepSkyController::post(std::function<void()> action){QMetaObject::invokeMethod(this,std::move(action),Qt::QueuedConnection);}
void DeepSkyController::run(const QString& status,std::function<void(std::stop_token)> task) {
    if(busy_)return;
    busy_=true;progress_=0;status_=status;emit changed();
    worker_=std::jthread([this,task=std::move(task)](std::stop_token stop){
        bool success=true;
        try{task(stop);success=!stop.stop_requested();}catch(const Cancelled&){success=false;post([this]{status_=tr("Cancelled. Previous results are preserved.");});}
        catch(const std::exception& e){success=false;const auto message=QString::fromUtf8(e.what());post([this,message]{status_=message;});}
        post([this,success]{busy_=false;adjusting_=false;if(pendingAdjustments_){auto next=std::move(*pendingAdjustments_);pendingAdjustments_.reset();apply(next);return;}emit changed();emit operationFinished(success,status_);});
    });
}
void DeepSkyController::show(std::shared_ptr<const processing::FloatImage> image,bool result,QImage preview,std::shared_ptr<const processing::FloatImage> displayed,QVariantMap stretch) {
    comparingRaw_=false;displayingResult_=result;
    if(result){finishSettings_=defaultDeepSkySettings();for(auto it=finishSettings_.begin();it!=finishSettings_.end();++it)if(stretch.contains(it.key()))it.value()=stretch[it.key()];if(auto validated=checkedDeepSkySettings(finishSettings_))finishSettings_=*validated;linear_=image;finished_=displayed;resultPreview_=preview;stretch_=stretch;histogram_=stretch.value("histogram").toMap();emit resultChanged();}
    preview_=std::move(preview);emit imageChanged();emit changed();
}
void DeepSkyController::setRole(int index,const QString& role){if(busy_||index<0||index>=frames_.size()||!QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(role))return;auto row=frames_[index].toMap();if(references_.contains(row["group"].toString())&&references_[row["group"].toString()]==row["path"].toString())references_.erase(row["group"].toString());row["role"]=role;row["reference"]=false;frames_[index]=row;emit changed();}
void DeepSkyController::setGroup(int index,const QString& group){if(busy_||index<0||index>=frames_.size())return;auto row=frames_[index].toMap();const auto previous=row["group"].toString();if(references_.contains(previous)&&references_[previous]==row["path"].toString())references_.erase(previous);row["group"]=groupName(group);row["reference"]=false;frames_[index]=row;emit changed();}
void DeepSkyController::addFolder(const QUrl& url,const QString& role,const QString& group,double scale){
    if(busy_||!url.isLocalFile())return;QList<QUrl> files;
    QDirIterator iterator(url.toLocalFile(),QDir::Files|QDir::NoSymLinks,QDirIterator::Subdirectories);
    const QStringList formats{"fit","fits","fts","fz","tif","tiff","png","cr2","cr3","nef","nrw","arw","dng","orf","raf","rw2","pef"};
    while(iterator.hasNext()){const auto path=iterator.next();const auto name=QFileInfo(path).completeBaseName().toLower();if(role=="Auto"&&(name.startsWith("result")||name.startsWith("stack")||name.endsWith("-processed")||name=="rvb"||name=="hoo"))continue;if(formats.contains(QFileInfo(path).suffix().toLower()))files<<QUrl::fromLocalFile(path);if(files.size()>2000){status_=tr("Projects are limited to 2000 files.");emit changed();return;}}
    std::sort(files.begin(),files.end(),[](const QUrl& a,const QUrl& b){return a.toString()<b.toString();});addFiles(files,role,group,scale);
}
void DeepSkyController::addFiles(const QList<QUrl>& urls,const QString& role,const QString& group,double scale) {
    if(busy_)return;
    if(!QStringList{"Auto","Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(role)||!std::isfinite(scale)||scale<0){status_=tr("Invalid import settings.");emit changed();return;}
    if(frames_.size()+urls.size()>2000){status_=tr("Projects are limited to 2000 files.");emit changed();return;}
    const auto existing=frames_;
    run(tr("Reading images and detecting stars…"),[this,urls,role,group,scale,existing](std::stop_token stop){
        QVariantList added;QStringList errors;std::set<QString> seen;
        for(const auto& value:existing)seen.insert(value.toMap()["path"].toString());
        std::shared_ptr<processing::FloatImage> first;
        for(int n=0;n<urls.size();++n) {
            check(stop);const auto& url=urls[n];
            if(!url.isLocalFile())continue;
            const auto path=QFileInfo(url.toLocalFile()).canonicalFilePath();
            if(path.isEmpty()){errors<<tr("File not found: %1").arg(url.toLocalFile());continue;}
            if(!seen.insert(path).second)continue;
            try {
                QVariantMap row{{"path",path},{"name",QFileInfo(path).fileName()},{"role",role},{"selected",true},{"scale",scale}};
                auto data=readFrame(row);
                if(role=="Auto") {
                    const auto hint=(QFileInfo(path).fileName()+" "+QFileInfo(path).dir().dirName()+" "+QString::fromStdString(data.imageType)).toLower();
                    QString detected="Light";
                    if(hint.contains("darkflat")||hint.contains("dark_flat")||hint.contains("dark flat"))detected="DarkFlat";
                    else if(hint.contains("offset")||hint.contains("bias"))detected=data.exposure>=1?"DarkFlat":"Bias";
                    else if(hint.contains("dark"))detected="Dark";
                    else if(hint.contains("flat"))detected="Flat";
                    row["role"]=detected;
                }
                QString assigned=group=="Auto"?groupName(QString::fromStdString(data.filter)):groupName(group);
                if(assigned.isEmpty())assigned=data.channels==3||data.bayer!=domain::BayerPattern::None?"RGB":"Mono";
                row["group"]=assigned;row["exposure"]=data.exposure;row["width"]=data.width;row["height"]=data.height;
                row["bayer"]=QString::fromUtf8(domain::to_string(data.bayer).data());row["bits"]=data.bits;row["aduScale"]=data.scale;
                row["filter"]=QString::fromStdString(data.filter);if(data.hasGain)row["gain"]=data.gain;if(data.hasTemperature)row["temperature"]=data.temperature;
                auto rgb=std::make_shared<processing::FloatImage>(processing::linearRgb(data,[stop]{check(stop);}));
                if(row["role"]=="Light") {
                    const auto stars=processing::detectStars(*rgb,[stop]{check(stop);});
                    row["stars"]=int(stars.size());std::vector<double> fwhm;for(const auto& star:stars)fwhm.push_back(star.fwhm);
                    if(!fwhm.empty()){std::sort(fwhm.begin(),fwhm.end());row["fwhm"]=fwhm[fwhm.size()/2];}
                }
                if(!first)first=rgb;
                added<<row;
            }catch(const std::runtime_error& e){errors<<QFileInfo(path).fileName()+": "+QString::fromUtf8(e.what());}
            post([this,n,total=int(urls.size())]{progress_=(n+1)*100/std::max(1,total);emit changed();});
        }
        const auto display=first?prepare(*first):Display{};
        check(stop);
        post([this,added,errors,first,display,stop]{if(stop.stop_requested())return;frames_+=added;status_=tr("%1 images imported.").arg(added.size());if(!errors.isEmpty())status_+="\n"+errors.join("\n");if(first&&!hasResult())show(first,false,display.preview,display.pixels,display.stretch);emit changed();});
    });
}
void DeepSkyController::setSelected(int index,bool selected){if(busy_||index<0||index>=frames_.size())return;auto row=frames_[index].toMap();row["selected"]=selected;frames_[index]=row;emit changed();}
void DeepSkyController::setReference(int index) {
    if(busy_||index<0||index>=frames_.size())return;
    const auto row=frames_[index].toMap();if(row["role"]!="Light"||!row["selected"].toBool())return;
    const auto group=row["group"].toString();references_[group]=row["path"].toString();
    for(int n=0;n<frames_.size();++n){auto item=frames_[n].toMap();if(item["group"].toString()==group)item["reference"]=n==index;frames_[n]=item;}
    emit changed();
}
void DeepSkyController::remove(int index){if(busy_||index<0||index>=frames_.size())return;frames_.removeAt(index);emit changed();}
void DeepSkyController::clear(){if(busy_)return;projectOptions_.clear();finishSettings_.clear();rawPreview_={};rawName_.clear();comparingRaw_=false;histogram_.clear();pendingAdjustments_.reset();displayingResult_=false;stage_.clear();frames_.clear();references_.clear();stacks_.clear();groupExposure_.clear();groupCounts_.clear();exposureCount_=0;resultFilters_.clear();groups_.clear();linear_.reset();finished_.reset();preview_={};resultPreview_={};status_.clear();emit projectOptionsChanged();emit changed();emit imageChanged();}
void DeepSkyController::chooseRawComparison(int index){
    if(busy_||index<0||index>=frames_.size()||frames_[index].toMap()["role"]!="Light")return;
    const auto row=frames_[index].toMap();
    run(tr("Preparing raw comparison…"),[this,row](std::stop_token stop){auto raw=processing::linearRgb(readFrame(row),[stop]{check(stop);});const auto image=previewImage(raw,true);check(stop);post([this,image,row,stop]{if(stop.stop_requested())return;rawPreview_=image;rawName_=row["name"].toString();emit changed();});});
}
void DeepSkyController::showRawComparison(bool enabled){
    if(!hasResult()||(enabled&&(busy_||!hasRawComparison())))return;
    comparingRaw_=enabled;preview_=enabled?rawPreview_:resultPreview_;displayingResult_=!enabled;emit imageChanged();emit changed();
}
void DeepSkyController::restoreResult(){if(busy_||!hasResult())return;comparingRaw_=false;preview_=resultPreview_;displayingResult_=true;status_=tr("Showing the stacked result.");emit imageChanged();emit changed();}
void DeepSkyController::inspect(int index){if(busy_||index<0||index>=frames_.size())return;const auto row=frames_[index].toMap();run(tr("Opening frame…"),[this,row](std::stop_token stop){auto rgb=std::make_shared<processing::FloatImage>(processing::linearRgb(readFrame(row),[stop]{check(stop);}));const auto display=prepare(*rgb);check(stop);post([this,rgb,display,row,stop]{if(!stop.stop_requested()){show(rgb,false,display.preview,display.pixels,display.stretch);status_=tr("Source preview: %1. Export still uses the integrated result.").arg(row["name"].toString());emit changed();}});});}
void DeepSkyController::integrate(bool calibrated,double sigma,bool normalize,bool correctBadPixels) {
    const auto frames=frames_;const auto references=references_;
    run(tr("Calibrating and integrating…"),[this,frames,calibrated,sigma,normalize,correctBadPixels,references](std::stop_token stop){
        std::map<QString,QVariantList> lights,flats;QVariantList darks,biases,darkFlats,badPixels;
        for(const auto& value:frames){const auto row=value.toMap();if(!row["selected"].toBool())continue;const auto role=row["role"].toString(),group=row["group"].toString();if(role=="Light")lights[group]<<row;else if(role=="Flat")flats[group]<<row;else if(role=="Dark")darks<<row;else if(role=="Bias")biases<<row;else if(role=="DarkFlat")darkFlats<<row;else if(role=="BadPixels")badPixels<<row;}
        if(lights.empty())throw std::runtime_error("Add and select light frames first.");
        if(calibrated&&(!darks.empty()||!flats.empty()||!biases.empty()||!darkFlats.empty()||!badPixels.empty()))
            throw std::runtime_error("Calibration files are selected but Lights already calibrated is enabled. Disable it to apply your calibration files, or deselect those files.");
        auto checkpoint=[stop]{check(stop);};
        auto master=[&](const QVariantList& rows,bool normalize=false){return processing::masterFrame(int(rows.size()),[&](int n){check(stop);return readFrame(rows[n].toMap());},normalize,checkpoint);};
        processing::Calibration calibration;
        if(!calibrated){if(!darks.empty())calibration.dark=master(darks);if(!biases.empty())calibration.bias=master(biases);if(!darkFlats.empty())calibration.darkFlat=master(darkFlats);if(correctBadPixels){if(!badPixels.empty())calibration.badPixels=master(badPixels);else if(calibration.dark)calibration.badPixels=processing::detectBadPixels(*calibration.dark);}}
        std::map<QString,std::shared_ptr<const processing::FloatImage>> results;std::map<QString,double> exposures;std::map<QString,int> counts;QStringList summary;double exposure=0;
        for(auto& [group,rows]:lights) {
            if(references.contains(group)) {
                for(int n=0;n<rows.size();++n)if(rows[n].toMap()["path"].toString()==references.at(group)){rows.swapItemsAt(0,n);break;}
            }
            check(stop);auto groupCalibration=calibration;
            if(!calibrated) {
                auto found=flats.find(group);
                if(found==flats.end()&&flats.size()==1&&lights.size()==1)found=flats.begin();
                if(found!=flats.end()) {
                    processing::Calibration flatCalibration;flatCalibration.dark=calibration.darkFlat;if(!flatCalibration.dark)flatCalibration.bias=calibration.bias;
                    auto corrected=[&](int n){check(stop);return processing::calibrate(readFrame(found->second[n].toMap()),flatCalibration,checkpoint);};
                    groupCalibration.flat=processing::masterFrame(int(found->second.size()),corrected,true,checkpoint);
                }
            }
            auto result=processing::integrate(int(rows.size()),[&](int n){check(stop);return readFrame(rows[n].toMap());},groupCalibration,calibrated,sigma,checkpoint,
                [this,group](int done,int total,const std::string& stage){post([this,group,done,total,stage]{progress_=done*100/std::max(1,total);stage_=QString::fromStdString(stage);status_=group+" · "+stage_;emit changed();});},normalize);
            exposure+=result.totalExposure;exposures[group]=result.totalExposure;counts[group]=int(result.accepted.size());
            summary<<tr("%1: %2/%3 frames, %4 s, %5 rejected samples").arg(group).arg(result.accepted.size()).arg(rows.size()).arg(result.totalExposure,0,'f',1).arg(result.rejectedSamples);
            if(result.accepted.size()<2)summary<<tr("Only one usable light in %1: no multi-frame noise reduction was possible.").arg(group);
            for(std::size_t n=0;n<result.rejected.size();++n)summary<<rows[result.rejected[n]].toMap()["name"].toString()+": "+QString::fromStdString(result.reasons[n]);
            results[group]=std::make_shared<processing::FloatImage>(std::move(result.image));
        }
        const auto display=prepare(*results.begin()->second);
        const auto rawRow=lights.begin()->second[0].toMap();
        const auto rawPreview=previewImage(processing::linearRgb(readFrame(rawRow),checkpoint),true);
        check(stop);
        post([this,results=std::move(results),exposures,counts,display,summary,exposure,rawRow,rawPreview,stop]()mutable{if(stop.stop_requested())return;rawPreview_=rawPreview;rawName_=rawRow["name"].toString();stacks_=std::move(results);groups_.clear();for(const auto& [group,image]:stacks_)groups_<<group;groupExposure_=exposures;groupCounts_=counts;exposureCount_=counts.begin()->second;resultFilters_={groups_.front()};totalExposure_=exposures.begin()->second;show(stacks_.begin()->second,true,display.preview,display.pixels,display.stretch);progress_=100;stage_=tr("Complete");status_=summary.join("\n");emit changed();});
    });
}
void DeepSkyController::selectResult(const QString& group){if(busy_)return;const auto found=stacks_.find(group);if(found==stacks_.end())return;const auto image=found->second;run(tr("Displaying integration…"),[this,image,group](std::stop_token stop){const auto display=prepare(*image);check(stop);post([this,image,display,group,stop]{if(!stop.stop_requested()){totalExposure_=groupExposure_[group];exposureCount_=groupCounts_[group];resultFilters_={group};show(image,true,display.preview,display.pixels,display.stretch);}});});}
void DeepSkyController::compose(const QString& red,const QString& green,const QString& blue,const QString& luminance) {
    if(busy_)return;
    if(!stacks_.contains(red)||!stacks_.contains(green)||!stacks_.contains(blue)||(!luminance.isEmpty()&&!stacks_.contains(luminance))){status_=tr("Select integrated groups for each color channel.");emit changed();return;}
    auto r=stacks_.at(red),g=stacks_.at(green),b=stacks_.at(blue),l=luminance.isEmpty()?nullptr:stacks_.at(luminance);
    double exposure=0;int count=0;std::set<QString> selected{red,green,blue};if(!luminance.isEmpty())selected.insert(luminance);QStringList filters;for(const auto& name:selected){exposure+=groupExposure_[name];count+=groupCounts_[name];filters<<name;}
    run(tr("Aligning and combining channels…"),[this,r,g,b,l,exposure,count,filters](std::stop_token stop){auto combined=std::make_shared<processing::FloatImage>(processing::combineChannels(*r,*g,*b,l.get(),[stop]{check(stop);}));const auto display=prepare(*combined);check(stop);post([this,combined,display,exposure,count,filters,stop]{if(stop.stop_requested())return;totalExposure_=exposure;exposureCount_=count;resultFilters_=filters;show(combined,true,display.preview,display.pixels,display.stretch);status_=tr("Channels combined. Adjust the stretch, then export.");progress_=100;emit changed();});});
}
void DeepSkyController::apply(const QVariantMap& options) {
    if(busy_){if(adjusting_){pendingAdjustments_=options;worker_.request_stop();}return;}
    if(!linear_)return;
    auto settings=defaultDeepSkySettings();for(auto it=settings.begin();it!=settings.end();++it)if(options.contains(it.key()))it.value()=options[it.key()];
    const auto validated=checkedDeepSkySettings(settings);
    if(!validated){status_=tr("Invalid finishing settings. Check levels and parameter ranges.");emit changed();return;}
    const auto image=linear_;processing::Finish f;
    f.background=options["background"].toBool();f.stretch=options.value("stretch",true).toBool();f.black=options.value("black",0).toFloat();f.white=options.value("white",1).toFloat();f.midtones=options.value("midtones",.15).toFloat();
    f.color={options.value("red",1).toFloat(),options.value("green",1).toFloat(),options.value("blue",1).toFloat()};f.denoise=options.value("denoise",0).toFloat();f.asinh=options.value("asinh",false).toBool();f.asinhStrength=options.value("asinhStrength",20).toFloat();f.saturation=options.value("saturation",1).toFloat();
    for(float value:{f.black,f.white,f.midtones,f.color[0],f.color[1],f.color[2],f.denoise,f.asinhStrength,f.saturation})if(!std::isfinite(value)){status_=tr("Adjustment values must be finite.");emit changed();return;}
    adjusting_=true;
    run(tr("Adjusting background, colors and stretch…"),[this,image,f,settings=*validated](std::stop_token stop){auto preview=allocateImagePreview(*image);const auto target=previewTarget(preview);auto result=std::make_shared<processing::FloatImage>(processing::finishDeepSky(*image,f,[stop]{check(stop);},nullptr,&target));const auto histogram=makeLinearHistogram(*image,f.color,f.black,f.white);check(stop);post([this,result,preview,histogram,settings,stop]{if(stop.stop_requested())return;finishSettings_=settings;histogram_=histogram;finished_=result;resultPreview_=preview;preview_=preview;comparingRaw_=false;displayingResult_=true;status_=tr("Adjustments applied. Linear FITS retains the original integration.");progress_=100;emit imageChanged();emit changed();});});
}
void DeepSkyController::exportImage(const QUrl& url,bool linear) {
    if(!linear_||!url.isLocalFile())return;
    const auto path=url.toLocalFile();
    for(const auto& value:frames_)if(QFileInfo(value.toMap()["path"].toString()).canonicalFilePath()==QFileInfo(path).canonicalFilePath()){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    const auto image=linear?linear_:finished_;const auto exposure=totalExposure_;
    run(tr("Exporting image…"),[this,image,path,linear,exposure](std::stop_token stop){
        QTemporaryDir directory(QFileInfo(path).dir().filePath(".astrocapture-XXXXXX"));if(!directory.isValid())throw std::runtime_error("Cannot create temporary export files.");
        const auto temp=directory.filePath("image");
        const auto suffix=QFileInfo(path).suffix().toLower();
        if(suffix=="fits"||suffix=="fit"||suffix=="fts") {
            recording::AstroImage output;output.width=image->width;output.height=image->height;output.channels=3;output.bits=32;output.exposure=exposure;output.samples=image->rgb;
            recording::writeAstroFits(nativePath(temp),output);
        }else if(suffix=="tif"||suffix=="tiff") {
            if(linear)throw std::runtime_error("Use FITS to preserve linear floating-point data.");
            std::vector<std::uint16_t> samples(image->rgb.size());for(std::size_t i=0;i<samples.size();++i){if(i%65536==0)check(stop);const float v=image->rgb[i];samples[i]=std::isfinite(v)?std::uint16_t(std::lround(std::clamp(v,0.f,1.f)*65535)):0;}
            const auto icc=QColorSpace(QColorSpace::SRgb).iccProfile();
            const auto saved=recording::writeTiff16Samples(temp.toStdString(),image->width,image->height,samples.data(),std::span(reinterpret_cast<const std::uint8_t*>(icc.constData()),std::size_t(icc.size())));if(!saved)throw std::runtime_error(saved.error().message);
        }else if(suffix=="jpg"||suffix=="jpeg") {
            if(linear)throw std::runtime_error("Use FITS to preserve linear data.");
            auto display=previewImage(*image,false);display.setColorSpace(QColorSpace::SRgb);
            QImageWriter writer(temp,"JPEG");writer.setQuality(95);
            if(!writer.write(display))throw std::runtime_error("Cannot save JPEG image.");
        }else throw std::runtime_error("Choose FITS, TIFF or JPEG for export.");
        check(stop);atomicCopy(temp,path,stop);post([this,path]{status_=tr("Saved %1").arg(path);emit imageExported(QUrl::fromLocalFile(path));emit changed();});
    });
}
void DeepSkyController::saveProject(const QUrl& url) {
    if(busy_||!url.isLocalFile())return;
    for(const auto& value:frames_)if(QFileInfo(url.toLocalFile()).canonicalFilePath()==QFileInfo(value.toMap()["path"].toString()).canonicalFilePath()){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    const QJsonObject project{{"version",1},{"frames",QJsonArray::fromVariantList(frames_)},{"options",QJsonObject::fromVariantMap(projectOptions_)}};
    QSaveFile file(url.toLocalFile());const auto bytes=QJsonDocument(project).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())status_=tr("Cannot save project.");else status_=tr("Project saved. Original files are referenced, not copied.");emit changed();
}
void DeepSkyController::loadProject(const QUrl& url) {
    if(busy_||!url.isLocalFile())return;
    QFile file(url.toLocalFile());
    if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024){status_=tr("Cannot read project.");emit changed();return;}
    QJsonParseError error;const auto object=QJsonDocument::fromJson(file.readAll(),&error).object();
    if(error.error!=QJsonParseError::NoError||object["version"].toInt()!=1||!object["frames"].isArray()||object["frames"].toArray().size()>2000){status_=tr("Invalid AstroCapture project.");emit changed();return;}
    auto frames=object["frames"].toArray().toVariantList();
    for(const auto& value:frames){const auto row=value.toMap();if(!QFileInfo(row["path"].toString()).isFile()||!QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(row["role"].toString())){status_=tr("Project contains missing files or invalid roles.");emit changed();return;}}
    auto options=object["options"].toObject().toVariantMap();
    if(options.contains("finish")) {
        auto finish=defaultDeepSkySettings();const auto saved=options["finish"].toMap();
        for(auto it=saved.cbegin();it!=saved.cend();++it)finish[it.key()]=it.value();
        const auto checked=checkedDeepSkySettings(finish);
        if(!checked){status_=tr("Invalid AstroCapture project.");emit changed();return;}
        options["finish"]=*checked;
    }
    if(options.contains("versions")) {
        const auto checked=checkedDeepSkySnapshot(options["versions"].toMap());
        if(!checked){status_=tr("Invalid AstroCapture project.");emit changed();return;}
        options["versions"]=*checked;
        options["finish"]=(*checked)[(*checked)["active"].toString()];
    }
    clear();projectOptions_=std::move(options);emit projectOptionsChanged();frames_=std::move(frames);for(const auto& value:frames_){const auto row=value.toMap();if(row["reference"].toBool())references_[row["group"].toString()]=row["path"].toString();}status_=tr("Project loaded. Integrate to recreate the result.");emit changed();
}
QVariant DeepSkyController::processingDocument() const {
    if(!finished_)return {};
    QStringList protectedPaths;for(const auto& value:frames_)protectedPaths<<value.toMap()["path"].toString();
    ImageDocument document{finished_,resultPreview_,{},tr("Deep-sky result"),{},protectedPaths};
    document.exposureCount=exposureCount_;document.totalExposure=totalExposure_;document.filters=resultFilters_;
    document.processingSummary=tr("Deep sky · %1 exposures · %2 s total · filters: %3").arg(exposureCount_).arg(totalExposure_,0,'f',1).arg(resultFilters_.join(", "));
    return QVariant::fromValue(document);
}
}
