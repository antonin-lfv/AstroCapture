#include "ObservationPlot.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
namespace astro::app {
namespace {
QRectF frame(double width, double height) {
  return {48, 20, std::max(1.0, width - 64), std::max(1.0, height - 55)};
}
QRectF worldFrame(double width, double height) {
  double w = std::min(width - 32, (height - 40) * 2);
  return {(width - w) / 2, 16, w, w / 2};
}
QJsonArray land() {
  static const QJsonArray value = []() {
    QFile f(":/observation/world-land.json");
    f.open(QIODevice::ReadOnly);
    return QJsonDocument::fromJson(f.readAll()).array();
  }();
  return value;
}
} // namespace
ObservationPlot::ObservationPlot(QQuickItem *p) : QQuickPaintedItem(p) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  connect(this, &ObservationPlot::colorsChanged, this, [this] { update(); });
}
void ObservationPlot::setController(ObservationController *c) {
  if (controller_)
    disconnect(controller_, nullptr, this, nullptr);
  controller_ = c;
  if (c) {
    connect(c, &ObservationController::changed, this, [this] { update(); });
    connect(c, &ObservationController::instantChanged, this,
            [this] { update(); });
    connect(c, &ObservationController::weatherChanged, this,
            [this] { update(); });
    connect(c, &QObject::destroyed, this, [this] {
      controller_ = nullptr;
      update();
    });
  }
  emit controllerChanged();
  update();
}
void ObservationPlot::paint(QPainter *p) {
  auto color = [this](const char *name) {
    return colors_.value(name).value<QColor>();
  };
  const auto text = color("text"), secondary = color("secondary"),
             border = color("border"), accent = color("accent"),
             surface = color("surface"), image = color("image"),
             primary = color("primary"), warning = color("warning");
  if (!controller_ || width() < 100 || height() < 100)
    return;
  p->setRenderHint(QPainter::Antialiasing);
  QFont font = p->font();
  font.setPixelSize(12);
  p->setFont(font);
  const auto samples = controller_->samples();
  const auto instant = controller_->instant();
  const auto settings = controller_->settings();
  auto rect = frame(width(), height());
  p->fillRect(boundingRect(), image);
  if (mode_ == "clouds") {
    auto x = [&](double fraction) {
      return rect.left() + fraction * rect.width();
    };
    auto y = [&](double cloud) {
      return rect.bottom() - cloud / 100 * rect.height();
    };
    for (int cloud : {0, 25, 50, 75, 100}) {
      p->setPen(QPen(border, 1));
      p->drawLine(QPointF(rect.left(), y(cloud)),
                  QPointF(rect.right(), y(cloud)));
      p->setPen(secondary);
      p->drawText(QRectF(0, y(cloud) - 10, 40, 20),
                  Qt::AlignRight | Qt::AlignVCenter,
                  QString::number(cloud) + "%");
    }
    for (int i = 0; i <= 4 && !samples.isEmpty(); ++i) {
      auto sample = samples[qRound((samples.size() - 1) * i / 4.0)].toMap();
      p->drawText(QRectF(x(i / 4.0) - (i == 0   ? 0
                                       : i == 4 ? 48
                                                : 24),
                         rect.bottom() + 8, 48, 20),
                  i == 0   ? Qt::AlignLeft
                  : i == 4 ? Qt::AlignRight
                           : Qt::AlignCenter,
                  sample["time"].toString());
    }
    p->save();
    p->setClipRect(rect);
    const auto forecast = controller_->weatherSamples();
    QPainterPath curve;
    bool first = true, any = false;
    qint64 previous = 0;
    for (const auto &value : forecast) {
      auto sample = value.toMap();
      if (!sample.value("available").toBool()) {
        first = true;
        continue;
      }
      const auto utc = sample.value("utc").toLongLong();
      if (!first && utc - previous != 3600)
        first = true;
      previous = utc;
      any = true;
      const QPointF point(x(sample.value("fraction").toDouble()),
                          y(sample.value("cloud_cover").toDouble()));
      if (first)
        curve.moveTo(point);
      else
        curve.lineTo(point);
      first = false;
      p->setPen(Qt::NoPen);
      p->setBrush(accent);
      p->drawEllipse(point, 3, 3);
    }
    p->setPen(QPen(accent, 2));
    p->setBrush(Qt::NoBrush);
    p->drawPath(curve);
    const auto weather = controller_->weather();
    const double cursor = x(instant.value("fraction").toDouble());
    p->setPen(QPen(secondary, 1));
    p->drawLine(QPointF(cursor, rect.top()), QPointF(cursor, rect.bottom()));
    if (weather.value("available").toBool()) {
      const auto
          start = controller_->result().value("startUtcSeconds").toLongLong(),
          end = controller_->result().value("endUtcSeconds").toLongLong();
      const double selected =
          x(double(weather.value("utc").toLongLong() - start) / (end - start));
      p->setBrush(accent);
      p->setPen(QPen(text, 1));
      p->drawEllipse(
          QPointF(selected, y(weather.value("cloud_cover").toDouble())), 5, 5);
    }
    if (!any) {
      p->setPen(secondary);
      p->drawText(rect.adjusted(12, 12, -12, -12),
                  Qt::AlignCenter | Qt::TextWordWrap,
                  tr("No hourly cloud data for this period"));
    }
    p->restore();
    return;
  }
  if (mode_ == "world" || mode_ == "daylight") {
    rect = worldFrame(width(), height());
    auto project = [&](double lon, double lat) {
      return QPointF(rect.left() + (lon + 180) / 360 * rect.width(),
                     rect.top() + (90 - lat) / 180 * rect.height());
    };
    p->fillRect(rect, surface);
    for (const auto &v : controller_->world()) {
      auto c = v.toMap();
      const int state = c[mode_ == "daylight" ? "solarState" : "state"].toInt();
      QColor color = mode_ == "daylight" ? (state == 0   ? warning
                                            : state == 1 ? primary
                                                         : image)
                     : state == 0        ? image
                     : state == 1        ? warning
                     : state == 2        ? secondary
                                         : primary;
      color.setAlpha(mode_ == "daylight" ? (state == 0   ? 180
                                            : state == 1 ? 140
                                                         : 255)
                     : state == 0        ? 255
                     : state == 3        ? 160
                     : state == 1        ? 70
                                         : 45);
      auto a = project(c["lon"].toDouble() - 3, c["lat"].toDouble() + 3);
      p->fillRect(
          QRectF(a, QSizeF(rect.width() / 60 + .4, rect.height() / 30 + .4)),
          color);
    }
    p->setPen(QPen(secondary, 1));
    for (const auto &poly : land()) {
      QPolygonF polygon;
      for (const auto &v : poly.toArray()) {
        auto pair = v.toArray();
        polygon << project(pair[0].toDouble(), pair[1].toDouble());
      }
      p->drawPolyline(polygon);
    }
    p->setPen(QPen(border, 1));
    for (int lat = -60; lat <= 60; lat += 30)
      p->drawLine(project(-180, lat), project(180, lat));
    for (int lon = -120; lon <= 120; lon += 60)
      p->drawLine(project(lon, -90), project(lon, 90));
    auto pos = project(settings["longitude"].toDouble(),
                       settings["latitude"].toDouble());
    p->setPen(QPen(text, 2));
    p->setBrush(primary);
    p->drawEllipse(pos, 5, 5);
    return;
  }
  if (mode_ == "sky") {
    const QPointF center(width() / 2, height() / 2);
    const double radius = std::min(width() / 2 - 45, height() / 2 - 28);
    p->setPen(QPen(border, 1));
    p->setBrush(surface);
    p->drawEllipse(center, radius, radius);
    p->setBrush(Qt::NoBrush);
    p->drawEllipse(center, radius * 2 / 3, radius * 2 / 3);
    p->drawEllipse(center, radius / 3, radius / 3);
    auto point = [&](double alt, double az) {
      double r = (90 - std::max(0.0, alt)) / 90 * radius;
      double a = az * M_PI / 180;
      return center + QPointF(r * std::sin(a), -r * std::cos(a));
    };
    QPainterPath path;
    bool first = true;
    for (const auto &v : samples) {
      auto s = v.toMap();
      if (s["altitude"].toDouble() < 0) {
        first = true;
        continue;
      }
      auto pt = point(s["altitude"].toDouble(), s["azimuth"].toDouble());
      if (first)
        path.moveTo(pt);
      else
        path.lineTo(pt);
      first = false;
    }
    p->setPen(QPen(accent, 2));
    p->drawPath(path);
    if (instant["altitude"].toDouble() >= 0) {
      auto pos =
          point(instant["altitude"].toDouble(), instant["azimuth"].toDouble());
      p->setBrush(accent);
      p->drawEllipse(pos, 5, 5);
    }
    p->setPen(secondary);
    p->drawText(QRectF(center.x() - 40, center.y() - radius - 25, 80, 20),
                Qt::AlignCenter, tr("North"));
    p->drawText(QRectF(center.x() - 40, center.y() + radius + 3, 80, 20),
                Qt::AlignCenter, tr("South"));
    p->drawText(QRectF(center.x() - radius - 36, center.y() - 10, 30, 20),
                Qt::AlignCenter, tr("West"));
    p->drawText(QRectF(center.x() + radius + 6, center.y() - 10, 30, 20),
                Qt::AlignCenter, tr("East"));
    return;
  }
  auto x = [&](double fraction) {
    return rect.left() + fraction * rect.width();
  };
  auto y = [&](double altitude) {
    return rect.bottom() - (altitude + 30) / 120 * rect.height();
  };
  // Darkness and eligible intervals share the exact same time axis.
  bool dark = false;
  double darkStart = 0;
  for (int i = 0; i < samples.size(); ++i) {
    auto s = samples[i].toMap();
    bool now = s["sun"].toDouble() <= settings["sunLimit"].toDouble();
    double f = s["fraction"].toDouble();
    if (now && !dark)
      darkStart = f;
    if ((!now && dark) || (dark && i == samples.size() - 1))
      p->fillRect(
          QRectF(x(darkStart), rect.top(), x(f) - x(darkStart), rect.height()),
          surface);
    dark = now;
  }
  for (const auto &v : controller_->result()["slots"].toList()) {
    auto slot = v.toMap();
    QColor band = primary;
    band.setAlpha(60);
    p->fillRect(QRectF(x(slot["startFraction"].toDouble()), rect.top(),
                       x(slot["endFraction"].toDouble()) -
                           x(slot["startFraction"].toDouble()),
                       rect.height()),
                band);
  }
  for (int a = -30; a <= 90; a += 30) {
    p->setPen(QPen(border, 1));
    p->drawLine(QPointF(rect.left(), y(a)), QPointF(rect.right(), y(a)));
    p->setPen(secondary);
    p->drawText(QRectF(0, y(a) - 10, 40, 20), Qt::AlignRight | Qt::AlignVCenter,
                QString::number(a) + QChar(0xb0));
  }
  for (int i = 0; i <= 4 && !samples.isEmpty(); ++i) {
    auto s = samples[qRound((samples.size() - 1) * i / 4.0)].toMap();
    p->drawText(QRectF(x(i / 4.0) - (i == 0   ? 0
                                     : i == 4 ? 48
                                              : 24),
                       rect.bottom() + 8, 48, 20),
                i == 0   ? Qt::AlignLeft
                : i == 4 ? Qt::AlignRight
                         : Qt::AlignCenter,
                s["time"].toString());
  }
  p->save();
  p->setClipRect(rect);
  QPainterPath path;
  bool first = true;
  for (const auto &v : samples) {
    auto s = v.toMap();
    QPointF pt(x(s["fraction"].toDouble()), y(s["altitude"].toDouble()));
    if (first)
      path.moveTo(pt);
    else
      path.lineTo(pt);
    first = false;
  }
  p->setPen(QPen(accent, 2));
  p->drawPath(path);
  p->setPen(QPen(secondary, 1, Qt::DashLine));
  p->drawLine(QPointF(rect.left(), y(settings["minimum"].toDouble())),
              QPointF(rect.right(), y(settings["minimum"].toDouble())));
  const double cursor = x(instant["fraction"].toDouble());
  p->setPen(QPen(secondary, 1));
  p->drawLine(QPointF(cursor, rect.top()), QPointF(cursor, rect.bottom()));
  p->setBrush(accent);
  p->drawEllipse(QPointF(cursor, y(instant["altitude"].toDouble())), 5, 5);
  p->restore();
}
void ObservationPlot::choose(QPointF pos) {
  if (mode_ == "world" || mode_ == "daylight") {
    auto r = worldFrame(width(), height());
    if (r.contains(pos))
      emit locationChosen(90 - (pos.y() - r.top()) / r.height() * 180,
                          (pos.x() - r.left()) / r.width() * 360 - 180);
  } else if (mode_ == "night" || mode_ == "clouds") {
    auto r = frame(width(), height());
    emit fractionChosen(std::clamp((pos.x() - r.left()) / r.width(), 0.0, 1.0));
  }
}
void ObservationPlot::mousePressEvent(QMouseEvent *e) {
  choose(e->position());
  e->accept();
}
void ObservationPlot::mouseMoveEvent(QMouseEvent *e) {
  if ((mode_ == "night" || mode_ == "clouds") &&
      (e->buttons() & Qt::LeftButton))
    choose(e->position());
  e->accept();
}
} // namespace astro::app
