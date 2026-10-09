#pragma once
#include "SessionStorage.hpp"
#include "ProcessingController.hpp"
#include "DeepSkyController.hpp"
#include "MosaicController.hpp"
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <thread>

namespace astro::app {
class SessionController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::ProcessingController* processing READ processing WRITE setProcessing NOTIFY controllersChanged)
    Q_PROPERTY(astro::app::DeepSkyController* deepSky READ deepSky WRITE setDeepSky NOTIFY controllersChanged)
    Q_PROPERTY(astro::app::MosaicController* mosaic READ mosaic WRITE setMosaic NOTIFY controllersChanged)
    Q_PROPERTY(QVariantMap uiState READ uiState WRITE setUiState NOTIFY uiStateChanged)
    Q_PROPERTY(QString storageDirectory READ storageDirectory WRITE setStorageDirectory NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
    Q_PROPERTY(QString savedAt READ savedAt NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit SessionController(QObject* parent=nullptr);
    ~SessionController() override;
    ProcessingController* processing() const{return processing_;}
    DeepSkyController* deepSky() const{return deepSky_;}
    MosaicController* mosaic() const{return mosaic_;}
    void setProcessing(ProcessingController*);
    void setDeepSky(DeepSkyController*);
    void setMosaic(MosaicController*);
    QVariantMap uiState() const{return uiState_;}
    void setUiState(const QVariantMap&);
    QString storageDirectory() const{return directory_;}
    void setStorageDirectory(const QString&);
    bool busy() const{return busy_;}
    bool recoveryAvailable() const{return available_;}
    QString savedAt() const{return savedAt_;}
    QString status() const{return status_;}
    Q_INVOKABLE bool saveNow();
    Q_INVOKABLE bool restoreLast();
    Q_INVOKABLE void discardRecovery();
    void scheduleSave();
signals:
    void changed();
    void controllersChanged();
    void uiStateChanged();
    void saveFinished(bool success);
    void sessionRestored(const QVariantMap& uiState);
private:
    bool workOpen() const;
    bool processingBusy() const;
    QPointer<ProcessingController> processing_;
    QPointer<DeepSkyController> deepSky_;
    QPointer<MosaicController> mosaic_;
    QVariantMap uiState_;
    QString directory_,savedAt_,status_;
    std::unique_ptr<SessionStorage> storage_;
    QTimer timer_;
    bool busy_{},available_{},armed_{},restoring_{},saveAgain_{};
    std::jthread worker_;
};
}
