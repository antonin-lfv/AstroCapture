#include <astro/processing/StarLayers.hpp>
#include "MasterLibrary.hpp"
#include "FileProtection.hpp"
#include "ProjectStorage.hpp"
#include "MaskSettings.hpp"
#include "ImageGeometry.hpp"
#include "DeepSkyController.hpp"
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <numeric>
#include <QTransform>
#include "ImagePreview.hpp"
#include "DeepSkySettings.hpp"
#include "AstrometrySettings.hpp"
#include "GaiaCatalog.hpp"
#include "WcsGeometry.hpp"
#include <astro/recording/AstroImage.hpp>
#include <astro/recording/TiffWriter.hpp>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QImageReader>
#include <QImageWriter>
#include <QColorSpace>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QSettings>
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
    if(row.contains("masterHash")&&calibrationFileHash(path)!=row["masterHash"].toString())throw std::runtime_error("Master integrity check failed. Import an intact copy.");
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
recording::AstroImage readMaskFrame(const QString& path){
    if(!QStringList{"fits","fit","fts","png","tif","tiff"}.contains(QFileInfo(path).suffix().toLower()))throw std::runtime_error("Choose a FITS, PNG or TIFF mask.");
    auto image=readFrame({{"path",path},{"scale",1.0}});
    if(image.fitsBitpix>0){const double maximum=std::pow(2.,image.fitsBitpix)-1;for(auto& v:image.samples)v=float(v/maximum);}
    for(float v:image.samples)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Mask values must be between zero and one.");
    return image;
}
QStringList referencedMasks(const QVariantMap& finish,const QVariantMap& project){
    QStringList paths;auto append=[&](const QVariantMap& parameters){const auto file=parameters["maskFile"].toString();if(!file.isEmpty())paths<<file;for(const auto& operation:parameters["operations"].toList()){const auto path=operation.toMap()["settings"].toMap()["maskFile"].toString();if(!path.isEmpty())paths<<path;}};
    append(finish);append(project["finish"].toMap());const auto versions=project["versions"].toMap();append(versions["A"].toMap());append(versions["B"].toMap());paths.removeDuplicates();return paths;
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
    QVariantList channels,shadowChannels,highlightChannels;unsigned below=0,above=0;
    for(const auto& channel:samples){unsigned shadow=0,highlight=0;std::array<unsigned,1024> bins{};for(float value:channel){shadow+=value<=black;highlight+=value>=white;if(value>=low&&value<=high)++bins[std::clamp(int((value-low)/(high-low)*1023),0,1023)];}below+=shadow;above+=highlight;shadowChannels<<100.*shadow/std::max(std::size_t(1),channel.size());highlightChannels<<100.*highlight/std::max(std::size_t(1),channel.size());QVariantList counts;for(unsigned value:bins)counts<<value;channels<<QVariant::fromValue(counts);}
    return {{"channels",channels},{"minimum",low},{"maximum",high},{"shadowChannels",shadowChannels},{"highlightChannels",highlightChannels},{"sampledPixels",qulonglong(samples[0].size())},{"blackLoss",100.*below/combined.size()},{"whiteLoss",100.*above/combined.size()}};
}

struct Display {
    std::shared_ptr<const processing::FloatImage> pixels;
    QImage preview;
    QVariantMap stretch;
    QImage before;
};
Display prepare(const processing::FloatImage& image,bool withComparison=false,const processing::CheckCancel& check={}) {
    const auto settings=suggested(image);processing::Finish f;
    f.black=settings["black"].toFloat();f.white=settings["white"].toFloat();f.midtones=settings["midtones"].toFloat();f.color={settings["red"].toFloat(),settings["green"].toFloat(),settings["blue"].toFloat()};
    auto preview=allocateImagePreview(image);const auto target=previewTarget(preview);
    auto pixels=std::make_shared<processing::FloatImage>(processing::finishDeepSky(image,f,{},nullptr,&target));
    auto complete=settings;complete["histogram"]=makeLinearHistogram(image,f.color,f.black,f.white);
    return {pixels,preview,complete,withComparison?makeImagePreview(image,check):QImage{}};
}

QVariantList comparisonOrientations(const QVariantMap& settings) {
    QVariantList orientations;
    auto append=[&](const QVariantMap& values) {
        const int rotation=values.value("rotation").toInt();
        const bool horizontal=values.value("mirrorHorizontal").toBool(),vertical=values.value("mirrorVertical").toBool();
        if(rotation||horizontal||vertical||geometryActive(values["geometry"].toMap()))orientations<<QVariant::fromValue(QVariantList{rotation,horizontal,vertical,values["geometry"]});
    };
    for(const auto& value:settings.value("operations").toList()) {
        const auto operation=value.toMap();
        if(operation.value("enabled").toBool())append(operation.value("settings").toMap());
    }
    append(settings);
    return orientations;
}
QImage orientComparison(QImage image,const QVariantList& orientations) {
    for(const auto& value:orientations) {
        const auto step=value.toList();
        if(step[0].toInt())image=image.transformed(QTransform().rotate(step[0].toInt()*90));
        if(step[1].toBool()||step[2].toBool())image=image.transformed(QTransform().scale(step[1].toBool()?-1:1,step[2].toBool()?-1:1));
        if(step.size()>3)image=imageGeometry(image,step[3].toMap());
    }
    return image;
}

processing::Finish finishFromSettings(const QVariantMap& s) {
    processing::Finish f;
    f.background=s["background"].toBool();
    f.backgroundOptions.degree=s["backgroundDegree"].toInt();
    f.backgroundOptions.samplesPerLine=s["backgroundSamples"].toInt();
    f.backgroundOptions.tolerance=s["backgroundTolerance"].toDouble();
    for(const auto& value:s["backgroundPoints"].toList()) {
        const auto point=value.toList();
        f.backgroundOptions.samples.push_back({point[0].toDouble(),point[1].toDouble()});
    }
    f.stretch=s["stretch"].toBool();f.black=s["black"].toFloat();f.white=s["white"].toFloat();
    f.asinhLuminance=s["asinhLuminance"].toInt();f.midtones=s["midtones"].toFloat();f.asinh=s["asinh"].toBool();f.asinhStrength=s["asinhStrength"].toFloat();
    f.color={s["red"].toFloat(),s["green"].toFloat(),s["blue"].toFloat()};
    f.ghs=s["ghs"].toBool();f.ghsStrength=s["ghsStrength"].toDouble();f.ghsLocal=s["ghsLocal"].toDouble();f.ghsSymmetry=s["ghsSymmetry"].toDouble();f.ghsShadows=s["ghsShadows"].toDouble();f.ghsHighlights=s["ghsHighlights"].toDouble();f.ghsInverse=s["ghsInverse"].toBool();f.ghsMode=s["ghsMode"].toInt();
    f.saturation=s["saturation"].toFloat();f.denoise=s["denoise"].toFloat();
    return f;
}
processing::MaskOptions maskFromSettings(const QVariantMap& s,const processing::FloatImage& input) {
    auto options=maskOptions(s);
    if(options.type==5){const auto path=s["maskFile"].toString();if(path.isEmpty())throw std::runtime_error("Choose a mask image first.");const auto image=readMaskFrame(path);if(image.width!=input.width||image.height!=input.height)throw std::runtime_error("The imported mask must match the image dimensions.");options.raster.resize(std::size_t(input.width)*input.height);for(std::size_t p=0;p<options.raster.size();++p){double v=image.channels==1?image.samples[p]:.2126*image.samples[p*image.channels]+.7152*image.samples[p*image.channels+1]+.0722*image.samples[p*image.channels+2];if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Mask values must be between zero and one.");options.raster[p]=float(v);}}
    return options;
}
processing::FloatImage scientificPreparation(const processing::FloatImage& source,const QVariantMap& s,const processing::CheckCancel& check,QVariantMap* report=nullptr,const processing::Progress& progress={},const std::vector<float>* mask=nullptr) {
    auto image=source;
    const float pedestal=s["pedestal"].toFloat();if(pedestal!=0)for(std::size_t i=0;i<image.rgb.size();++i){if(i%65536==0&&check)check();image.rgb[i]+=pedestal;}
    if(s["photometry"].toBool()) {
        std::vector<processing::PhotometricReference> references;
        for(const auto& value:s["photometryReferences"].toList()){const auto row=value.toList();references.push_back({row[0].toDouble(),row[1].toDouble(),{row[2].toDouble(),row[3].toDouble(),row[4].toDouble()}});}
        auto measured=processing::calibratePhotometry(image,references,{s["photometryAperture"].toDouble(),s["photometryInner"].toDouble(),s["photometryOuter"].toDouble(),s["photometrySaturation"].toDouble()},check);
        if(!s["photometryMethod"].toString().isEmpty()&&measured.accepted<8)throw std::runtime_error("Catalogue calibration requires at least eight accepted reference stars. Adjust aperture, sky ring and saturation after checking the field.");
        if(report){(*report)["photometryEdgeRejected"]=measured.edgeRejected;(*report)["photometrySaturatedRejected"]=measured.saturatedRejected;(*report)["photometrySignalRejected"]=measured.signalRejected;(*report)["photometryOutlierRejected"]=measured.outlierRejected;(*report)["photometryAccepted"]=measured.accepted;(*report)["photometryRejected"]=measured.rejected;(*report)["photometryScatter"]=measured.scatter;(*report)["photometryGains"]=QVariantList{measured.gains[0],measured.gains[1],measured.gains[2]};}
        image=std::move(measured.image);
    }
    if(s["advancedDenoise"].toBool()) {
        auto reduced=processing::denoiseMultiscale(image,{s["noiseScales"].toInt(),s["noiseThreshold"].toDouble(),s["noiseAmount"].toDouble(),s["noiseChroma"].toDouble()},check);
        if(report){(*report)["noiseBefore"]=reduced.noiseBefore;(*report)["noiseAfter"]=reduced.noiseAfter;}
        if(mask&&s["maskScope"].toInt()==1)processing::blendMask(image,reduced.image,*mask,check);
        image=std::move(reduced.image);
    }
    if(s["deconvolution"].toBool()) {
        processing::DeconvolutionOptions options;options.psf={s["psfProfile"].toInt(),s["psfFwhm"].toDouble(),s["psfBeta"].toDouble()};options.iterations=s["rlIterations"].toInt();options.damping=s["rlDamping"].toDouble();options.blend=s["rlBlend"].toDouble();options.stopping=s["rlStopping"].toDouble();
        auto restored=processing::deconvolve(image,options,check,progress);
        if(report){(*report)["rlIterations"]=restored.iterations;(*report)["rlChange"]=restored.relativeChange;(*report)["psfFwhm"]=options.psf.fwhm;}
        if(mask&&s["maskScope"].toInt()==2)processing::blendMask(image,restored.image,*mask,check);
        image=std::move(restored.image);
    }
    return image;
}
processing::StarLayers finishLayers(const processing::FloatImage& source,const QVariantMap& s,const processing::Finish& f,const processing::CheckCancel& check) {
    processing::StarSeparationOptions o;o.psf={s["starPsfProfile"].toInt(),s["starFwhm"].toDouble(),s["starBeta"].toDouble()};o.maximumStars=s["starLimit"].toInt();o.fitTolerance=s["starFitTolerance"].toDouble();o.saturation=s["starSaturationLimit"].toDouble();
    return processing::finishStarLayers(source,o,f,{s["starsGain"].toDouble(),s["starsSaturation"].toDouble(),s["starsMidtones"].toDouble(),s["starBackgroundGain"].toDouble(),s["starBackgroundSaturation"].toDouble(),s["starBackgroundDenoise"].toDouble()},check);
}
processing::FloatImage evaluateOperations(const processing::FloatImage& source,const QVariantList& operations,const processing::CheckCancel& check) {
    processing::FloatImage image=source;
    for(const auto& item:operations) {
        const auto op=item.toMap();if(!op["enabled"].toBool())continue;
        if(check)check();
        const auto settings=op["settings"].toMap();auto f=finishFromSettings(settings);
        std::optional<processing::FloatImage> original;std::vector<float> mask;
        if(settings["maskType"].toInt()){original=image;mask=processing::makeMask(image,maskFromSettings(settings,image),check);}
        if(f.background){image=processing::extractBackground(image,f.backgroundOptions,check).image;f.background=false;}
        if(settings["pedestal"].toDouble()!=0||settings["photometry"].toBool()||settings["advancedDenoise"].toBool()||settings["deconvolution"].toBool())image=scientificPreparation(image,settings,check,nullptr,{},mask.empty()?nullptr:&mask);
        const auto finishingInput=settings["maskScope"].toInt()==3&&!mask.empty()?std::optional<processing::FloatImage>(image):std::nullopt;
        image=settings["starSeparation"].toBool()?finishLayers(image,settings,f,check).image:processing::finishDeepSky(image,f,check);
        if(settings["scnr"].toBool())image=processing::removeGreen(image,settings["scnrMethod"].toInt(),settings["scnrAmount"].toFloat(),settings["scnrPreserve"].toBool(),check);
        if(original&&settings["maskScope"].toInt()==0)processing::blendMask(*original,image,mask,check);
        if(finishingInput)processing::blendMask(*finishingInput,image,mask,check);
        if(settings["rotation"].toInt()||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool())image=processing::orientImage(image,settings["rotation"].toInt(),settings["mirrorHorizontal"].toBool(),settings["mirrorVertical"].toBool(),check);
        if(geometryActive(settings["geometry"].toMap()))image=processing::transformGeometry(image,geometryFrom(settings["geometry"].toMap()),check);
    }
    return image;
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
    busy_=true;progress_=0;status_=status;stage_=status;emit changed();
    worker_=std::jthread([this,task=std::move(task)](std::stop_token stop){
        bool success=true;
        try{task(stop);success=!stop.stop_requested();}catch(const Cancelled&){success=false;post([this]{status_=tr("Cancelled. Previous results are preserved.");});}
        catch(const std::exception& e){success=false;const auto message=tr(e.what());post([this,message]{status_=message;});}
        post([this,success]{busy_=false;adjusting_=false;exporting_=false;if(pendingAdjustments_){auto next=std::move(*pendingAdjustments_);pendingAdjustments_.reset();apply(next);return;}emit changed();emit operationFinished(success,status_);});
    });
}
void DeepSkyController::show(std::shared_ptr<const processing::FloatImage> image,bool result,QImage preview,std::shared_ptr<const processing::FloatImage> displayed,QVariantMap stretch,QImage before) {
    if(result){starsLayer_.reset();starBackgroundLayer_.reset();starsLayerPreview_={};starBackgroundLayerPreview_={};}starLayerView_=0;showingMask_=false;maskPreview_={};if(result)advancedReport_.clear();psfKernel_.clear();showingBackgroundModel_=false;backgroundPreview_={};comparingRaw_=false;displayingResult_=result;
    if(result){astrometry_.clear();finishSettings_=defaultDeepSkySettings();for(auto it=finishSettings_.begin();it!=finishSettings_.end();++it)if(stretch.contains(it.key()))it.value()=stretch[it.key()];if(auto validated=checkedDeepSkySettings(finishSettings_))finishSettings_=*validated;linear_=image;linearPreview_=before;comparisonPreview_=before;comparisonOrientationKey_=QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact);finished_=displayed;resultPreview_=preview;stretch_=stretch;histogram_=stretch.value("histogram").toMap();emit resultChanged();}
    if(result){geometryPreview_=preview;maskSourcePreview_=preview;}preview_=std::move(preview);emit imageChanged();emit changed();
}
void DeepSkyController::setStarLayerView(int view){if(busy_||!hasStarLayers()||view<0||view>2)return;starLayerView_=view;showingMask_=showingBackgroundModel_=comparingRaw_=false;displayingResult_=true;preview_=view==1?starsLayerPreview_:view==2?starBackgroundLayerPreview_:resultPreview_;emit imageChanged();emit changed();}
void DeepSkyController::setShowingMask(bool enabled){starLayerView_=0;if(busy_||!linear_)return;showingMask_=enabled&&!maskPreview_.isNull();showingBackgroundModel_=false;comparingRaw_=false;preview_=showingMask_?maskPreview_:resultPreview_;emit imageChanged();emit changed();}
QVariantList DeepSkyController::ghsCurve(const QVariantMap& parameters) const {
    QVariantList values;try{const auto f=finishFromSettings(parameters);const processing::GhsCurve curve({f.ghsStrength,f.ghsLocal,f.ghsSymmetry,f.ghsShadows,f.ghsHighlights,f.ghsInverse,f.ghsMode});for(int n=0;n<=512;++n)values<<curve(n/512.);}catch(const std::exception&){}return values;
}
QVariantList DeepSkyController::importPhotometryCatalog(const QUrl& url){
    if(busy_||!url.isLocalFile())return {};
    try{QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>256*1024)throw std::runtime_error("Cannot read photometry CSV (maximum 256 KiB).");QVariantList references;const auto lines=QString::fromUtf8(file.readAll()).split('\n');bool header=false;for(auto line:lines){line=line.trimmed();if(line.isEmpty()||line.startsWith('#'))continue;if(!header){if(line.toLower()!=QStringLiteral("x,y,red,green,blue"))throw std::runtime_error("CSV header must be x,y,red,green,blue. Coordinates are zero-based pixels; values are positive reference fluxes, not magnitudes.");header=true;continue;}const auto parts=line.split(',');if(parts.size()!=5)throw std::runtime_error("Each photometry row must have five numeric values.");QVariantList row;for(int c=0;c<5;++c){bool ok;const double v=parts[c].toDouble(&ok);if(!ok||!std::isfinite(v)||v<0||v>1e12||(c>=2&&v<=0))throw std::runtime_error("Invalid photometry coordinate or flux.");row<<v;}references<<QVariant::fromValue(row);if(references.size()>500)throw std::runtime_error("Photometry is limited to 500 references.");}if(references.size()<3)throw std::runtime_error("At least three reference stars are required.");status_=tr("Loaded %1 photometric references.").arg(references.size());emit changed();return references;}catch(const std::exception& e){status_=tr(e.what());emit changed();emit operationFinished(false,status_);return {};}
}
void DeepSkyController::estimatePsf(){
    if(busy_||!linear_)return;
    const auto image=linear_;const auto settings=finishSettings_;auto test=settings;test["deconvolution"]=true;if(!checkedDeepSkySettings(test)){status_=tr("PSF estimation requires a linear, unrotated recipe input. Remove earlier stretches or use a new version.");emit changed();return;}
    run(tr("Measuring stellar PSF…"),[this,image,settings](std::stop_token stop){const auto checkpoint=[stop]{check(stop);};auto source=evaluateOperations(*image,settings["operations"].toList(),checkpoint);auto f=finishFromSettings(settings);if(f.background)source=processing::extractBackground(source,f.backgroundOptions,checkpoint).image;auto scientific=settings;scientific["deconvolution"]=false;source=scientificPreparation(source,scientific,checkpoint);const auto measured=processing::estimatePsf(source,checkpoint);post([this,stop,measured]{if(stop.stop_requested())return;advancedReport_["estimatedFwhm"]=measured.fwhm;advancedReport_["estimatedScatter"]=measured.scatter;advancedReport_["estimatedStars"]=measured.stars;status_=tr("PSF measured from %1 stars. Review the estimate before using it.").arg(measured.stars);emit changed();});});
}
void DeepSkyController::setRole(int index,const QString& role){if(busy_||index<0||index>=frames_.size()||!QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(role))return;auto row=frames_[index].toMap();if(row["master"].toBool())return;if(references_.contains(row["group"].toString())&&references_[row["group"].toString()]==row["path"].toString())references_.erase(row["group"].toString());row["role"]=role;row["reference"]=false;frames_[index]=row;emit changed();}
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
                row["sourceFingerprint"]=ProjectStorage::fingerprint(path,[stop]{check(stop);});
                const auto metadata=calibrationMetadata(data);for(auto it=metadata.begin();it!=metadata.end();++it)row[it.key()]=it.value();
                if(!data.masterRole.empty()){row["role"]=QString::fromStdString(data.masterRole);row["masterHash"]=calibrationFileHash(path,stop);}
                if(role=="Auto"&&data.masterRole.empty()) {
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
                row["channels"]=data.channels;row["bayer"]=QString::fromUtf8(domain::to_string(data.bayer).data());row["bits"]=data.bits;row["aduScale"]=data.originalScale;
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
        post([this,added,errors,first,display,stop]{if(stop.stop_requested())return;frames_+=added;status_=tr("%1 images imported.").arg(added.size());if(!errors.isEmpty())status_+="\n"+errors.join("\n");if(first&&!hasResult()){sourceRow_=added.front().toMap();sourceDebayer_=sourceAutoStretch_=true;sourceInterpretation_=sourceRow_["channels"].toInt()==3?tr("RGB source · three channels"):sourceRow_["bayer"].toString()!="None"?tr("Bayer %1 source · color preview is debayered").arg(sourceRow_["bayer"].toString()):tr("Monochrome source · one channel");show(first,false,display.preview,display.pixels,display.stretch);}emit changed();});
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
void DeepSkyController::clear(){if(busy_)return;calibrationBatchResults_.clear();diagnosticMaps_.clear();hdrReport_.clear();diagnosticPreview_={};diagnosticInfo_.clear();emit diagnosticChanged();integrationReport_.clear();starsLayer_.reset();starBackgroundLayer_.reset();starsLayerPreview_={};starBackgroundLayerPreview_={};starLayerView_=0;projectUrl_=QUrl();sourceFingerprints_.clear();maskImportPreview_={};maskImportPath_.clear();geometryPreview_={};maskSourcePreview_={};advancedReport_.clear();psfKernel_.clear();maskPreview_={};showingMask_=false;projectOptions_.clear();sourceRow_.clear();sourceInterpretation_.clear();sourceDebayer_=sourceAutoStretch_=true;finishSettings_.clear();rawPreview_={};backgroundPreview_={};showingBackgroundModel_=false;rawName_.clear();comparingRaw_=false;histogram_.clear();pendingAdjustments_.reset();displayingResult_=false;stage_.clear();frames_.clear();backgroundSamples_.clear();references_.clear();stacks_.clear();astrometry_.clear();stackAstrometry_.clear();groupExposure_.clear();groupCounts_.clear();exposureCount_=0;resultFilters_.clear();groups_.clear();linear_.reset();finished_.reset();pipelineCache_.reset();pipelineSource_.reset();pipelineCacheKey_.clear();preview_={};resultPreview_={};linearPreview_={};comparisonPreview_={};comparisonOrientationKey_.clear();status_.clear();emit projectOptionsChanged();emit changed();emit imageChanged();}
void DeepSkyController::chooseRawComparison(int index){
    if(busy_||index<0||index>=frames_.size()||frames_[index].toMap()["role"]!="Light")return;
    const auto row=frames_[index].toMap();
    run(tr("Preparing raw comparison…"),[this,row](std::stop_token stop){auto raw=processing::linearRgb(readFrame(row),[stop]{check(stop);});const auto image=previewImage(raw,true);check(stop);post([this,image,row,stop]{if(stop.stop_requested())return;rawPreview_=image;rawName_=row["name"].toString();emit changed();});});
}
void DeepSkyController::showRawComparison(bool enabled){starLayerView_=0;
    if(!hasResult()||(enabled&&(busy_||!hasRawComparison())))return;
    showingBackgroundModel_=false;comparingRaw_=enabled;preview_=enabled?rawPreview_:resultPreview_;displayingResult_=!enabled;emit imageChanged();emit changed();
}
void DeepSkyController::restoreResult(){if(busy_||!hasResult())return;starLayerView_=0;comparingRaw_=false;showingBackgroundModel_=false;showingMask_=false;preview_=resultPreview_;displayingResult_=true;status_=tr("Showing the stacked result.");emit imageChanged();emit changed();}
void DeepSkyController::setShowingBackgroundModel(bool enabled) {starLayerView_=0;
    if(busy_||(enabled&&backgroundPreview_.isNull()))return;
    showingBackgroundModel_=enabled;showingMask_=false;comparingRaw_=false;
    preview_=enabled?backgroundPreview_:resultPreview_;displayingResult_=true;
    emit imageChanged();emit changed();
}
void DeepSkyController::setSourceDebayer(bool value){if(busy_||value==sourceDebayer_)return;sourceDebayer_=value;emit changed();if(!sourceRow_.isEmpty())for(int i=0;i<frames_.size();++i)if(frames_[i].toMap()["path"]==sourceRow_["path"]){inspect(i);break;}}
void DeepSkyController::setSourceAutoStretch(bool value){if(busy_||value==sourceAutoStretch_)return;sourceAutoStretch_=value;emit changed();if(!sourceRow_.isEmpty())for(int i=0;i<frames_.size();++i)if(frames_[i].toMap()["path"]==sourceRow_["path"]){inspect(i);break;}}
void DeepSkyController::inspect(int index){
    if(busy_||index<0||index>=frames_.size())return;const auto row=frames_[index].toMap();const bool debayer=sourceDebayer_,autoStretch=sourceAutoStretch_;
    run(tr("Opening frame…"),[this,row,debayer,autoStretch](std::stop_token stop){auto raw=readFrame(row);QString interpretation=raw.channels==3?tr("RGB source · three channels"):raw.bayer!=domain::BayerPattern::None?(debayer?tr("Bayer %1 source · color preview is debayered"):tr("Bayer %1 source · raw mosaic preview")).arg(QString::fromUtf8(domain::to_string(raw.bayer).data())):tr("Monochrome source · one channel");if(!debayer)raw.bayer=domain::BayerPattern::None;auto rgb=std::make_shared<processing::FloatImage>(processing::linearRgb(raw,[stop]{check(stop);}));const auto display=autoStretch?prepare(*rgb):Display{rgb,makeImagePreview(*rgb),{}};check(stop);post([this,rgb,display,row,interpretation,stop]{if(!stop.stop_requested()){sourceRow_=row;sourceInterpretation_=interpretation;show(rgb,false,display.preview,display.pixels,display.stretch);status_=tr("Source preview: %1. This individual exposure is for inspection only. Final image exports the stack with your finishing adjustments; Linear FITS exports the stack before finishing.").arg(row["name"].toString());emit changed();}});});
}
QVariantList DeepSkyController::calibrationMasters() const {
    try{auto rows=calibrationMasterInventory();for(auto& v:rows){auto r=v.toMap();const auto compatibility=masterCompatibility(r,frames_);for(auto it=compatibility.begin();it!=compatibility.end();++it)r[it.key()]=it.value();v=r;}return rows;}catch(const std::exception&){return {};}
}
std::uint64_t DeepSkyController::retainedImageBytes() const {
    std::set<const processing::FloatImage*> seen;std::uint64_t retained=256ULL*1024*1024;
    auto keep=[&](const auto& image){if(image&&seen.insert(image.get()).second)retained+=image->rgb.size()*sizeof(float);};
    for(const auto& [key,image]:stacks_)keep(image);for(const auto& [key,image]:diagnosticMaps_)keep(image);
    for(const auto& image:{linear_,finished_,starsLayer_,starBackgroundLayer_,pipelineCache_,pipelineSource_})keep(image);
    std::set<qint64> previews;
    for(const auto& image:{preview_,resultPreview_,rawPreview_,backgroundPreview_,linearPreview_,comparisonPreview_,geometryPreview_,maskSourcePreview_,maskImportPreview_,maskPreview_,starsLayerPreview_,starBackgroundLayerPreview_,diagnosticPreview_})
        if(!image.isNull()&&previews.insert(image.cacheKey()).second)retained+=image.sizeInBytes();
    return retained;
}
QVariantMap DeepSkyController::calibrationBatchPlan() const {
    QVariantList inventory;QString libraryError;
    try{inventory=calibrationMasterInventory();}catch(const std::exception& e){libraryError=tr(e.what());}
    auto plan=astro::app::calibrationBatchPlan(frames_,inventory);plan["libraryError"]=libraryError;
    const auto physical=processing::physicalMemoryBytes();
    const auto budget=physical?std::min<std::uint64_t>(8ULL*1024*1024*1024,physical/2):2ULL*1024*1024*1024;
    const auto retained=retainedImageBytes();
    std::error_code error;const auto space=std::filesystem::space(nativePath(integrationTemporaryDirectory()),error);
    const auto folder=QFileInfo(integrationTemporaryDirectory());
    const bool diskKnown=!error&&space.available!=std::uintmax_t(-1)&&folder.isDir()&&folder.isWritable();
    auto groups=plan["groups"].toList();
    for(auto& v:groups) {
        auto job=v.toMap();auto temporary=job["temporaryBytes"].toULongLong();
        for(const auto& c:job["corrections"].toList())temporary=std::max(temporary,c.toMap()["temporaryBytes"].toULongLong());
        const auto working=retained+job["workingBytes"].toULongLong();job["workingBytes"]=qulonglong(working);job["temporaryBytes"]=qulonglong(temporary);
        if(!libraryError.isEmpty())job["error"]=libraryError;
        else if(!job["blocked"].toBool()) {
            if(working>budget)job["error"]=tr("The master batch memory estimate exceeds the safety budget. Close other results or use smaller source images.");
            else if(!diskKnown||temporary>space.available||256ULL*1024*1024>space.available-temporary)job["error"]=tr("Not enough accessible temporary disk space for this master group. Choose another temporary folder.");
        }
        job["blocked"]=job.contains("error");v=job;
    }
    plan["groups"]=groups;plan["memoryBudget"]=qulonglong(budget);plan["temporaryDirectory"]=integrationTemporaryDirectory();
    return plan;
}
void DeepSkyController::createCalibrationMasters(const QVariantList& ids,const QVariantMap& corrections,const QString& prefix,bool reviewed,const QString& revision) {
    if(busy_)return;
    const auto plan=calibrationBatchPlan();
    QVariantList jobs;
    try {
        if(ids.empty()||ids.size()>64||prefix.size()>40)throw std::runtime_error("Choose between 1 and 64 master groups and a name prefix of at most 40 characters.");
        if(revision!=plan["revision"].toString())throw std::runtime_error("The calibration selection changed. Review the refreshed batch plan.");
        QSet<QString> selected;
        for(const auto& id:ids) {
            const auto key=id.toString();if(selected.contains(key))throw std::runtime_error("Duplicate master group selection.");selected.insert(key);
            bool found=false;
            for(const auto& v:plan["groups"].toList())if(v.toMap()["id"]==key) {
                auto job=v.toMap();found=true;
                if(job["blocked"].toBool())throw std::runtime_error(job["error"].toString().toStdString());
                if(job["reviewRequired"].toBool()&&!reviewed)throw std::runtime_error("Review the missing calibration metadata before building this batch.");
                if(job["role"]=="Flat") {
                    const auto requested=corrections.value(key,job["defaultCorrection"]).toString();QVariantMap correction;
                    for(const auto& c:job["corrections"].toList())if(c.toMap()["id"]==requested)correction=c.toMap();
                    if(correction.empty())throw std::runtime_error("Choose an explicit correction for each ambiguous flat group.");
                    if((correction["reviewRequired"].toBool()||correction["compatibility"].toInt()==1)&&!reviewed)
                        throw std::runtime_error("Review the missing calibration metadata before building this batch.");
                    job["correction"]=correction;
                }
                jobs<<job;break;
            }
            if(!found)throw std::runtime_error("The calibration selection changed. Review the refreshed batch plan.");
        }
    }catch(const std::exception& e){status_=tr(e.what());emit changed();return;}
    // Keep dependency order from the reviewed plan rather than caller order.
    std::stable_sort(jobs.begin(),jobs.end(),[](const QVariant& a,const QVariant& b){return QStringList{"Bias","DarkFlat","Dark","Flat"}.indexOf(a.toMap()["role"].toString())<QStringList{"Bias","DarkFlat","Dark","Flat"}.indexOf(b.toMap()["role"].toString());});
    calibrationBatchResults_.clear();emit changed();
    const auto temporary=integrationTemporaryDirectory();
    run(tr("Building calibration masters…"),[this,jobs,prefix,reviewed,temporary](std::stop_token stop){
        QVariantList report;int completed=0;QMap<QString,QString> verifiedHashes;
        const auto checkpoint=[stop]{check(stop);};
        auto readVerified=[&](const QVariantMap& row) {
            checkpoint();const auto path=row["path"].toString();const auto expected=row["sourceFingerprint"].toMap()["sha256"].toString();
            const auto before=ProjectStorage::fingerprint(path,checkpoint);
            if(!expected.isEmpty()&&expected!=before["sha256"].toString())throw std::runtime_error("A calibration source changed since import. Reimport it before building masters.");
            auto image=readFrame(row);
            if(ProjectStorage::fingerprint(path,checkpoint)["sha256"]!=before["sha256"])
                throw std::runtime_error("A calibration source changed during reading. Reimport it before building masters.");
            verifiedHashes[path]=before["sha256"].toString();
            return image;
        };
        for(int n=0;n<jobs.size();++n) {
            const auto job=jobs[n].toMap();const auto name=(prefix.trimmed().isEmpty()?QString():prefix.trimmed()+" · ")+job["name"].toString();
            QVariantMap result{{"id",job["id"]},{"name",name.left(120)},{"role",job["role"]},{"count",job["count"]}};
            post([this,n,total=int(jobs.size()),name]{progress_=n*100/total;status_=tr("Master %1/%2 · %3").arg(n+1).arg(total).arg(name);emit changed();});
            try {
                checkpoint();processing::IntegrationLimits limits;limits.temporaryDirectory=nativePath(temporary);
                auto image=buildCalibrationBatchMaster(job,job["correction"].toMap(),readVerified,stop,limits);
                QVariantList sourceHashes,correctionHashes;
                for(const auto& v:job["sources"].toList())sourceHashes<<verifiedHashes[v.toMap()["path"].toString()];
                const auto correction=job["correction"].toMap();
                if(correction["savedMaster"].toBool())correctionHashes<<verifiedHashes[correction["path"].toString()];
                else for(const auto& v:correction["sources"].toList())correctionHashes<<verifiedHashes[v.toMap()["path"].toString()];
                const auto entry=storeCalibrationMaster(std::move(image),name.left(120),stop,{{"sourceHashes",sourceHashes},{"correctionHashes",correctionHashes},{"metadataReviewed",reviewed}});
                result["state"]="saved";result["masterId"]=entry["id"];result["path"]=entry["path"];result["rejectedSamples"]=entry["rejectedSamples"];++completed;
            }catch(const Cancelled&) {
                result["state"]="cancelled";result["error"]=tr("Cancelled. Previous results are preserved.");report<<result;
                for(int k=n+1;k<jobs.size();++k){const auto skipped=jobs[k].toMap();report<<QVariantMap{{"id",skipped["id"]},{"name",skipped["name"]},{"state","pending"}};}
                break;
            }catch(const std::exception& e) {
                result["state"]=stop.stop_requested()?"cancelled":"failed";result["error"]=tr(e.what());report<<result;
                // Stop on the first failure: no repeated writes on a full disk.
                for(int k=n+1;k<jobs.size();++k){auto skipped=jobs[k].toMap();report<<QVariantMap{{"id",skipped["id"]},{"name",skipped["name"]},{"state","pending"}};}
                break;
            }
            report<<result;
            post([this,report]{calibrationBatchResults_=report;emit changed();});
        }
        post([this,report,completed,total=int(jobs.size()),stop]{calibrationBatchResults_=report;progress_=completed*100/std::max(1,total);status_=stop.stop_requested()?tr("Batch cancelled: %1/%2 masters saved. Completed masters remain in the library; sources and the image result are preserved.").arg(completed).arg(total):completed==total?tr("Batch complete: %1 masters saved. Choose them explicitly from the library to apply them.").arg(completed):tr("Batch stopped: %1/%2 masters saved. Review the failed group; completed masters remain available.").arg(completed).arg(total);emit changed();});
        if(!stop.stop_requested()&&completed!=jobs.size())throw std::runtime_error(tr("Batch stopped: %1/%2 masters saved. Review the failed group; completed masters remain available.").arg(completed).arg(jobs.size()).toStdString());
    });
}

void DeepSkyController::createCalibrationMaster(const QString& role,const QString& name){
    const auto frames=frames_;run(tr("Building a calibration master…"),[this,frames,role,name](std::stop_token stop){
        if(!QStringList{"Dark","Bias","Flat","DarkFlat"}.contains(role))throw std::runtime_error("Choose a calibration role.");QVariantList sources,biases,darkFlats;for(const auto& v:frames){const auto r=v.toMap();if(!r["selected"].toBool())continue;if(r["role"]==role)sources<<r;if(r["role"]=="Bias")biases<<r;if(r["role"]=="DarkFlat")darkFlats<<r;}
        if(sources.isEmpty())throw std::runtime_error("Select source frames for this master first.");
        const auto group=sources.front().toMap()["group"];const auto first=sources.front().toMap();for(const auto& v:sources){const auto row=v.toMap();for(const auto& key:{"camera","binX","binY","gain","offset","temperature"})if(first.contains(key)!=row.contains(key)||(QString(key)=="camera"&&first[key].toString().isEmpty()!=row[key].toString().isEmpty())||(QString(key).startsWith("bin")&&(first[key].toInt()>0)!=(row[key].toInt()>0)))throw std::runtime_error("Source metadata must be consistently present. Check the selected calibration frames.");}
        for(const auto& v:sources)if(v.toMap()["master"].toBool()||v.toMap()["group"]!=group)throw std::runtime_error("Select raw calibration frames from one filter and configuration. Existing masters cannot be averaged with sources.");
        const auto checkpoint=[stop]{check(stop);};auto build=[&](const QVariantList& rows){for(const auto& v:rows)if(v.toMap()["master"].toBool()){if(rows.size()!=1)throw std::runtime_error("Select one master or its source frames, never both.");return readFrame(v.toMap());}return processing::masterFrame(rows.size(),[&](int n){check(stop);return readFrame(rows[n].toMap());},false,checkpoint,true);};
        recording::AstroImage result;
        if(role=="Flat"){processing::Calibration correction;if(!darkFlats.isEmpty())correction.dark=build(darkFlats);else if(!biases.isEmpty())correction.bias=build(biases);else throw std::runtime_error("Select a matching dark-flat or bias before creating a normalized master flat.");result=processing::masterFrame(sources.size(),[&](int n){return processing::calibrate(readFrame(sources[n].toMap()),correction,checkpoint);},true,checkpoint,true);result.normalizedMasterFlat=true;}else result=build(sources);
        result.masterRole=role.toStdString();result.masterCount=sources.size();const auto entry=storeCalibrationMaster(std::move(result),name,stop);post([this,entry]{status_=tr("Master saved: %1 · %2 source frames. Select it from the library to use it.").arg(entry["name"].toString()).arg(entry["masterCount"].toInt());emit changed();});
    });
}
void DeepSkyController::importCalibrationMaster(const QUrl& url,const QString& role,const QString& name,bool correctedFlat){
    if(!url.isLocalFile())return;run(tr("Importing a calibration master…"),[this,url,role,name,correctedFlat](std::stop_token stop){auto a=recording::readAstroImage(url.toLocalFile().toStdString());if(role=="Flat"){if(!a.normalizedMasterFlat&&!correctedFlat)throw std::runtime_error("Confirm that the imported flat is already bias/dark-flat corrected.");if(!a.normalizedMasterFlat)a=processing::masterFrame(1,[&](int){return a;},true,[stop]{check(stop);});a.normalizedMasterFlat=true;}a.masterRole=role.toStdString();const auto r=storeCalibrationMaster(std::move(a),name,stop);post([this,r]{status_=tr("Master imported: %1. Original file preserved.").arg(r["name"].toString());emit changed();});});
}
void DeepSkyController::useCalibrationMaster(const QString& id,bool reviewed){
    const auto frames=frames_;run(tr("Checking calibration master…"),[this,id,frames,reviewed](std::stop_token stop){auto r=calibrationMaster(id,stop);const auto c=masterCompatibility(r,frames);if(c["compatibility"].toInt()==2)throw std::runtime_error((tr("Master incompatible: %1").arg(c["mismatch"].toString())).toStdString());if(c["compatibility"].toInt()==1&&!reviewed)throw std::runtime_error((tr("Check missing metadata before using this master: %1").arg(c["missing"].toString())).toStdString());r["masterHash"]=r["hash"];r["sourceFingerprint"]=ProjectStorage::fingerprint(r["path"].toString(),[stop]{check(stop);});r["selected"]=true;r["scale"]=0;check(stop);post([this,r]{for(int n=0;n<frames_.size();++n){auto old=frames_[n].toMap();if(old["role"]==r["role"]&&(r["role"]!="Flat"||old["group"]==r["group"])){old["selected"]=false;frames_[n]=old;}}bool found=false;for(auto& v:frames_)if(v.toMap()["path"]==r["path"]){v=r;found=true;}if(!found)frames_<<r;status_=tr("Master selected: %1. Other files in this calibration role are deselected.").arg(r["name"].toString());emit changed();});});
}
void DeepSkyController::removeCalibrationMasterEntry(const QString& id){if(busy_)return;try{removeCalibrationMaster(id);status_=tr("Master removed from the library. Its FITS file is retained for existing projects.");}catch(const std::exception& e){status_=tr(e.what());}emit changed();}
QString DeepSkyController::integrationTemporaryDirectory() const {
    return QSettings().value("application/integrationTemporaryDirectory",QDir::tempPath()).toString();
}
void DeepSkyController::chooseIntegrationTemporaryDirectory(const QUrl& url) {
    if(busy_)return;
    if(url.isEmpty())QSettings().remove("application/integrationTemporaryDirectory");
    else {
        const QFileInfo folder(url.toLocalFile());
        if(!url.isLocalFile() || !folder.isDir() || !folder.isWritable()) {
            status_=tr("Choose an existing writable local temporary folder.");emit changed();return;
        }
        QSettings().setValue("application/integrationTemporaryDirectory",folder.canonicalFilePath());
    }
    emit changed();
}
QVariantMap DeepSkyController::integrationResources(const QVariantMap& options) const {
    const auto physical=processing::physicalMemoryBytes();
    const auto budget=physical?std::min<std::uint64_t>(8ULL*1024*1024*1024,physical/2):2ULL*1024*1024*1024;
    const auto directory=integrationTemporaryDirectory();
    QVariantMap plan{{"memoryBudget",qulonglong(budget)},{"directory",directory},{"blocked",false},{"groups",QVariantList{}},{"availableDisk",qlonglong(-1)}};
    try {
        std::map<QString,QVariantList> groups;
        std::map<QString,std::uint64_t> calibrations;
        std::uint64_t calibrationBytes=0,largestCalibration=0;
        for(const auto& v:frames_) {
            const auto row=v.toMap();if(!row["selected"].toBool())continue;
            if(row["role"]=="Light")groups[row["group"].toString()]<<row;
            else if(!options.value("calibrated").toBool()) {
                const auto w=row["width"].toInt(),h=row["height"].toInt(),c=row["channels"].toInt();
                if(w<1 || h<1 || std::uint64_t(w)*h>32000000 || (c!=1 && c!=3))throw std::runtime_error("Invalid integration resource settings.");
                const auto bytes=std::uint64_t(w)*h*c*4;
                const auto key=row["role"].toString()+(row["role"]=="Flat"?"/"+row["group"].toString():QString());
                calibrations[key]=std::max(calibrations[key],bytes);largestCalibration=std::max(largestCalibration,bytes);
            }
        }
        for(const auto& [key,bytes]:calibrations)calibrationBytes+=bytes;
        if(groups.empty()){plan["message"]=tr("Select light frames to estimate resources.");return plan;}
        // Count shared native images and QImages once. Keep the previous result
        // while the new stack is computed, including pipeline caches.
        const auto retained=retainedImageBytes();
        std::uint64_t peak=retained+calibrationBytes*2+largestCalibration*6,results=0,disk=0,largestResult=0;
        QVariantList rows;
        for(const auto& [name,group]:groups) {
            auto reference=group.front().toMap();
            if(references_.contains(name))for(const auto& row:group)if(row.toMap()["path"].toString()==references_.at(name))reference=row.toMap();
            processing::IntegrationResourceRequest request;
            request.width=reference["width"].toInt();request.height=reference["height"].toInt();request.channels=reference["channels"].toInt();request.count=int(group.size());
            request.drizzle=options.value("drizzle").toBool();request.scale=options.value("scale",1).toDouble();request.nativeBayer=options.value("nativeBayer").toBool();
            request.diagnosticMaps=options.value("diagnosticMaps").toBool();request.sequenceBackground=options.value("sequenceBackground").toBool();request.wcsProjection=options.value("wcsProjection").toBool();request.calibrationBytes=calibrationBytes;
            const auto estimate=processing::estimateIntegrationResources(request);
            peak=std::max(peak,retained+results+estimate.workingBytes);results+=estimate.resultBytes;
            disk=std::max(disk,estimate.temporaryBytes);largestResult=std::max(largestResult,std::uint64_t(estimate.width)*estimate.height*12);
            rows<<QVariantMap{{"group",name},{"width",estimate.width},{"height",estimate.height},{"count",request.count},{"stripRows",estimate.stripRows},{"stripBytes",qulonglong(estimate.stripBytes)},{"temporaryBytes",qulonglong(estimate.temporaryBytes)}};
        }
        plan["groups"]=rows;plan["workingBytes"]=qulonglong(peak);plan["resultBytes"]=qulonglong(results+largestResult);
        plan["temporaryBytes"]=qulonglong(disk);plan["requiredDisk"]=qulonglong(disk+256ULL*1024*1024);
        // Query the actual path as the engine does. macOS firmlinks can make
        // a mount-table lookup misclassify writable /var as the read-only root.
        const QFileInfo folder(directory);std::error_code error;
        const auto storage=std::filesystem::space(nativePath(directory),error);
        if(!folder.isDir() || !folder.isWritable() || error || storage.available==std::uintmax_t(-1))
            plan["error"]=tr("The temporary folder is unavailable or its free space cannot be checked. Choose another folder.");
        else {
            plan["availableDisk"]=qulonglong(storage.available);
            if(storage.available<disk+256ULL*1024*1024)
                plan["error"]=tr("Not enough temporary disk space. Choose another folder or fewer exposures.");
        }
        if(peak>budget)plan["error"]=tr("The estimated memory exceeds the safety budget. Disable diagnostic maps, reduce drizzle scale or use fewer filter groups.");
        if(results+largestResult>4ULL*1024*1024*1024 || groups.size()*(options.value("diagnosticMaps").toBool()?4:1)+2>64)
            plan["error"]=tr("The planned results exceed automatic recovery limits (4 GiB / 64 images). Disable maps or process fewer filter groups together.");
    }catch(const std::exception& e){plan["error"]=tr(e.what());}
    plan["blocked"]=plan.contains("error");return plan;
}
void DeepSkyController::integrate(bool calibrated,double sigma,bool normalize,bool correctBadPixels,bool sequenceBackground,bool drizzle,double drizzleScale,double pixelFraction,bool nativeBayer,QString bayerPattern,bool diagnosticMaps,bool wcsProjection) {
    if(busy_)return;
    const auto resources=integrationResources({{"calibrated",calibrated},{"sequenceBackground",sequenceBackground},{"drizzle",drizzle},{"scale",drizzleScale},{"nativeBayer",nativeBayer},{"diagnosticMaps",diagnosticMaps},{"wcsProjection",wcsProjection}});
    if(resources.value("blocked").toBool()){status_=resources["error"].toString();emit changed();return;}
    const auto temporaryDirectory=integrationTemporaryDirectory();
    const auto frames=frames_;const auto references=references_;
    run(tr("Calibrating and integrating…"),[this,frames,calibrated,sigma,normalize,correctBadPixels,sequenceBackground,references,drizzle,drizzleScale,pixelFraction,nativeBayer,bayerPattern,diagnosticMaps,wcsProjection,temporaryDirectory](std::stop_token stop){
        const QStringList patterns{"RGGB","BGGR","GRBG","GBRG"};
        if(drizzle&&nativeBayer&&!bayerPattern.isEmpty()&&!patterns.contains(bayerPattern))throw std::runtime_error("Invalid Bayer pattern selection.");
        const auto declaredPattern=drizzle&&nativeBayer&&!bayerPattern.isEmpty()?static_cast<domain::BayerPattern>(patterns.indexOf(bayerPattern)+1):domain::BayerPattern::None;
        auto read=[&](const QVariantMap& row){auto image=readFrame(row);if(declaredPattern!=domain::BayerPattern::None&&image.channels==1){if(image.bayer!=domain::BayerPattern::None&&image.bayer!=declaredPattern)throw std::runtime_error("The declared Bayer pattern conflicts with the source metadata.");image.bayer=declaredPattern;}return image;};
        std::map<QString,QVariantList> lights,flats;QVariantList darks,biases,darkFlats,badPixels;
        for(const auto& value:frames){const auto row=value.toMap();if(!row["selected"].toBool())continue;const auto role=row["role"].toString(),group=row["group"].toString();if(role=="Light")lights[group]<<row;else if(role=="Flat")flats[group]<<row;else if(role=="Dark")darks<<row;else if(role=="Bias")biases<<row;else if(role=="DarkFlat")darkFlats<<row;else if(role=="BadPixels")badPixels<<row;}
        if(lights.empty())throw std::runtime_error("Add and select light frames first.");
        if(drizzle&&nativeBayer)for(const auto& [group,rows]:lights)for(const auto& value:rows){const auto row=value.toMap();const auto pattern=row["bayer"].toString();if(row["channels"].toInt()!=1||(declaredPattern==domain::BayerPattern::None&&!patterns.contains(pattern)))throw std::runtime_error("Native Bayer drizzle requires single-channel CFA sources with a recognized Bayer pattern.");if(declaredPattern!=domain::BayerPattern::None&&patterns.contains(pattern)&&pattern!=bayerPattern)throw std::runtime_error("The declared Bayer pattern conflicts with the source metadata.");}
        if(calibrated&&(!darks.empty()||!flats.empty()||!biases.empty()||!darkFlats.empty()||!badPixels.empty()))
            throw std::runtime_error("Calibration files are selected but Lights already calibrated is enabled. Disable it to apply your calibration files, or deselect those files.");
        auto checkpoint=[stop]{check(stop);};
        auto master=[&](const QVariantList& rows,bool normalize=false){
            for(const auto& v:rows)if(v.toMap()["master"].toBool()){if(rows.size()!=1)throw std::runtime_error("Select one master or its source frames, never both.");return read(v.toMap());}
            return processing::masterFrame(int(rows.size()),[&](int n){check(stop);return read(rows[n].toMap());},normalize,checkpoint);};
        processing::Calibration calibration;
        if(!calibrated){if(!darks.empty())calibration.dark=master(darks);if(!biases.empty())calibration.bias=master(biases);if(!darkFlats.empty())calibration.darkFlat=master(darkFlats);if(correctBadPixels){if(!badPixels.empty())calibration.badPixels=master(badPixels);else if(calibration.dark)calibration.badPixels=processing::detectBadPixels(*calibration.dark);}}
        std::map<QString,std::shared_ptr<const processing::FloatImage>> results,maps;std::map<QString,QVariantMap> astrometry;std::map<QString,double> exposures;std::map<QString,int> counts;QStringList summary;QVariantList integrationReport;double exposure=0;
        for(auto& [group,rows]:lights) {
            if(references.contains(group)) {
                for(int n=0;n<rows.size();++n)if(rows[n].toMap()["path"].toString()==references.at(group)){rows.swapItemsAt(0,n);break;}
            }
            check(stop);auto groupCalibration=calibration;
            if(!calibrated) {
                auto found=flats.find(group);
                if(found==flats.end()&&flats.size()==1&&lights.size()==1)found=flats.begin();
                if(found!=flats.end()) {
                    bool stored=false;for(const auto& v:found->second)stored|=v.toMap()["master"].toBool();
                    if(stored){if(found->second.size()!=1||!found->second.front().toMap()["masterFlat"].toBool())throw std::runtime_error("Select one calibrated normalized master flat, without raw flats.");groupCalibration.flat=read(found->second.front().toMap());}
                    else {processing::Calibration flatCalibration;flatCalibration.dark=calibration.darkFlat;if(!flatCalibration.dark)flatCalibration.bias=calibration.bias;
                    auto corrected=[&](int n){check(stop);return processing::calibrate(read(found->second[n].toMap()),flatCalibration,checkpoint);};
                    groupCalibration.flat=processing::masterFrame(int(found->second.size()),corrected,true,checkpoint);}
                }
            }
            processing::BackgroundOptions sequenceGradient;
            processing::IntegrationLimits limits;limits.temporaryDirectory=nativePath(temporaryDirectory);
            const processing::DrizzleOptions drizzleOptions{drizzleScale,pixelFraction,nativeBayer};
            auto result=processing::integrate(int(rows.size()),[&](int n){check(stop);return read(rows[n].toMap());},groupCalibration,calibrated,sigma,checkpoint,
                [this,group](int done,int total,const std::string& stage){post([this,group,done,total,stage]{progress_=done*100/std::max(1,total);stage_=tr(stage.c_str());status_=group+" · "+stage_;emit changed();});},normalize,sequenceBackground?&sequenceGradient:nullptr,drizzle?&drizzleOptions:nullptr,diagnosticMaps,wcsProjection,&limits);
            recording::AstroImage metadata;metadata.width=result.image.width;metadata.height=result.image.height;metadata.wcs=result.wcs;metadata.centerRa=result.centerRa;metadata.centerDec=result.centerDec;metadata.pixelScale=result.pixelScale;metadata.date=result.date;astrometry[group]=imageAstrometry(metadata);
            exposure+=result.totalExposure;exposures[group]=result.totalExposure;counts[group]=int(result.accepted.size());
            summary<<tr("%1: %2/%3 frames, %4 s, %5 rejected samples").arg(group).arg(result.accepted.size()).arg(rows.size()).arg(result.totalExposure,0,'f',1).arg(result.rejectedSamples);
            if(result.accepted.size()<2)summary<<tr("Only one usable light in %1: no multi-frame noise reduction was possible.").arg(group);
            QVariantList wcsChecks,rejectedFrames;
            for(std::size_t n=0;n<result.rejected.size();++n)rejectedFrames<<QVariantMap{{"frame",result.rejected[n]},{"name",rows[result.rejected[n]].toMap()["name"]},{"reason",QString::fromStdString(result.reasons[n])}};
            for(const auto& item:result.wcsChecks)wcsChecks<<QVariantMap{{"frame",item.frame},{"name",rows[item.frame].toMap()["name"]},{"matches",item.matches},{"sipOrder",item.sipOrder},{"rms",item.residual}};
            if(result.wcsProjection)summary<<tr("WCS/SIP projection: %1 validated frames, undistorted TAN output. Absolute astrometry remains unverified.").arg(result.accepted.size());
            integrationReport<<QVariantMap{{"group",group},{"wcsProjection",result.wcsProjection},{"wcsChecks",wcsChecks},{"rejectedFrames",rejectedFrames},{"drizzle",drizzle},{"nativeBayer",result.nativeBayer},{"bayerPattern",result.nativeBayer?(declaredPattern==domain::BayerPattern::None?rows.front().toMap()["bayer"].toString():bayerPattern):QString()},{"scale",drizzle?drizzleScale:1},{"pixelFraction",pixelFraction},{"width",result.image.width},{"height",result.image.height},{"accepted",int(result.accepted.size())},{"minimum",result.minimumContributors},{"mean",result.meanContributors},{"subpixelDiversity",result.subpixelDiversity}};
            if(drizzle) {
                summary<<tr("Drizzle ×%1 · pixel fraction %2 · %3 × %4 px · contributors: min %5, mean %6").arg(drizzleScale,0,'f',1).arg(pixelFraction,0,'f',2).arg(result.image.width).arg(result.image.height).arg(result.minimumContributors).arg(result.meanContributors,0,'f',1);
                if(result.nativeBayer)summary<<tr("Native Bayer reconstruction · contributors counted separately for R, G and B.");
                if(!result.subpixelDiversity)summary<<tr("Subpixel shifts are very similar: more output pixels do not guarantee finer detail.");
                if(result.minimumContributors<3)summary<<tr("Some drizzle pixels have fewer than three contributing frames; robust rejection is unavailable there.");
            }
            for(std::size_t n=0;n<result.rejected.size();++n)summary<<rows[result.rejected[n]].toMap()["name"].toString()+": "+tr(result.reasons[n].c_str());
            if(diagnosticMaps){maps[group+"/weights"]=std::make_shared<processing::FloatImage>(std::move(result.weights));maps[group+"/contributors"]=std::make_shared<processing::FloatImage>(std::move(result.contributors));maps[group+"/rejections"]=std::make_shared<processing::FloatImage>(std::move(result.rejectionMap));}
            results[group]=std::make_shared<processing::FloatImage>(std::move(result.image));
        }
        const auto display=prepare(*results.begin()->second,true,checkpoint);
        const auto rawRow=lights.begin()->second[0].toMap();
        const auto rawPreview=previewImage(processing::linearRgb(read(rawRow),checkpoint),true);
        check(stop);
        post([this,results=std::move(results),maps=std::move(maps),astrometry,exposures,counts,display,summary,integrationReport,exposure,rawRow,rawPreview,stop]()mutable{if(stop.stop_requested())return;diagnosticMaps_=std::move(maps);hdrReport_.clear();diagnosticPreview_={};diagnosticInfo_.clear();emit diagnosticChanged();integrationReport_=integrationReport;rawPreview_=rawPreview;rawName_=rawRow["name"].toString();stacks_=std::move(results);stackAstrometry_=astrometry;groups_.clear();for(const auto& [group,image]:stacks_)groups_<<group;groupExposure_=exposures;groupCounts_=counts;exposureCount_=counts.begin()->second;resultFilters_={groups_.front()};totalExposure_=exposures.begin()->second;show(stacks_.begin()->second,true,display.preview,display.pixels,display.stretch,display.before);astrometry_=stackAstrometry_.begin()->second;const auto saved=projectOptions_["astrometry"].toMap();if(validAstrometryState(saved)&&saved["imageKey"].toString()==QString::fromLatin1(linearImageKey(*linear_))&&readWcs(saved["wcs"]))astrometry_=saved;progress_=100;stage_=tr("Complete");status_=summary.join("\n");emit changed();});
    });
}

QVariantList DeepSkyController::diagnosticMaps() const {
    QVariantList rows;for(const auto& [key,image]:diagnosticMaps_){const auto slash=key.lastIndexOf('/');rows<<QVariantMap{{"key",key},{"group",key.left(slash)},{"kind",key.mid(slash+1)},{"width",image->width},{"height",image->height}};}return rows;
}
void DeepSkyController::inspectDiagnostic(const QString& key,int channel) {
    if(busy_||channel<0||channel>3||!diagnosticMaps_.contains(key))return;
    const auto image=diagnosticMaps_.at(key);
    run(tr("Preparing diagnostic map…"),[this,image,key,channel](std::stop_token stop){
        double minimum=1e300,maximum=0;std::uint64_t positive=0;const auto pixels=std::size_t(image->width)*image->height;
        auto value=[&](std::size_t p){return channel<3?image->rgb[p*3+channel]:std::max({image->rgb[p*3],image->rgb[p*3+1],image->rgb[p*3+2]});};
        for(std::size_t p=0;p<pixels;++p){if(p%65536==0)check(stop);const auto v=value(p);minimum=std::min(minimum,double(v));maximum=std::max(maximum,double(v));positive+=v>0;}
        QImage preview(std::min(1200,image->width),std::max(1,image->height*std::min(1200,image->width)/image->width),QImage::Format_RGB32);
        if(preview.isNull())throw std::runtime_error("Cannot allocate diagnostic preview.");
        for(int y=0;y<preview.height();++y){check(stop);auto* row=reinterpret_cast<QRgb*>(preview.scanLine(y));for(int x=0;x<preview.width();++x){const auto p=std::size_t(y*image->height/preview.height())*image->width+x*image->width/preview.width();const int v=maximum>0?int(std::lround(std::clamp(double(value(p))/maximum,0.,1.)*255)):0;row[x]=qRgb(v,v,v);}}
        const QVariantMap info{{"key",key},{"channel",channel},{"minimum",minimum},{"maximum",maximum},{"positivePercent",100.*positive/pixels},{"positivePixels",qulonglong(positive)},{"width",image->width},{"height",image->height}};
        post([this,stop,preview,info]{if(stop.stop_requested())return;diagnosticPreview_=preview;diagnosticInfo_=info;status_=tr("Diagnostic map ready. Display scaling does not change exported values.");emit diagnosticChanged();emit changed();});
    });
}
void DeepSkyController::exportDiagnostic(const QUrl& url,const QString& key) {
    if(busy_||!url.isLocalFile()||!diagnosticMaps_.contains(key))return;
    const auto path=url.toLocalFile();
    if(!QStringList{"fits","fit","fts"}.contains(QFileInfo(path).suffix().toLower())){status_=tr("Diagnostic maps require floating-point FITS export.");emit changed();return;}
    if(projectUrl_.isLocalFile()&&sameFile(projectUrl_.toLocalFile(),path)){status_=tr("A project file cannot be overwritten by an image.");emit changed();return;}
    for(const auto& value:frames_)if(sameFile(value.toMap()["path"].toString(),path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    for(const auto& mask:referencedMasks(finishSettings_,projectOptions_))if(sameFile(mask,path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    const auto image=diagnosticMaps_.at(key);const auto group=key.left(key.lastIndexOf('/'));const auto state=stackAstrometry_.find(group);const auto wcs=state==stackAstrometry_.end()?std::optional<recording::CelestialWcs>{}:readWcs(state->second.value("wcs"));
    exporting_=true;run(tr("Exporting diagnostic map…"),[this,image,path,wcs](std::stop_token stop){
        QTemporaryDir directory(QFileInfo(path).dir().filePath(".astrocapture-XXXXXX"));if(!directory.isValid())throw std::runtime_error("Cannot create temporary export files.");
        recording::AstroImage output;output.width=image->width;output.height=image->height;output.channels=3;output.bits=32;output.samples=image->rgb;output.wcs=wcs;output.imageType="Diagnostic";
        const auto temporary=directory.filePath("map.fits");check(stop);recording::writeAstroFits(nativePath(temporary),output);check(stop);atomicCopy(temporary,path,stop);
        post([this,path]{status_=tr("Saved %1").arg(path);emit changed();});
    });
}
void DeepSkyController::mergeHdr(const QVariantList& sources,double transition,bool reviewed) {
    if(busy_)return;const auto frames=frames_;
    run(tr("Aligning and merging HDR exposures…"),[this,sources,transition,reviewed,frames](std::stop_token stop){
        if(!reviewed)throw std::runtime_error("Confirm that HDR sources are calibrated, linear and use the same acquisition settings and sample units.");
        if(sources.size()<2||sources.size()>8)throw std::runtime_error("HDR requires between two and eight calibrated linear images.");
        for(const auto& v:frames)if(v.toMap()["selected"].toBool()&&v.toMap()["role"]!="Light")throw std::runtime_error("Deselect calibration files for HDR. Calibrate each exposure series before merging.");
        QVariantList rows,provenance;std::vector<processing::HdrInput> inputs;std::set<int> seen;QString group;
        double referenceExposure=0;int reference=0;
        for(const auto& v:sources){const auto input=v.toMap();bool valid=false;const int index=input["index"].toInt(&valid);if(!valid||index<0||index>=frames.size()||!seen.insert(index).second)throw std::runtime_error("Invalid HDR source selection.");const auto row=frames[index].toMap();if(!row["selected"].toBool()||row["role"]!="Light"||!QStringList{"fit","fits","fts","fz"}.contains(QFileInfo(row["path"].toString()).suffix().toLower()))throw std::runtime_error("HDR sources must be selected linear FITS lights.");if(rows.empty())group=row["group"].toString();if(row["group"].toString()!=group)throw std::runtime_error("HDR sources must belong to the same filter group.");
            const double exposure=input["exposure"].toDouble(),saturation=input["saturation"].toDouble();if(exposure>referenceExposure){referenceExposure=exposure;reference=int(rows.size());}rows<<row;inputs.push_back({exposure,saturation});provenance<<QVariantMap{{"path",row["path"]},{"exposure",exposure},{"saturation",saturation}};
        }
        const auto checkpoint=[stop]{check(stop);};
        auto merged=processing::mergeHdr(inputs,[&](int n){checkpoint();return readFrame(rows[n].toMap());},transition,checkpoint);
        auto metadata=readFrame(rows[reference].toMap());const auto astrometry=imageAstrometry(metadata);
        auto image=std::make_shared<processing::FloatImage>(std::move(merged.image));const auto display=prepare(*image,true,checkpoint);
        std::map<QString,std::shared_ptr<const processing::FloatImage>> maps;
        maps["HDR/recovered"]=std::make_shared<processing::FloatImage>(std::move(merged.recoveredMap));maps["HDR/clipped"]=std::make_shared<processing::FloatImage>(std::move(merged.clippedMap));
        QVariantList residuals;for(const auto& a:merged.alignments)residuals<<QVariantMap{{"matches",a.matches},{"residual",a.residual}};
        const QVariantMap report{{"sources",provenance},{"transition",transition},{"referenceExposure",referenceExposure},{"recovered",qulonglong(merged.recoveredPixels)},{"clipped",qulonglong(merged.clippedPixels)},{"pixels",qulonglong(std::size_t(image->width)*image->height)},{"alignments",residuals},{"imageKey",QString::fromLatin1(linearImageKey(*image))},{"filter",group}};
        checkpoint();post([this,stop,image,display,maps=std::move(maps),astrometry,report]()mutable{if(stop.stop_requested())return;
            hdrReport_=report;projectOptions_["hdrOptions"]=QVariantMap{{"sources",report["sources"]},{"transition",report["transition"]}};emit projectOptionsChanged();integrationReport_.clear();diagnosticMaps_=std::move(maps);diagnosticPreview_={};diagnosticInfo_.clear();emit diagnosticChanged();stacks_.clear();stacks_["HDR"]=image;stackAstrometry_.clear();stackAstrometry_["HDR"]=astrometry;groups_={"HDR"};groupExposure_.clear();groupCounts_.clear();groupExposure_["HDR"]=0;groupCounts_["HDR"]=0;totalExposure_=0;exposureCount_=0;resultFilters_={report["filter"].toString()};rawPreview_={};rawName_.clear();show(image,true,display.preview,display.pixels,display.stretch,display.before);astrometry_=astrometry;
            status_=tr("HDR complete: %1 pixels recovered, %2 still saturated. Output units match the longest individual exposure; total integration time is unknown.").arg(report["recovered"].toULongLong()).arg(report["clipped"].toULongLong());progress_=100;emit changed();
        });
    });
}
void DeepSkyController::selectResult(const QString& group){if(busy_)return;const auto found=stacks_.find(group);if(found==stacks_.end())return;const auto image=found->second;run(tr("Displaying integration…"),[this,image,group](std::stop_token stop){const auto display=prepare(*image,true,[stop]{check(stop);});check(stop);post([this,image,display,group,stop]{if(!stop.stop_requested()){totalExposure_=groupExposure_[group];exposureCount_=groupCounts_[group];resultFilters_={group};show(image,true,display.preview,display.pixels,display.stretch,display.before);astrometry_=stackAstrometry_[group];emit changed();}});});}
void DeepSkyController::compose(const QString& red,const QString& green,const QString& blue,const QString& luminance) {
    if(busy_)return;
    if(!stacks_.contains(red)||!stacks_.contains(green)||!stacks_.contains(blue)||(!luminance.isEmpty()&&!stacks_.contains(luminance))){status_=tr("Select integrated groups for each color channel.");emit changed();return;}
    auto r=stacks_.at(red),g=stacks_.at(green),b=stacks_.at(blue),l=luminance.isEmpty()?nullptr:stacks_.at(luminance);
    double exposure=0;int count=0;std::set<QString> selected{red,green,blue};if(!luminance.isEmpty())selected.insert(luminance);QStringList filters;for(const auto& name:selected){exposure+=groupExposure_[name];count+=groupCounts_[name];filters<<name;}
    run(tr("Aligning and combining channels…"),[this,r,g,b,l,exposure,count,filters](std::stop_token stop){auto combined=std::make_shared<processing::FloatImage>(processing::combineChannels(*r,*g,*b,l.get(),[stop]{check(stop);}));const auto display=prepare(*combined,true,[stop]{check(stop);});check(stop);post([this,combined,display,exposure,count,filters,stop]{if(stop.stop_requested())return;totalExposure_=exposure;exposureCount_=count;resultFilters_=filters;show(combined,true,display.preview,display.pixels,display.stretch,display.before);status_=tr("Channels combined. Adjust the stretch, then export.");progress_=100;emit changed();});});
}
void DeepSkyController::apply(const QVariantMap& options) {
    if(busy_){if(adjusting_){pendingAdjustments_=options;worker_.request_stop();}return;}
    if(!linear_)return;
    auto settings=defaultDeepSkySettings();
    for(auto it=settings.begin();it!=settings.end();++it)if(options.contains(it.key()))it.value()=options[it.key()];
    const auto validated=checkedDeepSkySettings(settings);
    if(!validated){status_=tr("Invalid finishing settings. Check levels and parameter ranges.");emit changed();return;}
    const auto image=linear_;
    const auto key=QString::fromLatin1(linearImageKey(*image));
    auto bound=[&](const QVariantMap& v){return !v["photometry"].toBool()||v["photometryImageKey"].toString().isEmpty()||v["photometryImageKey"].toString()==key;};
    bool validReferences=bound(*validated);for(const auto& op:(*validated)["operations"].toList())if(op.toMap()["enabled"].toBool())validReferences&=bound(op.toMap()["settings"].toMap());
    if(!validReferences){status_=tr("Catalogue references belong to a different image. Prepare PCC/SPCC again for this image.");emit changed();return;}
    auto cacheKey=QJsonDocument(QJsonArray::fromVariantList((*validated)["operations"].toList())).toJson(QJsonDocument::Compact);
    for(const auto& operation:(*validated)["operations"].toList()){const auto parameters=operation.toMap()["settings"].toMap();if(parameters["maskType"].toInt()==5){const QFileInfo file(parameters["maskFile"].toString());cacheKey+=QByteArray::number(file.size())+":"+QByteArray::number(file.lastModified().toMSecsSinceEpoch());}}
    const auto cache=(pipelineSource_==image&&pipelineCacheKey_==cacheKey)?pipelineCache_:nullptr;
    const auto orientations=comparisonOrientations(*validated);
    const auto orientationKey=QJsonDocument(QJsonArray::fromVariantList(orientations)).toJson(QJsonDocument::Compact);
    const auto beforeSource=linearPreview_;
    const auto maskSourceCache=(pipelineSource_==image&&pipelineCacheKey_==cacheKey)?maskSourcePreview_:QImage{};
    const auto beforeCache=orientationKey==comparisonOrientationKey_?comparisonPreview_:QImage{};
    adjusting_=true;
    run(tr("Adjusting background, colors and stretch…"),[this,image,settings=*validated,cache,cacheKey,beforeSource,beforeCache,orientations,orientationKey,maskSourceCache](std::stop_token stop) {
        const auto checkpoint=[stop]{check(stop);};
        const auto before=beforeCache.isNull()?orientComparison(beforeSource,orientations):beforeCache;
        checkpoint();
        auto f=finishFromSettings(settings);
        std::shared_ptr<const processing::FloatImage> pipeline=cache;
        if(!pipeline&&!settings["operations"].toList().isEmpty())
            pipeline=std::make_shared<processing::FloatImage>(evaluateOperations(*image,settings["operations"].toList(),checkpoint));
        const auto& operationInput=pipeline?*pipeline:*image;
        QVariantList samples;QImage backgroundPreview;
        std::optional<processing::FloatImage> corrected;
        if(f.background) {
            auto extraction=processing::extractBackground(operationInput,f.backgroundOptions,checkpoint);
            for(const auto& point:extraction.samples)samples<<QVariant::fromValue(QVariantList{point[0],point[1]});
            processing::FloatImage small;
            small.width=std::min(1024,extraction.model.width);
            small.height=std::max(1,extraction.model.height*small.width/extraction.model.width);
            small.rgb.resize(std::size_t(small.width)*small.height*3);
            for(int y=0;y<small.height;++y){checkpoint();for(int x=0;x<small.width;++x)for(int c=0;c<3;++c)
                small.rgb[(std::size_t(y)*small.width+x)*3+c]=extraction.model.rgb[(std::size_t(y*extraction.model.height/small.height)*extraction.model.width+x*extraction.model.width/small.width)*3+c];}
            backgroundPreview=previewImage(small,true,checkpoint).scaled(extraction.model.width,extraction.model.height);
            corrected=std::move(extraction.image);f.background=false;
        }
        QVariantMap advancedReport;QImage maskPreview;std::vector<float> mask;
        if(settings["maskType"].toInt()) {
            mask=processing::makeMask(operationInput,maskFromSettings(settings,operationInput),checkpoint);
            maskPreview=QImage(operationInput.width,operationInput.height,QImage::Format_RGB32);if(maskPreview.isNull())throw std::runtime_error("Cannot allocate mask preview.");
            for(int y=0;y<operationInput.height;++y){checkpoint();auto* row=reinterpret_cast<QRgb*>(maskPreview.scanLine(y));for(int x=0;x<operationInput.width;++x){const int v=int(std::lround(mask[std::size_t(y)*operationInput.width+x]*255));row[x]=qRgb(v,v,v);}}
            advancedReport["maskCoverage"]=100.*std::accumulate(mask.begin(),mask.end(),0.)/mask.size();
        }
        std::optional<processing::FloatImage> prepared;
        if(settings["pedestal"].toDouble()!=0||settings["photometry"].toBool()||settings["deconvolution"].toBool()||settings["advancedDenoise"].toBool())
            prepared=scientificPreparation(corrected?*corrected:operationInput,settings,checkpoint,&advancedReport,[this,stop](int done,int total,const std::string& label){post([this,stop,done,total,label]{if(stop.stop_requested())return;progress_=total?done*100/total:0;stage_=QString::fromStdString(label);emit changed();});},mask.empty()?nullptr:&mask);
        const auto& source=prepared?*prepared:corrected?*corrected:operationInput;
        QVariantList psfKernel;int psfSize;const auto kernel=processing::makePsf({settings["psfProfile"].toInt(),settings["psfFwhm"].toDouble(),settings["psfBeta"].toDouble()},&psfSize);for(float v:kernel)psfKernel<<v;advancedReport["psfSize"]=psfSize;
        auto preview=allocateImagePreview(source);const auto target=previewTarget(preview);
        const bool extras=settings["starSeparation"].toBool()||!mask.empty()||settings["scnr"].toBool()||settings["rotation"].toInt()||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool();
        std::shared_ptr<processing::FloatImage> result,starBase;std::shared_ptr<const processing::FloatImage> starsLayer,starBackgroundLayer;QImage starsPreview,starBasePreview;
        if(settings["starSeparation"].toBool()){auto layers=finishLayers(source,settings,f,checkpoint);advancedReport["starsDetected"]=layers.detected;advancedReport["starsAccepted"]=layers.accepted;advancedReport["starsRejected"]=layers.rejected;advancedReport["starsSaturated"]=layers.saturated;advancedReport["starsFitRms"]=layers.fitRms;result=std::make_shared<processing::FloatImage>(std::move(layers.image));starBase=std::make_shared<processing::FloatImage>(std::move(layers.background));}
        else result=std::make_shared<processing::FloatImage>(processing::finishDeepSky(source,f,checkpoint,nullptr,extras?nullptr:&target));
        const auto histogram=makeLinearHistogram(source,f.color,f.black,f.white);
        if(extras){if(settings["scnr"].toBool())*result=processing::removeGreen(*result,settings["scnrMethod"].toInt(),settings["scnrAmount"].toFloat(),settings["scnrPreserve"].toBool(),checkpoint);if(!mask.empty()&&settings["maskScope"].toInt()==0)processing::blendMask(operationInput,*result,mask,checkpoint);if(!mask.empty()&&settings["maskScope"].toInt()==3)processing::blendMask(source,*result,mask,checkpoint);if(settings["rotation"].toInt()||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool())*result=processing::orientImage(*result,settings["rotation"].toInt(),settings["mirrorHorizontal"].toBool(),settings["mirrorVertical"].toBool(),checkpoint);preview=makeImagePreview(*result,checkpoint);}
        if(!maskPreview.isNull()&&(settings["rotation"].toInt()||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool())){maskPreview=maskPreview.transformed(QTransform().rotate(settings["rotation"].toInt()*90));maskPreview=maskPreview.transformed(QTransform().scale(settings["mirrorHorizontal"].toBool()?-1:1,settings["mirrorVertical"].toBool()?-1:1));}
        const auto geometryPreview=preview;
        const auto maskSourcePreview=maskSourceCache.isNull()?previewImage(operationInput,true,checkpoint):maskSourceCache;
        if(geometryActive(settings["geometry"].toMap())){*result=processing::transformGeometry(*result,geometryFrom(settings["geometry"].toMap()),checkpoint);preview=makeImagePreview(*result,checkpoint);if(!maskPreview.isNull())maskPreview=imageGeometry(maskPreview,settings["geometry"].toMap());}
        if(starBase){
            if(settings["scnr"].toBool())*starBase=processing::removeGreen(*starBase,settings["scnrMethod"].toInt(),settings["scnrAmount"].toFloat(),settings["scnrPreserve"].toBool(),checkpoint);
            if(settings["rotation"].toInt()||settings["mirrorHorizontal"].toBool()||settings["mirrorVertical"].toBool())*starBase=processing::orientImage(*starBase,settings["rotation"].toInt(),settings["mirrorHorizontal"].toBool(),settings["mirrorVertical"].toBool(),checkpoint);
            if(geometryActive(settings["geometry"].toMap()))*starBase=processing::transformGeometry(*starBase,geometryFrom(settings["geometry"].toMap()),checkpoint);
            auto stars=std::make_shared<processing::FloatImage>(*result);for(std::size_t i=0;i<stars->rgb.size();++i){if(i%65536==0)checkpoint();stars->rgb[i]-=starBase->rgb[i];}
            starsPreview=makeImagePreview(*stars,checkpoint);starBasePreview=makeImagePreview(*starBase,checkpoint);starsLayer=stars;starBackgroundLayer=starBase;
        }
        double low=std::numeric_limits<double>::infinity(),high=-low;std::uint64_t below=0,above=0;
        for(std::size_t i=0;i<result->rgb.size();++i){if(i%65536==0)checkpoint();const double value=result->rgb[i];if(!std::isfinite(value))throw std::runtime_error("Processing produced a non-finite sample; previous result is preserved.");low=std::min(low,value);high=std::max(high,value);below+=value<0;above+=value>1;}
        advancedReport["outputMinimum"]=low;advancedReport["outputMaximum"]=high;advancedReport["outputBelowZero"]=100.*below/result->rgb.size();advancedReport["outputAboveOne"]=100.*above/result->rgb.size();
        check(stop);
        post([this,image,pipeline,cacheKey,result,preview,backgroundPreview,histogram,samples,settings,stop,advancedReport,maskPreview,psfKernel,before,orientationKey,geometryPreview,maskSourcePreview,starsLayer,starBackgroundLayer,starsPreview,starBasePreview] {
            if(stop.stop_requested())return;
            pipelineSource_=image;pipelineCache_=pipeline;pipelineCacheKey_=cacheKey;
            starsLayer_=starsLayer;starBackgroundLayer_=starBackgroundLayer;starsLayerPreview_=starsPreview;starBackgroundLayerPreview_=starBasePreview;if(!starsLayer)starLayerView_=0;
            advancedReport_=advancedReport;maskPreview_=maskPreview;psfKernel_=psfKernel;if(maskPreview.isNull())showingMask_=false;
            geometryPreview_=geometryPreview;maskSourcePreview_=maskSourcePreview;finishSettings_=settings;backgroundSamples_=samples;histogram_=histogram;
            comparisonPreview_=before;comparisonOrientationKey_=orientationKey;
            finished_=result;resultPreview_=preview;backgroundPreview_=backgroundPreview;preview_=showingMask_&&!maskPreview.isNull()?maskPreview:showingBackgroundModel_&&!backgroundPreview.isNull()?backgroundPreview:preview;
            if(starLayerView_)preview_=starLayerView_==1?starsLayerPreview_:starBackgroundLayerPreview_;
            if(backgroundPreview.isNull())showingBackgroundModel_=false;
            comparingRaw_=false;displayingResult_=true;
            status_=tr("Adjustments applied. Linear FITS retains the original integration.");progress_=100;
            emit imageChanged();emit changed();
        });
    });
}
void DeepSkyController::exportStarLayer(const QUrl& url,int layer){
    if(busy_||!hasStarLayers()||!url.isLocalFile()||(layer!=1&&layer!=2))return;const auto path=url.toLocalFile();if(!QStringList{"fits","fit","fts"}.contains(QFileInfo(path).suffix().toLower())){status_=tr("Export layers as floating-point FITS to preserve signed values.");emit changed();return;}for(const auto& v:frames_)if(sameFile(v.toMap()["path"].toString(),path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}if(projectUrl_.isLocalFile()&&sameFile(projectUrl_.toLocalFile(),path)){status_=tr("A project file cannot be overwritten by an image.");emit changed();return;}
    auto wcs=readWcs(astrometry_.value("wcs"));if(wcs)wcs=finishWcs(*wcs,linear_->width,linear_->height,finishSettings_);const auto pixels=layer==1?starsLayer_:starBackgroundLayer_;exporting_=true;run(tr("Exporting image layer…"),[this,path,pixels,wcs,layer](std::stop_token stop){QTemporaryDir temp(QFileInfo(path).absolutePath()+"/.astro-layer-XXXXXX");if(!temp.isValid())throw std::runtime_error("Cannot open export destination.");recording::AstroImage a;a.width=pixels->width;a.height=pixels->height;a.channels=3;a.bits=32;a.samples=pixels->rgb;a.wcs=wcs;a.imageType=layer==1?"STARS":"PSF RESIDUAL";const auto file=temp.filePath("layer.fits");check(stop);recording::writeAstroFits(file.toStdString(),a);atomicCopy(file,path,stop);post([this]{status_=tr("Image layer exported as Float32 FITS.");emit changed();});});
}
QUrl DeepSkyController::exportUrl() const {
    if(frames_.isEmpty())return {};
    const QFileInfo source(projectUrl_.isLocalFile()?projectUrl_.toLocalFile():frames_.front().toMap()["path"].toString());
    return QUrl::fromLocalFile(source.dir().filePath(source.completeBaseName()+"-stacked.tif"));
}
void DeepSkyController::exportImage(const QUrl& url,bool linear) {
    if(busy_||!linear_||!url.isLocalFile())return;
    const auto path=url.toLocalFile();
    if(projectUrl_.isLocalFile()&&sameFile(projectUrl_.toLocalFile(),path)){status_=tr("A project file cannot be overwritten by an image.");emit changed();return;}
    for(const auto& value:frames_)if(sameFile(value.toMap()["path"].toString(),path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    const auto masks=referencedMasks(finishSettings_,projectOptions_);for(const auto& mask:masks)if(sameFile(mask,path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    auto exportWcs=readWcs(astrometry_.value("wcs"));if(exportWcs&&!linear)exportWcs=finishWcs(*exportWcs,linear_->width,linear_->height,finishSettings_);
    const auto image=linear?linear_:finished_;const auto exposure=linear && hdrReport_.value("imageKey").toString()==QString::fromLatin1(linearImageKey(*linear_))?hdrReport_.value("referenceExposure").toDouble():totalExposure_;
    exporting_=true;
    run(tr("Exporting image…"),[this,image,path,linear,exposure,exportWcs](std::stop_token stop){
        QTemporaryDir directory(QFileInfo(path).dir().filePath(".astrocapture-XXXXXX"));if(!directory.isValid())throw std::runtime_error("Cannot create temporary export files.");
        const auto temp=directory.filePath("image");
        const auto suffix=QFileInfo(path).suffix().toLower();
        if(suffix=="fits"||suffix=="fit"||suffix=="fts") {
            recording::AstroImage output;output.width=image->width;output.height=image->height;output.channels=3;output.bits=32;output.exposure=exposure;output.samples=image->rgb;output.wcs=exportWcs;
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
    for(const auto& value:frames_)if(sameFile(url.toLocalFile(),value.toMap()["path"].toString())){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    for(const auto& path:referencedMasks(finishSettings_,projectOptions_))if(sameFile(url.toLocalFile(),path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    auto sources=sourceFingerprints_;for(const auto& value:frames_){const auto row=value.toMap();if(row.contains("sourceFingerprint"))sources[row["path"].toString()]=row["sourceFingerprint"];}
    auto savedOptions=projectOptions_;savedOptions["astrometry"]=astrometry_;
    const QJsonObject project{{"sources",QJsonObject::fromVariantMap(sources)},{"version",1},{"frames",QJsonArray::fromVariantList(frames_)},{"options",QJsonObject::fromVariantMap(savedOptions)}};
    QSaveFile file(url.toLocalFile());const auto bytes=QJsonDocument(project).toJson();
    if(bytes.size()>4*1024*1024){status_=tr("Project exceeds 4 MiB. Reduce saved sample or reference lists before saving.");emit changed();return;}
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())status_=tr("Cannot save project.");else {status_=tr("Project saved. Original files are referenced, not copied.");projectUrl_=url;emit projectSaved(url);}emit changed();
}
void DeepSkyController::loadProject(const QUrl& url) {
    if(busy_||!url.isLocalFile())return;
    QFile file(url.toLocalFile());
    if(!file.open(QIODevice::ReadOnly)){status_=tr("Cannot read project.");emit changed();return;}
    if(file.peek(8)==ProjectStorage::magic()) {
        const auto cache=projectCacheDirectory();file.close();
        run(tr("Opening portable project…"),[this,url,cache](std::stop_token stop){
            const auto snapshot=ProjectStorage::load(url.toLocalFile(),cache,[stop]{check(stop);},[this](int p){post([this,p]{progress_=p;emit changed();});},&DeepSkyController::validSession);
            check(stop);post([this,url,snapshot,stop]{if(stop.stop_requested())return;restoreProject(snapshot,url,tr("Portable project opened. Sources, masks and results are ready."));});
        });return;
    }
    if(file.size()>4*1024*1024){status_=tr("Cannot read project.");emit changed();return;}
    QJsonParseError error;const auto object=QJsonDocument::fromJson(file.readAll(),&error).object();
    if(error.error!=QJsonParseError::NoError||object["version"].toInt()!=1||!object["frames"].isArray()||object["frames"].toArray().size()>2000){status_=tr("Invalid AstroCapture project.");emit changed();return;}
    auto frames=object["frames"].toArray().toVariantList();
    for(const auto& value:frames){const auto row=value.toMap();if(row["path"].toString().isEmpty()||!QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(row["role"].toString())){status_=tr("Project contains missing files or invalid roles.");emit changed();return;}}
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
    clear();projectUrl_=url;sourceFingerprints_=object["sources"].toObject().toVariantMap();projectOptions_=std::move(options);emit projectOptionsChanged();frames_=std::move(frames);for(const auto& value:frames_){const auto row=value.toMap();if(row["reference"].toBool())references_[row["group"].toString()]=row["path"].toString();}status_=missingSources().isEmpty()?tr("Project loaded. Integrate to recreate the result."):tr("Project loaded. Find the moved sources before integrating.");emit projectOpened(url);emit changed();
}
QVariant DeepSkyController::processingDocument() const {
    if(!finished_)return {};
    QStringList protectedPaths;for(const auto& value:frames_)protectedPaths<<value.toMap()["path"].toString();protectedPaths<<referencedMasks(finishSettings_,projectOptions_);if(projectUrl_.isLocalFile())protectedPaths<<projectUrl_.toLocalFile();
    ImageDocument document{finished_,resultPreview_,{},tr("Deep-sky result"),exportUrl(),protectedPaths};
    document.exposureCount=exposureCount_;document.totalExposure=totalExposure_;document.filters=resultFilters_;
    document.processingSummary=tr("Deep sky · %1 exposures · %2 s total · filters: %3").arg(exposureCount_).arg(totalExposure_,0,'f',1).arg(resultFilters_.join(", "));
    if(linear_&&hdrReport_.value("imageKey").toString()==QString::fromLatin1(linearImageKey(*linear_)))document.processingSummary=tr("HDR · %1 input images · total integration time unknown").arg(hdrReport_.value("sources").toList().size());
    return QVariant::fromValue(document);
}
}

namespace astro::app {
void DeepSkyController::loadMaskFile(const QUrl& url) {
    if(busy_||!linear_||!url.isLocalFile())return;
    const auto source=pipelineCache_?pipelineCache_:linear_;const auto path=url.toLocalFile();const auto originalName=sourceFingerprints_[path].toMap()["name"].toString();
    run(tr("Reading mask image…"),[this,source,path,originalName](std::stop_token stop){
        const auto image=readMaskFrame(path);auto fingerprint=ProjectStorage::fingerprint(path,[stop]{check(stop);});if(!originalName.isEmpty())fingerprint["name"]=originalName;check(stop);
        if(image.width!=source->width||image.height!=source->height)throw std::runtime_error("The imported mask must match the image dimensions.");
        for(float v:image.samples){if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Mask values must be between zero and one.");}
        const int w=std::min(1024,image.width),h=std::max(1,int(std::lround(double(image.height)*w/image.width)));
        QImage preview(w,h,QImage::Format_Grayscale8);if(preview.isNull())throw std::runtime_error("Cannot allocate mask preview.");
        for(int y=0;y<h;++y){check(stop);auto* row=preview.scanLine(y);for(int x=0;x<w;++x){const auto p=std::size_t(y*image.height/h)*image.width+x*image.width/w;const double value=image.channels==1?image.samples[p]:.2126*image.samples[p*image.channels]+.7152*image.samples[p*image.channels+1]+.0722*image.samples[p*image.channels+2];row[x]=uchar(std::lround(value*255));}}
        post([this,path,preview,fingerprint,stop]{if(stop.stop_requested())return;sourceFingerprints_[path]=fingerprint;maskImportPath_=path;maskImportPreview_=preview;pipelineCache_.reset();pipelineSource_.reset();status_=tr("Mask image loaded. White applies; black protects.");emit maskFileChanged();emit maskFileReady(path);});
    });
}
}

namespace astro::app {
QString DeepSkyController::projectCacheDirectory() const { return projectCacheDirectory_.isEmpty()?ProjectStorage::defaultCacheDirectory():projectCacheDirectory_; }
double DeepSkyController::portableProjectBytes() const { return ProjectStorage::estimatedBytes(sessionSnapshot()); }
QVariantList DeepSkyController::missingSources() const {
    QVariantList missing;
    const auto snapshot=sessionSnapshot();
    for(const auto& path:ProjectStorage::sourcePaths(snapshot))if(!QFileInfo(path).isFile()) {
        auto fingerprint=sourceFingerprints_[path].toMap();QString name=fingerprint["name"].toString();
        for(const auto& value:frames_) {const auto row=value.toMap();if(row["path"].toString()==path){name=row["name"].toString();fingerprint=row.value("sourceFingerprint",fingerprint).toMap();break;}}
        missing<<QVariantMap{{"path",path},{"name",name.isEmpty()?QFileInfo(path).fileName():name},{"verified",!fingerprint["sha256"].toString().isEmpty()}};
    }
    return missing;
}
void DeepSkyController::savePortableProject(const QUrl& url) {
    if(busy_||!url.isLocalFile()||frames_.isEmpty())return;
    const auto snapshot=sessionSnapshot();
    for(const auto& path:ProjectStorage::sourcePaths(snapshot))if(sameFile(url.toLocalFile(),path)){status_=tr("An original image cannot be overwritten.");emit changed();return;}
    if(!validSession(snapshot)){status_=tr("Invalid portable project settings or results.");emit changed();return;}
    run(tr("Saving portable project…"),[this,url,snapshot](std::stop_token stop){
        ProjectStorage::save(url.toLocalFile(),snapshot,[stop]{check(stop);},[this](int p){post([this,p]{progress_=p;emit changed();});});
        post([this,url]{projectUrl_=url;status_=tr("Portable project saved with sources, masks and floating-point results.");progress_=100;emit projectSaved(url);emit changed();});
    });
}
void DeepSkyController::restoreProject(const WorkspaceSnapshot& snapshot,const QUrl& url,const QString& message) {
    // A completed worker publishes one validated snapshot. QML must restore A/B
    // and view settings rather than treating this as a newly computed integration.
    busy_=false;restoringProject_=true;restoreSession(snapshot);projectUrl_=url;
    status_=message;emit projectRestored(snapshot.state["options"].toMap()["view"].toMap());
    restoringProject_=false;busy_=true;if(url.isLocalFile())emit projectOpened(url);
}
void DeepSkyController::relinkSources(const QUrl& folder,bool allowMetadataMatch) {
    if(busy_||!folder.isLocalFile()||!QFileInfo(folder.toLocalFile()).isDir())return;
    const auto snapshot=sessionSnapshot();const auto missing=missingSources();const auto url=projectUrl_;
    if(missing.isEmpty())return;
    run(tr("Finding moved sources…"),[this,folder,snapshot,missing,url,allowMetadataMatch](std::stop_token stop){
        QMap<QString,QStringList> candidates;QDirIterator it(folder.toLocalFile(),QDir::Files|QDir::NoSymLinks,QDirIterator::Subdirectories);int count=0;
        while(it.hasNext()){check(stop);const auto path=it.next();if(++count>50000)throw std::runtime_error("Search is limited to 50000 files. Choose a more specific source folder.");candidates[QFileInfo(path).fileName()]<<path;}
        QMap<QString,QString> paths;QVariantMap fingerprints=snapshot.state["sourceFingerprints"].toMap();QMap<QString,QVariantMap> newlyVerified;
        int unverified=0,ambiguous=0;
        for(int i=0;i<missing.size();++i){
            check(stop);const auto item=missing[i].toMap();const auto original=item["path"].toString();auto fingerprint=fingerprints[original].toMap();QVariantMap sourceRow;
            for(const auto& value:snapshot.state["frames"].toList()){const auto row=value.toMap();if(row["path"].toString()==original){sourceRow=row;fingerprint=row.value("sourceFingerprint",fingerprint).toMap();break;}}
            auto list=candidates[item["name"].toString()];std::sort(list.begin(),list.end());QStringList matches;QVariantMap matchedFingerprint;
            const auto expected=fingerprint["sha256"].toString();
            if(expected.isEmpty()&&(!allowMetadataMatch||sourceRow.isEmpty())){++unverified;continue;}
            for(const auto& candidate:list){
                check(stop);
                if(!expected.isEmpty()&&QString::number(QFileInfo(candidate).size())!=fingerprint["bytes"].toString())continue;
                if(expected.isEmpty()){
                    // Old projects have no content fingerprint. Only a unique file
                    // with all recorded sensor metadata may be accepted, explicitly.
                    if(sourceRow["width"].toInt()<=0||sourceRow["height"].toInt()<=0)continue;
                    try{auto row=sourceRow;row["path"]=candidate;const auto data=readFrame(row);
                        if(data.width!=sourceRow["width"].toInt()||data.height!=sourceRow["height"].toInt()||data.channels!=sourceRow["channels"].toInt()||data.bits!=sourceRow["bits"].toInt()||std::abs(data.exposure-sourceRow["exposure"].toDouble())>1e-6||QString::fromStdString(data.filter)!=sourceRow["filter"].toString()||QString::fromUtf8(domain::to_string(data.bayer).data())!=sourceRow["bayer"].toString()||(sourceRow.contains("gain")&&(!data.hasGain||std::abs(data.gain-sourceRow["gain"].toDouble())>1e-6))||(sourceRow.contains("temperature")&&(!data.hasTemperature||std::abs(data.temperature-sourceRow["temperature"].toDouble())>1e-6)))continue;
                    }catch(const std::exception&){continue;}
                }
                const auto actual=ProjectStorage::fingerprint(candidate,[stop]{check(stop);});
                if(!expected.isEmpty()&&actual["sha256"].toString()!=expected)continue;
                matches<<candidate;matchedFingerprint=actual;if(!expected.isEmpty())break;
            }
            if(matches.size()==1){paths[original]=QFileInfo(matches.front()).canonicalFilePath();newlyVerified[original]=matchedFingerprint;}
            else if(matches.size()>1)++ambiguous;
            post([this,i,total=missing.size()]{progress_=int((i+1)*100/total);emit changed();});
        }
        if(paths.isEmpty())throw std::runtime_error("No matching sources found. Choose their new folder; older projects require explicit metadata matching. Ambiguous files are never linked.");
        auto restored=snapshot;auto rows=restored.state["frames"].toList();for(auto& value:rows){auto row=value.toMap();const auto original=row["path"].toString();if(newlyVerified.contains(original))row["sourceFingerprint"]=newlyVerified[original];value=row;}restored.state["frames"]=rows;
        for(auto i=newlyVerified.cbegin();i!=newlyVerified.cend();++i)fingerprints[i.key()]=i.value();restored.state["sourceFingerprints"]=fingerprints;restored.state=ProjectStorage::remap(restored.state,paths).toMap();
        check(stop);const auto message=tr("%1 sources linked; %2 still missing. Cached results and adjustments are preserved.").arg(paths.size()).arg(missing.size()-paths.size());
        post([this,restored,url,message,stop]{if(stop.stop_requested())return;restoreProject(restored,url,message);});
    });
}
}
