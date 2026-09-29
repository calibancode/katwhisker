#pragma once

#include "version.h"

#include <QString>

// radio-browser.info asks clients to identify themselves with a way to reach
// the project; Icecast server operators appreciate the same.
inline QString userAgent()
{
    return QStringLiteral("Katwhisker/" KATWHISKER_VERSION_STRING " (+https://github.com/calibancode/katwhisker)");
}
