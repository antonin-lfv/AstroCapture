#include "FileProtection.hpp"
#include "MosaicController.hpp"
#include "ImagePreview.hpp"
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QTemporaryFile>
#include <astro/recording/TiffWriter.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::app {
namespace {
QString localizedError(QString error) {
    static const char* messages[] = {
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "No pair of images has a reliable overlap. No mosaic was created; original files are unchanged."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Not enough surface detail in: "),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "No reliable overlap for: "),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", ". Add an intermediate view or remove this image."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Overlaps disagree. Remove a changed, blurred or mismatched view and try again."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Inconsistent image scales. Use images from the same setup."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "The mosaic exceeds the 128-megapixel limit. Use smaller images or fewer tiles."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Each image must be at least 64 pixels per side and no larger than 24 megapixels: "),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "An input image changed or could not be read: "),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "The image positions could not be resolved."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Choose between 2 and 100 overlapping images."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Image dimensions changed or exceed the 24-megapixel input limit."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Cannot read image: "),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Not enough memory for the mosaic preview."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Cannot create an export in this folder."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Cannot open export destination."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Writing the image failed. Check available disk space."),
        QT_TRANSLATE_NOOP("astro::app::MosaicController", "Could not finish saving the image."),
    };
    for (const auto* message : messages) {
        const QString source=QString::fromUtf8(message);
        if(error.startsWith(source)) error=QCoreApplication::translate("astro::app::MosaicController",message) + error.mid(source.size());
        if(error.endsWith(source)) { error.chop(source.size()); error += QCoreApplication::translate("astro::app::MosaicController",message); }
    }
    return error;
}
struct Cancelled {};
void check(std::stop_token stop) { if(stop.stop_requested()) throw Cancelled{}; }
processing::MosaicTile loadTile(const QString& path) {
    QImageReader reader(path); reader.setAutoTransform(true);
    const auto size=reader.size();
    if(size.width()<64 || size.height()<64 || std::uint64_t(size.width())*size.height()>24000000)
        throw std::runtime_error("Image dimensions changed or exceed the 24-megapixel input limit.");
    QImage image=reader.read().convertToFormat(QImage::Format_RGBA64);
    if(image.isNull()) throw std::runtime_error((QObject::tr("Cannot read image: ")+QFileInfo(path).fileName()+" · "+reader.errorString()).toStdString());
    processing::MosaicTile tile;
    tile.image={image.width(),image.height(),std::vector<float>(std::size_t(image.width())*image.height()*3)};
    if(image.hasAlphaChannel()) tile.mask.resize(std::size_t(image.width())*image.height());
    for(int y=0;y<image.height();++y) {
        const auto* row=reinterpret_cast<const QRgba64*>(image.constScanLine(y));
        for(int x=0;x<image.width();++x) {
            const std::size_t p=std::size_t(y)*image.width()+x;
            tile.image.rgb[p*3]=row[x].red()/65535.f;
            tile.image.rgb[p*3+1]=row[x].green()/65535.f;
            tile.image.rgb[p*3+2]=row[x].blue()/65535.f;
            if(!tile.mask.empty()) tile.mask[p]=std::uint8_t(row[x].alpha()/257);
        }
    }
    return tile;
}
QImage preview(const processing::FloatImage& image) {return makeImagePreview(image);}

}
MosaicController::~MosaicController() { worker_.request_stop(); if(worker_.joinable()) worker_.join(); }
void MosaicController::post(std::function<void()> action) { QMetaObject::invokeMethod(this,std::move(action),Qt::QueuedConnection); }
void MosaicController::run(std::function<void(std::stop_token)> task) {
    if(busy_) return;
    busy_=true; progress_=0; emit changed();
    worker_=std::jthread([this,task=std::move(task)](std::stop_token stop) {
        try { task(stop); }
        catch(const Cancelled&) { post([this]{status_=tr("Cancelled. Previous results are preserved.");}); }
        catch(const std::exception& e) {
            const auto error=localizedError(QString::fromUtf8(e.what()));
            post([this,error]{status_=tr("Mosaic could not be completed: %1").arg(error);});
        }
        post([this]{busy_=false;exporting_=false;emit changed();});
    });
}
QVariantList MosaicController::files() const {
    QVariantList out;
    for(const auto& input:inputs_) out.append(QVariantMap{{"name",QFileInfo(input.path).fileName()},{"path",input.path},{"url",QUrl::fromLocalFile(input.path)},{"width",input.width},{"height",input.height},{"selected",input.selected}});
    return out;
}
QVariantList MosaicController::selection() const {QVariantList values;for(const auto& i:inputs_)values.append(i.selected);return values;}
int MosaicController::selectedCount() const {return int(std::count_if(inputs_.begin(),inputs_.end(),[](const auto& i){return i.selected;}));}
QStringList MosaicController::excludedFiles() const {QStringList names;for(const auto& i:excluded_)names.append(QFileInfo(i.path).fileName());return names;}
QUrl MosaicController::exportUrl() const { return inputs_.empty()?QUrl{}:QUrl::fromLocalFile(QFileInfo(inputs_.front().path).dir().filePath("mosaic.tif")); }
void MosaicController::invalidate() { result_.reset(); resultCount_=0; edgeMask_.reset(); preview_={}; progress_=0; annotation_=cleanAnnotation({}); updateAnnotation(); emit imageChanged(); }
void MosaicController::setExportAnnotation(const QVariantMap& value) {
    auto normalized=cleanAnnotation(value); if(normalized==annotation_)return;
    annotation_=std::move(normalized); updateAnnotation();
}
QString MosaicController::captionTechnicalText() const {
    const auto suggestion = result_ ? tr("Mosaic · %1 images · %2 × %3 px").arg(resultCount_).arg(result_->width).arg(result_->height) : QString{};
    return annotationTechnicalText(annotation_, suggestion);
}
void MosaicController::updateAnnotation() {
    try { annotationImage_=result_ ? makeAnnotation({result_->width,result_->height},annotation_,captionTechnicalText()) : QImage{}; }
    catch(const std::exception& e) { annotationImage_={}; annotation_["enabled"]=false; status_=localizedError(QString::fromUtf8(e.what())); emit changed(); }
    emit annotationChanged();
}
QVariant MosaicController::processingDocument() const {
    if(busy_ || !result_) return {};
    const auto folder=QFileInfo(exportUrl().toLocalFile()).dir();
    QStringList originals; for(const auto& input:inputs_) originals.append(input.path);
    return QVariant::fromValue(ImageDocument{result_,preview_,annotation_,tr("Mosaic"),QUrl::fromLocalFile(folder.filePath("mosaic-processed.tif")),originals,edgeMask_,resultCount_});
}
void MosaicController::addImages(const QList<QUrl>& urls) {
    if(busy_) return;
    int added=0,rejected=0;
    for(const auto& url:urls) {
        if(!url.isLocalFile()) {++rejected;continue;}
        const QFileInfo info(url.toLocalFile()); const auto path=info.canonicalFilePath();
        if(std::any_of(inputs_.begin(),inputs_.end(),[&](const auto& i){return i.path==path;})) continue;
        const auto suffix=info.suffix().toLower();
        if(path.isEmpty() || !QStringList{"tif","tiff","png","jpg","jpeg"}.contains(suffix) || inputs_.size()>=100) {++rejected;continue;}
        QImageReader reader(path); reader.setAutoTransform(true); auto size=reader.size();
        if(int(reader.transformation()) & int(QImageIOHandler::TransformationRotate90)) size.transpose();
        if(!reader.canRead() || size.width()<64 || size.height()<64 || std::uint64_t(size.width())*size.height()>24000000) {++rejected;continue;}
        inputs_.push_back({path,size.width(),size.height()}); ++added;
    }
    if(added) { invalidate(); emit filesChanged(); }
    status_=tr("%1 images ready. Add overlapping views, then assemble.").arg(inputs_.size());
    if(rejected) status_+=tr(" · %1 skipped (unsupported, too large or over the 100-image limit)").arg(rejected);
    emit changed();
}
void MosaicController::removeImage(int index) {
    if(busy_ || index<0 || index>=int(inputs_.size())) return;
    const auto path=inputs_[index].path;std::erase_if(excluded_,[&](const auto& i){return i.path==path;});
    inputs_.erase(inputs_.begin()+index); invalidate(); emit filesChanged(); status_=tr("Image removed from this mosaic. The original file is unchanged."); emit changed();
}
void MosaicController::setImageSelected(int index,bool selected) {
    if(busy_ || index<0 || index>=int(inputs_.size()) || inputs_[index].selected==selected)return;
    inputs_[index].selected=selected;
    const auto path=inputs_[index].path;
    std::erase_if(excluded_,[&](const auto& i){return i.path==path;});
    invalidate();emit selectionChanged();status_=tr("%1 of %2 images selected.").arg(selectedCount()).arg(inputs_.size());emit changed();
}
void MosaicController::restoreExcluded() {
    if(busy_ || excluded_.empty())return;
    for(auto& i:inputs_) if(std::any_of(excluded_.begin(),excluded_.end(),[&](const auto& current){return current.path==i.path;})) i.selected=true;
    excluded_.clear();invalidate();emit selectionChanged();status_=tr("Excluded images restored. Add an intermediate view or adjust the selection.");emit changed();
}
void MosaicController::clear() { if(busy_)return; inputs_.clear();excluded_.clear();invalidate();emit filesChanged();status_.clear();emit changed(); }
void MosaicController::cancel() { if(busy_) {worker_.request_stop();status_=tr("Cancelling…");emit changed();} }
void MosaicController::assemble(bool balanceExposure,bool skipUnmatched,bool softenEdges) {
    if(busy_ || selectedCount()<2) return;
    std::vector<Input> inputs;for(const auto& i:inputs_)if(i.selected)inputs.push_back(i); status_=tr("Finding shared surface details…");
    run([this,inputs,balanceExposure,skipUnmatched,softenEdges](std::stop_token stop) {
        std::vector<processing::MosaicSource> sources;
        for(const auto& i:inputs) sources.push_back({QFileInfo(i.path).fileName().toStdString(),i.width,i.height});
        auto output=processing::buildMosaic(sources,[&](int i){check(stop);return loadTile(inputs[i].path);},balanceExposure,[&]{check(stop);},[&](int progress,const std::string& stage){
            const QString label=stage=="features"?tr("Finding surface details…"):stage=="matching"?tr("Matching overlapping views…"):stage=="alignment"?tr("Aligning the mosaic…"):stage=="blending"?tr("Blending overlaps…"):tr("Preparing the result…");
            post([this,progress,label]{progress_=progress;status_=label;emit changed();});
        },skipUnmatched,softenEdges);
        auto result=std::make_shared<processing::FloatImage>(std::move(output.image)); auto image=preview(*result); check(stop);
        QString status=tr("%1 images assembled · %2 × %3 px").arg(output.usedIndices.size()).arg(result->width).arg(result->height);
        if(!output.excludedIndices.empty()) status+=tr(" · %1 excluded: no reliable connection to the main group").arg(output.excludedIndices.size());
        std::vector<Input> kept,excluded;
        for(int i:output.usedIndices)kept.push_back(inputs[i]);
        for(int i:output.excludedIndices)excluded.push_back(inputs[i]);
        auto edgeMask=output.edgeMask.empty()?std::shared_ptr<const std::vector<float>>{}:std::make_shared<const std::vector<float>>(std::move(output.edgeMask));
        post([this,result,edgeMask,image,status,kept,excluded,stop]{
            if(stop.stop_requested())return;
            if(!excluded.empty()) {
                for(auto& i:inputs_) if(std::any_of(excluded.begin(),excluded.end(),[&](const auto& e){return i.path==e.path;})) i.selected=false;
                for(const auto& i:excluded) if(std::none_of(excluded_.begin(),excluded_.end(),[&](const auto& current){return current.path==i.path;}))excluded_.push_back(i);
                emit selectionChanged();
            }
            resultCount_=int(kept.size());result_=result;edgeMask_=edgeMask;preview_=image;status_=status;progress_=100;updateAnnotation();emit imageChanged();
        });
    });
}
void MosaicController::exportImage(const QUrl& url) {
    if(busy_ || !result_ || !url.isLocalFile()) return;
    QString path=url.toLocalFile(); if(!path.endsWith(".tif",Qt::CaseInsensitive)&&!path.endsWith(".tiff",Qt::CaseInsensitive)) path+=".tif";
    for(const auto& input:inputs_) if(sameFile(path,input.path)) {status_=tr("An original image cannot be overwritten.");emit changed();return;}
    for(const auto& input:excluded_) if(sameFile(path,input.path)) {status_=tr("An original image cannot be overwritten.");emit changed();return;}
    const auto result=result_; const auto overlay=annotationImage_; status_=tr("Exporting 16-bit TIFF…");
    exporting_=true;
    run([this,result,path,overlay](std::stop_token stop){
        std::vector<std::uint16_t> samples(result->rgb.size());
        for(int y=0;y<result->height;++y) { check(stop); for(int x=0;x<result->width*3;++x) {
            const auto i=std::size_t(y)*result->width*3+x; const QRgb ink=overlay.isNull()?0:reinterpret_cast<const QRgb*>(overlay.constScanLine(y))[x/3];
            samples[i]=annotatedSample(result->rgb[i],ink,x%3);
        }}
        QTemporaryFile temporary(QFileInfo(path).dir().filePath(".astrocapture-mosaic-XXXXXX.tif"));
        if(!temporary.open()) throw std::runtime_error("Cannot create an export in this folder.");
        const auto tempPath=temporary.fileName();temporary.close();
        const auto written=recording::writeTiff16Samples(tempPath.toStdString(),result->width,result->height,samples.data());
        if(!written) throw std::runtime_error(written.error().message);
        QFile input(tempPath);QSaveFile output(path);
        if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot open export destination.");
        while(!input.atEnd()) {check(stop);const auto chunk=input.read(1024*1024);if(chunk.isEmpty()||output.write(chunk)!=chunk.size()) throw std::runtime_error("Writing the image failed. Check available disk space.");}
        check(stop);if(!output.commit())throw std::runtime_error("Could not finish saving the image.");
        post([this,path]{progress_=100;status_=tr("Saved 16-bit TIFF: %1").arg(path);emit imageExported(QUrl::fromLocalFile(path));});
    });
}
}
