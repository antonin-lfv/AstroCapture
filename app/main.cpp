#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    // QSettings needs these to resolve a native storage location
    // (~/Library/Preferences/... on macOS) before any Settings use.
    QCoreApplication::setOrganizationName("AstroCapture");
    QCoreApplication::setApplicationName("AstroCapture");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("AstroCapture", "Main");

    return app.exec();
}
