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
#include "WorkspaceSnapshot.hpp"
namespace astro::app {
class DeepSkyController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QImage geometryImage READ geometryImage NOTIFY imageChanged)
    Q_PROPERTY(QImage maskSourceImage READ maskSourceImage NOTIFY imageChanged)
    Q_PROPERTY(QImage importedMaskImage READ importedMaskImage NOTIFY maskFileChanged)
    Q_PROPERTY(QString importedMaskPath READ importedMaskPath NOTIFY maskFileChanged)
    QML_ELEMENT
    Q_PROPERTY(bool hasFinishComparison READ hasFinishComparison NOTIFY imageChanged)
    Q_PROPERTY(bool hasStarLayers READ hasStarLayers NOTIFY changed)
    Q_PROPERTY(int starLayerView READ starLayerView WRITE setStarLayerView NOTIFY changed)
    Q_PROPERTY(QVariantList calibrationMasters READ calibrationMasters NOTIFY changed)
    Q_PROPERTY(QVariantMap blindSolver READ blindSolver NOTIFY changed)
    Q_PROPERTY(QVariantMap astrometry READ astrometry NOTIFY changed)
    Q_PROPERTY(bool hasCelestialCoordinates READ hasCelestialCoordinates NOTIFY changed)
    Q_PROPERTY(QVariantMap advancedReport READ advancedReport NOTIFY changed)
    Q_PROPERTY(bool showingMask READ showingMask WRITE setShowingMask NOTIFY changed)
    Q_PROPERTY(bool hasMask READ hasMask NOTIFY changed)
    Q_PROPERTY(QVariantList psfKernel READ psfKernel NOTIFY changed)
    Q_PROPERTY(bool showingBackgroundModel READ showingBackgroundModel WRITE setShowingBackgroundModel NOTIFY changed)
    Q_PROPERTY(bool hasBackgroundModel READ hasBackgroundModel NOTIFY changed)
    Q_PROPERTY(QString sourceInterpretation READ sourceInterpretation NOTIFY changed)
    Q_PROPERTY(bool sourceDebayer READ sourceDebayer WRITE setSourceDebayer NOTIFY changed)
    Q_PROPERTY(bool sourceAutoStretch READ sourceAutoStretch WRITE setSourceAutoStretch NOTIFY changed)
    Q_PROPERTY(QVariantList backgroundSamples READ backgroundSamples NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
    Q_PROPERTY(bool restoringProject READ restoringProject NOTIFY changed)
    Q_PROPERTY(QVariantList missingSources READ missingSources NOTIFY changed)
    Q_PROPERTY(double portableProjectBytes READ portableProjectBytes NOTIFY changed)
    Q_PROPERTY(QString projectCacheDirectory READ projectCacheDirectory WRITE setProjectCacheDirectory NOTIFY changed)
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
    Q_PROPERTY(QVariantList integrationReport READ integrationReport NOTIFY changed)
    Q_PROPERTY(QString integrationTemporaryDirectory READ integrationTemporaryDirectory NOTIFY changed)
    Q_PROPERTY(QUrl integrationTemporaryDirectoryUrl READ integrationTemporaryDirectoryUrl NOTIFY changed)
    Q_PROPERTY(QVariantList diagnosticMaps READ diagnosticMaps NOTIFY changed)
    Q_PROPERTY(QVariantMap hdrReport READ hdrReport NOTIFY changed)
    Q_PROPERTY(QImage diagnosticImage READ diagnosticImage NOTIFY diagnosticChanged)
    Q_PROPERTY(QVariantMap diagnosticInfo READ diagnosticInfo NOTIFY diagnosticChanged)
    Q_PROPERTY(QVariantMap projectOptions READ projectOptions WRITE setProjectOptions NOTIFY projectOptionsChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY changed)
    Q_PROPERTY(bool displayingResult READ displayingResult NOTIFY changed)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
    Q_PROPERTY(QVariantMap finishSettings READ finishSettings NOTIFY changed)
    Q_PROPERTY(QVariantMap suggestedStretch READ suggestedStretch NOTIFY resultChanged)
    Q_PROPERTY(double linearExportBytes READ linearExportBytes NOTIFY resultChanged)
public:
    explicit DeepSkyController(QObject* parent=nullptr):QObject(parent){}
    QString integrationTemporaryDirectory() const;
    QUrl integrationTemporaryDirectoryUrl() const{return QUrl::fromLocalFile(integrationTemporaryDirectory());}
    Q_INVOKABLE void chooseIntegrationTemporaryDirectory(const QUrl&);
    Q_INVOKABLE QVariantMap integrationResources(const QVariantMap& options) const;
    ~DeepSkyController() override;
    QVariantMap advancedReport() const{return advancedReport_;}
    QVariantMap astrometry() const {
        auto view=astrometry_;
        for(const auto& key:{"catalog","localCatalog","response"})view.remove(key);
        return view;
    }
    bool hasStarLayers() const{return bool(starsLayer_)&&bool(starBackgroundLayer_);}
    int starLayerView() const{return starLayerView_;}
    void setStarLayerView(int);
    Q_INVOKABLE void exportStarLayer(const QUrl&,int layer);
    QVariantList calibrationMasters() const;
    Q_INVOKABLE void createCalibrationMaster(const QString& role,const QString& name);
    Q_INVOKABLE void importCalibrationMaster(const QUrl&,const QString& role,const QString& name,bool correctedFlat=false);
    Q_INVOKABLE void useCalibrationMaster(const QString& id,bool reviewed=false);
    Q_INVOKABLE void removeCalibrationMasterEntry(const QString& id);
    QVariantMap blindSolver() const;
    Q_INVOKABLE void chooseBlindIndexes(const QUrl&);
    Q_INVOKABLE void installBlindIndex();
    Q_INVOKABLE void solveAstrometry(const QVariantMap& options);
    Q_INVOKABLE void prepareCatalogCalibration(int method,const QVariantMap& options);
    Q_INVOKABLE void importAstrometryCatalog(const QUrl&);
    Q_INVOKABLE bool importColorResponse(const QUrl&);
    Q_INVOKABLE void resolveTarget(const QString& name);
    Q_INVOKABLE QVariantMap colorResponse() const;
    Q_INVOKABLE bool useEquipmentProfile(const QVariantMap& profile);
    bool hasCelestialCoordinates() const{return linear_&&finished_&&displayingResult_&&!comparingRaw_&&!showingMask_&&!showingBackgroundModel_&&!busy_&&astrometry_["verified"].toBool();}
    Q_INVOKABLE QVariantMap skyCoordinates(double x,double y) const;
    Q_INVOKABLE bool copySkyCoordinates(double x,double y) const;
    QVariantList psfKernel() const{return psfKernel_;}
    bool showingMask() const{return showingMask_;}
    bool hasMask() const{return !maskPreview_.isNull();}
    void setShowingMask(bool);
    Q_INVOKABLE void estimatePsf();
    Q_INVOKABLE QVariantList importPhotometryCatalog(const QUrl&);
    Q_INVOKABLE QVariantList ghsCurve(const QVariantMap&) const;
    bool showingBackgroundModel() const{return showingBackgroundModel_;}
    bool hasBackgroundModel() const{return !backgroundPreview_.isNull();}
    void setShowingBackgroundModel(bool);
    QString sourceInterpretation() const{return sourceInterpretation_;}
    bool sourceDebayer() const{return sourceDebayer_;}
    bool sourceAutoStretch() const{return sourceAutoStretch_;}
    void setSourceDebayer(bool);
    void setSourceAutoStretch(bool);
    QVariantList backgroundSamples() const{return backgroundSamples_;}
    bool busy() const{return busy_;}
    bool exporting() const{return busy_ && exporting_;}
    bool restoringProject() const{return restoringProject_;}
    QVariantList missingSources() const;
    double portableProjectBytes() const;
    QString projectCacheDirectory() const;
    void setProjectCacheDirectory(const QString& path){if(!busy_){projectCacheDirectory_=path;emit changed();}}
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
    QVariantList integrationReport() const{return integrationReport_;}
    QVariantList diagnosticMaps() const;
    QVariantMap hdrReport() const{return hdrReport_;}
    QImage diagnosticImage() const{return diagnosticPreview_;}
    QVariantMap diagnosticInfo() const{return diagnosticInfo_;}
    Q_INVOKABLE void inspectDiagnostic(const QString& key,int channel=3);
    Q_INVOKABLE void exportDiagnostic(const QUrl&,const QString& key);
    Q_INVOKABLE void mergeHdr(const QVariantList& sources,double transition,bool reviewed);
    bool hasResult() const{return bool(linear_);}
    bool displayingResult() const{return displayingResult_;}
    int imageWidth() const{return preview_.width();}
    int imageHeight() const{return preview_.height();}
    QImage image() const{return preview_;}
    bool hasFinishComparison() const{return !comparisonPreview_.isNull();}
    QImage comparisonImage() const{return comparisonPreview_;}
    QVariantMap projectOptions() const{return projectOptions_;}
    void setProjectOptions(const QVariantMap& options){if(!busy_){projectOptions_=options;emit projectOptionsChanged();}}
    QVariantMap finishSettings() const{return finishSettings_;}
    QVariantMap suggestedStretch() const{return stretch_;}
    double linearExportBytes() const {
        if(!linear_)return -1;
        const auto bytes=std::uint64_t(linear_->width)*linear_->height*12;
        return double(2880+((bytes+2879)/2880)*2880);
    }
    Q_INVOKABLE void addFiles(const QList<QUrl>& urls,const QString& role="Light",const QString& group="Auto",double inputScale=0);
    Q_INVOKABLE void addFolder(const QUrl&,const QString& role="Auto",const QString& group="Auto",double inputScale=0);
    Q_INVOKABLE void setSelected(int index,bool selected);
    Q_INVOKABLE void setRole(int index,const QString& role);
    Q_INVOKABLE void setGroup(int index,const QString& group);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void clear();
    QImage geometryImage() const{return geometryPreview_;}
    Q_INVOKABLE void loadMaskFile(const QUrl&);
    QImage importedMaskImage()const{return maskImportPreview_;}
    QString importedMaskPath()const{return maskImportPath_;}
    QImage maskSourceImage() const{return maskSourcePreview_;}
    Q_INVOKABLE void inspect(int index);
    Q_INVOKABLE void restoreResult();
    Q_INVOKABLE void chooseRawComparison(int index);
    Q_INVOKABLE void showRawComparison(bool enabled);
    Q_INVOKABLE void integrate(bool calibrated,double sigma=3,bool normalize=true,bool correctBadPixels=true,bool sequenceBackground=false,
                              bool drizzle=false,double drizzleScale=2,double pixelFraction=0.8,bool nativeBayer=false,QString bayerPattern={},bool diagnosticMaps=false,bool wcsProjection=false);
    Q_INVOKABLE void setReference(int index);
    Q_INVOKABLE void selectResult(const QString& group);
    Q_INVOKABLE void compose(const QString& red,const QString& green,const QString& blue,const QString& luminance="");
    Q_INVOKABLE void apply(const QVariantMap& options);
    Q_PROPERTY(QUrl exportUrl READ exportUrl NOTIFY changed)
    QUrl exportUrl() const;
    Q_INVOKABLE void exportImage(const QUrl&,bool linear=true);
    Q_INVOKABLE void saveProject(const QUrl&);
    Q_INVOKABLE void savePortableProject(const QUrl&);
    Q_INVOKABLE void relinkSources(const QUrl& folder, bool allowMetadataMatch=false);
    Q_INVOKABLE void loadProject(const QUrl&);
    Q_INVOKABLE QVariant processingDocument() const;
    WorkspaceSnapshot sessionSnapshot() const;
    bool canRestoreSession(const WorkspaceSnapshot&) const;
    static bool validSession(const WorkspaceSnapshot&);
    void restoreSession(const WorkspaceSnapshot&);
    Q_INVOKABLE void cancel(){pendingAdjustments_.reset();worker_.request_stop();}
signals:
    void targetResolved(const QVariantMap& position);
    void maskFileChanged();
    void catalogCalibrationReady(const QVariantMap& settings);
    void maskFileReady(const QString& path);
    void operationFinished(bool success, const QString& message);
    void changed();
    void imageChanged();
    void diagnosticChanged();
    void resultChanged();
    void projectOptionsChanged();
    void imageExported(const QUrl&);
    void projectOpened(const QUrl&);
    void projectRestored(const QVariantMap& viewState);
    void projectSaved(const QUrl&);
private:
    QUrl projectUrl_;
    QString projectCacheDirectory_;
    QVariantMap sourceFingerprints_;
    bool restoringProject_{};
    void restoreProject(const WorkspaceSnapshot&, const QUrl&, const QString&);
    void run(const QString&,std::function<void(std::stop_token)>);
    void post(std::function<void()>);
    void show(std::shared_ptr<const processing::FloatImage>,bool result, QImage preview, std::shared_ptr<const processing::FloatImage> displayed, QVariantMap stretch, QImage before = {});
    QVariantList frames_,backgroundSamples_;
    QStringList groups_;
    QString status_,stage_,sourceInterpretation_;
    QVariantMap sourceRow_;
    bool sourceDebayer_{true},sourceAutoStretch_{true};
    std::map<QString,QString> references_;
    int progress_{};
    bool busy_{},adjusting_{},exporting_{};
    std::optional<QVariantMap> pendingAdjustments_;
    QVariantMap histogram_,advancedReport_;
    QVariantList integrationReport_;
    QVariantMap hdrReport_,diagnosticInfo_;
    QImage diagnosticPreview_;
    std::map<QString,std::shared_ptr<const processing::FloatImage>> diagnosticMaps_;
    int starLayerView_{};
    std::shared_ptr<const processing::FloatImage> starsLayer_,starBackgroundLayer_;
    QImage starsLayerPreview_,starBackgroundLayerPreview_;
    QVariantMap astrometry_;
    std::map<QString,QVariantMap> stackAstrometry_;
    QVariantList psfKernel_;
    QImage maskPreview_;
    bool showingMask_{};
    bool displayingResult_{},comparingRaw_{};
    QString rawName_;
    QImage geometryPreview_,maskSourcePreview_,maskImportPreview_;
    QString maskImportPath_;
    QImage preview_,resultPreview_,rawPreview_,backgroundPreview_;
    QImage linearPreview_,comparisonPreview_;
    QByteArray comparisonOrientationKey_;
    bool showingBackgroundModel_{};
    QVariantMap stretch_,projectOptions_,finishSettings_;
    std::map<QString,std::shared_ptr<const processing::FloatImage>> stacks_;
    std::shared_ptr<const processing::FloatImage> linear_,finished_,pipelineCache_,pipelineSource_;
    QByteArray pipelineCacheKey_;
    double totalExposure_{};
    int exposureCount_{};
    QStringList resultFilters_;
    std::map<QString,int> groupCounts_;
    std::map<QString,double> groupExposure_;
    std::jthread worker_;
};
}
