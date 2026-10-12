#include "WindowAppearance.hpp"
#include "Check.hpp"
#import <AppKit/AppKit.h>
#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QThread>

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QQuickWindow window;
  const QColor canvas("#141518");
  window.setTitle("AstroCapture");
  window.setColor(canvas);
  window.resize(900, 700);
  window.show();
  if (QGuiApplication::platformName() != "cocoa")
    return 0;
  NSView *content = reinterpret_cast<NSView *>(window.winId());
  NSWindow *native = content.window;
  ASTRO_TEST_CHECK(native);
  const auto initialMask = native.styleMask;
  // Apply before entering the event loop, as the application does at startup.
  astro::app::applyWindowAppearance(&window, canvas);
  for (int i = 0; i < 8; ++i) {
    app.processEvents();
    QThread::msleep(25);
  }
  app.processEvents();
  ASTRO_TEST_CHECK(native.styleMask == initialMask);
  ASTRO_TEST_CHECK(native && [native.title isEqualToString:@"AstroCapture"]);
  ASTRO_TEST_CHECK([native standardWindowButton:NSWindowCloseButton]);
  ASTRO_TEST_CHECK([native standardWindowButton:NSWindowMiniaturizeButton]);
  ASTRO_TEST_CHECK([native standardWindowButton:NSWindowZoomButton]);
  // Keep Qt's content below the native title bar; the navigation must not sit
  // behind the traffic-light buttons.
  ASTRO_TEST_CHECK(!(native.styleMask & NSWindowStyleMaskFullSizeContentView));
  ASTRO_TEST_CHECK(window.size() == QSize(900, 700));
  NSView *frame = content.superview;
  auto *titleBar = [native standardWindowButton:NSWindowCloseButton].superview;
  NSTextField *title = nil;
  for (NSView *child in titleBar.subviews)
    if ([child.identifier isEqualToString:@"AstroCaptureCenteredTitle"])
      title = static_cast<NSTextField *>(child);
  ASTRO_TEST_CHECK(title);
  [frame layoutSubtreeIfNeeded];
  const auto titleRect = [title convertRect:title.bounds toView:frame];
  ASTRO_TEST_CHECK(std::abs(NSMidX(titleRect) - NSMidX(frame.bounds)) < 1);
  NSBitmapImageRep *bitmap =
      [frame bitmapImageRepForCachingDisplayInRect:frame.bounds];
  [frame cacheDisplayInRect:frame.bounds toBitmapImageRep:bitmap];
  ASTRO_TEST_CHECK(bitmap.pixelsWide >= 900 && bitmap.pixelsHigh > 700);
  // Read the rendered native title bar, away from its text and buttons.
  NSColor *pixel = [[bitmap colorAtX:bitmap.pixelsWide / 4 y:10]
      colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
  ASTRO_TEST_CHECK(pixel.redComponent < .2 && pixel.greenComponent < .2 &&
                   pixel.blueComponent < .2);
  // Native snapshots can carry the display's calibrated color profile.
  ASTRO_TEST_CHECK(std::abs(pixel.redComponent - canvas.redF()) < .04 &&
                   std::abs(pixel.greenComponent - canvas.greenF()) < .04 &&
                   std::abs(pixel.blueComponent - canvas.blueF()) < .04);
  const auto directory = qEnvironmentVariable("ASTRO_PERSONAL_QA_DIR");
  if (!directory.isEmpty()) {
    ASTRO_TEST_CHECK(QDir().mkpath(directory));
    NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG
                                       properties:@{}];
    const auto path = (directory + "/native-titlebar-900.png").toUtf8();
    ASTRO_TEST_CHECK([png
        writeToFile:[NSString stringWithUTF8String:path.constData()]
         atomically:YES]);
  }
  for (const auto size : {QSize(1100, 800), QSize(1440, 960)}) {
    window.resize(size);
    app.processEvents();
    [frame layoutSubtreeIfNeeded];
    const auto resizedTitleRect = [title convertRect:title.bounds toView:frame];
    ASTRO_TEST_CHECK(std::abs(NSMidX(resizedTitleRect) - NSMidX(frame.bounds)) <
                     1);
    ASTRO_TEST_CHECK(window.size() == size);
  }
  // The title stays centred when a native window is resized.
  window.close();
}
