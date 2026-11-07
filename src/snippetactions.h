#pragma once

#include "snippet.h"

#include <QString>

namespace SnippetActions {

// Copies the snippet to the clipboard or launches the configured application.
// Returns false and fills *error when the action could not be performed.
bool activate(const Snippet &snippet, QString *error = nullptr);

} // namespace SnippetActions
