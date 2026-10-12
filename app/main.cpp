#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QTranslator>
#include "src/AppLibrary.hpp"
#include "src/QuitGuard.hpp"
#include "src/WindowAppearance.hpp"
#include <QColor>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral(ASTRO_APP_VERSION));
    // QSettings needs these to resolve a native storage location
    // (~/Library/Preferences/... on macOS) before any Settings use.
    QCoreApplication::setOrganizationName("AstroCapture");
#ifdef ASTRO_ENABLE_DEVTOOLS
    // Keep UI validation and development preferences out of the installed app.
    QCoreApplication::setApplicationName("AstroCapture Development");
#else
    QCoreApplication::setApplicationName("AstroCapture");
#endif
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/AstroCapture/qml/assets/logo.png")));

    QTranslator translator;
    auto applyLanguage = [&](const QString& language) {
        app.removeTranslator(&translator);
        if (language == "fr" && translator.load(":/i18n/astrocapture_fr.qm"))
            app.installTranslator(&translator);
    };
    applyLanguage(QSettings().value("application/language", "en").toString());
    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("AstroCapture", "Main");

    if (!engine.rootObjects().isEmpty()) {
        auto* root = engine.rootObjects().first();
        astro::app::applyWindowAppearance(qobject_cast<QWindow*>(root), root->property("color").value<QColor>());
    }

    astro::app::QuitGuard quitGuard(engine.rootObjects().isEmpty()?nullptr:qobject_cast<QWindow*>(engine.rootObjects().first()));
    app.installEventFilter(&quitGuard);

    if (!engine.rootObjects().isEmpty()) {
        auto* library = engine.rootObjects().first()->findChild<astro::app::AppLibrary*>("appLibrary");
        if (library) QObject::connect(library, &astro::app::AppLibrary::languageChanged, &engine, [&, library]() {
            applyLanguage(library->language());
            engine.retranslate();
        });
    }
    return app.exec();
}
