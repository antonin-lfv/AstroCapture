#pragma once
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QVariantMap>
#include <cmath>
#include <optional>

namespace astro::app {
// Stored profiles contain effective photon response, not image-specific stars.
// Validate here without a processing dependency so preferences stay
// lightweight.
inline QByteArray equipmentDataHash(const QVariantList &rows) {
  return QCryptographicHash::hash(
             QJsonDocument(QJsonArray::fromVariantList(rows))
                 .toJson(QJsonDocument::Compact),
             QCryptographicHash::Sha256)
      .toHex();
}
inline std::optional<QVariantMap>
checkedEquipmentProfile(const QVariantMap &input) {
  const QStringList keys{"version",  "name",    "camera",   "filters",
                         "source",   "notes",   "fileName", "responseHash",
                         "dataHash", "response"};
  if (input.size() != keys.size())
    return {};
  for (const auto &key : keys)
    if (!input.contains(key))
      return {};
  if (input["version"].typeId() == QMetaType::Bool ||
      input["version"].typeId() == QMetaType::QString ||
      input["version"].toDouble() != 1)
    return {};
  QVariantMap out = input;
  for (const auto &key : keys) {
    if (key == "version" || key == "response")
      continue;
    if (input[key].typeId() != QMetaType::QString)
      return {};
    const auto value = input[key].toString();
    if (value.size() > (key == "name" ? 80 : 512) ||
        value.contains(QChar::Null))
      return {};
    out[key] = value.trimmed();
  }
  if (out["name"].toString().isEmpty())
    return {};
  for (auto c : out["name"].toString())
    if (c.category() == QChar::Other_Control)
      return {};
  const QRegularExpression hash("^[0-9a-f]{64}$");
  if (!hash.match(out["responseHash"].toString()).hasMatch() ||
      !hash.match(out["dataHash"].toString()).hasMatch())
    return {};
  if (input["response"].typeId() != QMetaType::QVariantList)
    return {};
  const auto rows = input["response"].toList();
  if (rows.size() < 3 || rows.size() > 2000)
    return {};
  QVariantList normalized;
  double previous = 0;
  bool nonzero[3]{};
  for (qsizetype i = 0; i < rows.size(); ++i) {
    if (rows[i].typeId() != QMetaType::QVariantList)
      return {};
    const auto row = rows[i].toList();
    if (row.size() != 4)
      return {};
    QVariantList values;
    for (int c = 0; c < 4; ++c) {
      const auto type = row[c].typeId();
      if (type != QMetaType::Double && type != QMetaType::Float &&
          type != QMetaType::Int && type != QMetaType::LongLong)
        return {};
      const double v = row[c].toDouble();
      if (!std::isfinite(v) ||
          (c == 0 ? (v < 336 || v > 1020 || v <= previous) : (v < 0 || v > 1)))
        return {};
      if (c == 0)
        previous = v;
      else {
        if ((i == 0 || i == rows.size() - 1) && v != 0)
          return {};
        nonzero[c - 1] |= v > 0;
      }
      values << v;
    }
    normalized << QVariant::fromValue(values);
  }
  for (bool present : nonzero)
    if (!present)
      return {};
  if (out["dataHash"].toString().toLatin1() != equipmentDataHash(normalized))
    return {};
  out["response"] = normalized;
  return out;
}
} // namespace astro::app
