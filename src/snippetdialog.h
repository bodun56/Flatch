#pragma once

#include "snippet.h"

#include <QDialog>

class Database;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QStackedWidget;
class QTextCharFormat;
class QTextEdit;
class QToolButton;

class SnippetDialog : public QDialog
{
    Q_OBJECT

public:
    SnippetDialog(Database *database, const Snippet *existing, int defaultCategoryId,
                  QWidget *parent = nullptr);

    int savedSnippetId() const { return m_savedSnippetId; }
    int savedCategoryId() const { return m_savedCategoryId; }

public slots:
    void accept() override;

private:
    void buildUi();
    void loadCategories();
    void loadExisting();
    void onKindChanged(int index);
    void chooseFiles();
    void chooseApplication();
    void addFilePath(const QString &path);
    void removeSelectedFiles();
    void updateFormatButtons();
    void mergeCharFormat(const QTextCharFormat &format);
    void chooseTextColor();
    void toggleList(int style);
    void clearFormatting();
    void showWarning(const QString &text);

    SnippetKind currentKind() const;
    QStringList currentPaths() const;
    bool loadStoredContent(QString *content);

    Database *m_database = nullptr;
    Snippet m_original;
    bool m_editing = false;
    int m_defaultCategoryId = -1;
    int m_savedSnippetId = -1;
    int m_savedCategoryId = -1;
    bool m_updatingFormatControls = false;

    QLineEdit *m_name = nullptr;
    QComboBox *m_category = nullptr;
    QComboBox *m_kind = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPlainTextEdit *m_plain = nullptr;
    QTextEdit *m_rich = nullptr;
    QListWidget *m_files = nullptr;
    QLineEdit *m_appPath = nullptr;
    QCheckBox *m_encrypt = nullptr;
    QToolButton *m_bold = nullptr;
    QToolButton *m_italic = nullptr;
    QToolButton *m_underline = nullptr;
    QComboBox *m_fontSize = nullptr;
};
