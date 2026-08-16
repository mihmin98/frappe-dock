#include "platform/outputprovider.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

using namespace frappe;

namespace
{
OutputInfo toOutputInfo(const QScreen *screen)
{
    OutputInfo info;
    info.id = screen->name();
    info.geometry = screen->geometry();
    info.scale = screen->devicePixelRatio();
    info.isPrimary = screen == QGuiApplication::primaryScreen();
    return info;
}
}

OutputProvider::OutputProvider(QObject *parent)
    : QObject(parent)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        watchScreen(screen);
    }

    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        watchScreen(screen);
        notifyChanged();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        notifyChanged();
    });
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *) {
        notifyChanged();
    });
}

void OutputProvider::watchScreen(QScreen *screen)
{
    connect(screen, &QScreen::geometryChanged, this, [this](const QRect &) {
        notifyChanged();
    });
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, [this](qreal) {
        notifyChanged();
    });
}

void OutputProvider::notifyChanged()
{
    if (m_callback) {
        m_callback();
    }
}

std::vector<OutputInfo> OutputProvider::outputs() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    std::vector<OutputInfo> result;
    result.reserve(screens.size());
    for (const QScreen *screen : screens) {
        // Qt keeps an unnamed placeholder QScreen around when no real output is
        // present, and briefly reports it alongside the real ones while outputs
        // are being reconfigured. It cannot be anchored to or named in config,
        // and taking it for an output produces a second, stacked dock surface
        // whose exclusive zone is reserved on top of the real one's.
        if (screen->name().isEmpty()) {
            continue;
        }
        result.push_back(toOutputInfo(screen));
    }
    return result;
}

OutputInfo OutputProvider::activeOutput() const
{
    if (const QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
        return toOutputInfo(screen);
    }
    if (const QScreen *primary = QGuiApplication::primaryScreen()) {
        return toOutputInfo(primary);
    }
    return {};
}

void OutputProvider::setChangeCallback(std::function<void()> cb)
{
    m_callback = std::move(cb);
}
