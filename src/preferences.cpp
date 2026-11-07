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

} // namespace Prefs
