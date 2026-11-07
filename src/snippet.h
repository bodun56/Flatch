#pragma once

#include <QString>
#include <QStringList>

enum class SnippetKind : int {
    PlainText = 0,
    RichText = 1,
    Files = 2,
    Application = 3
};

struct Category {
    int id = -1;
    QString name;
};

struct Snippet {
    int id = -1;
    int categoryId = -1;
    QString name;
    SnippetKind kind = SnippetKind::PlainText;
    QString content;
    bool encrypted = false;
};

inline bool isTextKind(SnippetKind kind)
{
    return kind == SnippetKind::PlainText || kind == SnippetKind::RichText;
}

QString snippetKindName(SnippetKind kind);

// File lists are stored in the database as a compact JSON array.
QString encodePaths(const QStringList &paths);
QStringList decodePaths(const QString &stored);
