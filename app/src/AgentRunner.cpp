#include "AgentRunner.hpp"
#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include <QGuiApplication>
#include <QDirIterator>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <csignal>
#include <cstdio>
#include <stdexcept>

namespace astro::app {
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int signal) { interrupted = signal; }
struct Failure : std::runtime_error {
    QString code;
    Failure(QString c, QString message): std::runtime_error(message.toStdString()), code(std::move(c)) {}
};
void require(bool ok, const QString& message) { if (!ok) throw Failure("invalid_input", message); }
void emitJson(QJsonObject event) {
    event["schemaVersion"] = 1;
    const auto line = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, std::size_t(line.size()), stdout);
    std::fflush(stdout);
}
void saveJson(const QString& path, const QJsonObject& value) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(value).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw Failure("write_failed", "Cannot save " + path);
}
void keys(const QJsonObject& object, const QStringList& allowed) {
    for (auto it = object.begin(); it != object.end(); ++it)
        require(allowed.contains(it.key()), "Unknown recipe field: " + it.key());
}
QString pathFrom(const QJsonValue& value, const QDir& base) {
    require(value.isString() && !value.toString().trimmed().isEmpty(), "A nonempty path is required.");
    return QDir::cleanPath(base.absoluteFilePath(value.toString()));
}
QList<QUrl> imageFiles(const QString& path) {
    const QStringList formats{"fit","fits","fts","fz","tif","tiff","png","cr2","cr3","nef","nrw","arw","dng","orf","raf","rw2","pef"};
    const QFileInfo info(path);
    require(info.exists(), "Input does not exist: " + path);
    QStringList paths;
    if (info.isDir()) {
        QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (interrupted) throw Failure("cancelled", "Cancelled.");
            const auto candidate = it.next();
            if (formats.contains(QFileInfo(candidate).suffix().toLower())) paths << candidate;
            require(paths.size() <= 2000, "Projects are limited to 2000 images.");
        }
    } else {
        require(info.isFile() && formats.contains(info.suffix().toLower()), "Unsupported image: " + path);
        paths << path;
    }
    paths.sort();
    require(!paths.isEmpty(), "No supported images in " + path);
    QList<QUrl> urls;
    for (const auto& file : paths) urls << QUrl::fromLocalFile(QFileInfo(file).canonicalFilePath());
    return urls;
}
// All work still goes through the desktop controller and its validated engine.
// Observe completion signals, never infer success from translated status text.
void perform(DeepSkyController& controller, const QString& stage, const std::function<void()>& action) {
    if (interrupted) throw Failure("cancelled", "Cancelled.");
    QEventLoop loop;
    bool finished = false, success = false;
    QString message;
    const auto completion = QObject::connect(&controller, &DeepSkyController::operationFinished, &loop,
        [&](bool ok, const QString& text) { finished = true; success = ok; message = text; loop.quit(); });
    int lastProgress = -1;
    const auto progress = QObject::connect(&controller, &DeepSkyController::changed, &loop, [&] {
        if (controller.progress() != lastProgress) {
            lastProgress = controller.progress();
            emitJson({{"event","progress"},{"stage",stage},{"percent",lastProgress}});
        }
    });
    QTimer cancellation;
    QObject::connect(&cancellation, &QTimer::timeout, &loop, [&] { if (interrupted) controller.cancel(); });
    cancellation.start(100);
    emitJson({{"event","started"},{"stage",stage}});
    action();
    if (!finished && controller.busy()) loop.exec();
    QObject::disconnect(completion);
    QObject::disconnect(progress);
    if (interrupted) throw Failure("cancelled", "Cancelled. Completed outputs are preserved.");
    if (!finished || !success) throw Failure("operation_failed", message.isEmpty() ? controller.status() : message);
    emitJson({{"event","completed"},{"stage",stage}});
}
struct Input { QList<QUrl> files; QString role, group; };
void import(DeepSkyController& controller, const QList<Input>& inputs) {
    for (const auto& input : inputs) {
        const auto before = controller.frames().size();
        perform(controller, "import", [&] { controller.addFiles(input.files, input.role, input.group); });
        if (controller.frames().size() - before != input.files.size())
            throw Failure("import_failed", "Some images were not imported: " + controller.status());
    }
}
}
int runAgent(int argc, char** argv) {
    // This process needs QImage/font services for existing previews and exports,
    // but no display server, windows, QML engine, camera, or desktop preferences.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setOrganizationName("AstroCapture");
    app.setApplicationName("AstroCapture Agent");
    app.setApplicationVersion(QStringLiteral(ASTRO_APP_VERSION));
    std::signal(SIGINT, interrupt);
    std::signal(SIGTERM, interrupt);
    const auto args = app.arguments();
    QString output;
    QJsonObject report{{"schemaVersion",1},{"applicationVersion",app.applicationVersion()},{"state","running"}};
    QElapsedTimer elapsed;
    elapsed.start();
    try {
        require(args.size() >= 3 && args[1] == "--agent", "Usage: AstroCapture --agent help|inspect <folder>|run <recipe.json>");
        const auto command = args[2];
        if (command == "help" && args.size() == 3) {
            emitJson({{"event","help"},{"commands",QJsonArray{"inspect <folder-or-image>","run <recipe.json>"}},
                      {"recipeVersion",1},{"scope","deep-sky"},{"documentation","AGENT_MODE.md"},
                      {"exampleRecipe",QJsonObject{{"version",1},{"calibrated",true},{"output","new-results"},
                          {"inputs",QJsonArray{QJsonObject{{"path","images"},{"role","Light"}}}}}},
                      {"exitCodes",QJsonObject{{"success",0},{"invalidInput",2},{"failed",1},{"cancelled",130}}}});
            return 0;
        }
        require(args.size() == 4 && (command == "inspect" || command == "run"), "Usage: AstroCapture --agent help|inspect <folder>|run <recipe.json>");
        QTemporaryDir projectCache;
        DeepSkyController controller;
        controller.setProjectCacheDirectory(projectCache.path());
        if (command == "inspect") {
            if (QFileInfo(args[3]).suffix() == "astroproject") {
                require(projectCache.isValid(), "Cannot create temporary project cache.");
                perform(controller, "load_project", [&] { controller.loadProject(QUrl::fromLocalFile(QFileInfo(args[3]).absoluteFilePath())); });
                emitJson({{"event","project"},{"groups",QJsonArray::fromStringList(controller.groups())},
                          {"hasResult",controller.hasResult()},{"width",controller.imageWidth()},{"height",controller.imageHeight()},
                          {"finish",QJsonObject::fromVariantMap(controller.finishSettings())},
                          {"integration",QJsonArray::fromVariantList(controller.integrationReport())}});
                return 0;
            }
            import(controller, {{imageFiles(QFileInfo(args[3]).absoluteFilePath()), "Auto", "Auto"}});
            emitJson({{"event","inspection"},{"frames",QJsonArray::fromVariantList(controller.frames())},
                      {"warning","Roles and groups are suggestions. Explicit roles and calibration state are required for run."}});
            return 0;
        }
        const QFileInfo recipeInfo(args[3]);
        QFile file(recipeInfo.absoluteFilePath());
        require(file.open(QIODevice::ReadOnly) && file.size() <= 1024*1024, "Cannot read recipe (maximum 1 MiB).");
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        require(parse.error == QJsonParseError::NoError && document.isObject(), "Invalid JSON recipe.");
        const auto recipe = document.object();
        keys(recipe, {"version","inputs","output","calibrated","sigma","normalize","correctBadPixels","finish"});
        require(recipe["version"].isDouble() && recipe["version"].toDouble() == 1, "Recipe version must be 1.");
        require(recipe["calibrated"].isBool(), "calibrated must explicitly be true or false.");
        for (const auto& key : {"normalize","correctBadPixels"})
            require(!recipe.contains(key) || recipe[key].isBool(), QString(key) + " must be a boolean.");
        const auto sigma = recipe.value("sigma").toDouble(3);
        require(!recipe.contains("sigma") || (recipe["sigma"].isDouble() && sigma >= 1 && sigma <= 10), "sigma must be between 1 and 10.");
        const auto finish = recipe.value("finish");
        require(finish.isUndefined() || finish.isObject(), "finish must be an object.");
        // A small, typed surface deliberately excludes external masks/catalogue
        // references and operation graphs until their dependencies are supported.
        keys(finish.toObject(), {"stretch","black","white","midtones","red","green","blue","denoise","saturation","background"});
        auto checked = defaultDeepSkySettings();
        const auto overrides = finish.toObject();
        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            const bool boolean = it.key() == "stretch" || it.key() == "background";
            require(boolean ? it.value().isBool() : it.value().isDouble(), "Invalid finish type: " + it.key());
            checked[it.key()] = it.value().toVariant();
        }
        require(checkedDeepSkySettings(checked).has_value(), "Invalid finish values.");
        require(recipe["inputs"].isArray() && !recipe["inputs"].toArray().isEmpty(), "inputs must be a nonempty array.");
        QList<Input> inputs;
        QSet<QString> seen;
        bool lights = false, calibration = false;
        for (const auto& value : recipe["inputs"].toArray()) {
            require(value.isObject(), "Each input must be an object.");
            const auto input = value.toObject();
            keys(input, {"path","role","group"});
            const auto role = input["role"].toString();
            require(QStringList{"Light","Dark","Flat","Bias","DarkFlat","BadPixels"}.contains(role), "An explicit supported input role is required.");
            require(!input.contains("group") || (input["group"].isString() && !input["group"].toString().trimmed().isEmpty()), "group must be a nonempty string.");
            const auto urls = imageFiles(pathFrom(input["path"], recipeInfo.dir()));
            for (const auto& url : urls) {
                require(!seen.contains(url.toLocalFile()), "Duplicate input: " + url.toLocalFile());
                seen.insert(url.toLocalFile());
                require(seen.size() <= 2000, "Projects are limited to 2000 images.");
            }
            inputs << Input{urls, role, input.value("group").toString("Auto")};
            lights |= role == "Light";
            calibration |= role != "Light";
        }
        require(lights, "At least one Light input is required.");
        require(!recipe["calibrated"].toBool() || !calibration, "Already calibrated lights cannot be combined with calibration inputs.");
        const auto destination = pathFrom(recipe["output"], recipeInfo.dir());
        // mkdir is exclusive: never reuse or erase an earlier job or its files.
        require(QFileInfo(destination).dir().exists(), "The output parent directory must exist.");
        require(QDir().mkdir(destination), "Output must be a new directory: " + destination);
        output = QFileInfo(destination).canonicalFilePath();
        report["recipe"] = recipe;
        report["output"] = output;
        saveJson(output + "/report.json", report);
        import(controller, inputs);
        report["frames"] = QJsonArray::fromVariantList(controller.frames());
        const bool calibrated = recipe["calibrated"].toBool();
        controller.setProjectOptions({{"calibrated",calibrated},{"sigma",sigma},
            {"normalize",recipe.value("normalize").toBool(true)},
            {"badPixels",recipe.value("correctBadPixels").toBool(true)}});
        report["resources"] = QJsonObject::fromVariantMap(controller.integrationResources({{"calibrated",calibrated}}));
        saveJson(output + "/report.json", report);
        perform(controller, "integrate", [&] { controller.integrate(calibrated, sigma, recipe.value("normalize").toBool(true), recipe.value("correctBadPixels").toBool(true)); });
        report["integration"] = QJsonArray::fromVariantList(controller.integrationReport());
        perform(controller, "checkpoint", [&] { controller.savePortableProject(QUrl::fromLocalFile(output + "/stacked.astroproject")); });
        report["checkpoint"] = output + "/stacked.astroproject";
        saveJson(output + "/report.json", report);
        QJsonArray results;
        int index = 0;
        for (const auto& group : controller.groups()) {
            perform(controller, "select", [&] { controller.selectResult(group); });
            const auto directory = output + QString("/group-%1").arg(++index, 3, 10, QLatin1Char('0'));
            require(QDir().mkdir(directory), "Cannot create result directory.");
            auto settings = controller.finishSettings();
            for (auto it = overrides.begin(); it != overrides.end(); ++it) settings[it.key()] = it.value().toVariant();
            require(checkedDeepSkySettings(settings).has_value(), "Finish overrides conflict with the measured stretch.");
            perform(controller, "finish", [&] { controller.apply(settings); });
            for (const auto& name : {"linear.fits","finished.tiff","preview.jpg"}) {
                perform(controller, "export", [&] { controller.exportImage(QUrl::fromLocalFile(directory + "/" + name), QString(name) == "linear.fits"); });
            }
            auto projectOptions = controller.projectOptions();
            projectOptions["finish"] = controller.finishSettings();
            controller.setProjectOptions(projectOptions);
            perform(controller, "project", [&] { controller.savePortableProject(QUrl::fromLocalFile(directory + "/result.astroproject")); });
            results.append(QJsonObject{{"group",group},{"directory",directory},{"finish",QJsonObject::fromVariantMap(controller.finishSettings())}});
            report["results"] = results;
            saveJson(output + "/report.json", report);
        }
        report["state"] = "completed";
        report["elapsedMs"] = double(elapsed.elapsed());
        saveJson(output + "/report.json", report);
        emitJson({{"event","result"},{"report",report}});
        return 0;
    } catch (const std::exception& error) {
        const auto* failure = dynamic_cast<const Failure*>(&error);
        const QString code = failure ? failure->code : QStringLiteral("internal_error");
        report["state"] = code == "cancelled" ? "cancelled" : "failed";
        report["error"] = QJsonObject{{"code",code},{"message",QString::fromUtf8(error.what())}};
        report["elapsedMs"] = double(elapsed.elapsed());
        if (!output.isEmpty()) {
            try { saveJson(output + "/report.json", report); }
            catch (const std::exception& writeError) { report["reportWriteError"] = QString::fromUtf8(writeError.what()); }
        }
        emitJson({{"event","error"},{"report",report}});
        return code == "cancelled" ? 130 : code == "invalid_input" ? 2 : 1;
    }
}
}
