#pragma once

#include <QString>

namespace AppPaths {

// Application data directory (created on demand).
QString dataDir();
QString databaseFile();
QString keyFile();

} // namespace AppPaths
