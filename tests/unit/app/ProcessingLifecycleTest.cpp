#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <astro/recording/SerRecorder.hpp>
#include <astro/recording/RawSequenceRecorder.hpp>
#include <cmath>

#include "Check.hpp"
#include "ProcessingController.hpp"
using namespace astro;
void wait(app::ProcessingController &controller) {
  QElapsedTimer timer;
  timer.start();
  while (controller.busy() && timer.elapsed() < 15000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  ASTRO_TEST_CHECK(!controller.busy());
}
int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  QTemporaryDir directory;
  ASTRO_TEST_CHECK(directory.isValid());
  const QString path = directory.filePath("test.ser");
  domain::FrameDescriptor descriptor;
  descriptor.width = 96;
  descriptor.height = 64;
  descriptor.storage = domain::PixelStorage::Mono8;
  descriptor.validBits = 8;
  recording::SerRecorder recorder;
  ASTRO_TEST_CHECK(recorder.open(path.toStdString(), descriptor, 20));
  for (int n = 0; n < 12; ++n) {
    domain::FrameBuffer frame;
    frame.descriptor = descriptor;
    frame.metadata.sequence = n;
    std::vector<std::byte> bytes(96 * 64);
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 96; ++x)
        bytes[y * 96 + x] =
            std::byte(int(100 + 50 * std::sin((x - n % 3) * .16) *
                                    std::cos((y - n % 2) * .21)));
    frame.storage =
        std::make_shared<const domain::BufferStorage>(std::move(bytes));
    ASTRO_TEST_CHECK(recorder.writeFrame(frame));
  }
  ASTRO_TEST_CHECK(recorder.close());
  app::ProcessingController controller;
  int notifiedFrameCount = 0;
  QObject::connect(&controller, &app::ProcessingController::changed,
                   [&] { notifiedFrameCount = controller.frameCount(); });
  QObject::connect(&controller, &app::ProcessingController::sourceOpened, [&] {
    ASTRO_TEST_CHECK(notifiedFrameCount == controller.frameCount());
  });
  controller.openPath(path);
  wait(controller);
  ASTRO_TEST_CHECK(controller.loaded() && controller.frameCount() == 12 &&
                   !controller.image().isNull());
  for(const auto* extension:{"fit","FITS","fts","fz"}) {
    controller.openPath(directory.filePath(QString("unsupported.")+extension));
    ASTRO_TEST_CHECK(!controller.busy()&&controller.loaded()&&controller.frameCount()==12);
    ASTRO_TEST_CHECK(controller.status().contains("Deep sky"));
  }
  const auto navigationStatus = controller.status();
  const auto navigationProgress = controller.progress();
  const auto navigationConnection = QObject::connect(
      &controller, &app::ProcessingController::changed, [&] {
        ASTRO_TEST_CHECK(controller.status() == navigationStatus);
        ASTRO_TEST_CHECK(controller.progress() == navigationProgress);
      });
  controller.seek(4);
  wait(controller);
  ASTRO_TEST_CHECK(controller.frameIndex() == 4);
  // Rapid scrubbing must keep the most recent position, even during a read.
  controller.seek(1);
  controller.seek(7);
  controller.seek(11);
  wait(controller);
  ASTRO_TEST_CHECK(controller.frameIndex() == 11 && !controller.seeking());
  QObject::disconnect(navigationConnection);
  controller.analyze();
  ASTRO_TEST_CHECK(controller.analyzing() && !controller.stacking());
  wait(controller);
  ASTRO_TEST_CHECK(controller.ranked() && !controller.analyzing());
  controller.stack(8);
  ASTRO_TEST_CHECK(controller.stacking() && !controller.analyzing());
  wait(controller);
  ASTRO_TEST_CHECK(controller.hasResult() && !controller.stacking());
  const auto stackSummary = controller.stackSummary();
  ASTRO_TEST_CHECK(stackSummary.contains("66.7% selected"));
  controller.apply({1, 1, 0, 0, 0, 0}, 0, 1, 1, 1, 1);
  wait(controller);
  ASTRO_TEST_CHECK(controller.status().contains("Adjustments applied"));
  ASTRO_TEST_CHECK(controller.stackSummary() == stackSummary);
  const auto firstAdjustment = controller.image();
  int applied = 0;
  QObject::connect(&controller, &app::ProcessingController::adjustmentsApplied,
                   [&] { ++applied; });
  controller.apply({5, 5, 5, 5, 5, 5}, 0, 1, 1, 1, 1);
  ASTRO_TEST_CHECK(controller.adjusting());
  controller.apply({0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 1);
  controller.apply({1, 1, 0, 0, 0, 0}, 0, 1, 1, 1, 1);
  wait(controller);
  ASTRO_TEST_CHECK(applied == 1 && controller.image() == firstAdjustment &&
                   !controller.adjusting());
  controller.seek(2);
  wait(controller);
  ASTRO_TEST_CHECK(controller.imageLabel().startsWith("Source"));
  controller.showResult(false);
  ASTRO_TEST_CHECK(controller.image() == firstAdjustment);
  controller.apply({5, 5, 5, 5, 5, 5}, 0, 1, 1, 1, 1);
  controller.apply({0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 1);
  controller.cancel();
  wait(controller);
  ASTRO_TEST_CHECK(applied == 1 && controller.image() == firstAdjustment);
  int exports = 0;
  QObject::connect(&controller, &app::ProcessingController::imageExported, [&](const QUrl& url) {
    ASTRO_TEST_CHECK(QFileInfo(url.toLocalFile()).size() > 10000);
    ++exports;
  });
  controller.exportImage(QUrl::fromLocalFile(directory.filePath("result.tif")));
  wait(controller);
  ASTRO_TEST_CHECK(exports == 1);
  controller.exportImage(QUrl::fromLocalFile(directory.filePath("missing-folder/result.tif")));
  wait(controller);
  ASTRO_TEST_CHECK(exports == 1);
  ASTRO_TEST_CHECK(QFileInfo(directory.filePath("result.tif")).size() > 10000);
  const auto result = controller.image();
  controller.openPath(directory.filePath("missing.ser"));
  wait(controller);
  ASTRO_TEST_CHECK(controller.hasResult() && controller.image() == result &&
                   controller.frameCount() == 12);
  ASTRO_TEST_CHECK(controller.stackSummary() == stackSummary);
  controller.analyze();
  controller.cancel();
  wait(controller);
  ASTRO_TEST_CHECK(controller.hasResult() && controller.ranked() &&
                   controller.status().contains("Cancelled") && !controller.analyzing());
  controller.stack(6);
  ASTRO_TEST_CHECK(controller.stacking());
  controller.cancel();
  wait(controller);
  ASTRO_TEST_CHECK(!controller.stacking() && controller.stackSummary() == stackSummary);
  controller.apply({1, 1, 0, 0, 0, 0}, 0, 1, 1, 1, 0);
  wait(controller);
  ASTRO_TEST_CHECK(controller.hasResult() &&
                   controller.status().contains("Invalid"));
  controller.openPath(path);
  wait(controller);
  ASTRO_TEST_CHECK(controller.stackSummary().isEmpty());
  {
    app::ProcessingController closing;
    closing.openPath(path);
  } // Shutdown while a worker has queued work.
  QCoreApplication::processEvents();
  const auto rawPath=directory.filePath("roundtrip.acraw");
  recording::RawSequenceRecorder raw;
  ASTRO_TEST_CHECK(raw.open(rawPath.toStdString(),descriptor,20));
  for(int n=0;n<3;++n){domain::FrameBuffer frame;frame.descriptor=descriptor;std::vector<std::byte> pixels(96*64);for(int y=0;y<64;++y)for(int x=0;x<96;++x)pixels[y*96+x]=std::byte(int(100+50*std::sin(x*.16)*std::cos(y*.21)));frame.storage=std::make_shared<const domain::BufferStorage>(std::move(pixels));ASTRO_TEST_CHECK(raw.writeFrame(frame));}
  ASTRO_TEST_CHECK(raw.close());controller.openPath(rawPath);wait(controller);
  ASTRO_TEST_CHECK(controller.loaded()&&controller.frameCount()==3);
  controller.seek(2);wait(controller);ASTRO_TEST_CHECK(controller.frameIndex()==2);
  controller.analyze();wait(controller);controller.stack(3);wait(controller);ASTRO_TEST_CHECK(controller.hasResult());

  app::ImageDocument deepSky;deepSky.pixels=std::make_shared<processing::FloatImage>(processing::FloatImage{96,64,std::vector<float>(96*64*3,.1f)});deepSky.preview=QImage(96,64,QImage::Format_RGB888);deepSky.preview.fill(Qt::black);deepSky.name="Deep sky";deepSky.processingSummary="15 exposures · 2700 s total · RGB";deepSky.exposureCount=15;deepSky.totalExposure=2700;
  ASTRO_TEST_CHECK(controller.openDocument(QVariant::fromValue(deepSky)));ASTRO_TEST_CHECK(controller.stackSummary()==deepSky.processingSummary);

}
