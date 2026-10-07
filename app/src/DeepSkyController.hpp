#pragma once
#include <QtQml/qqmlregistration.h>
#include <QObject>
#include <QImage>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <astro/processing/DeepSky.hpp>
#include <thread>
#include "ImageDocument.hpp"
namespace astro::app {
class DeepSkyController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool adjusting READ adjusting NOTIFY changed)
    Q_PROPERTY(bool hasRawComparison READ hasRawComparison NOTIFY changed)
    Q_PROPERTY(bool comparingRaw READ comparingRaw NOTIFY changed)
    Q_PROPERTY(QString rawComparisonName READ rawComparisonName NOTIFY changed)
    Q_PROPERTY(QVariantMap linearHistogram READ linearHistogram NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList frames READ frames NOTIFY changed)
    Q_PROPERTY(QStringList groups READ groups NOTIFY changed)
    Q_PROPERTY(QVariantMap projectOptions READ projectOptions WRITE setProjectOptions NOTIFY projectOptionsChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY changed)
    Q_PROPERTY(bool displayingResult READ displayingResult NOTIFY changed)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
    Q_PROPERTY(QVariantMap suggestedStretch READ suggestedStretch NOTIFY resultChanged)
public:
    explicit DeepSkyController(QObject* parent=nullptr):QObject(parent){}
    ~DeepSkyController() override;
    bool busy() const{return busy_;}
    bool adjusting() const{return adjusting_;}
    bool hasRawComparison() const{return !rawPreview_.isNull();}
    bool comparingRaw() const{return comparingRaw_;}
    QString rawComparisonName() const{return rawName_;}
    QVariantMap linearHistogram() const{return histogram_;}
    int progress() const{return progress_;}
    QString stage() const{return stage_;}
    QString status() const{return status_;}
    QVariantList frames() const{return frames_;}
    QStringList groups() const{return groups_;}
    bool hasResult() const{return bool(linear_);}
    bool displayingResult() const{return displayingResult_;}
    int imageWidth() const{return preview_.width();}
    int imageHeight() const{return preview_.height();}
    QImage image() const{return preview_;}
    QVariantMap projectOptions() const{return projectOptions_;}
    void setProjectOptions(const QVariantMap& options){if(!busy_){projectOptions_=options;emit projectOptionsChanged();}}
    QVariantMap suggestedStretch() const{return stretch_;}
    Q_INVOKABLE void addFiles(const QList<QUrl>& urls,const QString& role="Light",const QString& group="Auto",double inputScale=0);
    Q_INVOKABLE void addFolder(const QUrl&,const QString& role="Auto",const QString& group="Auto",double inputScale=0);
    Q_INVOKABLE void setSelected(int index,bool selected);
    Q_INVOKABLE void setRole(int index,const QString& role);
    Q_INVOKABLE void setGroup(int index,const QString& group);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void inspect(int index);
    Q_INVOKABLE void restoreResult();
    Q_INVOKABLE void chooseRawComparison(int index);
    Q_INVOKABLE void showRawComparison(bool enabled);
    Q_INVOKABLE void integrate(bool calibrated,double sigma=3,bool normalize=true,bool correctBadPixels=true);
    Q_INVOKABLE void setReference(int index);
    Q_INVOKABLE void selectResult(const QString& group);
    Q_INVOKABLE void compose(const QString& red,const QString& green,const QString& blue,const QString& luminance="");
    Q_INVOKABLE void apply(const QVariantMap& options);
    Q_INVOKABLE void exportImage(const QUrl&,bool linear=true);
    Q_INVOKABLE void saveProject(const QUrl&);
    Q_INVOKABLE void loadProject(const QUrl&);
    Q_INVOKABLE QVariant processingDocument() const;
    Q_INVOKABLE void cancel(){pendingAdjustments_.reset();worker_.request_stop();}
signals:
    void changed();
    void imageChanged();
    void resultChanged();
    void projectOptionsChanged();
    void imageExported(const QUrl&);
private:
    void run(const QString&,std::function<void(std::stop_token)>);
    void post(std::function<void()>);
    void show(std::shared_ptr<const processing::FloatImage>,bool result, QImage preview, std::shared_ptr<const processing::FloatImage> displayed, QVariantMap stretch);
    QVariantList frames_;
    QStringList groups_;
    QString status_,stage_;
    std::map<QString,QString> references_;
    int progress_{};
    bool busy_{},adjusting_{};
    std::optional<QVariantMap> pendingAdjustments_;
    QVariantMap histogram_;
    bool displayingResult_{},comparingRaw_{};
    QString rawName_;
    QImage preview_,resultPreview_,rawPreview_;
    QVariantMap stretch_,projectOptions_;
    std::map<QString,std::shared_ptr<const processing::FloatImage>> stacks_;
    std::shared_ptr<const processing::FloatImage> linear_,finished_;
    double totalExposure_{};
    int exposureCount_{};
    QStringList resultFilters_;
    std::map<QString,int> groupCounts_;
    std::map<QString,double> groupExposure_;
    std::jthread worker_;
};
}
