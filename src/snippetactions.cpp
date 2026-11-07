#include "snippetactions.h"

#include "crypto.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QTextDocument>
#include <QUrl>

namespace {

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

bool decodedText(const Snippet &snippet, QString *text, QString *error)
{
    if (!snippet.encrypted) {
        *text = snippet.content;
        return true;
    }
    return Crypto::decrypt(snippet.content, text, error);
}

void copyText(const QString &text)
{
    QApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void copyRichText(const QString &html)
{
    auto *mime = new QMimeData;
    mime->setHtml(html);

    QTextDocument document;
    document.setHtml(html);
    mime->setText(document.toPlainText());

    QApplication::clipboard()->setMimeData(mime, QClipboard::Clipboard);
}

void copyFiles(const QStringList &paths)
{
    QList<QUrl> urls;
    QStringList textPaths;
    urls.reserve(paths.size());
    for (const QString &path : paths) {
        const QString cleaned = QDir::cleanPath(path);
        if (cleaned.isEmpty())
            continue;
        urls.append(QUrl::fromLocalFile(cleaned));
        textPaths.append(QDir::toNativeSeparators(cleaned));
    }

    auto *mime = new QMimeData;
    mime->setUrls(urls);
    mime->setText(textPaths.join(QLatin1Char('\n')));
    QApplication::clipboard()->setMimeData(mime, QClipboard::Clipboard);
}

bool launchApplication(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        setError(error, QCoreApplication::translate(
                            "SnippetActions", "Приложение не найдено: «%1».").arg(path));
        return false;
    }

    const QString program = info.absoluteFilePath();
    const QString workingDirectory = info.absolutePath();

    bool started = false;

#ifdef Q_OS_WIN
    const QString suffix = info.suffix().toLower();
    if (suffix == QLatin1String("bat") || suffix == QLatin1String("cmd")) {
        started = QProcess::startDetached(QStringLiteral("cmd.exe"),
                                          QStringList{QStringLiteral("/c"), program},
                                          workingDirectory);
    } else {
        started = QProcess::startDetached(program, QStringList(), workingDirectory);
    }
#else
    if (info.suffix().compare(QLatin1String("desktop"), Qt::CaseInsensitive) == 0) {
        started = QDesktopServices::openUrl(QUrl::fromLocalFile(program));
    } else {
        started = QProcess::startDetached(program, QStringList(), workingDirectory);
    }
#endif

    if (!started) {
        // Fallback through the desktop shell: also handles .lnk, documents and
        // scripts that are not directly executable.
        started = QDesktopServices::openUrl(QUrl::fromLocalFile(program));
    }

    if (!started) {
        setError(error, QCoreApplication::translate(
                            "SnippetActions", "Не удалось запустить приложение «%1».").arg(program));
        return false;
    }
    return true;
}

} // namespace

namespace SnippetActions {

bool activate(const Snippet &snippet, QString *error)
{
    if (error)
        error->clear();

    switch (snippet.kind) {
    case SnippetKind::PlainText: {
        QString text;
        if (!decodedText(snippet, &text, error))
            return false;
        copyText(text);
        return true;
    }

    case SnippetKind::RichText: {
        QString html;
        if (!decodedText(snippet, &html, error))
            return false;
        copyRichText(html);
        return true;
    }

    case SnippetKind::Files: {
        const QStringList paths = decodePaths(snippet.content);
        if (paths.isEmpty()) {
            setError(error, QCoreApplication::translate(
                                "SnippetActions", "В сниппете не указаны файлы."));
            return false;
        }
        copyFiles(paths);
        return true;
    }

    case SnippetKind::Application: {
        const QStringList paths = decodePaths(snippet.content);
        if (paths.isEmpty()) {
            setError(error, QCoreApplication::translate(
                                "SnippetActions", "В сниппете не указан путь к приложению."));
            return false;
        }
        return launchApplication(paths.first(), error);
    }
    }

    setError(error, QCoreApplication::translate("SnippetActions", "Неизвестный тип сниппета."));
    return false;
}

} // namespace SnippetActions
