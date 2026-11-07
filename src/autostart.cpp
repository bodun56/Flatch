#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

QString programPath()
{
    // When packaged as an AppImage, applicationFilePath() points into the
    // temporary mount; the original AppImage path is the stable one.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty() && QFileInfo::exists(appImage))
        return QDir::cleanPath(appImage);
    return QDir::cleanPath(QCoreApplication::applicationFilePath());
}

#ifdef Q_OS_WIN

QString runKey()
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

QString launchCommand()
{
    return QStringLiteral("\"%1\" --autostart")
        .arg(QDir::toNativeSeparators(programPath()));
}

#else

QString autostartDirectory()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configDir.isEmpty())
        configDir = QDir::homePath() + QStringLiteral("/.config");
    return QDir(configDir).filePath(QStringLiteral("autostart"));
}

QString desktopFilePath()
{
    return QDir(autostartDirectory()).filePath(QStringLiteral("flatch.desktop"));
}

QString quoteForDesktopExec(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    escaped.replace(QLatin1Char('`'), QLatin1String("\\`"));
    escaped.replace(QLatin1Char('$'), QLatin1String("\\$"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString desktopFileContents()
{
    return QStringLiteral(
               "[Desktop Entry]\n"
               "Type=Application\n"
               "Name=Flatch\n"
               "Comment=Snippet manager\n"
               "Exec=%1 --autostart\n"
               "Terminal=false\n"
               "StartupNotify=false\n"
               "Categories=Utility;\n"
               "X-GNOME-Autostart-enabled=true\n")
        .arg(quoteForDesktopExec(programPath()));
}

#endif

} // namespace

namespace Autostart {

bool isSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool isEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings(runKey(), QSettings::NativeFormat);
    return !settings.value(QStringLiteral("Flatch")).toString().isEmpty();
#elif defined(Q_OS_LINUX)
    return QFileInfo::exists(desktopFilePath());
#else
    return false;
#endif
}

bool setEnabled(bool enabled, QString *error)
{
    if (error)
        error->clear();

#if defined(Q_OS_WIN)
    QSettings settings(runKey(), QSettings::NativeFormat);
    if (enabled)
        settings.setValue(QStringLiteral("Flatch"), launchCommand());
    else
        settings.remove(QStringLiteral("Flatch"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(error, QStringLiteral("Не удалось изменить параметры автозагрузки в реестре Windows."));
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    const QString path = desktopFilePath();
    if (!enabled) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            setError(error, QStringLiteral("Не удалось удалить файл автозагрузки «%1».").arg(path));
            return false;
        }
        return true;
    }

    if (!QDir().mkpath(autostartDirectory())) {
        setError(error, QStringLiteral("Не удалось создать папку автозагрузки «%1».")
                            .arg(autostartDirectory()));
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(error, QStringLiteral("Не удалось записать файл автозагрузки «%1»: %2")
                            .arg(path, file.errorString()));
        return false;
    }
    const QByteArray contents = desktopFileContents().toUtf8();
    if (file.write(contents) != contents.size() || !file.commit()) {
        setError(error, QStringLiteral("Не удалось сохранить файл автозагрузки «%1»: %2")
                            .arg(path, file.errorString()));
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return true;
#else
    Q_UNUSED(enabled);
    setError(error, QStringLiteral("Автозагрузка поддерживается только в Windows и Linux."));
    return false;
#endif
}

} // namespace Autostart
