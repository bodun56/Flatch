#pragma once

#include <QDialog>

class Database;
class QCheckBox;
class QListWidget;
class QPushButton;
class QTreeWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Database *database, QWidget *parent = nullptr);

private:
    void buildUi();
    void reloadCategories(int selectCategoryId = -1);
    void reloadSnippets(int selectSnippetId = -1);
    void updateCategoryButtons();
    void updateSnippetButtons();
    void selectSnippetById(int snippetId);

    int selectedCategoryId() const;
    int selectedSnippetId() const;

    void addCategory();
    void renameCategory();
    void deleteCategory();

    void addSnippet();
    void editSnippet();
    void deleteSnippet();

    void showError(const QString &text);

    Database *m_database = nullptr;
    QListWidget *m_categories = nullptr;
    QTreeWidget *m_snippets = nullptr;
    QPushButton *m_addCategory = nullptr;
    QPushButton *m_renameCategory = nullptr;
    QPushButton *m_deleteCategory = nullptr;
    QPushButton *m_addSnippet = nullptr;
    QPushButton *m_editSnippet = nullptr;
    QPushButton *m_deleteSnippet = nullptr;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_notifications = nullptr;
};
