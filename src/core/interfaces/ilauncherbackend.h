#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <expected>
#include <utility>
#include <vector>

#include "core/error.h"

namespace frappe
{

struct DesktopEntry {
    QString id;
    QString name;
    QString iconName;
    /// Additional launch actions, in the order the entry declares them: (id, display name).
    std::vector<std::pair<QString, QString>> actions;
    /// The MIME types the entry declares it can open. Empty means it declares
    /// none, which is a real answer: a file dropped on it has nowhere to go.
    QStringList mimeTypes;
};

/// Desktop entry lookup and application launching.
///
/// Plain virtual class: no Q_OBJECT, no signals, so core stays free of moc and
/// the fake in tests/fakes/ is trivial to write.
class ILauncherBackend
{
public:
    virtual ~ILauncherBackend() = default;

    virtual std::expected<DesktopEntry, Error> lookup(const QString &id) const = 0;
    virtual std::expected<void, Error> launch(const QString &id) const = 0;
    virtual std::expected<void, Error> launchAction(const QString &id, const QString &actionId) const = 0;
    virtual std::expected<void, Error> openWith(const QString &id, const QList<QUrl> &files) const = 0;

    /// Opens \a url with whatever the desktop says should handle it.
    ///
    /// Distinct from openWith(): there is no chosen application here, which is
    /// what activating a file inside a stack means — the user picked the file,
    /// not the program.
    virtual std::expected<void, Error> openUrl(const QUrl &url) const = 0;

    /// Opens the file manager with the application's desktop entry selected.
    virtual std::expected<void, Error> reveal(const QString &id) const = 0;

    /// Whether the application is set to start with the session.
    virtual bool launchesAtLogin(const QString &id) const = 0;
    virtual std::expected<void, Error> setLaunchAtLogin(const QString &id, bool enabled) const = 0;
};

}
