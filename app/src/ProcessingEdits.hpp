#pragma once
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <optional>
#include <vector>
#include "ProcessingSettings.hpp"

namespace astro::app {
// Stores parameters only, never copies full-resolution images into undo history.
class ProcessingEdits : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
public:
    explicit ProcessingEdits(QObject* parent = nullptr) : QObject(parent) {}
    QVariantMap values() const { return values_; }
    bool canUndo() const { return !undo_.empty() || (gesture_ && *gesture_ != values_); }
    bool canRedo() const { return !redo_.empty() && (!gesture_ || *gesture_ == values_); }
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE void setValue(const QString& key, double value);
    Q_INVOKABLE bool applyValues(const QVariantMap& values);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void resetSession();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
signals:
    void valuesChanged();
    void historyChanged();
private:
    void pushUndo(const QVariantMap& values);
    QVariantMap values_{defaultProcessingSettings()};
    std::optional<QVariantMap> gesture_;
    std::vector<QVariantMap> undo_, redo_;
};
}
