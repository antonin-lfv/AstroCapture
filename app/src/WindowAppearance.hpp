#pragma once

class QColor;
class QWindow;

namespace astro::app {
// Keep native window controls while matching the application's canvas.
void applyWindowAppearance(QWindow *window, const QColor &canvas);
} // namespace astro::app
