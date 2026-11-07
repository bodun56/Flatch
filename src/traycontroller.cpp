#include "traycontroller.h"

#include "appicon.h"
#include "database.h"
#include "preferences.h"
#include "snippetactions.h"

#include <QCursor>
#include <QMenu>
#include <QMessageBox>
#include <QSystemTrayIcon>

namespace {

QString menuLabel(const QString &name)
{
    QString escaped = name;
    escaped.replace(QLatin1Char('&'), QLatin1String("&&"));
    return escaped;
}

QString snippetLabel(const Snippet &snippet)
{
    QString label = menuLabel(snippet.name);
    if (snippet.encrypted)
        label += QStringLiteral("  🔒");
    return label;
}

} // namespace

TrayController::TrayController(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    m_tray = new QSystemTrayIcon(flatchIcon(), this);
    retranslate();

    m_menu = new QMenu();
    m_menu->setToolTipsVisible(true);
    m_tray->setContextMenu(m_menu);

    connect(m_menu, &QMenu::aboutToShow, this, &TrayController::rebuildMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger)
                    m_menu->popup(QCursor::pos());
            });
}

TrayController::~TrayController()
{
    if (m_tray)
        m_tray->setContextMenu(nullptr);
    delete m_menu;
    m_menu = nullptr;
}

void TrayController::show()
{
    m_tray->show();
}

void TrayController::retranslate()
{
    if (m_tray)
        m_tray->setToolTip(tr("Flatch — менеджер сниппетов"));
}

void TrayController::rebuildMenu()
{
    m_menu->clear();

    const QVector<Category> categories = m_database->categories();
    if (categories.isEmpty()) {
        QAction *empty = m_menu->addAction(tr("Нет категорий"));
        empty->setEnabled(false);
        QAction *create = m_menu->addAction(tr("Создать категорию…"));
        connect(create, &QAction::triggered, this, &TrayController::settingsRequested);
    } else {
        for (const Category &category : categories) {
            QMenu *submenu = m_menu->addMenu(menuLabel(category.name));
            const QVector<Snippet> snippets = m_database->snippets(category.id);
            if (snippets.isEmpty()) {
                QAction *empty = submenu->addAction(tr("(пусто)"));
                empty->setEnabled(false);
                continue;
            }
            for (const Snippet &snippet : snippets) {
                QAction *action = submenu->addAction(snippetLabel(snippet));
                action->setToolTip(snippetKindName(snippet.kind));
                connect(action, &QAction::triggered, this, [this, id = snippet.id]() {
                    triggerSnippet(id);
                });
            }
        }
    }

    m_menu->addSeparator();
    QAction *settings = m_menu->addAction(tr("Настройки…"));
    connect(settings, &QAction::triggered, this, &TrayController::settingsRequested);

    QAction *about = m_menu->addAction(tr("О программе…"));
    connect(about, &QAction::triggered, this, &TrayController::aboutRequested);

    m_menu->addSeparator();
    QAction *quit = m_menu->addAction(tr("Выход"));
    connect(quit, &QAction::triggered, this, &TrayController::quitRequested);
}

void TrayController::triggerSnippet(int snippetId)
{
    Snippet snippet;
    QString error;
    if (!m_database->snippet(snippetId, &snippet, &error)) {
        showError(error);
        return;
    }

    if (!SnippetActions::activate(snippet, &error)) {
        showError(error);
        return;
    }

    if (snippet.kind != SnippetKind::Application && Prefs::notificationsEnabled()) {
        m_tray->showMessage(QStringLiteral("Flatch"),
                            tr("Скопировано: %1").arg(snippet.name),
                            QSystemTrayIcon::Information, 1500);
    }
}

void TrayController::showError(const QString &message)
{
    QMessageBox box(QMessageBox::Critical, QStringLiteral("Flatch"), message,
                    QMessageBox::Ok, nullptr);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
}
