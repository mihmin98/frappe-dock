#include "platform/launcherbackend.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KServiceAction>

#include <QLoggingCategory>

using namespace frappe;

Q_LOGGING_CATEGORY(FRAPPE_LAUNCHER, "frappe.launcher")

namespace
{
/// An id may be a storage id ("org.kde.konsole.desktop") or a desktop name
/// ("org.kde.konsole"). Try both rather than making callers care which they hold.
KService::Ptr serviceFor(const QString &id)
{
    if (KService::Ptr service = KService::serviceByStorageId(id)) {
        return service;
    }
    return KService::serviceByDesktopName(id);
}

/// ApplicationLauncherJob deletes itself when it finishes, so the job must not
/// be owned or waited on. Failure is reported asynchronously and only logged;
/// there is nothing useful to hand back to a caller that has already returned.
std::expected<void, Error> startJob(KIO::ApplicationLauncherJob *job, const QString &id)
{
    QObject::connect(job, &KJob::result, job, [job, id] {
        if (job->error()) {
            qCWarning(FRAPPE_LAUNCHER) << "Launch failed for" << id << job->errorString();
        }
    });
    job->start();
    return {};
}
}

std::expected<DesktopEntry, Error> LauncherBackend::lookup(const QString &id) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }
    if (service->exec().isEmpty()) {
        return std::unexpected(Error::InvalidDesktopEntry);
    }

    DesktopEntry entry;
    entry.id = service->storageId();
    entry.name = service->name();
    entry.iconName = service->icon();

    const QList<KServiceAction> actions = service->actions();
    entry.actions.reserve(actions.size());
    for (const KServiceAction &action : actions) {
        entry.actions.emplace_back(action.name(), action.text());
    }

    return entry;
}

std::expected<void, Error> LauncherBackend::launch(const QString &id) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }
    return startJob(new KIO::ApplicationLauncherJob(service), id);
}

std::expected<void, Error> LauncherBackend::launchAction(const QString &id, const QString &actionId) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }

    const QList<KServiceAction> actions = service->actions();
    for (const KServiceAction &action : actions) {
        if (action.name() == actionId) {
            return startJob(new KIO::ApplicationLauncherJob(action), id);
        }
    }
    return std::unexpected(Error::NotFound);
}

std::expected<void, Error> LauncherBackend::openWith(const QString &id, const QList<QUrl> &files) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }

    auto *job = new KIO::ApplicationLauncherJob(service);
    job->setUrls(files);
    return startJob(job, id);
}
