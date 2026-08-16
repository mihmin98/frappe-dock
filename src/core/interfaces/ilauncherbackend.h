#pragma once

#include <QList>
#include <QString>
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
};

}
