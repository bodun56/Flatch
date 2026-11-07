#pragma once

#include <QString>

namespace Prefs {

bool notificationsEnabled();
void setNotificationsEnabled(bool enabled);

// Interface language: "system", "ru" or "en".
QString uiLanguage();
void setUiLanguage(const QString &language);

} // namespace Prefs
