#include "WindowAppearance.hpp"
#import <AppKit/AppKit.h>
#include <QColor>
#include <QGuiApplication>
#include <QWindow>

@interface AstroCaptureWindowTitle : NSTextField
@end
@implementation AstroCaptureWindowTitle
- (NSView *)hitTest:(NSPoint)point {
  // Let the native title bar handle dragging and double-clicking.
  return nil;
}
@end

namespace astro::app {
void applyWindowAppearance(QWindow *window, const QColor &canvas) {
  if (!window || QGuiApplication::platformName() != QStringLiteral("cocoa"))
    return;
  NSView *view = reinterpret_cast<NSView *>(window->winId());
  NSWindow *native = view.window;
  if (!native)
    return;
  native.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
  native.backgroundColor = [NSColor colorWithSRGBRed:canvas.redF()
                                               green:canvas.greenF()
                                                blue:canvas.blueF()
                                               alpha:canvas.alphaF()];
  native.titlebarAppearsTransparent = YES;
  native.titleVisibility = NSWindowTitleVisible;
  NSButton *close = [native standardWindowButton:NSWindowCloseButton];
  NSView *titleBar = close.superview;
  if (!titleBar)
    return;
  // macOS can lead-align its system title. Keep the native frame and place
  // our non-interactive title in that frame using public AppKit constraints.
  native.titleVisibility = NSWindowTitleHidden;
  NSTextField *title = nil;
  for (NSView *child in titleBar.subviews) {
    if ([child.identifier isEqualToString:@"AstroCaptureCenteredTitle"])
      title = static_cast<NSTextField *>(child);
  }
  if (!title) {
    title = [AstroCaptureWindowTitle labelWithString:native.title];
    title.identifier = @"AstroCaptureCenteredTitle";
    title.translatesAutoresizingMaskIntoConstraints = NO;
    title.alignment = NSTextAlignmentCenter;
    title.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    title.textColor = NSColor.labelColor;
    [titleBar addSubview:title];
    [NSLayoutConstraint activateConstraints:@[
      [title.centerXAnchor constraintEqualToAnchor:titleBar.centerXAnchor],
      [title.centerYAnchor constraintEqualToAnchor:close.centerYAnchor],
      [title.widthAnchor constraintLessThanOrEqualToAnchor:titleBar.widthAnchor
                                                  constant:-160]
    ]];
  }
  title.stringValue = native.title;
}
} // namespace astro::app
