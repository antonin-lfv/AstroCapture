#include "SessionController.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace astro::app {
SessionController::SessionController(QObject* parent):QObject(parent) {
    timer_.setSingleShot(true);timer_.setInterval(2000);connect(&timer_,&QTimer::timeout,this,&SessionController::saveNow);
    setStorageDirectory(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("session"));
}
SessionController::~SessionController(){worker_.request_stop();if(worker_.joinable())worker_.join();}
void SessionController::setStorageDirectory(const QString& path) {
    if(busy_||directory_==path)return;timer_.stop();directory_=path;storage_=std::make_unique<SessionStorage>(path);available_=storage_->available();savedAt_=storage_->savedAt();armed_=!available_;emit changed();
}
void SessionController::setProcessing(ProcessingController* c) {
    if(processing_==c)return;if(processing_)disconnect(processing_,nullptr,this,nullptr);processing_=c;
    if(c){connect(c,&ProcessingController::changed,this,&SessionController::scheduleSave);connect(c,&ProcessingController::annotationChanged,this,&SessionController::scheduleSave);connect(c,&ProcessingController::imageChanged,this,&SessionController::scheduleSave);}emit controllersChanged();
}
void SessionController::setDeepSky(DeepSkyController* c) {
    if(deepSky_==c)return;if(deepSky_)disconnect(deepSky_,nullptr,this,nullptr);deepSky_=c;
    if(c){connect(c,&DeepSkyController::changed,this,&SessionController::scheduleSave);connect(c,&DeepSkyController::projectOptionsChanged,this,&SessionController::scheduleSave);connect(c,&DeepSkyController::imageChanged,this,&SessionController::scheduleSave);}emit controllersChanged();
}
void SessionController::setMosaic(MosaicController* c) {
    if(mosaic_==c)return;if(mosaic_)disconnect(mosaic_,nullptr,this,nullptr);mosaic_=c;
    if(c){connect(c,&MosaicController::changed,this,&SessionController::scheduleSave);connect(c,&MosaicController::filesChanged,this,&SessionController::scheduleSave);connect(c,&MosaicController::annotationChanged,this,&SessionController::scheduleSave);}emit controllersChanged();
}
void SessionController::setUiState(const QVariantMap& state){if(uiState_==state)return;uiState_=state;emit uiStateChanged();scheduleSave();}
bool SessionController::workOpen() const{return (processing_&&(processing_->loaded()||processing_->stillImage()||processing_->hasResult()))||(deepSky_&&!deepSky_->frames().isEmpty())||(mosaic_&&!mosaic_->files().isEmpty());}
bool SessionController::processingBusy() const{return (processing_&&processing_->busy())||(deepSky_&&deepSky_->busy())||(mosaic_&&mosaic_->busy());}
void SessionController::scheduleSave(){if(restoring_)return;if(workOpen())armed_=true;if(!armed_)return;timer_.start();}
bool SessionController::saveNow() {
    timer_.stop();if(restoring_)return false;if(busy_){saveAgain_=true;return true;}
    if(processingBusy()){timer_.start();return false;}if(!armed_||!processing_||!deepSky_||!mosaic_)return false;
    SavedSession session{uiState_,{{"processing",processing_->sessionSnapshot()},{"deepSky",deepSky_->sessionSnapshot()},{"mosaic",mosaic_->sessionSnapshot()}}};
    // No workspace is open after an explicit clear: remove the recovery only.
    if(!workOpen()){discardRecovery();return true;}
    busy_=true;saveAgain_=false;status_=tr("Saving session…");emit changed();
    worker_=std::jthread([this,session=std::move(session)](std::stop_token stop){QString error,time;try{storage_->save(session,[stop]{if(stop.stop_requested())throw std::runtime_error("Session saving cancelled.");});time=storage_->savedAt();}catch(const std::exception& e){error=QString::fromUtf8(e.what());}
        QMetaObject::invokeMethod(this,[this,error,time]{busy_=false;available_=storage_->available();if(!time.isEmpty())savedAt_=time;status_=error.isEmpty()?tr("Session saved automatically."):tr("Session could not be saved: %1").arg(error);emit changed();if(saveAgain_&&error.isEmpty()){saveAgain_=false;saveNow();return;}emit saveFinished(error.isEmpty());},Qt::QueuedConnection);
    });return true;
}
bool SessionController::restoreLast() {
    if(busy_||processingBusy()||!available_||!processing_||!deepSky_||!mosaic_)return false;
    if(workOpen()){status_=tr("A workspace is already open. Save it before restoring another session.");emit changed();return false;}
    timer_.stop();busy_=restoring_=true;status_=tr("Restoring session…");emit changed();
    worker_=std::jthread([this](std::stop_token stop){SavedSession session;QString error;try{session=storage_->load([stop]{if(stop.stop_requested())throw std::runtime_error("Session restoration cancelled.");});}catch(const std::exception& e){error=QString::fromUtf8(e.what());}
        QMetaObject::invokeMethod(this,[this,session=std::move(session),error]() mutable {
            if(!error.isEmpty()||!session.workspaces.contains("processing")||!session.workspaces.contains("deepSky")||!session.workspaces.contains("mosaic")||!processing_||!deepSky_||!mosaic_||!processing_->canRestoreSession(session.workspaces["processing"])||!deepSky_->canRestoreSession(session.workspaces["deepSky"])||!mosaic_->canRestoreSession(session.workspaces["mosaic"])){
                busy_=restoring_=false;status_=tr("Session could not be restored: %1").arg(error.isEmpty()?tr("Invalid saved settings."):error);emit changed();return;
            }
            processing_->restoreSession(session.workspaces["processing"]);deepSky_->restoreSession(session.workspaces["deepSky"]);mosaic_->restoreSession(session.workspaces["mosaic"]);
            uiState_=session.ui;QSet<QString> paths;const auto ps=session.workspaces["processing"].state;const auto source=ps["source"].toMap();if(!source["path"].toString().isEmpty())paths.insert(source["path"].toString());for(const auto& path:ps["document"].toMap()["protected"].toStringList())paths.insert(path);for(const auto& row:deepSky_->frames())paths.insert(row.toMap()["path"].toString());for(const auto& row:session.workspaces["mosaic"].state["inputs"].toList())paths.insert(row.toMap()["path"].toString());int missing=0;for(const auto& path:paths)if(!QFileInfo::exists(path))++missing;
            busy_=false;armed_=true;available_=false;status_=missing?tr("Session restored. %1 source files are unavailable; cached results can still be exported.").arg(missing):tr("Session restored. Cached results are ready.");emit uiStateChanged();emit sessionRestored(uiState_);restoring_=false;emit changed();
        },Qt::QueuedConnection);
    });return true;
}
void SessionController::discardRecovery(){if(busy_)return;try{storage_->discard();available_=false;savedAt_.clear();status_=tr("Saved session removed. Original files are unchanged.");}catch(const std::exception& e){status_=tr("Cannot remove the saved session: %1").arg(QString::fromUtf8(e.what()));}emit changed();}
}
