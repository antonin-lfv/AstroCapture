#pragma once
#include <QImage>
#include <QPainter>
#include <QTextLayout>
#include <QTextCharFormat>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace astro::app {
inline QVariantMap cleanAnnotation(const QVariantMap& values) {
    double size = values.value("size", 1.5).toDouble();
    if (!std::isfinite(size)) size = 1.5;
    return {{"enabled", values.value("enabled", false).toBool()},
            {"technical", values.value("technical", true).toBool()},
            {"title", values.value("title").toString().left(120).replace('\n', ' ').replace('\r', ' ')},
            {"top", values.value("top").toString().left(400)},
            {"bottom", values.value("bottom").toString().left(400)},
            {"gold", values.value("gold", false).toBool()},
            {"size", std::clamp(size, 0.8, 3.0)}};
}
// One full-resolution overlay is shared by preview and export. The astronomical
// image remains untouched; alpha is blended into 16-bit export samples only.
inline QImage makeAnnotation(QSize size, const QVariantMap& settings, const QString& technical) {
    if (!settings.value("enabled").toBool() || size.isEmpty()) return {};
    const QString title = settings.value("title").toString().trimmed();
    QString top = settings.value("top").toString().trimmed();
    QString bottom = settings.value("bottom").toString().trimmed();
    if (settings.value("technical").toBool() && !technical.isEmpty())
        bottom += (bottom.isEmpty() ? "" : "\n") + technical;
    if (title.isEmpty() && top.isEmpty() && bottom.isEmpty()) return {};
    QImage overlay(size, QImage::Format_ARGB32);
    if (overlay.isNull()) throw std::runtime_error("Not enough memory for the image caption.");
    overlay.fill(Qt::transparent);
    QPainter painter(&overlay);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font;
    font.setFamily("Helvetica");
    font.setPixelSize(std::max(8, int(std::min(size.width(), size.height()) * settings.value("size").toDouble() / 100.0)));
    painter.setFont(font);
    const QColor ink = settings.value("gold").toBool() ? QColor("#dcc76d") : QColor("#f2f4f8");
    const int margin = std::max(2, int(std::min(size.width(), size.height()) * .018));
    const qreal availableWidth = std::max(1, size.width() - 2 * margin);
    const qreal availableHeight = std::max(1.0, (size.height() - 2 * margin) * .42);
    auto drawBlock = [&](QString text, bool atBottom, const QString& heading = QString{}) {
        if (!heading.isEmpty()) text = heading + (text.isEmpty() ? "" : "\n") + text;
        // QTextLayout handles wrapping and Unicode. Each area is bounded so
        // an excessively long caption cannot cover the entire photograph.
        text.replace('\n', QChar::LineSeparator);
        QTextLayout layout(text, font);
        if (!heading.isEmpty()) {
            QFont titleFont = font; titleFont.setBold(true); titleFont.setPixelSize(int(font.pixelSize() * 1.25));
            QTextCharFormat format; format.setFont(titleFont);
            layout.setFormats({QTextLayout::FormatRange{0, int(heading.size()), format}});
        }
        QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        qreal height = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(availableWidth);
            line.setPosition({0, height});
            height += line.height();
        }
        layout.endLayout();
        const qreal blockHeight = std::min(height, availableHeight);
        const QPointF origin(margin, atBottom ? size.height() - margin - blockHeight : margin);
        painter.save();
        painter.setClipRect(QRectF(origin, QSizeF(availableWidth, blockHeight)));
        painter.setPen(QColor(0, 0, 0, 210));
        layout.draw(&painter, origin + QPointF(1, 1));
        painter.setPen(ink);
        layout.draw(&painter, origin);
        painter.restore();
    };
    drawBlock(top, false, title); drawBlock(bottom, true);
    return overlay;
}
inline std::uint16_t annotatedSample(float sample, QRgb ink, int channel) {
    const double base = std::clamp(sample, 0.f, 1.f);
    const double alpha = qAlpha(ink) / 255.0;
    const int value = channel == 0 ? qRed(ink) : channel == 1 ? qGreen(ink) : qBlue(ink);
    return std::uint16_t(std::lround((base * (1 - alpha) + value / 255.0 * alpha) * 65535));
}
}
