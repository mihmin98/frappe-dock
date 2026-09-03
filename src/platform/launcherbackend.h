#pragma once

#include "core/interfaces/ilauncherbackend.h"

namespace frappe
{

/// ILauncherBackend over KService and KIO::ApplicationLauncherJob.
///
/// Deliberately stateless: KService maintains its own cache of the desktop entry
/// database, so there is nothing here worth holding on to.
class LauncherBackend : public ILauncherBackend
{
public:
    std::expected<DesktopEntry, Error> lookup(const QString &id) const override;
    std::expected<void, Error> launch(const QString &id) const override;
    std::expected<void, Error> launchAction(const QString &id, const QString &actionId) const override;
    std::expected<void, Error> openWith(const QString &id, const QList<QUrl> &files) const override;
    std::expected<void, Error> openUrl(const QUrl &url) const override;
    std::expected<void, Error> reveal(const QString &id) const override;
    bool launchesAtLogin(const QString &id) const override;
    std::expected<void, Error> setLaunchAtLogin(const QString &id, bool enabled) const override;
};

}
