#pragma once

#include <QIcon>
#include <QSize>
#include <QString>

inline QIcon flatchIcon()
{
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    for (int size : sizes) {
        icon.addFile(QStringLiteral(":/icons/flatch_%1.png").arg(size),
                     QSize(size, size));
    }
    return icon;
}
