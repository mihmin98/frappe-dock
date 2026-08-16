#pragma once

#include "core/interfaces/itaskbackend.h"

#include <QStringList>

namespace frappe
{

/// In-memory ITaskBackend with a settable window list that records what it was
/// asked to do.
///
/// A real implementation of the interface rather than a generated mock: the
/// interface is four commands and a getter, so hand-writing it is shorter than
/// configuring a framework and reads like the production one.
///
/// The control methods only record. Tests that want a command to take effect on
/// the window list call setWindows() again — that is what the real compositor
/// does, asynchronously, and pretending otherwise would hide ordering bugs.
class FakeTaskBackend : public ITaskBackend
{
public:
    void setWindows(std::vector<WindowInfo> windows)
    {
        m_windows = std::move(windows);
    }

    void addWindow(const WindowInfo &window)
    {
        m_windows.push_back(window);
    }

    void addWindow(const QString &windowId, const QString &appId, const QString &title = {})
    {
        WindowInfo info;
        info.windowId = windowId;
        info.appId = appId;
        info.title = title;
        m_windows.push_back(info);
    }

    void removeWindow(const QString &windowId)
    {
        std::erase_if(m_windows, [&windowId](const WindowInfo &w) {
            return w.windowId == windowId;
        });
    }

    std::vector<WindowInfo> windows() const override
    {
        return m_windows;
    }

    void activate(const QString &windowId) override
    {
        m_activated.append(windowId);
    }

    void minimize(const QString &windowId) override
    {
        m_minimized.append(windowId);
    }

    void close(const QString &windowId) override
    {
        m_closed.append(windowId);
    }

    void hideOthers(const QString &exceptAppId) override
    {
        m_hideOthers.append(exceptAppId);
    }

    void setChangeCallback(std::function<void()> cb) override
    {
        m_callback = std::move(cb);
    }

    /// Pretend the window list changed, the way the compositor would.
    void notifyChange() const
    {
        if (m_callback) {
            m_callback();
        }
    }

    /// Window ids passed to each control method, in call order.
    QStringList activated() const
    {
        return m_activated;
    }
    QStringList minimized() const
    {
        return m_minimized;
    }
    QStringList closed() const
    {
        return m_closed;
    }
    /// App ids passed to hideOthers(), in call order.
    QStringList hideOthersCalls() const
    {
        return m_hideOthers;
    }

    void clearRecords()
    {
        m_activated.clear();
        m_minimized.clear();
        m_closed.clear();
        m_hideOthers.clear();
    }

private:
    std::vector<WindowInfo> m_windows;
    std::function<void()> m_callback;

    QStringList m_activated;
    QStringList m_minimized;
    QStringList m_closed;
    QStringList m_hideOthers;
};

}
