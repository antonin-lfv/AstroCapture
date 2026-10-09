#include "ProcessingController.hpp"
#include "MosaicController.hpp"
#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include <QFileInfo>
#include <QTransform>
#include <set>
#include <cmath>

namespace astro::app {
namespace {
bool validImage(const std::shared_ptr<const processing::FloatImage>& p) {
    return p && p->width>0 && p->height>0 && std::uint64_t(p->width)*p->height<=32000000 && p->rgb.size()==std::size_t(p->width)*p->height*3;
}

}
WorkspaceSnapshot ProcessingController::sessionSnapshot() const {
    WorkspaceSnapshot s;
    s.state={{"annotation",annotation_},{"applied",appliedSettings_},{"index",index_},{"showingResult",showingResult_},{"summary",stackSummary_},{"label",imageLabel_},{"qualities",qualities_},{"still",stillImage()}};
    if(source_)s.state["source"]=QVariantMap{{"path",source_->path},{"width",source_->width},{"height",source_->height},{"count",source_->count},{"bits",source_->bits}};
    s.state["document"]=QVariantMap{{"name",document_.name},{"export",document_.exportSuggestion.toString()},{"protected",document_.protectedPaths},{"mosaicCount",document_.mosaicImageCount},{"processingSummary",document_.processingSummary}};
    QVariantList ranks;for(const auto& r:ranks_)ranks<<QVariantMap{{"index",r.index},{"quality",r.quality}};s.state["ranks"]=ranks;
    if(stack_)s.images["stack"]=stack_;if(result_)s.images["result"]=result_;if(frameCache_)s.images["frame"]=frameCache_;
    if(document_.edgeMask)s.planes["edges"]=document_.edgeMask;
    if(!geometryPreview_.isNull())s.previews["geometry"]=geometryPreview_;if(!preview_.isNull())s.previews["display"]=preview_;if(!beforePreview_.isNull())s.previews["before"]=beforePreview_;if(!afterPreview_.isNull())s.previews["after"]=afterPreview_;
    return s;
}
bool ProcessingController::canRestoreSession(const WorkspaceSnapshot& s) const {
    if(busy_ || !checkedProcessingSettings(s.state["applied"].toMap()))return false;
    const auto source=s.state["source"].toMap();const auto stack=s.images.value("stack");const auto result=s.images.value("result");
    if(stack && (!validImage(stack)||!validImage(result)||s.previews.value("display").isNull()||s.previews.value("before").size()!=QSize(result->width,result->height)||s.previews.value("after").size()!=QSize(result->width,result->height)))return false;
    const auto geometry=s.previews.value("geometry");if(!geometry.isNull()&&(!stack||geometry.size()!=QSize(stack->width,stack->height)))return false;
    if(result && !stack)return false;
    if(s.state["still"].toBool() && !stack)return false;
    if(!source.isEmpty()) {
        const int count=source["count"].toInt();const auto width=source["width"].toInt(),height=source["height"].toInt();
        if(source["path"].toString().isEmpty() || width<=0||height<=0||std::uint64_t(width)*height>32000000||count<1||count>10000000||s.state["index"].toInt()<0||s.state["index"].toInt()>=count)return false;
        std::set<int> indices;for(const auto& row:s.state["ranks"].toList()){const auto r=row.toMap();const int index=r["index"].toInt();const auto quality=r["quality"].toDouble();if(index<0||index>=count||!indices.insert(index).second||!std::isfinite(quality)||quality<0)return false;}
    }
    const auto edges=s.planes.value("edges");if(edges && (!stack||edges->size()!=std::size_t(stack->width)*stack->height))return false;
    return true;
}
void ProcessingController::restoreSession(const WorkspaceSnapshot& s) {
    if(!canRestoreSession(s))return;
    const auto source=s.state["source"].toMap();source_.reset();if(!source.isEmpty())source_=std::make_shared<Source>(Source{source["path"].toString(),source["width"].toInt(),source["height"].toInt(),source["count"].toInt(),source["bits"].toInt()});
    stack_=s.images.value("stack");result_=s.images.value("result");frameCache_=s.images.value("frame");document_={};
    if(s.state["still"].toBool()) {const auto d=s.state["document"].toMap();document_.pixels=stack_;document_.preview=s.previews.value("geometry",s.previews.value("before"));document_.name=d["name"].toString();document_.exportSuggestion=QUrl(d["export"].toString());document_.protectedPaths=d["protected"].toStringList();document_.edgeMask=s.planes.value("edges");document_.mosaicImageCount=d["mosaicCount"].toInt();document_.processingSummary=d["processingSummary"].toString();}
    annotation_=cleanAnnotation(s.state["annotation"].toMap());appliedSettings_=*checkedProcessingSettings(s.state["applied"].toMap());index_=s.state["index"].toInt();showingResult_=s.state["showingResult"].toBool();stackSummary_=s.state["summary"].toString();imageLabel_=s.state["label"].toString();qualities_=s.state["qualities"].toList();ranks_.clear();for(const auto& row:s.state["ranks"].toList()){const auto r=row.toMap();ranks_.push_back({r["index"].toInt(),r["quality"].toDouble()});}
    geometryPreview_=s.previews.value("geometry",s.previews.value("before"));preview_=s.previews.value("display");beforePreview_=s.previews.value("before");afterPreview_=s.previews.value("after");pendingAdjustments_.reset();pendingSeek_.reset();++adjustmentRevision_;progress_=100;updateAnnotation();status_=tr("Session restored. Cached results are ready.");emit changed();emit imageChanged();
}
WorkspaceSnapshot MosaicController::sessionSnapshot() const {
    WorkspaceSnapshot s;s.state={{"annotation",annotation_},{"resultCount",resultCount_}};
    QVariantList inputs,excluded;auto row=[](const Input& i){return QVariantMap{{"path",i.path},{"width",i.width},{"height",i.height},{"selected",i.selected}};};
    for(const auto& i:inputs_)inputs<<row(i);for(const auto& i:excluded_)excluded<<row(i);s.state["inputs"]=inputs;s.state["excluded"]=excluded;
    if(result_)s.images["result"]=result_;if(edgeMask_)s.planes["edges"]=edgeMask_;if(!preview_.isNull())s.previews["display"]=preview_;
    return s;
}
bool MosaicController::canRestoreSession(const WorkspaceSnapshot& s) const {
    if(busy_)return false;for(const auto& key:{"inputs","excluded"}){const auto list=s.state[key].toList();if(list.size()>2000)return false;for(const auto& value:list){const auto row=value.toMap();if(row["path"].toString().isEmpty()||row["width"].toInt()<1||row["height"].toInt()<1||std::uint64_t(row["width"].toInt())*row["height"].toInt()>24000000)return false;}}
    const auto p=s.images.value("result");if(p&&(!validImage(p)||s.previews.value("display").size()!=QSize(p->width,p->height)))return false;const auto edges=s.planes.value("edges");return !edges || (p && edges->size()==std::size_t(p->width)*p->height);
}
void MosaicController::restoreSession(const WorkspaceSnapshot& s) {
    if(!canRestoreSession(s))return;inputs_.clear();excluded_.clear();auto rows=[](const QVariantList& list,std::vector<Input>& target){for(const auto& value:list){const auto r=value.toMap();target.push_back({r["path"].toString(),r["width"].toInt(),r["height"].toInt(),r["selected"].toBool()});}};
    rows(s.state["inputs"].toList(),inputs_);rows(s.state["excluded"].toList(),excluded_);result_=s.images.value("result");edgeMask_=s.planes.value("edges");resultCount_=s.state["resultCount"].toInt();preview_=s.previews.value("display");annotation_=cleanAnnotation(s.state["annotation"].toMap());updateAnnotation();progress_=100;status_=tr("Session restored. Cached results are ready.");emit filesChanged();emit imageChanged();emit changed();
}
}
