#pragma once
#include <QPointer>
#include <QQuickPaintedItem>

#include "ProcessingController.hpp"
namespace astro::app {
class ProcessingImageItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::ProcessingController* controller READ controller WRITE setController NOTIFY
                   controllerChanged)
    Q_PROPERTY(bool showBefore READ showBefore WRITE setShowBefore NOTIFY comparisonChanged)
    Q_PROPERTY(bool compare READ compare WRITE setCompare NOTIFY comparisonChanged)
    Q_PROPERTY(double split READ split WRITE setSplit NOTIFY comparisonChanged)
public:
    bool showBefore() const { return showBefore_; }
    bool compare() const { return compare_; }
    double split() const { return split_; }
    void setShowBefore(bool value) { if (showBefore_ != value) { showBefore_ = value; emit comparisonChanged(); update(); } }
    void setCompare(bool value) { if (compare_ != value) { compare_ = value; emit comparisonChanged(); update(); } }
    void setSplit(double value);
    explicit ProcessingImageItem(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {}
    ProcessingController* controller() const { return controller_; }
    void setController(ProcessingController* controller);
    void paint(QPainter* painter) override;
signals:
    void controllerChanged();
    void comparisonChanged();

private:
    QPointer<ProcessingController> controller_;
    QImage before_, annotation_;
    bool showBefore_{}, compare_{}, showingResult_{};
    double split_{0.5};
    QImage image_;  // GUI-owned snapshot; GUI is blocked during QQuickPaintedItem::paint.
};
}  // namespace astro::app
