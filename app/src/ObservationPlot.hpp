#pragma once
#include "ObservationController.hpp"
#include <QQuickPaintedItem>
namespace astro::app {
class ObservationPlot : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(ObservationController *controller READ controller WRITE
                 setController NOTIFY controllerChanged)
  Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
  Q_PROPERTY(QVariantMap colors MEMBER colors_ NOTIFY colorsChanged)
public:
  explicit ObservationPlot(QQuickItem *parent = nullptr);
  ObservationController *controller() const { return controller_; }
  void setController(ObservationController *);
  QString mode() const { return mode_; }
  void setMode(const QString &value) {
    mode_ = value;
    emit modeChanged();
    update();
  }
  void paint(QPainter *) override;
signals:
  void controllerChanged();
  void modeChanged();
  void colorsChanged();
  void fractionChosen(double fraction);
  void locationChosen(double latitude, double longitude);

protected:
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;

private:
  void choose(QPointF);
  ObservationController *controller_{};
  QString mode_{"night"};
  QVariantMap colors_;
};
} // namespace astro::app
