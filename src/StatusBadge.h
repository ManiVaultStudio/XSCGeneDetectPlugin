#pragma once

#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace StatusBadge {
enum class State { Ready, Busy, Warning, Error, Unknown };

inline QColor readableColor(const QColor& preferred, const QColor& background)
{
    const auto luminance = [](const QColor& color) {
        const auto linear = [](double value) {
            return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    };
    const double surface = luminance(background) + 0.05;
    const double foreground = luminance(preferred) + 0.05;
    if (std::max(surface, foreground) / std::min(surface, foreground) >= 4.5)
        return preferred;
    return surface / 0.05 >= 1.05 / surface ? QColor(Qt::black) : QColor(Qt::white);
}

inline void paint(QPainter& painter, const QColor& color, State state, int phase)
{
    painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (state) {
    case State::Ready:
        painter.drawPolyline(QPolygonF{QPointF(3, 9), QPointF(7, 13), QPointF(13, 5)});
        break;
    case State::Busy:
        painter.drawArc(QRectF(4, 4, 9, 9), phase * 16, 250 * 16);
        break;
    case State::Warning:
        painter.drawLine(QPointF(8, 4), QPointF(8, 9));
        painter.drawPoint(QPointF(8, 13));
        break;
    case State::Error:
        painter.drawLine(QPointF(4, 5), QPointF(12, 13));
        painter.drawLine(QPointF(12, 5), QPointF(4, 13));
        break;
    case State::Unknown:
        painter.drawPoint(QPointF(8, 9));
        break;
    }
}
}
