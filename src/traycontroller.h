#pragma once

#include <QObject>

class Database;
class QMenu;
class QSystemTrayIcon;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(Database *database, QObject *parent = nullptr);
    ~TrayController() override;

    void show();

signals:
    void settingsRequested();
    void aboutRequested();
    void quitRequested();

private slots:
    void rebuildMenu();
    void triggerSnippet(int snippetId);

private:
    void showError(const QString &message);

    Database *m_database = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
};
