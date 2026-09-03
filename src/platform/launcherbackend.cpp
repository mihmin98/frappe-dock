#include "platform/launcherbackend.h"

#include <KIO/ApplicationLauncherJob>
#include <KIO/OpenUrlJob>
#include <KJob>
#include <KIO/OpenFileManagerWindowJob>
#include <KService>
#include <KServiceAction>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

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

/// Where the session looks for entries to start at login. Honours
/// QStandardPaths test mode, so tests never touch the real one.
QString autostartPath(const KService::Ptr &service)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart");
    return dir + QLatin1Char('/') + QFileInfo(service->entryPath()).fileName();
}

/// These jobs delete themselves when they finish, so one must not be owned or
/// waited on. Failure is reported asynchronously and only logged; there is
/// nothing useful to hand back to a caller that has already returned.
///
/// Takes a KJob rather than a concrete type: every job started here is one, and
/// nothing below reaches past that interface.
std::expected<void, Error> startJob(KJob *job, const QString &id)
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
    // What the entry declares it opens, which is what decides whether a file
    // dropped on its tile has anywhere to go.
    entry.mimeTypes = service->mimeTypes();

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

std::expected<void, Error> LauncherBackend::openUrl(const QUrl &url) const
{
    if (!url.isValid()) {
        return std::unexpected(Error::NotFound);
    }

    // OpenUrlJob rather than ApplicationLauncherJob: it resolves the handler
    // from the MIME type itself, which is the whole point — nobody chose a
    // program, and asking KIO is what makes the answer the desktop's default
    // rather than ours.
    auto *job = new KIO::OpenUrlJob(url);
    // And for the same reason, executables and desktop entries are KIO's to
    // decide about too. With this false, a desktop entry opened from a stack is
    // handed to a text editor — so a stack pointed at an applications directory
    // becomes an application grid in which clicking an application reads its
    // source, which is the one thing that use of it must not do.
    //
    // True does not mean running whatever it is handed: KIO applies its own
    // trust rule, launching entries in the standard application directories and
    // prompting for anything outside them. That rule is the desktop's, which is
    // exactly what the comment above says we are here to defer to.
    job->setRunExecutables(true);
    return startJob(job, url.toString());
}

std::expected<void, Error> LauncherBackend::reveal(const QString &id) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }

    // The desktop entry itself is what there is to show: an application is a
    // bundle on the reference platform and a .desktop file here, so revealing it
    // means selecting that file in the file manager.
    const QString path = service->entryPath();
    if (path.isEmpty()) {
        return std::unexpected(Error::InvalidDesktopEntry);
    }

    // Self-deleting like the launcher jobs, and equally fire-and-forget.
    KIO::highlightInFileManager({QUrl::fromLocalFile(path)});
    return {};
}

bool LauncherBackend::launchesAtLogin(const QString &id) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service || service->entryPath().isEmpty()) {
        return false;
    }
    return QFile::exists(autostartPath(service));
}

std::expected<void, Error> LauncherBackend::setLaunchAtLogin(const QString &id, bool enabled) const
{
    const KService::Ptr service = serviceFor(id);
    if (!service) {
        return std::unexpected(Error::NotFound);
    }
    const QString source = service->entryPath();
    if (source.isEmpty()) {
        return std::unexpected(Error::InvalidDesktopEntry);
    }

    const QString target = autostartPath(service);
    if (!enabled) {
        // Absent already counts as done; removing what is not there is not a
        // failure the caller can act on.
        return QFile::exists(target) && !QFile::remove(target) ? std::unexpected(Error::IoFailed)
                                                               : std::expected<void, Error>{};
    }

    if (QFile::exists(target)) {
        return {};
    }
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        return std::unexpected(Error::IoFailed);
    }
    // A copy rather than a symlink: the entry stays valid if the application is
    // upgraded and its file replaced, and the session reads it without following
    // links out of the config tree.
    if (!QFile::copy(source, target)) {
        return std::unexpected(Error::IoFailed);
    }
    return {};
}
