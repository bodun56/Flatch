#include "apppaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace AppPaths {

QString dataDir()
{
    static QString cached;
    if (!cached.isEmpty())
        return cached;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data"));

    dir = QDir::cleanPath(dir);
    QDir().mkpath(dir);

#ifndef Q_OS_WIN
    // The data directory contains the encryption key and the database.
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner);
#endif

    cached = dir;
    return cached;
}

QString databaseFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("flatch.db"));
}

QString keyFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("key.bin"));
}

} // namespace AppPaths
