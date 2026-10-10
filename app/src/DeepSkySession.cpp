#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include "AstrometrySettings.hpp"
#include "GaiaCatalog.hpp"
#include <QFileInfo>

namespace astro::app {
namespace {
bool validImage(const std::shared_ptr<const processing::FloatImage>& p) {
    return p && p->width>0 && p->height>0 && std::uint64_t(p->width)*p->height<=32000000 && p->rgb.size()==std::size_t(p->width)*p->height*3;
}
bool validRows(const QVariantList& rows) {
    if(rows.size()>2000)return false;
    for(const auto& value:rows) { const auto row=value.toMap(); if(row["path"].toString().isEmpty() || !QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(row["role"].toString()))return false; }
    return true;
}
}
WorkspaceSnapshot DeepSkyController::sessionSnapshot() const {
    QVariantMap stackAstrometry;for(const auto& [key,state]:stackAstrometry_)stackAstrometry[key]=state;
    WorkspaceSnapshot s;s.state={{"integrationReport",integrationReport_},{"astrometry",astrometry_},{"stackAstrometry",stackAstrometry},{"frames",frames_},{"groups",groups_},{"options",projectOptions_},{"finish",finishSettings_},{"stretch",stretch_},{"histogram",histogram_},{"exposure",totalExposure_},{"exposureCount",exposureCount_},{"filters",resultFilters_},{"backgroundSamples",backgroundSamples_},{"report",advancedReport_},{"psf",psfKernel_},{"debayer",sourceDebayer_},{"autoStretch",sourceAutoStretch_},{"showingMask",showingMask_},{"showingBackground",showingBackgroundModel_},{"comparingRaw",comparingRaw_},{"sourceRow",sourceRow_},{"sourceInterpretation",sourceInterpretation_},{"rawName",rawName_}};
    QVariantMap counts,exposures,references;for(const auto& [key,v]:groupCounts_)counts[key]=v;for(const auto& [key,v]:groupExposure_)exposures[key]=v;for(const auto& [key,v]:references_)references[key]=v;s.state["counts"]=counts;s.state["exposures"]=exposures;s.state["references"]=references;
    s.state["starLayerView"]=starLayerView_;if(starsLayer_)s.images["stars"]=starsLayer_;if(starBackgroundLayer_)s.images["starBackground"]=starBackgroundLayer_;if(!starsLayerPreview_.isNull())s.previews["stars"]=starsLayerPreview_;if(!starBackgroundLayerPreview_.isNull())s.previews["starBackground"]=starBackgroundLayerPreview_;
    if(linear_)s.images["linear"]=linear_;if(finished_)s.images["finished"]=finished_;for(const auto& [key,image]:stacks_)s.images["stack/"+key]=image;
    if(!preview_.isNull())s.previews["display"]=preview_;if(!geometryPreview_.isNull())s.previews["geometry"]=geometryPreview_;if(!maskSourcePreview_.isNull())s.previews["maskSource"]=maskSourcePreview_;if(!resultPreview_.isNull())s.previews["result"]=resultPreview_;if(!linearPreview_.isNull())s.previews["linear"]=linearPreview_;if(!comparisonPreview_.isNull())s.previews["comparison"]=comparisonPreview_;if(!maskPreview_.isNull())s.previews["mask"]=maskPreview_;if(!backgroundPreview_.isNull())s.previews["background"]=backgroundPreview_;if(!rawPreview_.isNull())s.previews["raw"]=rawPreview_;
    s.state["projectUrl"]=projectUrl_.toString();s.state["sourceFingerprints"]=sourceFingerprints_;
    s.state["comparisonKey"]=QString::fromLatin1(comparisonOrientationKey_.toBase64());s.state["displayingResult"]=displayingResult_;
    return s;
}
bool DeepSkyController::canRestoreSession(const WorkspaceSnapshot& s) const { return !busy_ && validSession(s); }
bool DeepSkyController::validSession(const WorkspaceSnapshot& s) {
    try{const auto state=s.state["astrometry"].toMap();if(!validAstrometryState(state))return false;const auto stacks=s.state["stackAstrometry"].toMap();if(stacks.size()>500)return false;for(auto i=stacks.cbegin();i!=stacks.cend();++i){if(!validAstrometryState(i.value().toMap()))return false;const auto a=i.value().toMap();if(a["verified"].toBool()&&(!s.images.value("stack/"+i.key())||a["imageKey"].toString()!=QString::fromLatin1(linearImageKey(*s.images.value("stack/"+i.key())))))return false;}if(state.contains("wcs")&&!readWcs(state["wcs"]))return false;readCatalogValues(state["catalog"].toList());readCatalogValues(state["localCatalog"].toList());if(state["verified"].toBool()&&(!readWcs(state["wcs"])||!s.images.value("linear")||state["imageKey"].toString()!=QString::fromLatin1(linearImageKey(*s.images.value("linear")))))return false;}catch(const std::exception&){return false;}
    if(!validRows(s.state["frames"].toList()))return false;
    const auto finish=s.state["finish"].toMap();if(!finish.isEmpty()&&!checkedDeepSkySettings(finish))return false;
    const auto options=s.state["options"].toMap();if(options.contains("finish")&&!checkedDeepSkySettings(options["finish"].toMap()))return false;if(options.contains("versions")&&!checkedDeepSkySnapshot(options["versions"].toMap()))return false;
    const auto linear=s.images.value("linear"),finished=s.images.value("finished");if(bool(linear)!=bool(finished))return false;
    if(linear && (!validImage(linear)||!validImage(finished)||s.previews.value("display").isNull()||s.previews.value("linear").size()!=QSize(linear->width,linear->height)||s.previews.value("result").size()!=QSize(finished->width,finished->height)))return false;
    for(const auto& p:s.images)if(!validImage(p))return false;
    if(s.images.contains("stars")!=s.images.contains("starBackground"))return false;
    if(s.images.contains("stars")){if(!finished||!finish["starSeparation"].toBool())return false;for(const auto& key:{"stars","starBackground"})if(s.images[key]->width!=finished->width||s.images[key]->height!=finished->height||s.previews[key].size()!=QSize(finished->width,finished->height))return false;}
    if(s.state.value("starLayerView",0).toInt()<0||s.state.value("starLayerView",0).toInt()>2)return false;
    return true;
}
void DeepSkyController::restoreSession(const WorkspaceSnapshot& s) {
    if(!canRestoreSession(s))return;clear();integrationReport_=s.state["integrationReport"].toList();astrometry_=s.state["astrometry"].toMap();const auto stackAstrometry=s.state["stackAstrometry"].toMap();for(auto i=stackAstrometry.cbegin();i!=stackAstrometry.cend();++i)stackAstrometry_[i.key()]=i.value().toMap();projectUrl_=s.state["projectUrl"].toUrl();sourceFingerprints_=s.state["sourceFingerprints"].toMap();frames_=s.state["frames"].toList();groups_=s.state["groups"].toStringList();projectOptions_=s.state["options"].toMap();finishSettings_=s.state["finish"].toMap();stretch_=s.state["stretch"].toMap();histogram_=s.state["histogram"].toMap();totalExposure_=s.state["exposure"].toDouble();exposureCount_=s.state["exposureCount"].toInt();resultFilters_=s.state["filters"].toStringList();backgroundSamples_=s.state["backgroundSamples"].toList();advancedReport_=s.state["report"].toMap();psfKernel_=s.state["psf"].toList();sourceDebayer_=s.state["debayer"].toBool();sourceAutoStretch_=s.state["autoStretch"].toBool();maskPreview_=s.previews.value("mask");backgroundPreview_=s.previews.value("background");rawPreview_=s.previews.value("raw");showingMask_=s.state["showingMask"].toBool()&&!maskPreview_.isNull();showingBackgroundModel_=s.state["showingBackground"].toBool()&&!backgroundPreview_.isNull();comparingRaw_=s.state["comparingRaw"].toBool()&&!rawPreview_.isNull();sourceRow_=s.state["sourceRow"].toMap();sourceInterpretation_=s.state["sourceInterpretation"].toString();rawName_=s.state["rawName"].toString();
    auto counts=s.state["counts"].toMap(),exposures=s.state["exposures"].toMap(),refs=s.state["references"].toMap();for(auto it=counts.cbegin();it!=counts.cend();++it)groupCounts_[it.key()]=it.value().toInt();for(auto it=exposures.cbegin();it!=exposures.cend();++it)groupExposure_[it.key()]=it.value().toDouble();for(auto it=refs.cbegin();it!=refs.cend();++it)references_[it.key()]=it.value().toString();
    starsLayer_=s.images.value("stars");starBackgroundLayer_=s.images.value("starBackground");starsLayerPreview_=s.previews.value("stars");starBackgroundLayerPreview_=s.previews.value("starBackground");starLayerView_=hasStarLayers()?s.state.value("starLayerView",0).toInt():0;
    geometryPreview_=s.previews.value("geometry",s.previews.value("result"));maskSourcePreview_=s.previews.value("maskSource",s.previews.value("linear"));linear_=s.images.value("linear");finished_=s.images.value("finished");for(auto it=s.images.cbegin();it!=s.images.cend();++it)if(it.key().startsWith("stack/"))stacks_[it.key().mid(6)]=it.value();preview_=s.previews.value("display");resultPreview_=s.previews.value("result");linearPreview_=s.previews.value("linear");comparisonPreview_=s.previews.value("comparison");comparisonOrientationKey_=QByteArray::fromBase64(s.state["comparisonKey"].toString().toLatin1());displayingResult_=s.state["displayingResult"].toBool();progress_=100;status_=tr("Session restored. Cached results are ready.");emit projectOptionsChanged();emit resultChanged();emit imageChanged();emit changed();
}
}
