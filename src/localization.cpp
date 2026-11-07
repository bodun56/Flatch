#include "localization.h"

#include "preferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {

QTranslator *s_appTranslator = nullptr;
QTranslator *s_qtTranslator = nullptr;

void removeTranslators()
{
    if (s_appTranslator) {
        QCoreApplication::removeTranslator(s_appTranslator);
        delete s_appTranslator;
        s_appTranslator = nullptr;
    }
    if (s_qtTranslator) {
        QCoreApplication::removeTranslator(s_qtTranslator);
        delete s_qtTranslator;
        s_qtTranslator = nullptr;
    }
}

QString localTranslationsDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
}

// The application's source strings are Russian, so only English needs a
// translation file. It is embedded into the executable at :/i18n/ and can be
// overridden by a file next to the executable.
bool loadAppTranslator(const QString &code)
{
    if (code != QLatin1String("en"))
        return false;

    auto *translator = new QTranslator;
    const bool loaded =
        translator->load(QStringLiteral("flatch_") + code, localTranslationsDirectory())
        || translator->load(QStringLiteral(":/i18n/flatch_") + code + QStringLiteral(".qm"));
    if (!loaded) {
        delete translator;
        return false;
    }

    s_appTranslator = translator;
    QCoreApplication::installTranslator(s_appTranslator);
    return true;
}

// Qt's own strings (standard dialog buttons, file dialogs, ...) are English by
// default, so a translator is only needed for Russian.
bool loadQtTranslator(const QString &code)
{
    if (code != QLatin1String("ru"))
        return false;

    auto *translator = new QTranslator;
    const QLocale russianLocale(QLocale::Russian, QLocale::Russia);
    const QString local = localTranslationsDirectory();
    const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    // windeployqt ships a merged qt_ru.qm; a Qt installation has qtbase_ru.qm.
    const bool loaded =
        translator->load(russianLocale, QStringLiteral("qt"), QStringLiteral("_"), local)
        || translator->load(russianLocale, QStringLiteral("qtbase"), QStringLiteral("_"), local)
        || translator->load(russianLocale, QStringLiteral("qt"), QStringLiteral("_"), qtPath)
        || translator->load(russianLocale, QStringLiteral("qtbase"), QStringLiteral("_"), qtPath);
    if (!loaded) {
        delete translator;
        return false;
    }

    s_qtTranslator = translator;
    QCoreApplication::installTranslator(s_qtTranslator);
    return true;
}

} // namespace

namespace Localization {

QString systemLanguageCode()
{
    return QLocale::system().language() == QLocale::Russian ? QStringLiteral("ru")
                                                            : QStringLiteral("en");
}

QString effectiveLanguageCode()
{
    const QString saved = Prefs::uiLanguage();
    if (saved == QLatin1String("ru") || saved == QLatin1String("en"))
        return saved;
    return systemLanguageCode();
}

QString installTranslators()
{
    removeTranslators();

    const QString code = effectiveLanguageCode();
    loadAppTranslator(code);
    loadQtTranslator(code);
    return code;
}

QString languageName(const QString &language)
{
    if (language == QLatin1String("ru"))
        return QCoreApplication::translate("Localization", "Русский");
    if (language == QLatin1String("en"))
        return QCoreApplication::translate("Localization", "English");
    return QCoreApplication::translate("Localization", "Системный (автоматически)");
}

} // namespace Localization
