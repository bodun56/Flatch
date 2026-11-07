#include "appcontroller.h"

#include "aboutdialog.h"
#include "database.h"
#include "settingsdialog.h"
#include "traycontroller.h"

#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>

AppController::AppController(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

void AppController::start(bool autostartLaunch)
{
    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();

    if (trayAvailable) {
        m_tray = new TrayController(m_database, this);
        connect(m_tray, &TrayController::settingsRequested, this, &AppController::showSettings);
        connect(m_tray, &TrayController::aboutRequested, this, &AppController::showAbout);
        connect(m_tray, &TrayController::quitRequested, qApp, &QApplication::quit);
        m_tray->show();
    }

    if (!trayAvailable) {
        // Without a tray icon the only window is the settings dialog; closing
        // it must terminate the application.
        QApplication::setQuitOnLastWindowClosed(true);
        QMessageBox::warning(
            nullptr, QStringLiteral("Flatch"),
            QStringLiteral("Системный трей недоступен. Flatch откроет окно настроек.\n\n"
                           "Проверьте, что панель задач и область уведомлений работают."));
        showSettings();
        return;
    }

    if (m_database->wasCreated() && !autostartLaunch)
        showSettings();
}

void AppController::showSettings()
{
    if (!m_settings) {
        m_settings = new SettingsDialog(m_database);
        m_settings->setAttribute(Qt::WA_DeleteOnClose, true);
    }

    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void AppController::showAbout()
{
    AboutDialog dialog(m_settings ? m_settings.data() : nullptr);
    dialog.exec();
}
