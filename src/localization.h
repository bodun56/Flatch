#pragma once

#include <QString>

namespace Localization {

// "ru" when the operating system language is Russian, otherwise "en".
QString systemLanguageCode();

// The saved preference, or the system language when the preference is
// "system". Returns "ru" or "en".
QString effectiveLanguageCode();

// Installs the application and Qt translators for the effective language and
// returns the active language code ("ru" or "en"). The Russian interface is
// the source language, so only the English application translator is loaded.
QString installTranslators();

// Localized label for a language option ("system", "ru" or "en").
QString languageName(const QString &language);

} // namespace Localization
