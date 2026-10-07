#include "PreviewController.hpp"
#include "Check.hpp"
#include "FileNaming.hpp"
#include <astro/recording/AstroImage.hpp>
#include <QDir>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>

template <typename Predicate>
bool waitFor(Predicate done) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    return done();
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    ASTRO_TEST_CHECK(temporary.isValid());
    const auto firstPath = astro::app::reserveCapturePath(temporary.path(), "same-second", "ser");
    ASTRO_TEST_CHECK(!firstPath.isEmpty());
    QFile existing(firstPath);
    ASTRO_TEST_CHECK(existing.open(QIODevice::WriteOnly));
    ASTRO_TEST_CHECK(existing.write("preserve") == 8);
    existing.close();
    const auto secondPath = astro::app::reserveCapturePath(temporary.path(), "same-second", "ser");
    ASTRO_TEST_CHECK(!secondPath.isEmpty() && secondPath != firstPath);
    ASTRO_TEST_CHECK(existing.open(QIODevice::ReadOnly));
    ASTRO_TEST_CHECK(existing.readAll() == "preserve");
    existing.close();
    QCoreApplication::setOrganizationName("AstroCaptureTests");
    QCoreApplication::setApplicationName("RecordingLifecycle");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());

    astro::app::PreviewController controller;
    controller.setSaveDirectory(temporary.path());
    controller.connectMock();
    ASTRO_TEST_CHECK(controller.connected());
    ASTRO_TEST_CHECK(waitFor([&] { return controller.frameCount() >= 3; }));
    ASTRO_TEST_CHECK(!controller.previewImage().isNull());

    controller.setRecordingTargetFrameCount(5);
    controller.startRecording();
    ASTRO_TEST_CHECK(controller.recording());
    ASTRO_TEST_CHECK(waitFor([&] { return !controller.recording(); }));
    ASTRO_TEST_CHECK(controller.recordingFrameCount() == 5);
    ASTRO_TEST_CHECK(controller.recordingDroppedFrames() == 0);
    QFile output(controller.recordingPath());
    ASTRO_TEST_CHECK(output.open(QIODevice::ReadOnly));
    const auto header = output.read(178);
    ASTRO_TEST_CHECK(header.size() == 178);
    ASTRO_TEST_CHECK(static_cast<unsigned char>(header[38]) == 5);
    ASTRO_TEST_CHECK(controller.recordingStatus().startsWith("Saved "));
    output.close();

    // Keep acquisition running while the recorder stops and drains.
    controller.setRecordingTargetFrameCount(0);
    controller.setTargetName("manual-stop");
    controller.startRecording();
    // A delayed completion from the previous recording must not stop
    // this newly started session.
    controller.recordingTargetReached(1);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(controller.recording());
    ASTRO_TEST_CHECK(waitFor([&] { return controller.recordingFrameCount() >= 3; }));
    controller.stopRecording();
    ASTRO_TEST_CHECK(!controller.recording());
    ASTRO_TEST_CHECK(controller.recordingDroppedFrames() == 0);
    const auto acquired = controller.frameCount();
    ASTRO_TEST_CHECK(waitFor([&] { return controller.frameCount() > acquired; }));

    controller.setTargetName("duration-stop");
    controller.setRecordingTargetDurationSeconds(0.15);
    controller.startRecording();
    ASTRO_TEST_CHECK(controller.recording());
    ASTRO_TEST_CHECK(waitFor([&] { return !controller.recording(); }));
    ASTRO_TEST_CHECK(controller.recordingFrameCount() > 0);
    ASTRO_TEST_CHECK(controller.recordingDroppedFrames() == 0);

    controller.setRoi(0, 0, 320, 240);
    ASTRO_TEST_CHECK(waitFor([&] { return controller.previewImage().size() == QSize(320, 240); }));
    controller.setVideoFormat("FITS");
    controller.setCaptureFilter("HA");controller.setCaptureType("Light");
    controller.setRecordingTargetFrameCount(3);controller.setRecordingTargetDurationSeconds(0);
    const auto exposure=controller.exposureMicroseconds();
    controller.startRecording();ASTRO_TEST_CHECK(controller.recording());
    controller.setExposureMicroseconds(exposure*2);ASTRO_TEST_CHECK(controller.exposureMicroseconds()==exposure);
    ASTRO_TEST_CHECK(waitFor([&]{return !controller.recording();}));
    const auto files=QDir(controller.recordingPath()).entryList({"*.fits"},QDir::Files);
    ASTRO_TEST_CHECK(files.size()==3&&controller.recordingFrameCount()==3);
    const auto fits=astro::recording::readAstroImage(QDir(controller.recordingPath()).filePath(files.front()).toStdString());
    ASTRO_TEST_CHECK(fits.filter=="HA"&&fits.imageType=="Light"&&fits.exposure==exposure/1000000.0&&fits.bayer!=astro::domain::BayerPattern::None);
    controller.disconnectCamera();
    ASTRO_TEST_CHECK(!controller.connected());
    ASTRO_TEST_CHECK(controller.previewImage().isNull());
    std::cout << "OK\n";
}
