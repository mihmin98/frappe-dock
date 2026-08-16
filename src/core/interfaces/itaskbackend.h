#pragma once

#include <QString>

#include <functional>
#include <vector>

namespace frappe
{

/// One window as the dock cares about it.
struct WindowInfo {
    QString windowId;
    /// Maps to a desktop entry id; may be empty when the compositor reports none.
    QString appId;
    QString title;
    bool isMinimized = false;
    bool isActive = false;
};

/// Window list and window control.
///
/// This is the containment wall around the window-management library: core sees
/// only this, platform implements it, tests fake it. Plain virtual class — no
/// Q_OBJECT, no signals — so core stays free of moc; the platform side bridges
/// its own change notifications into setChangeCallback().
class ITaskBackend
{
public:
    virtual ~ITaskBackend() = default;

    virtual std::vector<WindowInfo> windows() const = 0;

    virtual void activate(const QString &windowId) = 0;
    virtual void minimize(const QString &windowId) = 0;
    virtual void close(const QString &windowId) = 0;
    /// Hide every window whose app id differs from exceptAppId.
    virtual void hideOthers(const QString &exceptAppId) = 0;

    /// Invoked whenever the window list changes in any way. Core re-queries and
    /// diffs rather than tracking deltas.
    virtual void setChangeCallback(std::function<void()> cb) = 0;
};

}
