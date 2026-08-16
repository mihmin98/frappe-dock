#pragma once

#include "core/interfaces/ilauncherbackend.h"

#include <QSet>
#include <QStringList>

#include <map>

namespace frappe
{

/// In-memory ILauncherBackend that records what it was asked to do.
///
/// Nothing here touches the desktop entry database or starts a process, so tests
/// using it are deterministic and safe to run anywhere.
class FakeLauncherBackend : public ILauncherBackend
{
public:
    void addEntry(const DesktopEntry &entry)
    {
        m_entries[entry.id] = entry;
    }

    void addEntry(const QString &id, const QString &name, const QString &iconName)
    {
        DesktopEntry entry;
        entry.id = id;
        entry.name = name;
        entry.iconName = iconName;
        addEntry(entry);
    }

    void removeEntry(const QString &id)
    {
        m_entries.erase(id);
    }

    std::expected<DesktopEntry, Error> lookup(const QString &id) const override
    {
        const auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return std::unexpected(Error::NotFound);
        }
        return it->second;
    }

    std::expected<void, Error> launch(const QString &id) const override
    {
        if (!m_entries.contains(id)) {
            return std::unexpected(Error::NotFound);
        }
        m_launched.append(id);
        return {};
    }

    std::expected<void, Error> launchAction(const QString &id, const QString &actionId) const override
    {
        if (!m_entries.contains(id)) {
            return std::unexpected(Error::NotFound);
        }
        m_launchedActions.append(id + QLatin1Char(':') + actionId);
        return {};
    }

    std::expected<void, Error> openWith(const QString &id, const QList<QUrl> &files) const override
    {
        if (!m_entries.contains(id)) {
            return std::unexpected(Error::NotFound);
        }
        for (const QUrl &file : files) {
            m_opened.append(id + QLatin1Char(':') + file.toString());
        }
        return {};
    }

    std::expected<void, Error> reveal(const QString &id) const override
    {
        if (!m_entries.contains(id)) {
            return std::unexpected(Error::NotFound);
        }
        m_revealed.append(id);
        return {};
    }

    bool launchesAtLogin(const QString &id) const override
    {
        return m_autostart.contains(id);
    }

    std::expected<void, Error> setLaunchAtLogin(const QString &id, bool enabled) const override
    {
        if (!m_entries.contains(id)) {
            return std::unexpected(Error::NotFound);
        }
        if (enabled) {
            m_autostart.insert(id);
        } else {
            m_autostart.remove(id);
        }
        return {};
    }

    /// Ids passed to launch(), in call order.
    QStringList launched() const
    {
        return m_launched;
    }

    /// "<id>:<actionId>" for each launchAction() call, in order.
    QStringList launchedActions() const
    {
        return m_launchedActions;
    }

    /// "<id>:<url>" for each url passed to openWith(), in order.
    QStringList opened() const
    {
        return m_opened;
    }

    /// Ids passed to reveal(), in call order.
    QStringList revealed() const
    {
        return m_revealed;
    }

    void clearRecords()
    {
        m_launched.clear();
        m_launchedActions.clear();
        m_opened.clear();
        m_revealed.clear();
    }

private:
    std::map<QString, DesktopEntry> m_entries;
    // Mutable because the interface's launch methods are const — they are
    // commands to the outside world, not mutations of the backend.
    mutable QStringList m_launched;
    mutable QStringList m_launchedActions;
    mutable QStringList m_opened;
    mutable QStringList m_revealed;
    mutable QSet<QString> m_autostart;
};

}
