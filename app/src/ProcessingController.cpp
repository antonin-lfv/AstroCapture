#include "ProcessingController.hpp"
#include "ImagePreview.hpp"

#include <QDir>
#include "SequenceReader.hpp"
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QColorSpace>
#include <QSaveFile>
#include <QTemporaryFile>
#include <algorithm>
#include <astro/recording/SerReader.hpp>
#include <astro/processing/Surface.hpp>
#include <astro/recording/TiffWriter.hpp>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace astro::app {
namespace {
using namespace processing;
struct Cancelled {};
void check(std::stop_token stop) {
  if (stop.stop_requested())
    throw Cancelled{};
}
std::filesystem::path nativePath(const QString &path) {
#ifdef _WIN32
  return std::filesystem::path(path.toStdWString());
#else
  return std::filesystem::path(path.toStdString());
#endif
}
FloatImage readImage(SequenceReader& reader,int index) {return reader.image(index);}
QImage preview(const FloatImage &image,const processing::CheckCancel& check={}) {return makeImagePreview(image,check);}
} // namespace
ProcessingController::ProcessingController(QObject *parent) : QObject(parent) {
  connect(this,&ProcessingController::imageChanged,this,&ProcessingController::exportSizeInputChanged);
  connect(this,&ProcessingController::annotationChanged,this,&ProcessingController::exportSizeInputChanged);
}
ProcessingController::~ProcessingController() {
  worker_.request_stop();
  if (worker_.joinable())
    worker_.join();
}
int ProcessingController::frameCount() const {
  return source_ ? source_->count : 0;
}
QString ProcessingController::sourceName() const {
  if(stillImage()) return tr("%1 · %2 × %3 px").arg(document_.name).arg(stack_->width).arg(stack_->height);
  return source_ ? tr("%1 · %2 × %3 · %4-bit · %5 frames")
                       .arg(QFileInfo(source_->path).fileName())
                       .arg(source_->width)
                       .arg(source_->height)
                       .arg(source_->bits)
                       .arg(source_->count)
                 : QString();
}
QUrl ProcessingController::exportUrl() const {
  if(stillImage()) return document_.exportSuggestion;
  if (!source_)
    return {};
  const QFileInfo file(source_->path);
  return QUrl::fromLocalFile(
      file.dir().filePath(file.completeBaseName() + "-processed.tif"));
}
void ProcessingController::setExportAnnotation(const QVariantMap &value) {
  const auto normalized = cleanAnnotation(value);
  if (normalized == annotation_) return;
  annotation_ = normalized;
  updateAnnotation();
}
QString ProcessingController::captionTechnicalText() const {
  const QString suggestion = result_ ? tr("%1 × %2 px").arg(result_->width).arg(result_->height)
                                      + "\n" + stackSummary_ : QString{};
  return annotationTechnicalText(annotation_, suggestion);
}
void ProcessingController::updateAnnotation() {
  try {
    annotationImage_ = result_ ? makeAnnotation({result_->width, result_->height}, annotation_, captionTechnicalText()) : QImage{};
  } catch (const std::exception &error) {
    annotationImage_ = {};
    annotation_["enabled"] = false;
    status_ = QString::fromUtf8(error.what());
    emit changed();
    emit failed();
  }
  emit annotationChanged();
}
void ProcessingController::post(std::function<void()> action) {
  QMetaObject::invokeMethod(this, std::move(action), Qt::QueuedConnection);
}
void ProcessingController::display(QImage image, QString label, bool result) {
  showingResult_ = result;
  preview_ = std::move(image);
  imageLabel_ = std::move(label);
  emit imageChanged();
}
void ProcessingController::run(QString status,
                               std::function<void(std::stop_token)> task,
                               Work work) {
  if (busy_)
    return;
  work_ = work;
  busy_ = true;
  // Frame navigation is background preview work, not a new processing job.
  // Preserve the last useful status instead of flashing it on every frame.
  if (work != Work::Seek) {
    progress_ = 0;
    status_ = std::move(status);
  }
  emit changed();
  worker_ = std::jthread([this, task = std::move(task)](std::stop_token stop) {
    try {
      task(stop);
    } catch (const Cancelled &) {
      post([this] {
        if (!pendingAdjustments_)
          status_ = tr("Cancelled. Previous results are preserved.");
      });
    } catch (const std::exception &e) {
      const QString error = tr(e.what());
      post([this, error] {
        status_ = error;
        emit failed();
      });
    }
    post([this] {
      busy_ = false;
      // One waiting request at most: dragging never creates a backlog.
      if (auto settings = std::exchange(pendingAdjustments_, std::nullopt)) {
        startAdjustments(*settings);
        return;
      }
      if (auto index = std::exchange(pendingSeek_, std::nullopt)) {
        seek(*index);
        return;
      }
      emit changed();
    });
  });
}
void ProcessingController::report(int completed, int total,
                                  const QString &stage) {
  if (completed != total && completed % std::max(1, total / 100) != 0)
    return;
  post([this, completed, total, stage] {
    progress_ = completed * 100 / total;
    status_ = tr("%1 · %2 / %3").arg(stage).arg(completed).arg(total);
    emit changed();
  });
}
void ProcessingController::cancel() {
  pendingSeek_.reset();
  pendingAdjustments_.reset();
  ++adjustmentRevision_;
  if (busy_) {
    worker_.request_stop();
    status_ = tr("Cancelling…");
    emit changed();
  }
}
void ProcessingController::open(const QUrl &url) {
  if (busy_)
    return;
  if (!url.isLocalFile()) {
    status_ = tr("Choose a local SER, ACRAW, PNG or TIFF file.");
    emit changed();
    return;
  }
  const QString path = url.toLocalFile();
  const auto suffix=QFileInfo(path).suffix().toLower();
  if(QStringList{"fit","fits","fts","fz"}.contains(suffix)) {
    status_=tr("Open FITS images in the Deep sky tab.");
    emit changed();
    emit failed();
    return;
  }
  if(QStringList{"png","tif","tiff"}.contains(suffix)) {
    run(tr("Opening image…"), [this,path](std::stop_token stop) {
      QImageReader reader(path); reader.setAutoTransform(true);
      const auto size=reader.size();
      if(size.width()<=0 || size.height()<=0)
        throw std::runtime_error("Cannot read this image. Choose a valid PNG or TIFF file.");
      if(std::uint64_t(size.width())*size.height()>32000000)
        throw std::runtime_error("Images are limited to 32 megapixels.");
      if(reader.imageCount()>1)
        throw std::runtime_error("Choose a single-image PNG or TIFF, not a multi-page file.");
      const auto image=reader.read().convertToFormat(QImage::Format_RGBA64);
      if(image.isNull())
        throw std::runtime_error("Cannot read this image. Choose a valid PNG or TIFF file.");
      if(std::uint64_t(image.width())*image.height()>32000000)
        throw std::runtime_error("Images are limited to 32 megapixels.");
      auto pixels=std::make_shared<FloatImage>(FloatImage{image.width(),image.height(),std::vector<float>(std::size_t(image.width())*image.height()*3)});
      for(int y=0;y<image.height();++y) {
        check(stop);
        const auto* row=reinterpret_cast<const QRgba64*>(image.constScanLine(y));
        for(int x=0;x<image.width();++x) {
          const auto at=(std::size_t(y)*image.width()+x)*3;
          const float alpha=row[x].alpha()/65535.f;
          pixels->rgb[at]=row[x].red()/65535.f*alpha;
          pixels->rgb[at+1]=row[x].green()/65535.f*alpha;
          pixels->rgb[at+2]=row[x].blue()/65535.f*alpha;
        }
      }
      const QFileInfo file(path);
      ImageDocument document{pixels,preview(*pixels),{},file.fileName(),
        QUrl::fromLocalFile(file.dir().filePath(file.completeBaseName()+"-processed.tif")),{file.canonicalFilePath()}};
      check(stop);
      post([this,document=std::move(document),stop]() mutable {
        if(!stop.stop_requested()) acceptDocument(std::move(document));
      });
    });
    return;
  }
  if(suffix!="ser" && suffix!="acraw") {
    status_=tr("Choose a local SER, ACRAW, PNG or TIFF file."); emit changed(); return;
  }
  run(tr("Opening video…"), [this, path](std::stop_token stop) {
    SequenceReader reader(nativePath(path));
    auto pixels = std::make_shared<FloatImage>(readImage(reader, 0));
    auto image = preview(*pixels);
    auto source = std::make_shared<Source>(
        Source{path, int(reader.width), int(reader.height), int(reader.count),
               int(reader.bits)});
    check(stop);
    post([this, source, image, pixels] {
      source_ = source;
      frameCache_ = pixels;
      appliedSettings_ = defaultProcessingSettings();
      document_ = {};
      ranks_.clear();
      qualities_.clear();
      stack_.reset();
      result_.reset();
      stackSummary_.clear();
      // Captions describe an observation. Never carry one to a different SER.
      annotation_ = cleanAnnotation({});
      updateAnnotation();
      beforePreview_ = {};
      afterPreview_ = {};
      index_ = 0;
      progress_ = 100;
      status_ = tr("Video ready. Analyze to rank the sharpest frames.");
      display(image, tr("Source · frame 1"));
      emit changed(); // Update QML frame-count bounds before selecting the
                      // default subset.
      emit sourceOpened();
    });
  });
}
bool ProcessingController::openDocument(const QVariant& value) {
  if(busy_ || !value.canConvert<ImageDocument>()) return false;
  auto document=value.value<ImageDocument>();
  if(!document.pixels || document.pixels->width<=0 || document.pixels->height<=0 ||
     std::uint64_t(document.pixels->width)*document.pixels->height>32000000 ||
     document.pixels->rgb.size()!=std::size_t(document.pixels->width)*document.pixels->height*3 ||
     document.preview.size()!=QSize(document.pixels->width,document.pixels->height) ||
     (document.edgeMask && document.edgeMask->size()!=std::size_t(document.pixels->width)*document.pixels->height)) return false;
  acceptDocument(std::move(document));
  return true;
}
void ProcessingController::acceptDocument(ImageDocument document) {
  document_=std::move(document); source_.reset(); ranks_.clear(); qualities_.clear();
  pendingSeek_.reset(); pendingAdjustments_.reset(); ++adjustmentRevision_;
  stack_=document_.pixels; result_=stack_; beforePreview_=document_.preview; afterPreview_=beforePreview_;
  frameCache_.reset(); appliedSettings_=defaultProcessingSettings();
  stackSummary_=!document_.processingSummary.isEmpty()?document_.processingSummary:document_.mosaicImageCount>0
    ? tr("Mosaic · %1 images assembled").arg(document_.mosaicImageCount)
    : tr("Still image · no stacking needed");
  annotation_=cleanAnnotation(document_.annotation);
  updateAnnotation(); progress_=100;
  status_=!document_.processingSummary.isEmpty()?tr("Deep-sky stack ready for detail, color and caption adjustments."):document_.mosaicImageCount>0 ? tr("Mosaic ready for detail, color and caption adjustments.") : tr("Image ready. Adjust details, color and caption, then export.");
  display(beforePreview_,document_.mosaicImageCount>0 ? tr("Mosaic · before adjustments") : tr("Image · before adjustments"),true);
  emit changed(); emit stackReady();
}
void ProcessingController::seek(int index) {
  if (!source_ || index < 0 || index >= source_->count)
    return;
  if (busy_) {
    if (seeking())
      pendingSeek_ = index;
    return;
  }
  const auto source = source_;
  run(
      {},
      [this, source, index](std::stop_token stop) {
        SequenceReader reader(nativePath(source->path));
        auto pixels = std::make_shared<FloatImage>(readImage(reader, index));
        auto image = preview(*pixels);
        check(stop);
        post([this, image, pixels, index, stop] {
          if (stop.stop_requested())
            return;
          index_ = index;
          frameCache_ = pixels;
          display(image, tr("Source · frame %1").arg(index + 1));
        });
      },
      Work::Seek);
}
void ProcessingController::analyze() {
  if (busy_ || !source_)
    return;
  const auto source = source_;
  run(tr("Analyzing sharpness…"), [this, source](std::stop_token stop) {
    SequenceReader reader(nativePath(source->path));
    std::vector<RankedFrame> ranks;
    ranks.reserve(reader.count);
    for (int i = 0; i < int(reader.count); ++i) {
      check(stop);
      ranks.push_back({i, frameQuality(readImage(reader, i))});
      report(i + 1, reader.count, tr("Analyzing sharpness"));
    }
    std::stable_sort(
        ranks.begin(), ranks.end(),
        [](const auto &a, const auto &b) { return a.quality > b.quality; });
    if (ranks.front().quality <= 1e-12)
      throw std::runtime_error("No usable detail found. Check focus, exposure "
                               "and the target before stacking.");
    QVariantList curve;
    const int bins = std::min(240, int(ranks.size()));
    for (int i = 0; i < bins; ++i)
      curve.append(
          ranks[std::size_t(i) * (ranks.size() - 1) / std::max(1, bins - 1)]
              .quality /
          ranks.front().quality);
    const int best = ranks.front().index;
    auto pixels = std::make_shared<FloatImage>(readImage(reader, best));
    auto image = preview(*pixels);
    check(stop);
    post([this, ranks = std::move(ranks), curve, image, pixels, best]() mutable {
      ranks_ = std::move(ranks);
      qualities_ = curve;
      index_ = best;
      frameCache_ = pixels;
      progress_ = 100;
      status_ = tr("Ranked by sharpness. Choose how many frames to keep.");
      display(image, tr("Sharpest source frame · %1").arg(best + 1));
    });
  }, Work::Analyze);
}
void ProcessingController::stack(int keepCount, bool surface, int patchSize, bool localSelection) {
  if (busy_ || !source_ || ranks_.empty())
    return;
  const auto source = source_;
  auto ranks = ranks_;
  keepCount = std::clamp(keepCount, 1, int(ranks.size()));
  if(!surface || !localSelection) ranks.resize(keepCount);
  run(tr("Aligning and stacking…"),
      [this, source, surface, patchSize, localSelection, keepCount, ranks = std::move(ranks)](std::stop_token stop) {
        SequenceReader reader(nativePath(source->path));
        std::shared_ptr<FloatImage> output;
        int localFrames=0, rejected=0, usedFrames=0;
        if(surface && localSelection) {
          std::vector<int> indices; for(const auto& frame:ranks) indices.push_back(frame.index);
          auto selected=stackLocalSelection(indices,keepCount,[&](int index){check(stop);return readImage(reader,index);},patchSize,[&]{check(stop);},[&](int done,int total){report(done,total,tr("Selecting and stacking by area"));});
          output=std::make_shared<FloatImage>(std::move(selected.image));
          localFrames=selected.correctedFrames; rejected=selected.rejectedFrames; usedFrames=selected.usedFrames;
        } else {
        const auto ref = readImage(reader, ranks.front().index);
        const auto reference = makeAlignmentReference(ref);
        std::optional<SurfaceReference> local;
        if (surface) local.emplace(ref, patchSize);

        MeanStack accumulator(ref.width, ref.height);
        const auto checkpoint = [&] { check(stop); };
        accumulator.add(ref, 0, 0, checkpoint);

        for (std::size_t i = 1; i < ranks.size(); ++i) {
          check(stop);
          const auto frame = readImage(reader, ranks[i].index);
          const auto offset = alignToReference(reference, frame, checkpoint);
          if (offset.correlation < .5)
            ++rejected;
          else if (local) {
            auto warped = local->align(frame, offset, checkpoint);
            accumulator.restrictSupport(warped.left, warped.top, warped.right, warped.bottom);
            accumulator.add(warped.image, 0, 0, checkpoint);
            if (warped.tracked > 0) ++localFrames;
          } else
            accumulator.add(frame, offset.x, offset.y, checkpoint);
          report(int(i + 1), int(ranks.size()), tr("Aligning and stacking"));
        }
        if (ranks.size() > 1 && accumulator.count() < 2)
          throw std::runtime_error("No other selected frame could be aligned "
                                   "reliably. Try a shorter sequence with less "
                                   "motion.");
        output = std::make_shared<FloatImage>(accumulator.finish());
        usedFrames=accumulator.count();
        }
        auto image = preview(*output);
        check(stop);
        QString summary = tr("%1 frames stacked · %2 rejected · %3 × %4 (common area)").arg(usedFrames).arg(rejected).arg(output->width).arg(output->height);
        if (surface) summary += tr(" · local correction: %1 frames").arg(localFrames);
        const QString selection = surface && localSelection
          ? tr("Local selection: up to %1 frames/area (%2%) · %3 distinct frames used").arg(keepCount).arg(100.0*keepCount/source->count,0,'f',1).arg(usedFrames)
          : tr("Last stack: %1% selected · %2 stacked · %3 rejected").arg(100.0 * keepCount / source->count, 0, 'f', 1).arg(usedFrames).arg(rejected);
        post([this, output, image, summary, selection] {
          stack_ = output;
          result_ = output;
          appliedSettings_ = defaultProcessingSettings();
          beforePreview_ = image;
          afterPreview_ = image;
          progress_ = 100;
          status_ = summary;
          stackSummary_ = selection;
          updateAnnotation();
          display(image, tr("Stack · before wavelets"), true);
          emit stackReady();
        });
      }, Work::Stack);
}
void ProcessingController::apply(const QVariantList &values, double denoise,
                                 double red, double green, double blue,
                                 double gamma) {
  if ((busy_ && !adjusting()) || !stack_ || values.size() != 6)
    return;
  Adjustments settings{{},
                       float(denoise),
                       {float(red), float(green), float(blue)},
                       float(gamma)};
  for (int i = 0; i < 6; ++i)
    settings.gains[i] = values[i].toFloat();
  ++adjustmentRevision_;
  if (adjusting()) {
    pendingAdjustments_ = settings;
    worker_.request_stop();
    return;
  }
  startAdjustments(settings);
}
void ProcessingController::startAdjustments(const Adjustments &settings) {
  const auto original = stack_;
  const auto edgeMask = document_.edgeMask;
  const auto revision = adjustmentRevision_;
  run(
      tr("Applying wavelets…"),
      [this, original, edgeMask, settings, revision](std::stop_token stop) {
        auto image=allocateImagePreview(*original);const auto target=previewTarget(image);
        auto result = std::make_shared<FloatImage>(
            wavelets(*original, settings.gains, settings.denoise,
                     settings.color, settings.gamma, [&] { check(stop); },
                     processing::ComputePreference::Auto,nullptr,edgeMask?nullptr:&target));
        if(edgeMask) {
          // Do not sharpen the artificial coverage boundary of a mosaic.
          for(int y=0;y<result->height;++y) {check(stop);for(int x=0;x<result->width;++x) {
            const auto at=std::size_t(y)*result->width+x;const float w=(*edgeMask)[at];
            for(int c=0;c<3;++c) result->rgb[at*3+c]=original->rgb[at*3+c]+w*(result->rgb[at*3+c]-original->rgb[at*3+c]);
          }}
        }
        if(edgeMask)image=preview(*result,[&]{check(stop);});
        check(stop);
        post([this, result, image, revision, settings] {
          if (revision != adjustmentRevision_)
            return;
          result_ = result;
          for(int i=0;i<6;++i)appliedSettings_[QString("level%1").arg(i+1)]=settings.gains[i];
          appliedSettings_["denoise"]=settings.denoise;appliedSettings_["red"]=settings.color[0];
          appliedSettings_["green"]=settings.color[1];appliedSettings_["blue"]=settings.color[2];appliedSettings_["gamma"]=settings.gamma;
          afterPreview_ = image;
          progress_ = 100;
          status_ = tr("Adjustments applied. Compare before / after, then export.");
          display(image, tr("Result · wavelets and color"), true);
          emit adjustmentsApplied();
        });
      },
      Work::Adjust);
}
void ProcessingController::showResult(bool before) {
  if (!busy_ && result_)
    display(before ? beforePreview_ : afterPreview_,
            before ? (stillImage() ? (document_.mosaicImageCount>0 ? tr("Mosaic · before adjustments") : tr("Image · before adjustments")) : tr("Stack · before wavelets")) : tr("Result · ready to export"), true);
}
void ProcessingController::exportImage(const QUrl &url) {
  if (busy_ || !result_)
    return;
  if (!url.isLocalFile()) {
    status_ = tr("Choose a local export location.");
    emit changed();
    return;
  }
  QString path = url.toLocalFile();
  if (!path.endsWith(".tif", Qt::CaseInsensitive) &&
      !path.endsWith(".tiff", Qt::CaseInsensitive) &&
      !path.endsWith(".png", Qt::CaseInsensitive) &&
      !path.endsWith(".jpg", Qt::CaseInsensitive) &&
      !path.endsWith(".jpeg", Qt::CaseInsensitive))
    path += ".tif";
  if (source_ && QFileInfo(path).canonicalFilePath() ==
                     QFileInfo(source_->path).canonicalFilePath()) {
    status_ = tr("The source video cannot be overwritten.");
    emit changed();
    return;
  }
  if(stillImage() && document_.protectedPaths.contains(QFileInfo(path).canonicalFilePath())) {
    status_=tr("An original image cannot be overwritten."); emit changed(); return;
  }
  const auto result = result_;
  const auto overlay = annotationImage_;
  run(tr("Exporting image…"), [this, result, path, overlay](std::stop_token stop) {
    std::vector<std::uint16_t> samples(result->rgb.size());
    for (int y = 0; y < result->height; ++y) {
      check(stop);
      const auto *ink = overlay.isNull() ? nullptr : reinterpret_cast<const QRgb *>(overlay.constScanLine(y));
      for (int x = 0; x < result->width; ++x)
        for (int c = 0; c < 3; ++c) {
          const std::size_t i = (std::size_t(y) * result->width + x) * 3 + c;
          samples[i] = annotatedSample(result->rgb[i], ink ? ink[x] : QRgb(0), c);
        }
    }
    check(stop);
    QTemporaryFile temporary(
        QFileInfo(path).dir().filePath(".astrocapture-XXXXXX.tif"));
    if (!temporary.open())
      throw std::runtime_error("Cannot create an export in this folder.");
    const QString tempPath = temporary.fileName();
    temporary.close();
    const auto suffix=QFileInfo(path).suffix().toLower();
    if(suffix=="png" || suffix=="jpg" || suffix=="jpeg") {
      QImage image(result->width,result->height,QImage::Format_RGBA64);
      if(image.isNull())throw std::runtime_error("Cannot allocate export image.");
      image.setColorSpace(QColorSpace::SRgb);
      for(int y=0;y<result->height;++y) {
        check(stop);
        auto* row=reinterpret_cast<QRgba64*>(image.scanLine(y));
        for(int x=0;x<result->width;++x) {
          const auto i=(std::size_t(y)*result->width+x)*3;
          row[x]=QRgba64::fromRgba64(samples[i],samples[i+1],samples[i+2],65535);
        }
      }
      const bool png=suffix=="png";
      if(!png)image=image.convertToFormat(QImage::Format_RGB888);
      QImageWriter writer(tempPath,png?"PNG":"JPEG");
      if(!png)writer.setQuality(95);
      if(!writer.write(image))throw std::runtime_error(writer.errorString().toStdString());
    } else {
      auto written = recording::writeTiff16Samples(
          tempPath.toStdString(), result->width, result->height, samples.data());
      if (!written)throw std::runtime_error(written.error().message);
    }
    QFile input(tempPath);
    QSaveFile output(path);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
      throw std::runtime_error("Cannot open export destination.");
    while (!input.atEnd()) {
      check(stop);
      const auto bytes = input.read(1024 * 1024);
      if (bytes.isEmpty() || output.write(bytes) != bytes.size())
        throw std::runtime_error(
            "Writing the image failed. Check available disk space.");
    }
    check(stop);
    if (!output.commit())
      throw std::runtime_error("Could not finish saving the image.");
    post([this, path] {
      progress_ = 100;
      status_ = tr("Saved image: ") + path;
      emit imageExported(QUrl::fromLocalFile(path));
    });
  });
}
} // namespace astro::app
