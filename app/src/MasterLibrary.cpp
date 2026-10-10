#include "MasterLibrary.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <stdexcept>
namespace astro::app {
namespace {
void check(std::stop_token s) {
  if (s.stop_requested())
    throw std::runtime_error("Cancelled. Previous results are preserved.");
}
QString root() {
  return QSettings()
      .value("calibration/libraryDirectory",
             QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                 "/calibration-masters")
      .toString();
}
QVariantList inventory() {
  QFile file(root() + "/library.json");
  if (!file.exists())
    return {};
  if (file.size() > 2097152 || !file.open(QIODevice::ReadOnly))
    throw std::runtime_error("Cannot read the master library.");
  QJsonParseError error;
  const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject() ||
      doc.object()["version"].toInt() != 1 ||
      !doc.object()["masters"].isArray())
    throw std::runtime_error("Invalid master library.");
  const auto rows = doc.object()["masters"].toArray().toVariantList();
  if (rows.size() > 1000)
    throw std::runtime_error("Invalid master library.");
  for (const auto &v : rows) {
    const auto r = v.toMap();
    if (!QRegularExpression("^[a-f0-9-]{36}$")
             .match(r["id"].toString())
             .hasMatch() ||
        !QRegularExpression("^[a-f0-9]{64}$")
             .match(r["hash"].toString())
             .hasMatch() ||
        !QStringList{"Dark", "Bias", "Flat", "DarkFlat"}.contains(
            r["role"].toString()))
      throw std::runtime_error("Invalid master library.");
  }
  return rows;
}
void save(const QVariantList &rows) {
  QSaveFile file(root() + "/library.json");
  const auto bytes =
      QJsonDocument(QJsonObject{{"version", 1},
                                {"masters", QJsonArray::fromVariantList(rows)}})
          .toJson();
  if (bytes.size() > 2097152 || !file.open(QIODevice::WriteOnly) ||
      file.write(bytes) != bytes.size() || !file.commit())
    throw std::runtime_error("Cannot save the master library.");
}
QString reasons(QStringList keys) {
  static const QMap<QString, const char *> labels{
      {"width", "Width"},
      {"height", "Height"},
      {"channels", "Channels"},
      {"bayer", "Bayer pattern"},
      {"camera", "Camera"},
      {"binX", "Horizontal binning"},
      {"binY", "Vertical binning"},
      {"gain", "Gain"},
      {"offset", "Offset"},
      {"temperature", "Temperature"},
      {"exposure", "Exposure"},
      {"filter", "Filter"},
      {"aduScale", "Sample normalization units"},
      {"selected lights / filter group",
       "Select matching lights (dark-flat: select flats)"}};
  for (auto &key : keys)
    key = QCoreApplication::translate("MasterLibraryDialog",
                                      labels.value(key, "Unknown metadata"));
  return keys.join(", ");
}
double median(std::vector<float> a) {
  const auto mid = a.begin() + a.size() / 2;
  std::nth_element(a.begin(), mid, a.end());
  return *mid;
}
} // namespace
namespace {
QString batchText(const char* text) { return QCoreApplication::translate("MasterLibraryDialog",text); }
QVariantMap configuration(const QVariantMap& row) {
  QVariantMap config;
  for(const auto* key:{"role","width","height","channels","bayer","camera","binX","binY","gain","offset","temperature","exposure","aduScale"})
    config[key]=row.value(key);
  if(row["role"]=="Flat") {config["group"]=row["group"];config["filter"]=row["filter"].toString().toUpper();}
  return config;
}
QString configurationId(const QVariantMap& row) {
  return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument::fromVariant(configuration(row)).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
QStringList missingConfiguration(const QVariantMap& row) {
  QStringList missing;
  if(row["camera"].toString().isEmpty())missing<<"camera";
  for(const auto* key:{"binX","binY"})if(row[key].toInt()<=0)missing<<key;
  for(const auto* key:{"gain","offset","temperature"})if(!row.contains(key))missing<<key;
  if(row["exposure"].toDouble()<=0)missing<<"exposure";
  if(!row.contains("aduScale"))missing<<"aduScale";
  if(row["role"]=="Flat"&&row["filter"].toString().isEmpty())missing<<"filter";
  return missing;
}
QString batchName(const QVariantMap& row) {
  const auto camera=row["camera"].toString().isEmpty()?batchText("Unknown camera"):row["camera"].toString();
  const auto role=row["role"].toString();
  const auto label=role=="DarkFlat"?batchText("Dark flat"):batchText(role.toUtf8().constData());
  auto name=QString("%1 · %2 · %3 s").arg(camera,label,QString::number(row["exposure"].toDouble(),'g',8));
  if(role=="Flat")name+=" · "+row["group"].toString();
  if(row.contains("temperature"))name+=QString(" · %1 °C").arg(row["temperature"].toDouble(),0,'g',6);
  if(row.contains("gain"))name+=QString(" · ")+batchText("Gain")+" "+QString::number(row["gain"].toDouble(),'g',6);
  return name.left(100);
}
}
QVariantMap calibrationCorrectionCompatibility(const QVariantMap& candidate,const QVariantMap& flatGroup) {
  auto sources=flatGroup["sources"].toList();
  const bool bias=candidate["role"]=="Bias";
  for(auto& v:sources) {auto row=v.toMap();row["selected"]=true;row["role"]=bias?"Light":"Flat";v=row;}
  auto result=masterCompatibility(candidate,sources);
  // AC_ORIG describes the input normalization units even for Float32 masters.
  for(const auto& v:sources)if(candidate.contains("aduScale") && v.toMap().contains("aduScale") && candidate["aduScale"].toDouble()!=v.toMap()["aduScale"].toDouble()) {
    result["compatibility"]=2;result["mismatch"]=batchText("Sample normalization units");
  }
  return result;
}
QVariantMap calibrationBatchPlan(const QVariantList& frames,const QVariantList& library) {
  QMap<QString,QVariantMap> groups;QVariantList candidates,selected;
  int ignored=0;
  for(const auto& v:frames) {
    const auto row=v.toMap();
    if(!row["selected"].toBool()||!QStringList{"Bias","DarkFlat","Dark","Flat"}.contains(row["role"].toString()))continue;
    selected<<row;
    if(row["master"].toBool()) {
      ++ignored;
      if(row["role"]=="Bias"||row["role"]=="DarkFlat") {auto c=row;c["id"]="selected:"+row["path"].toString();c["savedMaster"]=true;c["label"]=row["name"];candidates<<c;}
      continue;
    }
    const auto id=configurationId(row);
    auto group=groups.value(id,row);group["id"]=id;group["name"]=batchName(row);
    auto sources=group["sources"].toList();sources<<row;group["sources"]=sources;group["count"]=sources.size();
    group["missing"]=reasons(missingConfiguration(row));group["reviewRequired"]=!missingConfiguration(row).isEmpty();
    const auto pixels=std::uint64_t(std::max(0,row["width"].toInt()))*std::max(0,row["height"].toInt());
    const auto samples=pixels*std::max(0,row["channels"].toInt());
    if(pixels<1||pixels>32000000||(row["channels"].toInt()!=1&&row["channels"].toInt()!=3)||sources.size()>2000)
      group["error"]=batchText("Invalid calibration dimensions or source count.");
    for(const auto* key:{"exposure","aduScale","gain","offset","temperature"})if(row.contains(key) && (!std::isfinite(row[key].toDouble()) || (QString(key)=="exposure"&&row[key].toDouble()<0) || (QString(key)=="aduScale"&&row[key].toDouble()<=0)))
      group["error"]=batchText("Invalid calibration metadata.");
    const auto extension=QFileInfo(row["path"].toString()).suffix().toLower();
    if(!QStringList{"fits","fit","fts","fz","cr2","cr3","nef","nrw","arw","dng","orf","raf","rw2","pef"}.contains(extension))
      group["error"]=batchText("Batch masters require FITS or native RAW source files.");
    group["workingBytes"]=qulonglong(samples*(row["role"]=="Flat"?44:36)+32ULL*1024*1024);
    group["temporaryBytes"]=qulonglong(sources.size()>=3?samples*4*sources.size():0);
    groups[id]=group;
  }
  for(const auto& v:library) {
    auto candidate=v.toMap();if(candidate["role"]!="Bias"&&candidate["role"]!="DarkFlat")continue;
    const auto canonical=QFileInfo(candidate["path"].toString()).canonicalFilePath();
    bool duplicate=false;for(const auto& c:candidates) {
      const auto existing=c.toMap()["path"].toString();
      if(existing==candidate["path"].toString()||(!canonical.isEmpty()&&QFileInfo(existing).canonicalFilePath()==canonical))duplicate=true;
    }
    if(duplicate)continue;
    candidate["masterHash"]=candidate["hash"];candidate["id"]="library:"+candidate["id"].toString();candidate["savedMaster"]=true;candidate["label"]=candidate["name"];candidates<<candidate;
  }
  for(const auto& group:groups)if(group["role"]=="Bias"||group["role"]=="DarkFlat") {
    if(group.contains("error"))continue;
    auto c=group;c["label"]=group["name"];c["savedMaster"]=false;candidates<<c;
  }
  QVariantList jobs;
  for(auto group:groups) {
    if(group["role"]=="Flat") {
      QVariantList options;QStringList preferred, fallback;
      for(auto c:candidates) {
        auto candidate=c.toMap();const auto compatibility=calibrationCorrectionCompatibility(candidate,group);
        if(compatibility["compatibility"].toInt()==2)continue;
        candidate["compatibility"]=compatibility["compatibility"];candidate["missing"]=compatibility["missing"];
        options<<candidate;
        (candidate["role"]=="DarkFlat"?preferred:fallback)<<candidate["id"].toString();
      }
      group["corrections"]=options;
      const auto& choices=preferred.isEmpty()?fallback:preferred;
      group["defaultCorrection"]=choices.size()==1?choices.front():QString();
      if(options.empty())group["error"]=batchText("Select a matching dark-flat or bias for this flat group.");
    }
    group["blocked"]=group.contains("error");jobs<<group;
  }
  std::stable_sort(jobs.begin(),jobs.end(),[](const QVariant& a,const QVariant& b){
    const QStringList order{"Bias","DarkFlat","Dark","Flat"};
    const auto aa=a.toMap(),bb=b.toMap();const auto ra=order.indexOf(aa["role"].toString()),rb=order.indexOf(bb["role"].toString());
    return ra==rb?aa["name"].toString()<bb["name"].toString():ra<rb;
  });
  const auto revision=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject{{"selected",QJsonArray::fromVariantList(selected)},{"library",QJsonArray::fromVariantList(library)}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
  return {{"groups",jobs},{"revision",revision},{"ignoredMasters",ignored}};
}
recording::AstroImage buildCalibrationBatchMaster(const QVariantMap& group,const QVariantMap& correction,
    const std::function<recording::AstroImage(const QVariantMap&)>& load,std::stop_token stop,const processing::IntegrationLimits& limits) {
  check(stop);
  if(group["blocked"].toBool()||group["sources"].toList().isEmpty())throw std::runtime_error("This calibration group is blocked. Review the batch plan.");
  const auto checkpoint=[stop]{check(stop);};
  auto checkedLoad=[&](const QVariantMap& row) {
    checkpoint();auto image=load(row);auto actual=calibrationMetadata(image);actual["role"]=row["role"];actual["group"]=row["group"];
    if(configurationId(actual)!=configurationId(row) || !image.masterRole.empty())throw std::runtime_error("Calibration source metadata changed. Reimport the source files.");
    return image;
  };
  auto average=[&](const QVariantMap& job,bool normalize,const processing::Calibration& calibration) {
    const auto sources=job["sources"].toList();
    return processing::masterFrame(sources.size(),[&](int n){auto source=checkedLoad(sources[n].toMap());return normalize?processing::calibrate(std::move(source),calibration,checkpoint):source;},normalize,checkpoint,true,&limits);
  };
  processing::Calibration calibration;
  if(group["role"]=="Flat") {
    if(correction.isEmpty()||calibrationCorrectionCompatibility(correction,group)["compatibility"].toInt()==2)
      throw std::runtime_error("A matching flat correction is required. Review the batch plan.");
    auto corrected=correction["savedMaster"].toBool()?load(correction):average(correction,false,{});
    // Recheck decoded correction metadata; known mismatches cannot be confirmed away.
    auto actual=calibrationMetadata(corrected);actual["role"]=correction["role"];
    if(calibrationCorrectionCompatibility(actual,group)["compatibility"].toInt()==2)
      throw std::runtime_error("Flat correction metadata changed. Reimport the correction.");
    if(correction["role"]=="DarkFlat")calibration.dark=std::move(corrected);else calibration.bias=std::move(corrected);
  }
  auto result=average(group,group["role"]=="Flat",calibration);
  result.masterRole=group["role"].toString().toStdString();result.masterCount=group["count"].toInt();result.normalizedMasterFlat=group["role"]=="Flat";
  return result;
}

QString calibrationFileHash(const QString &path, std::stop_token stop) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    throw std::runtime_error("Master file is missing or unreadable.");
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) {
    check(stop);
    const auto b = file.read(1048576);
    if (b.isEmpty() && file.error() != QFile::NoError)
      throw std::runtime_error("Cannot read master file.");
    hash.addData(b);
  }
  check(stop);
  return QString::fromLatin1(hash.result().toHex());
}
QVariantMap calibrationMetadata(const recording::AstroImage &a) {
  QVariantMap r{{"width", a.width},
                {"height", a.height},
                {"channels", a.channels},
                {"bayer", QString::fromUtf8(domain::to_string(a.bayer).data())},
                {"camera", QString::fromStdString(a.camera)},
                {"binX", a.binX},
                {"binY", a.binY},
                {"filter", QString::fromStdString(a.filter)},
                {"exposure", a.exposure},
                {"aduScale", a.originalScale},
                {"master", !a.masterRole.empty()},
                {"masterFlat", a.normalizedMasterFlat},
                {"masterCount", a.masterCount},
                {"rejectedSamples", qulonglong(a.masterRejected)}};
  if (a.hasGain)
    r["gain"] = a.gain;
  if (a.hasTemperature)
    r["temperature"] = a.temperature;
  if (a.hasOffset)
    r["offset"] = a.offset;
  return r;
}
QVariantMap masterCompatibility(const QVariantMap &master,
                                const QVariantList &rows) {
  QStringList missing, errors;
  int considered = 0;
  const auto role = master["role"].toString();
  for (const auto &v : rows) {
    const auto l = v.toMap();
    if (l["role"] != (role == "DarkFlat" ? "Flat" : "Light") ||
        !l["selected"].toBool())
      continue;
    if (role == "Flat" && master["group"] != l["group"])
      continue;
    ++considered;
    for (const auto &key : {"width", "height", "channels", "bayer"})
      if (master[key] != l[key])
        errors << QString::fromUtf8(key);
    for (const auto &key : {"camera", "binX", "binY", "gain", "offset"}) {
      const bool unknown =
          !master.contains(key) || !l.contains(key) ||
          (QString(key) == "camera"
               ? (master[key].toString().isEmpty() ||
                  l[key].toString().isEmpty())
               : (QString(key).startsWith("bin") &&
                  (master[key].toInt() <= 0 || l[key].toInt() <= 0)));
      if (unknown)
        missing << QString::fromUtf8(key);
      else if (QString(key) == "camera"
                   ? master[key].toString() != l[key].toString()
                   : std::abs(master[key].toDouble() - l[key].toDouble()) > .01)
        errors << QString::fromUtf8(key);
    }
    if(role!="Flat") {
      if(!master.contains("aduScale")||!l.contains("aduScale"))missing<<"aduScale";
      else if(!std::isfinite(master["aduScale"].toDouble())||!std::isfinite(l["aduScale"].toDouble())||master["aduScale"].toDouble()<=0||l["aduScale"].toDouble()<=0||master["aduScale"].toDouble()!=l["aduScale"].toDouble())errors<<"aduScale";
    }
    if (role == "Dark" || role == "DarkFlat") {
      if (!master.contains("temperature") || !l.contains("temperature"))
        missing << "temperature";
      else if (std::abs(master["temperature"].toDouble() -
                        l["temperature"].toDouble()) > 2)
        errors << "temperature";
      if (master["exposure"].toDouble() <= 0 || l["exposure"].toDouble() <= 0)
        missing << "exposure";
      else if (std::abs(master["exposure"].toDouble() -
                        l["exposure"].toDouble()) >
               std::max(.01, l["exposure"].toDouble() * .01))
        errors << "exposure";
    }
    if (role == "Flat") {
      if (master["filter"].toString().isEmpty() ||
          l["filter"].toString().isEmpty())
        missing << "filter";
      else if (master["filter"].toString().toUpper() !=
               l["filter"].toString().toUpper())
        errors << "filter";
    }
  }
  if (!considered)
    errors << "selected lights / filter group";
  missing.removeDuplicates();
  errors.removeDuplicates();
  return {{"compatibility", errors.isEmpty() ? (missing.isEmpty() ? 0 : 1) : 2},
          {"missing", reasons(missing)},
          {"mismatch", reasons(errors)}};
}
QVariantList calibrationMasterInventory() {
  auto rows = inventory();
  for (auto &v : rows) {
    auto r = v.toMap();
    r["path"] = root() + "/" + r["id"].toString() + ".fits";
    v = r;
  }
  return rows;
}
QVariantMap calibrationMaster(const QString &id, std::stop_token stop) {
  for (const auto &v : calibrationMasterInventory()) {
    auto r = v.toMap();
    if (r["id"] == id) {
      if (calibrationFileHash(r["path"].toString(), stop) !=
          r["hash"].toString())
        throw std::runtime_error(
            "Master integrity check failed. Import an intact copy.");
      return r;
    }
  }
  throw std::runtime_error("Master not found in the library.");
}
QVariantMap storeCalibrationMaster(recording::AstroImage image,
                                   const QString &name, std::stop_token stop, const QVariantMap &provenance) {
  check(stop);
  if (name.trimmed().isEmpty() || name.size() > 120 ||
      !QStringList{"Dark", "Flat", "Bias", "DarkFlat"}.contains(
          QString::fromStdString(image.masterRole)) ||
      image.samples.empty() ||
      (image.masterRole == "Flat" && !image.normalizedMasterFlat))
    throw std::runtime_error("Invalid master name, role or flat calibration.");
  std::vector<float> sample;
  double nonfinite = 0, zero = 0, minimum = INFINITY, maximum = -INFINITY;
  const auto step = std::max(std::size_t(1), image.samples.size() / 100000);
  for (std::size_t i = 0; i < image.samples.size(); ++i) {
    if (i % 65536 == 0)
      check(stop);
    const float v = image.samples[i];
    if (!std::isfinite(v)) {
      ++nonfinite;
      continue;
    }
    zero += v <= 0;
    minimum = std::min(minimum, double(v));
    maximum = std::max(maximum, double(v));
    if (i % step == 0)
      sample.push_back(v);
  }
  if (sample.empty() || nonfinite > 0)
    throw std::runtime_error(
        "A library master must contain finite samples only. Repair or exclude "
        "defective source frames.");
  const double med = median(sample);
  for (auto &v : sample)
    v = std::abs(v - med);
  const double noise = 1.4826 * median(sample);
  if (image.masterRole == "Flat" && (minimum <= 1e-6 || med <= 0))
    throw std::runtime_error(
        "Master flat contains zero or invalid illumination.");
  if (!QDir().mkpath(root()))
    throw std::runtime_error("Cannot create the master library.");
  QTemporaryDir temp(root() + "/build-XXXXXX");
  if (!temp.isValid())
    throw std::runtime_error("Cannot create master working files.");
  const auto file = temp.filePath("master.fits");
  recording::writeAstroFits(file.toStdString(), image);
  check(stop);
  auto r = calibrationMetadata(image);
  r["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
  r["name"] = name.trimmed();
  r["role"] = QString::fromStdString(image.masterRole);
  r["date"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  r["hash"] = calibrationFileHash(file, stop);
  for(const auto* key:{"sourceHashes","correctionHashes"}) {
    const auto hashes=provenance.value(key).toList();
    if(hashes.size()>2000)throw std::runtime_error("Invalid master provenance.");
    for(const auto& hash:hashes)if(!QRegularExpression("^[a-f0-9]{64}$").match(hash.toString()).hasMatch())throw std::runtime_error("Invalid master provenance.");
    if(!hashes.empty())r[key]=hashes;
  }
  if(provenance.contains("metadataReviewed"))r["metadataReviewed"]=provenance["metadataReviewed"].toBool();
  r["median"] = med;
  r["noiseMAD"] = noise;
  r["minimum"] = minimum;
  r["maximum"] = maximum;
  r["nonPositivePercent"] = 100 * zero / image.samples.size();
  r["group"] =
      image.filter.empty()
          ? (image.channels == 3 || image.bayer != domain::BayerPattern::None
                 ? "RGB"
                 : "Mono")
          : QString::fromStdString(image.filter).toUpper();
  auto group = r["group"].toString();
  if (group == "RED")
    group = "R";
  if (group == "GREEN")
    group = "G";
  if (group == "BLUE")
    group = "B";
  if (group == "LUMINANCE" || group == "LUM")
    group = "L";
  if (group == "HALPHA" || group == "H-ALPHA" || group == "H_ALPHA")
    group = "HA";
  r["group"] = group;
  QLockFile lock(root() + "/library.lock");
  if (!lock.tryLock(5000))
    throw std::runtime_error("Master library is in use. Retry shortly.");
  auto rows = inventory();
  if (rows.size() >= 1000)
    throw std::runtime_error("Master library is limited to 1000 entries.");
  check(stop);
  const auto destination = root() + "/" + r["id"].toString() + ".fits";
  if (!QFile::rename(file, destination))
    throw std::runtime_error("Cannot store the master file.");
  try {
    rows << r;
    save(rows);
  } catch (...) {
    QFile::remove(destination);
    throw;
  }
  r["path"] = destination;
  return r;
}
void removeCalibrationMaster(const QString &id) {
  QLockFile lock(root() + "/library.lock");
  if (!lock.tryLock(5000))
    throw std::runtime_error("Master library is in use. Retry shortly.");
  auto rows = inventory();
  for (int i = 0; i < rows.size(); ++i)
    if (rows[i].toMap()["id"] == id) {
      rows.removeAt(i);
      save(rows);
      return;
    }
  throw std::runtime_error("Master not found in the library.");
}
} // namespace astro::app
