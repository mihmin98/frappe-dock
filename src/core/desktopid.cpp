#include "core/desktopid.h"

using namespace frappe;

QString frappe::desktopEntryKey(const QString &id)
{
    // Case-sensitive: the suffix is lowercase by the freedesktop specification,
    // and a file genuinely named "Foo.DESKTOP" is not an entry we would have
    // resolved either.
    if (id.endsWith(QLatin1String(".desktop"))) {
        return id.left(id.size() - 8);
    }
    return id;
}
