#pragma once

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QSize>
#include <QWidget>

// Keeps dialogs usable on small screens / high-DPI laptops where the preferred
// size does not fit into the available desktop area.
inline void fitDialogToScreen(QWidget *widget, const QSize &desired, const QSize &minimum)
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    if (!screen) {
        widget->setMinimumSize(minimum);
        widget->resize(desired);
        return;
    }

    const QSize available = screen->availableGeometry().size();
    const int maxWidth = qMax(400, available.width() - 40);
    const int maxHeight = qMax(300, available.height() - 40);

    widget->setMinimumSize(qMin(minimum.width(), maxWidth),
                           qMin(minimum.height(), maxHeight));
    widget->resize(qMin(desired.width(), maxWidth), qMin(desired.height(), maxHeight));
}
