#pragma once
#include "DeepSkySettings.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <array>
#include <vector>

namespace astro::app {
// Two independent parameter histories. No full-resolution pixels are retained.
class DeepSkyEdits : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString version READ version NOTIFY valuesChanged)
public:
    explicit DeepSkyEdits(QObject* parent=nullptr):QObject(parent){}
    QVariantMap values() const{return states_[active_].values;}
    QString version() const{return active_==0?"A":"B";}
    bool canUndo() const{const auto& s=states_[active_];return !s.undo.empty()||(s.gesture&&*s.gesture!=s.values);}
    bool canRedo() const{const auto& s=states_[active_];return !s.redo.empty()&&(!s.gesture||*s.gesture==s.values);}
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE bool applyValues(const QVariantMap&);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool resetSession(const QVariantMap&);
    Q_INVOKABLE void selectVersion(const QString&);
    Q_INVOKABLE void copyToOther();
    Q_INVOKABLE QVariantMap snapshot() const;
    Q_INVOKABLE bool restoreSession(const QVariantMap&);
signals:
    void valuesChanged();
    void historyChanged();
private:
    struct State {
        QVariantMap values{defaultDeepSkySettings()};
        std::vector<QVariantMap> undo,redo;
        std::optional<QVariantMap> gesture;
    };
    void push(State&,const QVariantMap&);
    void notify(){emit valuesChanged();emit historyChanged();}
    std::array<State,2> states_;
    int active_{};
};
}
