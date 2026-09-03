#pragma once

#include <QString>

namespace frappe
{

/// The canonical form of a desktop entry id: the storage id without its
/// `.desktop` suffix.
///
/// The same entry reaches the dock spelled two ways. Config holds the bare name
/// — `org.kde.dolphin` — because that is what a user types and what the settings
/// schema documents. libtaskmanager's `AppId` role hands back the KService
/// **storage id**, which ends in `.desktop`. Comparing those with string
/// equality says a running application is a different application from the
/// pinned one, and the dock grows a second tile beside the first.
///
/// So ids are normalised wherever they are *compared*, and this is the one
/// definition of what that means. Resolution does not need it — `KService`
/// accepts either spelling — which is exactly why the mismatch stayed invisible
/// until a window turned up.
QString desktopEntryKey(const QString &id);

}
