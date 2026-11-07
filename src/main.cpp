#include "appcontroller.h"
#include "appicon.h"
#include "apppaths.h"
#include "database.h"

#include <QApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QThread>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Flatch"));
    QApplication::setApplicationName(QStringLiteral("Flatch"));
    QApplication::setApplicationDisplayName(QStringLiteral("Flatch"));
    QApplication::setApplicationVersion(QStringLiteral(FLATCH_VERSION));
    QApplication::setWindowIcon(flatchIcon());
    QApplication::setQuitOnLastWindowClosed(false);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral("flatch"));
#endif

    // Standard buttons of QMessageBox/QInputDialog/QColorDialog should be
    // Russian too, regardless of the system language. Translations are
    // deployed next to the executable; while running from Qt Creator the
    // Qt installation directory is used as a fallback.
    QTranslator qtTranslator;
    const QLocale russianLocale(QLocale::Russian, QLocale::Russia);
    const QString localTranslations =
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    const QString qtTranslations = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    // windeployqt ships a merged qt_ru.qm; a Qt installation has qtbase_ru.qm.
    const bool translatorLoaded =
        qtTranslator.load(russianLocale, QStringLiteral("qt"), QStringLiteral("_"),
                          localTranslations)
        || qtTranslator.load(russianLocale, QStringLiteral("qtbase"), QStringLiteral("_"),
                             localTranslations)
        || qtTranslator.load(russianLocale, QStringLiteral("qt"), QStringLiteral("_"),
                             qtTranslations)
        || qtTranslator.load(russianLocale, QStringLiteral("qtbase"), QStringLiteral("_"),
                             qtTranslations);
    if (translatorLoaded) {
        application.installTranslator(&qtTranslator);
    }

    const bool autostartLaunch = application.arguments().contains(QStringLiteral("--autostart"));

    // Only one instance may run: the second launch just opens the settings of
    // the running one. The lock also closes the tiny race between starting the
    // process and starting the local server.
    QLockFile instanceLock(QDir(AppPaths::dataDir()).filePath(QStringLiteral("instance.lock")));
    instanceLock.setStaleLockTime(10000);

    const QString serverName = QStringLiteral("Flatch.SingleInstance.v1.%1")
                                   .arg(static_cast<qulonglong>(qHash(QDir::homePath())), 0, 16);

    if (!instanceLock.tryLock(0)) {
        QLocalSocket probe;
        bool connected = false;
        for (int attempt = 0; attempt < 25 && !connected; ++attempt) {
            probe.connectToServer(serverName);
            if (probe.waitForConnected(120)) {
                connected = true;
                break;
            }
            probe.abort();
            QThread::msleep(120);
        }

        if (connected) {
            probe.write("show");
            probe.flush();
            probe.waitForBytesWritten(500);
            probe.disconnectFromServer();
            return 0;
        }

        QMessageBox::warning(
            nullptr, QStringLiteral("Flatch"),
            QStringLiteral("Flatch уже запущен, но не отвечает.\n\n"
                           "Завершите процесс Flatch.exe в диспетчере задач и попробуйте снова."));
        return 1;
    }

    Database database;
    QString error;
    if (!database.open(&error)) {
        QMessageBox::critical(
            nullptr, QStringLiteral("Flatch"),
            QStringLiteral("Не удалось открыть базу данных.\n\n%1\n\nПуть: %2")
                .arg(error, QDir::toNativeSeparators(AppPaths::databaseFile())));
        return 1;
    }

    AppController controller(&database);

    QLocalServer server;
    QObject::connect(&server, &QLocalServer::newConnection, &application, [&server, &controller] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            socket->disconnectFromServer();
            socket->deleteLater();
            controller.showSettings();
        }
    });

    if (!server.listen(serverName)) {
        QLocalServer::removeServer(serverName);
        if (!server.listen(serverName)) {
            qWarning("Flatch: cannot start the single-instance server: %s",
                     qPrintable(server.errorString()));
        }
    }

    controller.start(autostartLaunch);

    const int exitCode = application.exec();

    server.close();
    QLocalServer::removeServer(serverName);
    instanceLock.unlock();
    return exitCode;
}
