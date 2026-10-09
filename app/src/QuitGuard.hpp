#pragma once
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWindow>

namespace astro::app {
// Native Quit (including Cmd+Q) must take the same path as closing the window.
class QuitGuard final : public QObject {
public:
    explicit QuitGuard(QWindow* window, QObject* parent=nullptr)
        : QObject(parent), window_(window) {}
protected:
    bool eventFilter(QObject*, QEvent* event) override {
        if(event->type()!=QEvent::Quit || !window_ || !window_->isVisible())return false;
        window_->close();
        return true;
    }
private:
    QPointer<QWindow> window_;
};
}
