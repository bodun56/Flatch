#pragma once

#include <QObject>
#include <QPointer>

class Database;
class SettingsDialog;
class TrayController;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(Database *database, QObject *parent = nullptr);

    void start(bool autostartLaunch);

public slots:
    void showSettings();
    void showAbout();

private:
    Database *m_database = nullptr;
    TrayController *m_tray = nullptr;
    QPointer<SettingsDialog> m_settings;
};
