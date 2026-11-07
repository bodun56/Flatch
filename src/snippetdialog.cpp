#include "snippetdialog.h"

#include "appicon.h"
#include "crypto.h"
#include "database.h"
#include "uiutils.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextList>
#include <QTextListFormat>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QLabel *makeHintLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    label->setPalette(palette);
    return label;
}

QLabel *makeSectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QFrame *makeSeparator()
{
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    return separator;
}

QString documentsDirectory()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QDir::homePath();
    return dir;
}

} // namespace

SnippetDialog::SnippetDialog(Database *database, const Snippet *existing, int defaultCategoryId,
                             QWidget *parent)
    : QDialog(parent)
    , m_database(database)
    , m_defaultCategoryId(defaultCategoryId)
{
    m_editing = existing != nullptr;
    if (existing)
        m_original = *existing;

    setWindowTitle(m_editing ? tr("Редактирование сниппета")
                             : tr("Новый сниппет"));
    setWindowIcon(flatchIcon());
    setModal(true);
    setSizeGripEnabled(true);
    fitDialogToScreen(this, QSize(780, 640), QSize(640, 480));

    buildUi();
    loadCategories();

    if (m_editing)
        loadExisting();
    else
        onKindChanged(m_kind->currentIndex());
}

void SnippetDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("Например: Boosty"));
    m_name->setMaxLength(120);
    form->addRow(tr("Название:"), m_name);

    m_category = new QComboBox;
    form->addRow(tr("Категория:"), m_category);

    m_kind = new QComboBox;
    m_kind->addItem(tr("Простой текст"), static_cast<int>(SnippetKind::PlainText));
    m_kind->addItem(tr("Форматированный текст"), static_cast<int>(SnippetKind::RichText));
    m_kind->addItem(tr("Файлы"), static_cast<int>(SnippetKind::Files));
    m_kind->addItem(tr("Приложение"), static_cast<int>(SnippetKind::Application));
    form->addRow(tr("Тип:"), m_kind);

    mainLayout->addLayout(form);

    m_stack = new QStackedWidget;

    // --- Plain text ---
    m_plain = new QPlainTextEdit;
    m_plain->setPlaceholderText(tr("Введите текст, который нужно копировать…"));
    m_plain->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_stack->addWidget(m_plain);

    // --- Rich text ---
    auto *richPage = new QWidget;
    auto *richLayout = new QVBoxLayout(richPage);
    richLayout->setContentsMargins(0, 0, 0, 0);
    richLayout->setSpacing(6);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(4);

    auto makeFormatButton = [](const QString &text, const QString &toolTip, bool checkable) {
        auto *button = new QToolButton;
        button->setText(text);
        button->setToolTip(toolTip);
        button->setCheckable(checkable);
        button->setAutoRaise(true);
        button->setFixedSize(30, 28);
        return button;
    };

    m_bold = makeFormatButton(tr("Ж"), tr("Полужирный (Ctrl+B)"), true);
    QFont boldFont = m_bold->font();
    boldFont.setBold(true);
    m_bold->setFont(boldFont);
    toolbar->addWidget(m_bold);

    m_italic = makeFormatButton(tr("К"), tr("Курсив (Ctrl+I)"), true);
    QFont italicFont = m_italic->font();
    italicFont.setItalic(true);
    m_italic->setFont(italicFont);
    toolbar->addWidget(m_italic);

    m_underline = makeFormatButton(tr("Ч"), tr("Подчёркнутый (Ctrl+U)"), true);
    QFont underlineFont = m_underline->font();
    underlineFont.setUnderline(true);
    m_underline->setFont(underlineFont);
    toolbar->addWidget(m_underline);

    toolbar->addWidget(makeSeparator());

    m_fontSize = new QComboBox;
    m_fontSize->setEditable(true);
    m_fontSize->setFixedWidth(64);
    m_fontSize->setToolTip(tr("Размер шрифта"));
    m_fontSize->setValidator(new QDoubleValidator(4.0, 200.0, 1, m_fontSize));
    const int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48};
    for (int size : sizes)
        m_fontSize->addItem(QString::number(size));
    m_fontSize->setCurrentText(QStringLiteral("12"));
    toolbar->addWidget(m_fontSize);

    auto *colorButton = new QToolButton;
    colorButton->setText(tr("Цвет"));
    colorButton->setToolTip(tr("Цвет текста"));
    colorButton->setAutoRaise(true);
    colorButton->setFixedHeight(28);
    toolbar->addWidget(colorButton);

    toolbar->addWidget(makeSeparator());

    auto *bulletButton = makeFormatButton(QStringLiteral("•"), tr("Маркированный список"), false);
    toolbar->addWidget(bulletButton);
    auto *numberButton = makeFormatButton(QStringLiteral("1."), tr("Нумерованный список"), false);
    toolbar->addWidget(numberButton);

    auto *clearButton = new QToolButton;
    clearButton->setText(tr("Сброс"));
    clearButton->setToolTip(tr("Убрать форматирование"));
    clearButton->setAutoRaise(true);
    clearButton->setFixedHeight(28);
    toolbar->addWidget(clearButton);

    toolbar->addStretch(1);
    richLayout->addLayout(toolbar);

    m_rich = new QTextEdit;
    m_rich->setAcceptRichText(true);
    m_rich->setPlaceholderText(tr("Введите текст с оформлением…"));
    richLayout->addWidget(m_rich, 1);
    m_stack->addWidget(richPage);

    // --- Files ---
    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(6);

    m_files = new QListWidget;
    m_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_files->setAlternatingRowColors(true);
    filesLayout->addWidget(m_files, 1);

    auto *filesButtons = new QHBoxLayout;
    auto *addFilesButton = new QPushButton(tr("Добавить файлы…"));
    auto *removeFilesButton = new QPushButton(tr("Удалить"));
    filesButtons->addWidget(addFilesButton);
    filesButtons->addWidget(removeFilesButton);
    filesButtons->addStretch(1);
    filesLayout->addLayout(filesButtons);
    filesLayout->addWidget(makeHintLabel(
        tr("Файлы будут помещены в буфер обмена как файловые объекты. "
           "Вставляйте их в проводник, почту или другое приложение.")));

    m_stack->addWidget(filesPage);

    // --- Application ---
    auto *appPage = new QWidget;
    auto *appLayout = new QVBoxLayout(appPage);
    appLayout->setContentsMargins(0, 0, 0, 0);
    appLayout->setSpacing(8);

    appLayout->addWidget(makeHintLabel(
        tr("Приложение будет запущено сразу после выбора сниппета. "
           "Flatch не ждёт его завершения и не управляет им.")));

    auto *appRow = new QHBoxLayout;
    m_appPath = new QLineEdit;
    m_appPath->setReadOnly(true);
    m_appPath->setPlaceholderText(tr("Путь к исполняемому файлу"));
    auto *browseAppButton = new QPushButton(tr("Выбрать…"));
    auto *clearAppButton = new QPushButton(tr("Очистить"));
    appRow->addWidget(m_appPath, 1);
    appRow->addWidget(browseAppButton);
    appRow->addWidget(clearAppButton);
    appLayout->addLayout(appRow);
    appLayout->addStretch(1);

    m_stack->addWidget(appPage);

    mainLayout->addWidget(makeSectionLabel(tr("Содержимое")));
    mainLayout->addWidget(m_stack, 1);

    m_encrypt = new QCheckBox(
        tr("Хранить содержимое в зашифрованном виде (логины и пароли)"));
    m_encrypt->setToolTip(
        tr("Сниппет хранится в базе в зашифрованном виде, "
           "а в буфер обмена попадает расшифрованным."));
    mainLayout->addWidget(m_encrypt);

    mainLayout->addWidget(makeHintLabel(
        tr("Ключ шифрования хранится в профиле пользователя и защищён средствами "
           "операционной системы. Отдельный пароль для Flatch не нужен.")));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (QPushButton *saveButton = buttons->button(QDialogButtonBox::Save))
        saveButton->setText(tr("Сохранить"));
    if (QPushButton *cancelButton = buttons->button(QDialogButtonBox::Cancel))
        cancelButton->setText(tr("Отмена"));
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SnippetDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SnippetDialog::reject);
    connect(m_kind, &QComboBox::currentIndexChanged, this, &SnippetDialog::onKindChanged);
    connect(addFilesButton, &QPushButton::clicked, this, &SnippetDialog::chooseFiles);
    connect(removeFilesButton, &QPushButton::clicked, this, &SnippetDialog::removeSelectedFiles);
    connect(browseAppButton, &QPushButton::clicked, this, &SnippetDialog::chooseApplication);
    connect(clearAppButton, &QPushButton::clicked, m_appPath, &QLineEdit::clear);
    connect(colorButton, &QToolButton::clicked, this, &SnippetDialog::chooseTextColor);
    connect(bulletButton, &QToolButton::clicked, this, [this] {
        toggleList(static_cast<int>(QTextListFormat::ListDisc));
    });
    connect(numberButton, &QToolButton::clicked, this, [this] {
        toggleList(static_cast<int>(QTextListFormat::ListDecimal));
    });
    connect(clearButton, &QToolButton::clicked, this, &SnippetDialog::clearFormatting);

    connect(m_bold, &QToolButton::toggled, this, [this](bool checked) {
        if (m_updatingFormatControls)
            return;
        QTextCharFormat format;
        format.setFontWeight(checked ? QFont::Bold : QFont::Normal);
        mergeCharFormat(format);
    });
    connect(m_italic, &QToolButton::toggled, this, [this](bool checked) {
        if (m_updatingFormatControls)
            return;
        QTextCharFormat format;
        format.setFontItalic(checked);
        mergeCharFormat(format);
    });
    connect(m_underline, &QToolButton::toggled, this, [this](bool checked) {
        if (m_updatingFormatControls)
            return;
        QTextCharFormat format;
        format.setFontUnderline(checked);
        mergeCharFormat(format);
    });
    connect(m_fontSize, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (m_updatingFormatControls)
            return;
        bool ok = false;
        const double size = text.toDouble(&ok);
        if (!ok || size < 4.0 || size > 200.0)
            return;
        QTextCharFormat format;
        format.setFontPointSize(size);
        mergeCharFormat(format);
    });

    connect(m_rich, &QTextEdit::currentCharFormatChanged, this,
            [this](const QTextCharFormat &) { updateFormatButtons(); });
    connect(m_rich, &QTextEdit::cursorPositionChanged, this, &SnippetDialog::updateFormatButtons);

    auto *boldShortcut = new QShortcut(QKeySequence::Bold, m_rich);
    boldShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(boldShortcut, &QShortcut::activated, this, [this] { m_bold->toggle(); });

    auto *italicShortcut = new QShortcut(QKeySequence::Italic, m_rich);
    italicShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(italicShortcut, &QShortcut::activated, this, [this] { m_italic->toggle(); });

    auto *underlineShortcut = new QShortcut(QKeySequence::Underline, m_rich);
    underlineShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(underlineShortcut, &QShortcut::activated, this, [this] { m_underline->toggle(); });

    connect(m_encrypt, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked)
            return;
        QString error;
        if (!Crypto::isAvailable(&error)) {
            QSignalBlocker blocker(m_encrypt);
            m_encrypt->setChecked(false);
            QMessageBox::warning(this, tr("Шифрование недоступно"), error);
        }
    });
}

void SnippetDialog::loadCategories()
{
    QString error;
    const QVector<Category> categories = m_database->categories(&error);
    if (!error.isEmpty()) {
        showWarning(error);
        return;
    }

    for (const Category &category : categories)
        m_category->addItem(category.name, category.id);

    int index = m_category->findData(m_editing ? m_original.categoryId : m_defaultCategoryId);
    if (index < 0 && m_category->count() > 0)
        index = 0;
    m_category->setCurrentIndex(index);
}

void SnippetDialog::loadExisting()
{
    m_name->setText(m_original.name);

    const int kindIndex = m_kind->findData(static_cast<int>(m_original.kind));
    if (kindIndex >= 0)
        m_kind->setCurrentIndex(kindIndex);
    onKindChanged(m_kind->currentIndex());

    QString stored;
    const bool contentLoaded = loadStoredContent(&stored);

    switch (m_original.kind) {
    case SnippetKind::PlainText:
        if (contentLoaded)
            m_plain->setPlainText(stored);
        break;
    case SnippetKind::RichText:
        if (contentLoaded)
            m_rich->setHtml(stored);
        break;
    case SnippetKind::Files:
        if (contentLoaded) {
            for (const QString &path : decodePaths(stored))
                addFilePath(path);
        }
        break;
    case SnippetKind::Application: {
        if (contentLoaded) {
            const QStringList paths = decodePaths(stored);
            if (!paths.isEmpty())
                m_appPath->setText(paths.first());
        }
        break;
    }
    }

    {
        QSignalBlocker blocker(m_encrypt);
        m_encrypt->setChecked(m_original.encrypted);
    }
    m_encrypt->setEnabled(isTextKind(m_original.kind));

    if (m_original.kind == SnippetKind::RichText)
        updateFormatButtons();

    if (m_editing)
        m_name->setFocus();
    m_name->selectAll();
}

bool SnippetDialog::loadStoredContent(QString *content)
{
    if (!m_original.encrypted) {
        *content = m_original.content;
        return true;
    }

    QString error;
    if (!Crypto::decrypt(m_original.content, content, &error)) {
        showWarning(error
                    + tr("\n\nСодержимое не загружено. Если сохранить сниппет, "
                         "его нужно будет заполнить заново."));
        content->clear();
        return false;
    }
    return true;
}

void SnippetDialog::onKindChanged(int index)
{
    const SnippetKind kind = currentKind();
    m_stack->setCurrentIndex(qBound(0, index, m_stack->count() - 1));

    const bool text = isTextKind(kind);
    m_encrypt->setEnabled(text);
    if (!text) {
        QSignalBlocker blocker(m_encrypt);
        m_encrypt->setChecked(false);
    }
}

SnippetKind SnippetDialog::currentKind() const
{
    const int value = m_kind->currentData().toInt();
    switch (value) {
    case static_cast<int>(SnippetKind::PlainText):
    case static_cast<int>(SnippetKind::RichText):
    case static_cast<int>(SnippetKind::Files):
    case static_cast<int>(SnippetKind::Application):
        return static_cast<SnippetKind>(value);
    default:
        return SnippetKind::PlainText;
    }
}

void SnippetDialog::chooseFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Добавить файлы"), documentsDirectory());
    for (const QString &path : files)
        addFilePath(path);
}

void SnippetDialog::addFilePath(const QString &path)
{
    if (path.trimmed().isEmpty())
        return;

    const QFileInfo info(path);
    const QString absolute = QDir::cleanPath(info.absoluteFilePath());
    if (absolute.isEmpty())
        return;

    for (int i = 0; i < m_files->count(); ++i) {
        if (m_files->item(i)->data(Qt::UserRole).toString() == absolute)
            return;
    }

    const QString label = info.fileName().isEmpty() ? absolute : info.fileName();
    auto *item = new QListWidgetItem(label, m_files);
    item->setData(Qt::UserRole, absolute);
    item->setToolTip(QDir::toNativeSeparators(absolute));
}

void SnippetDialog::removeSelectedFiles()
{
    const QList<QListWidgetItem *> selected = m_files->selectedItems();
    for (QListWidgetItem *item : selected)
        delete item;
}

void SnippetDialog::chooseApplication()
{
    QString startDirectory = QDir::homePath();
#ifdef Q_OS_WIN
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    if (!programFiles.isEmpty() && QDir(programFiles).exists())
        startDirectory = programFiles;
#endif

#ifdef Q_OS_WIN
    const QString filter = tr(
        "Программы (*.exe *.bat *.cmd *.com *.lnk);;Все файлы (*.*)");
#else
    const QString filter = tr("Все файлы (*)");
#endif

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Выбрать приложение"), startDirectory, filter);
    if (!path.isEmpty())
        m_appPath->setText(QDir::cleanPath(path));
}

QStringList SnippetDialog::currentPaths() const
{
    QStringList paths;
    switch (currentKind()) {
    case SnippetKind::Files:
        for (int i = 0; i < m_files->count(); ++i) {
            const QString path = m_files->item(i)->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                paths.append(path);
        }
        break;
    case SnippetKind::Application:
        if (!m_appPath->text().trimmed().isEmpty())
            paths.append(m_appPath->text().trimmed());
        break;
    default:
        break;
    }
    return paths;
}

void SnippetDialog::updateFormatButtons()
{
    if (!m_rich)
        return;

    m_updatingFormatControls = true;
    const QTextCharFormat format = m_rich->currentCharFormat();
    m_bold->setChecked(format.fontWeight() >= QFont::Bold);
    m_italic->setChecked(format.fontItalic());
    m_underline->setChecked(format.fontUnderline());
    if (format.hasProperty(QTextFormat::FontPointSize))
        m_fontSize->setEditText(QString::number(format.fontPointSize(), 'g', 4));
    m_updatingFormatControls = false;
}

void SnippetDialog::mergeCharFormat(const QTextCharFormat &format)
{
    QTextCursor cursor = m_rich->textCursor();
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(format);
    } else {
        const int position = cursor.position();
        cursor.select(QTextCursor::WordUnderCursor);
        cursor.mergeCharFormat(format);
        cursor.clearSelection();
        cursor.setPosition(position);
        m_rich->setTextCursor(cursor);
    }
    m_rich->mergeCurrentCharFormat(format);
    m_rich->setFocus();
    updateFormatButtons();
}

void SnippetDialog::chooseTextColor()
{
    QColor current = m_rich->textColor();
    const QTextCharFormat format = m_rich->currentCharFormat();
    if (format.hasProperty(QTextFormat::ForegroundBrush))
        current = format.foreground().color();

    const QColor color = QColorDialog::getColor(current, this, tr("Цвет текста"));
    if (!color.isValid())
        return;

    QTextCharFormat colorFormat;
    colorFormat.setForeground(color);
    mergeCharFormat(colorFormat);
}

void SnippetDialog::toggleList(int style)
{
    QTextCursor cursor = m_rich->textCursor();
    QTextList *list = cursor.currentList();

    if (list && static_cast<int>(list->format().style()) == style) {
        list->remove(cursor.block());
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setIndent(0);
        cursor.setBlockFormat(blockFormat);
    } else if (list) {
        QTextListFormat listFormat = list->format();
        listFormat.setStyle(static_cast<QTextListFormat::Style>(style));
        list->setFormat(listFormat);
    } else {
        QTextListFormat listFormat;
        listFormat.setStyle(static_cast<QTextListFormat::Style>(style));
        listFormat.setIndent(1);
        cursor.createList(listFormat);
    }

    m_rich->setFocus();
}

void SnippetDialog::clearFormatting()
{
    QTextCursor cursor = m_rich->textCursor();
    if (cursor.hasSelection())
        cursor.setCharFormat(QTextCharFormat());
    else
        m_rich->setCurrentCharFormat(QTextCharFormat());

    if (QTextList *list = cursor.currentList()) {
        list->remove(cursor.block());
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setIndent(0);
        cursor.setBlockFormat(blockFormat);
    }

    m_rich->setFocus();
    updateFormatButtons();
}

void SnippetDialog::accept()
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        showWarning(tr("Введите название сниппета."));
        m_name->setFocus();
        return;
    }

    if (m_category->currentIndex() < 0) {
        showWarning(tr("Сначала создайте хотя бы одну категорию."));
        return;
    }

    const SnippetKind kind = currentKind();
    QString content;
    const QStringList paths = currentPaths();

    if (kind == SnippetKind::PlainText) {
        content = m_plain->toPlainText();
        if (content.trimmed().isEmpty()) {
            showWarning(tr("Введите текст сниппета."));
            m_plain->setFocus();
            return;
        }
    } else if (kind == SnippetKind::RichText) {
        if (m_rich->toPlainText().trimmed().isEmpty()) {
            showWarning(tr("Введите текст сниппета."));
            m_rich->setFocus();
            return;
        }
        content = m_rich->toHtml();
    } else {
        if (paths.isEmpty()) {
            if (kind == SnippetKind::Files)
                showWarning(tr("Добавьте хотя бы один файл."));
            else
                showWarning(tr("Выберите приложение."));
            return;
        }

        if (kind == SnippetKind::Application && !QFileInfo::exists(paths.first())) {
            const auto answer = QMessageBox::question(
                this, tr("Приложение не найдено"),
                tr("Файл «%1» не найден. Сохранить сниппет всё равно?")
                    .arg(QDir::toNativeSeparators(paths.first())),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }

        content = encodePaths(paths);
    }

    const bool encrypt = m_encrypt->isEnabled() && m_encrypt->isChecked();
    if (encrypt) {
        QString encrypted;
        QString error;
        if (!Crypto::encrypt(content, &encrypted, &error)) {
            showWarning(tr("Не удалось зашифровать содержимое.\n\n") + error);
            return;
        }
        content = encrypted;
    }

    Snippet snippet;
    snippet.categoryId = m_category->currentData().toInt();
    snippet.name = name;
    snippet.kind = kind;
    snippet.content = content;
    snippet.encrypted = encrypt;

    QString error;
    int snippetId = -1;
    if (m_editing) {
        snippet.id = m_original.id;
        if (!m_database->updateSnippet(snippet, &error)) {
            showWarning(tr("Не удалось сохранить сниппет.\n\n") + error);
            return;
        }
        snippetId = m_original.id;
    } else {
        if (!m_database->addSnippet(snippet, &snippetId, &error)) {
            showWarning(tr("Не удалось сохранить сниппет.\n\n") + error);
            return;
        }
    }

    m_savedSnippetId = snippetId;
    m_savedCategoryId = snippet.categoryId;
    QDialog::accept();
}

void SnippetDialog::showWarning(const QString &text)
{
    QMessageBox::warning(this, QStringLiteral("Flatch"), text);
}
