#pragma once

#include "snippet.h"

#include <QObject>
#include <QSqlDatabase>
#include <QVector>

class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    bool open(QString *error = nullptr);
    bool wasCreated() const { return m_created; }
    QString filePath() const { return m_path; }

    QVector<Category> categories(QString *error = nullptr) const;
    bool addCategory(const QString &name, int *newId = nullptr, QString *error = nullptr);
    bool renameCategory(int id, const QString &name, QString *error = nullptr);
    bool removeCategory(int id, QString *error = nullptr);

    QVector<Snippet> snippets(int categoryId, QString *error = nullptr) const;
    bool snippet(int id, Snippet *out, QString *error = nullptr) const;
    bool addSnippet(const Snippet &snippet, int *newId = nullptr, QString *error = nullptr);
    bool updateSnippet(const Snippet &snippet, QString *error = nullptr);
    bool removeSnippet(int id, QString *error = nullptr);

private:
    bool createSchema(QString *error);
    bool seedIfEmpty(QString *error);

    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_path;
    bool m_created = false;
};
