#pragma once
#include <QObject>
#include "ExportAnnotation.hpp"
#include "ImageDocument.hpp"
#include <QImage>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <astro/processing/Mosaic.hpp>
#include <memory>
#include <thread>
namespace astro::app {
class MosaicController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList files READ files NOTIFY filesChanged)
    Q_PROPERTY(QVariantList selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QStringList excludedFiles READ excludedFiles NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap exportAnnotation READ exportAnnotation WRITE setExportAnnotation NOTIFY annotationChanged)
    Q_PROPERTY(QVariant exportSizeInput READ exportSizeInput NOTIFY exportSizeInputChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
    Q_PROPERTY(QUrl exportUrl READ exportUrl NOTIFY changed)
public:
    explicit MosaicController(QObject* parent=nullptr) : QObject(parent) {
        connect(this,&MosaicController::filesChanged,this,&MosaicController::selectionChanged);
        connect(this,&MosaicController::imageChanged,this,&MosaicController::exportSizeInputChanged);
        connect(this,&MosaicController::annotationChanged,this,&MosaicController::exportSizeInputChanged);
    }
    ~MosaicController() override;
    QVariantList files() const;
    QStringList excludedFiles() const;
    int selectedCount() const;
    QVariantList selection() const;
    bool busy() const { return busy_; }
    bool hasResult() const { return bool(result_); }
    int progress() const { return progress_; }
    QString status() const { return status_; }
    int imageWidth() const { return result_ ? result_->width : 0; }
    int imageHeight() const { return result_ ? result_->height : 0; }
    QImage image() const { return preview_; }
    QUrl exportUrl() const;
    QVariantMap exportAnnotation() const { return annotation_; }
    void setExportAnnotation(const QVariantMap& value);
    QImage annotationImage() const { return annotationImage_; }
    QVariant exportSizeInput() const { return QVariant::fromValue(ExportSizeInput{result_,annotationImage_}); }
    Q_INVOKABLE QVariant processingDocument() const;
    Q_INVOKABLE void addImages(const QList<QUrl>& urls);
    Q_INVOKABLE void removeImage(int index);
    Q_INVOKABLE void setImageSelected(int index, bool selected);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void assemble(bool balanceExposure=true, bool skipUnmatched=true, bool softenEdges=true);
    Q_INVOKABLE void restoreExcluded();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void exportImage(const QUrl& url);
signals:
    void exportSizeInputChanged();
    void filesChanged();
    void selectionChanged();
    void annotationChanged();
    void changed();
    void imageChanged();
    void imageExported(const QUrl& url);
private:
    struct Input { QString path; int width{},height{}; bool selected{true}; };
    void invalidate();
    void updateAnnotation();
    void post(std::function<void()> action);
    void run(std::function<void(std::stop_token)> task);
    std::vector<Input> inputs_, excluded_;
    std::shared_ptr<const std::vector<float>> edgeMask_;
    std::shared_ptr<const processing::FloatImage> result_;
    QImage preview_, annotationImage_;
    QVariantMap annotation_{cleanAnnotation({})};
    QString status_;
    int progress_{}, resultCount_{};
    bool busy_{};
    std::jthread worker_;
};
}
