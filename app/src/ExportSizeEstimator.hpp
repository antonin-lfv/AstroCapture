#pragma once
#include "ImageDocument.hpp"
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <map>
#include <thread>

namespace astro::app {
// Estimates compressed exports from distributed, native-resolution tiles.
// Small images use the complete image and the same encoder as export.
class ExportSizeEstimator : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariant input READ input WRITE setInput NOTIFY changed)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(double bytes READ bytes NOTIFY changed)
    Q_PROPERTY(bool exact READ exact NOTIFY changed)
    Q_PROPERTY(bool calculating READ calculating NOTIFY changed)
public:
    explicit ExportSizeEstimator(QObject* parent=nullptr);
    ~ExportSizeEstimator() override;
    QVariant input() const { return QVariant::fromValue(input_); }
    void setInput(const QVariant&);
    QString format() const { return format_; }
    void setFormat(QString);
    bool active() const { return active_; }
    void setActive(bool);
    double bytes() const { return bytes_; }
    bool exact() const { return exact_; }
    bool calculating() const { return calculating_; }
signals:
    void changed();
private:
    void invalidate();
    void update();
    struct Estimate { double bytes; bool exact; };
    ExportSizeInput input_;
    QString format_="png";
    std::map<QString,Estimate> cache_;
    QTimer timer_;
    double bytes_=-1;
    bool active_{},exact_{},calculating_{},running_{};
    std::uint64_t revision_{};
    std::jthread worker_;
};
}
