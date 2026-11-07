#include "snippet.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

QString snippetKindName(SnippetKind kind)
{
    switch (kind) {
    case SnippetKind::PlainText:
        return QCoreApplication::translate("SnippetKind", "Простой текст");
    case SnippetKind::RichText:
        return QCoreApplication::translate("SnippetKind", "Форматированный текст");
    case SnippetKind::Files:
        return QCoreApplication::translate("SnippetKind", "Файлы");
    case SnippetKind::Application:
        return QCoreApplication::translate("SnippetKind", "Приложение");
    }
    return QCoreApplication::translate("SnippetKind", "Неизвестно");
}

QString encodePaths(const QStringList &paths)
{
    QJsonArray array;
    for (const QString &path : paths) {
        if (!path.trimmed().isEmpty())
            array.append(path);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QStringList decodePaths(const QString &stored)
{
    QStringList result;
    const QByteArray bytes = stored.toUtf8();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isArray()) {
        const QJsonArray array = doc.array();
        result.reserve(array.size());
        for (const QJsonValue &value : array) {
            if (value.isString()) {
                const QString path = value.toString();
                if (!path.isEmpty())
                    result.append(path);
            }
        }
        return result;
    }

    // Very old / manually edited databases: accept one path per line.
    const QStringList lines = stored.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString path = line.trimmed();
        if (!path.isEmpty())
            result.append(path);
    }
    return result;
}
