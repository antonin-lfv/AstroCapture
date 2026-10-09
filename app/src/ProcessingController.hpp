#pragma once
#include <QtQml/qqmlregistration.h>

#include <QImage>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include "ExportAnnotation.hpp"
#include "ImageDocument.hpp"
#include "WorkspaceSnapshot.hpp"
#include "ProcessingSettings.hpp"
#include <astro/processing/Planetary.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace astro::app {
class ProcessingController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantMap exportAnnotation READ exportAnnotation WRITE setExportAnnotation NOTIFY annotationChanged)
  Q_PROPERTY(QString captionTechnicalText READ captionTechnicalText NOTIFY annotationChanged)
  Q_PROPERTY(QVariantMap appliedSettings READ appliedSettings NOTIFY changed)
  Q_PROPERTY(QVariant exportSizeInput READ exportSizeInput NOTIFY exportSizeInputChanged)
  Q_PROPERTY(bool stillImage READ stillImage NOTIFY changed)
  Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY changed)
  Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
  Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
  Q_PROPERTY(bool showingResult READ showingResult NOTIFY imageChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool seeking READ seeking NOTIFY changed)
  Q_PROPERTY(bool adjusting READ adjusting NOTIFY changed)
  Q_PROPERTY(bool analyzing READ analyzing NOTIFY changed)
  Q_PROPERTY(bool stacking READ stacking NOTIFY changed)
  Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
  Q_PROPERTY(bool ranked READ ranked NOTIFY changed)
  Q_PROPERTY(bool hasResult READ hasResult NOTIFY changed)
  Q_PROPERTY(int frameCount READ frameCount NOTIFY changed)
  Q_PROPERTY(int frameIndex READ frameIndex NOTIFY changed)
  Q_PROPERTY(int progress READ progress NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString stackSummary READ stackSummary NOTIFY changed)
  Q_PROPERTY(QString sourceName READ sourceName NOTIFY changed)
  Q_PROPERTY(QString imageLabel READ imageLabel NOTIFY imageChanged)
  Q_PROPERTY(QUrl exportUrl READ exportUrl NOTIFY changed)
  Q_PROPERTY(QVariantList qualities READ qualities NOTIFY changed)
public:
  explicit ProcessingController(QObject *parent = nullptr);
  ~ProcessingController() override;
  bool busy() const { return busy_; }
  bool seeking() const { return busy_ && work_ == Work::Seek; }
  bool adjusting() const { return busy_ && work_ == Work::Adjust; }
  bool analyzing() const { return busy_ && work_ == Work::Analyze; }
  bool stacking() const { return busy_ && work_ == Work::Stack; }
  bool stillImage() const { return bool(document_.pixels); }
  bool loaded() const { return bool(source_); }
  bool ranked() const { return !ranks_.empty(); }
  bool hasResult() const { return bool(result_); }
  int frameCount() const;
  int frameIndex() const { return index_; }
  int progress() const { return progress_; }
  QString status() const { return status_; }
  QString stackSummary() const { return stackSummary_; }
  QString sourceName() const;
  QString imageLabel() const { return imageLabel_; }
  QUrl exportUrl() const;
  QVariantList qualities() const { return qualities_; }
  QImage image() const { return preview_; }
  QVariantMap exportAnnotation() const { return annotation_; }
  QString captionTechnicalText() const;
  void setExportAnnotation(const QVariantMap& value);
  QImage annotationImage() const { return annotationImage_; }
  QVariant exportSizeInput() const { return QVariant::fromValue(ExportSizeInput{result_,annotationImage_}); }
  QImage beforeImage() const { return beforePreview_; }
  QVariantMap appliedSettings() const { return appliedSettings_; }
  WorkspaceSnapshot sessionSnapshot() const;
  bool canRestoreSession(const WorkspaceSnapshot&) const;
  void restoreSession(const WorkspaceSnapshot&);
  QUrl sourceUrl() const { return source_ ? QUrl::fromLocalFile(source_->path) : QUrl{}; }
  int imageWidth() const { return preview_.width(); }
  int imageHeight() const { return preview_.height(); }
  bool showingResult() const { return showingResult_; }
  Q_INVOKABLE void open(const QUrl &url);
  Q_INVOKABLE void openPath(const QString &path) {
    open(QUrl::fromLocalFile(path));
  }
  Q_INVOKABLE bool openDocument(const QVariant& document);
  Q_INVOKABLE void seek(int index);
  Q_INVOKABLE void analyze();
  Q_INVOKABLE void stack(int keepCount, bool surface = false, int patchSize = 96, bool localSelection = false);
  Q_INVOKABLE void apply(const QVariantList &gains, double denoise, double red,
                         double green, double blue, double gamma);
  Q_INVOKABLE void showResult(bool before);
  Q_INVOKABLE void exportImage(const QUrl &url);
  Q_INVOKABLE void cancel();
signals:
  void exportSizeInputChanged();
  void annotationChanged();
  void changed();
  void imageChanged();
  void sourceOpened();
  void failed();
  void stackReady();
  void adjustmentsApplied();
  void imageExported(const QUrl& url);

private:
  enum class Work { Task, Seek, Adjust, Analyze, Stack };
  struct Adjustments {
    std::array<float, 6> gains;
    float denoise;
    std::array<float, 3> color;
    float gamma;
  };
  struct Source {
    QString path;
    int width{}, height{}, count{}, bits{};
  };
  struct RankedFrame {
    int index;
    double quality;
  };
  void run(QString status, std::function<void(std::stop_token)> task,
           Work work = Work::Task);
  void startAdjustments(const Adjustments &settings);
  void acceptDocument(ImageDocument document);
  void post(std::function<void()> action);
  void report(int completed, int total, const QString &stage);
  void display(QImage image, QString label, bool result = false);
  std::shared_ptr<const Source> source_;
  ImageDocument document_;
  std::vector<RankedFrame> ranks_;
  QVariantList qualities_;
  std::shared_ptr<const processing::FloatImage> stack_, result_;
  std::shared_ptr<const processing::FloatImage> frameCache_;
  QVariantMap appliedSettings_{defaultProcessingSettings()};
  void updateAnnotation();
  QVariantMap annotation_{cleanAnnotation({})};
  QImage annotationImage_;
  QImage preview_, beforePreview_, afterPreview_;
  QString status_, imageLabel_;
  QString stackSummary_;
  int index_{}, progress_{};
  bool busy_{}, showingResult_{};
  Work work_{Work::Task};
  std::optional<int> pendingSeek_;
  std::optional<Adjustments> pendingAdjustments_;
  std::uint64_t adjustmentRevision_{};
  std::jthread worker_; // Destroyed/joined before any state it may refer to.
};
} // namespace astro::app
