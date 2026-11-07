#include "settingsdialog.h"

#include "appicon.h"
#include "autostart.h"
#include "database.h"
#include "preferences.h"
#include "snippetdialog.h"
#include "uiutils.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QLabel *makeSectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QLabel *makeHintLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    label->setPalette(palette);
    return label;
}

} // namespace

SettingsDialog::SettingsDialog(Database *database, QWidget *parent)
    : QDialog(parent)
    , m_database(database)
{
    setWindowTitle(QStringLiteral("Flatch — настройки"));
    setWindowIcon(flatchIcon());
    setAttribute(Qt::WA_DeleteOnClose, true);
    setSizeGripEnabled(true);
    fitDialogToScreen(this, QSize(900, 640), QSize(760, 480));

    buildUi();
    reloadCategories();
}

void SettingsDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *splitter = new QSplitter(Qt::Horizontal);

    // --- Categories ---
    auto *leftPanel = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    leftLayout->addWidget(makeSectionLabel(QStringLiteral("Категории")));

    m_categories = new QListWidget;
    m_categories->setAlternatingRowColors(true);
    m_categories->setToolTip(QStringLiteral("Двойной щелчок — переименовать категорию"));
    leftLayout->addWidget(m_categories, 1);

    auto *categoryButtons = new QHBoxLayout;
    m_addCategory = new QPushButton(QStringLiteral("Добавить"));
    m_renameCategory = new QPushButton(QStringLiteral("Изменить"));
    m_renameCategory->setToolTip(QStringLiteral("Переименовать категорию"));
    m_deleteCategory = new QPushButton(QStringLiteral("Удалить"));
    categoryButtons->addWidget(m_addCategory);
    categoryButtons->addWidget(m_renameCategory);
    categoryButtons->addWidget(m_deleteCategory);
    leftLayout->addLayout(categoryButtons);

    // --- Snippets ---
    auto *rightPanel = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    rightLayout->addWidget(makeSectionLabel(QStringLiteral("Сниппеты")));

    m_snippets = new QTreeWidget;
    m_snippets->setColumnCount(3);
    m_snippets->setHeaderLabels({QStringLiteral("Название"), QStringLiteral("Тип"),
                                 QStringLiteral("Защита")});
    m_snippets->setRootIsDecorated(false);
    m_snippets->setUniformRowHeights(true);
    m_snippets->setAlternatingRowColors(true);
    m_snippets->setSelectionMode(QAbstractItemView::SingleSelection);
    m_snippets->setToolTip(QStringLiteral("Двойной щелчок — редактировать сниппет"));
    m_snippets->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_snippets->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_snippets->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rightLayout->addWidget(m_snippets, 1);

    auto *snippetButtons = new QHBoxLayout;
    m_addSnippet = new QPushButton(QStringLiteral("Добавить"));
    m_editSnippet = new QPushButton(QStringLiteral("Изменить"));
    m_deleteSnippet = new QPushButton(QStringLiteral("Удалить"));
    snippetButtons->addWidget(m_addSnippet);
    snippetButtons->addWidget(m_editSnippet);
    snippetButtons->addWidget(m_deleteSnippet);
    snippetButtons->addStretch(1);
    rightLayout->addLayout(snippetButtons);

    rightLayout->addWidget(makeHintLabel(
        QStringLiteral("Двойной щелчок открывает редактор. Сниппеты доступны в меню "
                       "по щелчку на иконке Flatch в системном трее.")));

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 600});
    leftPanel->setMinimumWidth(280);

    mainLayout->addWidget(splitter, 1);

    // --- System / startup ---
    auto *systemGroup = new QGroupBox(QStringLiteral("Система"));
    auto *systemLayout = new QVBoxLayout(systemGroup);

    m_autostart = new QCheckBox(QStringLiteral("Запускать Flatch при входе в систему"));
    m_notifications = new QCheckBox(
        QStringLiteral("Показывать уведомление после копирования в буфер обмена"));

    if (Autostart::isSupported()) {
        QSignalBlocker blocker(m_autostart);
        m_autostart->setChecked(Autostart::isEnabled());
        m_autostart->setToolTip(
            QStringLiteral("В автозагрузку записывается текущий путь к исполняемому файлу Flatch."));
    } else {
        m_autostart->setEnabled(false);
        m_autostart->setText(QStringLiteral("Автозагрузка (доступна в Windows и Linux)"));
    }

    QSignalBlocker notificationsBlocker(m_notifications);
    m_notifications->setChecked(Prefs::notificationsEnabled());

    systemLayout->addWidget(m_autostart);
    systemLayout->addWidget(m_notifications);
    mainLayout->addWidget(systemGroup);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    if (QPushButton *closeButton = buttons->button(QDialogButtonBox::Close))
        closeButton->setText(QStringLiteral("Закрыть"));
    mainLayout->addWidget(buttons);

    connect(m_categories, &QListWidget::currentRowChanged, this, [this](int) {
        reloadSnippets();
        updateSnippetButtons();
    });
    connect(m_categories, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        renameCategory();
    });
    connect(m_categories, &QListWidget::itemSelectionChanged, this,
            &SettingsDialog::updateCategoryButtons);

    connect(m_snippets, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *, QTreeWidgetItem *) { updateSnippetButtons(); });
    connect(m_snippets, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *, int) { editSnippet(); });

    connect(m_addCategory, &QPushButton::clicked, this, &SettingsDialog::addCategory);
    connect(m_renameCategory, &QPushButton::clicked, this, &SettingsDialog::renameCategory);
    connect(m_deleteCategory, &QPushButton::clicked, this, &SettingsDialog::deleteCategory);
    connect(m_addSnippet, &QPushButton::clicked, this, &SettingsDialog::addSnippet);
    connect(m_editSnippet, &QPushButton::clicked, this, &SettingsDialog::editSnippet);
    connect(m_deleteSnippet, &QPushButton::clicked, this, &SettingsDialog::deleteSnippet);

    connect(m_autostart, &QCheckBox::toggled, this, [this](bool checked) {
        QString error;
        if (!Autostart::setEnabled(checked, &error)) {
            QSignalBlocker blocker(m_autostart);
            m_autostart->setChecked(!checked);
            showError(error);
        }
    });
    connect(m_notifications, &QCheckBox::toggled, this, [](bool checked) {
        Prefs::setNotificationsEnabled(checked);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateCategoryButtons();
    updateSnippetButtons();
}

void SettingsDialog::reloadCategories(int selectCategoryId)
{
    const int previous = selectCategoryId >= 0 ? selectCategoryId : selectedCategoryId();

    QString error;
    const QVector<Category> categories = m_database->categories(&error);

    {
        QSignalBlocker blocker(m_categories);
        m_categories->clear();

        int rowToSelect = -1;
        for (int i = 0; i < categories.size(); ++i) {
            const Category &category = categories.at(i);
            auto *item = new QListWidgetItem(category.name, m_categories);
            item->setData(Qt::UserRole, category.id);
            if (category.id == previous)
                rowToSelect = i;
        }

        if (rowToSelect < 0 && m_categories->count() > 0)
            rowToSelect = 0;

        if (rowToSelect >= 0)
            m_categories->setCurrentRow(rowToSelect);
    }

    if (!error.isEmpty())
        showError(error);

    updateCategoryButtons();
    reloadSnippets();
}

void SettingsDialog::reloadSnippets(int selectSnippetId)
{
    const int previous = selectSnippetId >= 0 ? selectSnippetId : selectedSnippetId();
    const int categoryId = selectedCategoryId();

    {
        QSignalBlocker blocker(m_snippets);
        m_snippets->clear();

        if (categoryId > 0) {
            QString error;
            const QVector<Snippet> snippets = m_database->snippets(categoryId, &error);
            if (!error.isEmpty())
                showError(error);

            QTreeWidgetItem *itemToSelect = nullptr;
            for (const Snippet &snippet : snippets) {
                auto *item = new QTreeWidgetItem(m_snippets);
                item->setText(0, snippet.name);
                item->setText(1, snippetKindName(snippet.kind));
                item->setText(2, snippet.encrypted ? QStringLiteral("🔒") : QString());
                item->setData(0, Qt::UserRole, snippet.id);
                item->setToolTip(0, snippet.name);
                if (snippet.id == previous)
                    itemToSelect = item;
            }

            if (itemToSelect)
                m_snippets->setCurrentItem(itemToSelect);
            else if (m_snippets->topLevelItemCount() > 0)
                m_snippets->setCurrentItem(m_snippets->topLevelItem(0));
        }
    }

    updateSnippetButtons();
}

void SettingsDialog::updateCategoryButtons()
{
    const bool hasSelection = m_categories->currentItem() != nullptr;
    m_renameCategory->setEnabled(hasSelection);
    m_deleteCategory->setEnabled(hasSelection);
}

void SettingsDialog::updateSnippetButtons()
{
    const bool hasCategory = selectedCategoryId() > 0;
    const bool hasSelection = hasCategory && m_snippets->currentItem() != nullptr;
    m_addSnippet->setEnabled(hasCategory);
    m_editSnippet->setEnabled(hasSelection);
    m_deleteSnippet->setEnabled(hasSelection);
}

void SettingsDialog::selectSnippetById(int snippetId)
{
    if (snippetId <= 0)
        return;
    for (int i = 0; i < m_snippets->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_snippets->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toInt() == snippetId) {
            m_snippets->setCurrentItem(item);
            m_snippets->scrollToItem(item);
            return;
        }
    }
}

int SettingsDialog::selectedCategoryId() const
{
    QListWidgetItem *item = m_categories->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

int SettingsDialog::selectedSnippetId() const
{
    QTreeWidgetItem *item = m_snippets->currentItem();
    return item ? item->data(0, Qt::UserRole).toInt() : -1;
}

void SettingsDialog::addCategory()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Новая категория"), QStringLiteral("Название категории:"),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok)
        return;
    if (name.isEmpty()) {
        showError(QStringLiteral("Название категории не может быть пустым."));
        return;
    }

    int newId = -1;
    QString error;
    if (!m_database->addCategory(name, &newId, &error)) {
        showError(error);
        return;
    }
    reloadCategories(newId);
}

void SettingsDialog::renameCategory()
{
    QListWidgetItem *item = m_categories->currentItem();
    if (!item)
        return;

    const int id = item->data(Qt::UserRole).toInt();
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Переименовать категорию"), QStringLiteral("Новое название:"),
        QLineEdit::Normal, item->text(), &ok).trimmed();
    if (!ok || name.isEmpty() || name == item->text())
        return;

    QString error;
    if (!m_database->renameCategory(id, name, &error)) {
        showError(error);
        return;
    }
    reloadCategories(id);
}

void SettingsDialog::deleteCategory()
{
    QListWidgetItem *item = m_categories->currentItem();
    if (!item)
        return;

    const int id = item->data(Qt::UserRole).toInt();
    const QString name = item->text();
    const int snippetCount = m_database->snippets(id).size();

    QString text = QStringLiteral("Удалить категорию «%1»?").arg(name);
    if (snippetCount > 0) {
        text += QStringLiteral("\n\nВместе с ней будет удалено сниппетов: %1.")
                    .arg(snippetCount);
    }

    const auto answer = QMessageBox::question(this, QStringLiteral("Удаление категории"), text,
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!m_database->removeCategory(id, &error)) {
        showError(error);
        return;
    }
    reloadCategories();
}

void SettingsDialog::addSnippet()
{
    const int categoryId = selectedCategoryId();
    if (categoryId <= 0) {
        showError(QStringLiteral("Сначала создайте категорию."));
        return;
    }

    SnippetDialog dialog(m_database, nullptr, categoryId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    reloadCategories(dialog.savedCategoryId());
    selectSnippetById(dialog.savedSnippetId());
}

void SettingsDialog::editSnippet()
{
    const int snippetId = selectedSnippetId();
    if (snippetId <= 0)
        return;

    Snippet snippet;
    QString error;
    if (!m_database->snippet(snippetId, &snippet, &error)) {
        showError(error);
        return;
    }

    SnippetDialog dialog(m_database, &snippet, snippet.categoryId, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    reloadCategories(dialog.savedCategoryId());
    selectSnippetById(dialog.savedSnippetId());
}

void SettingsDialog::deleteSnippet()
{
    const int snippetId = selectedSnippetId();
    if (snippetId <= 0)
        return;

    const QString name = m_snippets->currentItem()->text(0);
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Удаление сниппета"),
        QStringLiteral("Удалить сниппет «%1»?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!m_database->removeSnippet(snippetId, &error)) {
        showError(error);
        return;
    }
    reloadSnippets();
}

void SettingsDialog::showError(const QString &text)
{
    QMessageBox::warning(this, QStringLiteral("Flatch"), text);
}
