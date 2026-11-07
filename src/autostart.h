#pragma once

#include <QString>

namespace Autostart {

bool isSupported();
bool isEnabled();
bool setEnabled(bool enabled, QString *error = nullptr);

} // namespace Autostart
