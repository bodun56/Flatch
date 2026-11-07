#include "preferences.h"

#include <QSettings>

namespace Prefs {

bool notificationsEnabled()
{
    QSettings settings;
    return settings.value(QStringLiteral("ui/notifications"), true).toBool();
}

void setNotificationsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/notifications"), enabled);
}

QString uiLanguage()
{
    QSettings settings;
    const QString value =
        settings.value(QStringLiteral("ui/language"), QStringLiteral("system")).toString();
    if (value == QLatin1String("ru") || value == QLatin1String("en")
        || value == QLatin1String("system"))
        return value;
    return QStringLiteral("system");
}

void setUiLanguage(const QString &language)
{
    QSettings settings;
    if (language == QLatin1String("ru") || language == QLatin1String("en")
        || language == QLatin1String("system")) {
        settings.setValue(QStringLiteral("ui/language"), language);
    } else {
        settings.setValue(QStringLiteral("ui/language"), QStringLiteral("system"));
    }
}

} // namespace Prefs
