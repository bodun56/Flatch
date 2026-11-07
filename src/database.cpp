#include "database.h"

#include "apppaths.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace {

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

bool isUniqueViolation(const QSqlError &error)
{
    const QString native = error.nativeErrorCode();
    return native == QLatin1String("19") || native == QLatin1String("2067")
           || error.text().contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive);
}

SnippetKind kindFromInt(int value)
{
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

Snippet readSnippet(const QSqlQuery &query)
{
    Snippet snippet;
    snippet.id = query.value(0).toInt();
    snippet.categoryId = query.value(1).toInt();
    snippet.name = query.value(2).toString();
    snippet.kind = kindFromInt(query.value(3).toInt());
    snippet.content = query.value(4).toString();
    snippet.encrypted = query.value(5).toInt() != 0;
    return snippet;
}

const char *kSnippetColumns =
    "id, category_id, name, kind, content, encrypted";

// SQLite's NOCASE collation only folds ASCII letters, so Cyrillic names like
// "Работа" and "работа" would be treated as different. Do the check in Qt,
// which handles Unicode case folding correctly.
bool categoryNameExists(const QSqlDatabase &database, const QString &name, int excludeId,
                        QString *error)
{
    QSqlQuery query(database);
    if (excludeId >= 0) {
        query.prepare(QStringLiteral("SELECT name FROM categories WHERE id <> ?"));
        query.addBindValue(excludeId);
    } else {
        query.prepare(QStringLiteral("SELECT name FROM categories"));
    }
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }

    while (query.next()) {
        if (query.value(0).toString().compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool snippetNameExists(const QSqlDatabase &database, int categoryId, const QString &name,
                       int excludeId, QString *error)
{
    QSqlQuery query(database);
    if (excludeId >= 0) {
        query.prepare(QStringLiteral(
            "SELECT name FROM snippets WHERE category_id = ? AND id <> ?"));
        query.addBindValue(categoryId);
        query.addBindValue(excludeId);
    } else {
        query.prepare(QStringLiteral(
            "SELECT name FROM snippets WHERE category_id = ?"));
        query.addBindValue(categoryId);
    }
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }

    while (query.next()) {
        if (query.value(0).toString().compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    if (m_db.isValid())
        m_db.close();
    m_db = QSqlDatabase();
    if (!m_connectionName.isEmpty())
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open(QString *error)
{
    if (error)
        error->clear();

    m_path = AppPaths::databaseFile();
    const QFileInfo info(m_path);
    m_created = !info.exists() || info.size() == 0;

    m_connectionName = QStringLiteral("flatch_db_%1")
                           .arg(reinterpret_cast<quintptr>(this), 0, 16);
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_path);
    if (!m_db.open()) {
        setError(error, QStringLiteral("Не удалось открыть базу данных «%1»: %2")
                            .arg(m_path, m_db.lastError().text()));
        return false;
    }

    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setError(error, pragma.lastError().text());
        return false;
    }
    pragma.exec(QStringLiteral("PRAGMA secure_delete = ON"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));

    int version = 0;
    if (pragma.exec(QStringLiteral("PRAGMA user_version")) && pragma.next())
        version = pragma.value(0).toInt();

    if (version > 1) {
        setError(error, QStringLiteral("База данных создана более новой версией Flatch "
                                       "(версия схемы %1). Обновите приложение.").arg(version));
        return false;
    }

    if (version < 1) {
        if (!createSchema(error))
            return false;
    }

    if (!seedIfEmpty(error))
        return false;

    return true;
}

bool Database::createSchema(QString *error)
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS categories ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " name TEXT NOT NULL COLLATE NOCASE UNIQUE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS snippets ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " category_id INTEGER NOT NULL REFERENCES categories(id) ON DELETE CASCADE,"
            " name TEXT NOT NULL COLLATE NOCASE,"
            " kind INTEGER NOT NULL,"
            " content TEXT NOT NULL DEFAULT '',"
            " encrypted INTEGER NOT NULL DEFAULT 0,"
            " created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            " updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            " UNIQUE(category_id, name)"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_snippets_category_id ON snippets(category_id)")
    };

    if (!m_db.transaction()) {
        setError(error, m_db.lastError().text());
        return false;
    }

    QSqlQuery query(m_db);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            setError(error, QStringLiteral("Не удалось создать структуру базы данных: %1")
                                .arg(query.lastError().text()));
            m_db.rollback();
            return false;
        }
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version = 1"))) {
        setError(error, query.lastError().text());
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        setError(error, m_db.lastError().text());
        m_db.rollback();
        return false;
    }
    return true;
}

bool Database::seedIfEmpty(QString *error)
{
    QSqlQuery count(m_db);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM categories"))) {
        setError(error, count.lastError().text());
        return false;
    }
    if (!count.next())
        return true;

    if (count.value(0).toInt() == 0) {
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral("INSERT INTO categories(name) VALUES(?)"));
        insert.addBindValue(QStringLiteral("Общее"));
        if (!insert.exec()) {
            setError(error, insert.lastError().text());
            return false;
        }
    }
    return true;
}

QVector<Category> Database::categories(QString *error) const
{
    if (error)
        error->clear();

    QVector<Category> result;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT id, name FROM categories "
                                   "ORDER BY name COLLATE NOCASE"))) {
        setError(error, query.lastError().text());
        return result;
    }

    while (query.next()) {
        Category category;
        category.id = query.value(0).toInt();
        category.name = query.value(1).toString();
        result.append(category);
    }
    return result;
}

bool Database::addCategory(const QString &name, int *newId, QString *error)
{
    if (error)
        error->clear();

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setError(error, QStringLiteral("Название категории не может быть пустым."));
        return false;
    }
    const bool categoryExists = categoryNameExists(m_db, trimmed, -1, error);
    if (error && !error->isEmpty())
        return false;
    if (categoryExists) {
        setError(error, QStringLiteral("Категория с названием «%1» уже существует.").arg(trimmed));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO categories(name) VALUES(?)"));
    query.addBindValue(trimmed);
    if (!query.exec()) {
        if (isUniqueViolation(query.lastError()))
            setError(error, QStringLiteral("Категория с названием «%1» уже существует.").arg(trimmed));
        else
            setError(error, query.lastError().text());
        return false;
    }

    if (newId)
        *newId = query.lastInsertId().toInt();
    return true;
}

bool Database::renameCategory(int id, const QString &name, QString *error)
{
    if (error)
        error->clear();

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        setError(error, QStringLiteral("Название категории не может быть пустым."));
        return false;
    }
    const bool categoryExists = categoryNameExists(m_db, trimmed, id, error);
    if (error && !error->isEmpty())
        return false;
    if (categoryExists) {
        setError(error, QStringLiteral("Категория с названием «%1» уже существует.").arg(trimmed));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE categories SET name = ? WHERE id = ?"));
    query.addBindValue(trimmed);
    query.addBindValue(id);
    if (!query.exec()) {
        if (isUniqueViolation(query.lastError()))
            setError(error, QStringLiteral("Категория с названием «%1» уже существует.").arg(trimmed));
        else
            setError(error, query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() == 0) {
        setError(error, QStringLiteral("Категория не найдена."));
        return false;
    }
    return true;
}

bool Database::removeCategory(int id, QString *error)
{
    if (error)
        error->clear();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM categories WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setError(error, QStringLiteral("Категория не найдена."));
        return false;
    }
    return true;
}

QVector<Snippet> Database::snippets(int categoryId, QString *error) const
{
    if (error)
        error->clear();

    QVector<Snippet> result;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM snippets "
                                 "WHERE category_id = ? ORDER BY name COLLATE NOCASE")
                      .arg(QLatin1String(kSnippetColumns)));
    query.addBindValue(categoryId);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return result;
    }

    while (query.next())
        result.append(readSnippet(query));
    return result;
}

bool Database::snippet(int id, Snippet *out, QString *error) const
{
    if (error)
        error->clear();
    if (!out) {
        setError(error, QStringLiteral("Внутренняя ошибка: не передан получатель данных."));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM snippets WHERE id = ?")
                      .arg(QLatin1String(kSnippetColumns)));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }
    if (!query.next()) {
        setError(error, QStringLiteral("Сниппет не найден."));
        return false;
    }

    *out = readSnippet(query);
    return true;
}

bool Database::addSnippet(const Snippet &snippet, int *newId, QString *error)
{
    if (error)
        error->clear();

    const QString name = snippet.name.trimmed();
    const bool snippetExists = snippetNameExists(m_db, snippet.categoryId, name, -1, error);
    if (error && !error->isEmpty())
        return false;
    if (snippetExists) {
        setError(error, QStringLiteral("В этой категории уже есть сниппет с названием «%1».")
                            .arg(name));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO snippets(category_id, name, kind, content, encrypted) "
        "VALUES(?, ?, ?, ?, ?)"));
    query.addBindValue(snippet.categoryId);
    query.addBindValue(name);
    query.addBindValue(static_cast<int>(snippet.kind));
    query.addBindValue(snippet.content);
    query.addBindValue(snippet.encrypted ? 1 : 0);
    if (!query.exec()) {
        if (isUniqueViolation(query.lastError()))
            setError(error, QStringLiteral("В этой категории уже есть сниппет с названием «%1».")
                                .arg(name));
        else
            setError(error, query.lastError().text());
        return false;
    }

    if (newId)
        *newId = query.lastInsertId().toInt();
    return true;
}

bool Database::updateSnippet(const Snippet &snippet, QString *error)
{
    if (error)
        error->clear();

    const QString name = snippet.name.trimmed();
    const bool snippetExists =
        snippetNameExists(m_db, snippet.categoryId, name, snippet.id, error);
    if (error && !error->isEmpty())
        return false;
    if (snippetExists) {
        setError(error, QStringLiteral("В этой категории уже есть сниппет с названием «%1».")
                            .arg(name));
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE snippets SET category_id = ?, name = ?, kind = ?, content = ?, "
        "encrypted = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?"));
    query.addBindValue(snippet.categoryId);
    query.addBindValue(name);
    query.addBindValue(static_cast<int>(snippet.kind));
    query.addBindValue(snippet.content);
    query.addBindValue(snippet.encrypted ? 1 : 0);
    query.addBindValue(snippet.id);
    if (!query.exec()) {
        if (isUniqueViolation(query.lastError()))
            setError(error, QStringLiteral("В этой категории уже есть сниппет с названием «%1».")
                                .arg(name));
        else
            setError(error, query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() == 0) {
        setError(error, QStringLiteral("Сниппет не найден."));
        return false;
    }
    return true;
}

bool Database::removeSnippet(int id, QString *error)
{
    if (error)
        error->clear();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM snippets WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() == 0) {
        setError(error, QStringLiteral("Сниппет не найден."));
        return false;
    }
    return true;
}
