#pragma once

#include <QCoreApplication>
#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>
#include <memory>
#include "ProcessingController.hpp"

namespace astro::app {
// Checks the deployed application, including the actual QML and codec plugins.
// It uses its own application identity so recovery and user preferences stay separate.
inline void scheduleInstallationCheck(QGuiApplication &app, QQuickWindow *window) {
    QTimer::singleShot(0, &app, [&app, window] {
        const auto readers = QImageReader::supportedImageFormats();
        const bool complete = window && readers.contains("png") &&
            (readers.contains("jpg") || readers.contains("jpeg")) &&
            (readers.contains("tif") || readers.contains("tiff"));
        QJsonArray formats;
        for (const auto &format : readers) formats.append(QString::fromLatin1(format));
        QJsonArray cameras;
#ifdef ASTRO_ENABLE_MOCK_CAMERA
        cameras.append("mock");
#endif
#ifdef ASTRO_ENABLE_DRIVER_UVC
        cameras.append("uvc");
#endif
#ifdef ASTRO_ENABLE_DRIVER_EUVC
        cameras.append("euvc");
#endif
#ifdef ASTRO_ENABLE_DRIVER_ZWO
        cameras.append("zwo");
#endif
        QJsonObject result{{"ok", complete}, {"version", QCoreApplication::applicationVersion()},
            {"qt", QString::fromLatin1(qVersion())}, {"platform", QGuiApplication::platformName()},
            {"imageFormats", formats}, {"cameraDrivers", cameras}};
        auto finish = [&app, result](bool ok) mutable {
            result["ok"] = ok;
            std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
            app.exit(ok ? 0 : 2);
        };
        const auto args = app.arguments();
        const auto imageAt = args.indexOf("--check-installation-image");
        if (!complete || imageAt < 0) { finish(complete); return; }
        const auto outputAt = args.indexOf("--check-installation-output");
        if (imageAt + 1 >= args.size() || outputAt < 0 || outputAt + 1 >= args.size()) {
            finish(false); return;
        }
        auto *processing = window->findChild<ProcessingController*>("processingController");
        const auto directory = args[outputAt + 1];
        if (!processing || !QDir().mkpath(directory)) { finish(false); return; }
        processing->open(QUrl::fromLocalFile(QFileInfo(args[imageAt + 1]).absoluteFilePath()));
        window->setProperty("currentTab", "processing");
        auto *timer = new QTimer(&app);
        auto frame = std::make_shared<int>(-1);
        auto elapsed = std::make_shared<QElapsedTimer>();
        elapsed->start();
        QObject::connect(timer, &QTimer::timeout, &app, [=]() mutable {
            if (elapsed->elapsed() > 60000) { timer->stop(); finish(false); return; }
            if (processing->busy()) return;
            if (!processing->stillImage()) { timer->stop(); finish(false); return; }
            const QSize sizes[] = {{1440, 960}, {1100, 800}, {900, 700}};
            if (*frame >= 0) {
                const auto size = sizes[*frame % 3];
                const auto tab = *frame < 3 ? "processing" : "observation";
                const auto path = QDir(directory).filePath(QString("%1-%2x%3.png").arg(tab).arg(size.width()).arg(size.height()));
                const auto image = window->grabWindow();
                if (image.isNull() || !image.save(path)) { timer->stop(); finish(false); return; }
            }
            ++*frame;
            if (*frame == 6) { timer->stop(); finish(true); return; }
            window->setProperty("currentTab", *frame < 3 ? "processing" : "observation");
            window->resize(sizes[*frame % 3]);
        });
        timer->start(750);
    });
}
} // namespace astro::app
