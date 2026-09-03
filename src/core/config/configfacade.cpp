#include "core/config/configfacade.h"

#include "frappeconfig.h"

#include <KSharedConfig>

#include <QCoreApplication>

using namespace frappe;

namespace
{
/// How long a write waits for the gesture to finish.
///
/// Long enough that a drag's stream of moves collapses into one write — they
/// arrive milliseconds apart — and short enough that the window in which a hard
/// crash could lose a reorder is not worth worrying about.
constexpr int saveDelayMs = 250;
}

ConfigFacade::ConfigFacade(QObject *parent)
    : QObject(parent)
{
    m_save.setSingleShot(true);
    m_save.setInterval(saveDelayMs);
    connect(&m_save, &QTimer::timeout, this, [] { FrappeConfig::self()->save(); });

    // The process-wide instance outlives most things but not the application,
    // and its destructor at static teardown is too late to be relied on.
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
                [this] { flushPendingSave(); });
    }
}

ConfigFacade::ConfigFacade(const QString &configPath, QObject *parent)
    : ConfigFacade(parent)
{
    redirectTo(configPath);
}

ConfigFacade::~ConfigFacade()
{
    flushPendingSave();
}

void ConfigFacade::saveSoon()
{
    m_save.start();
}

void ConfigFacade::flushPendingSave()
{
    if (!m_save.isActive()) {
        return;
    }
    m_save.stop();
    FrappeConfig::self()->save();
}

void ConfigFacade::redirectTo(const QString &configPath)
{
    KSharedConfig::Ptr shared = KSharedConfig::openConfig(configPath, KConfig::SimpleConfig);
    // openConfig() caches by name, so a second open of the same path hands back
    // the same object with its existing in-memory state. Without an explicit
    // reparse, a file rewritten behind our back is never seen — which is exactly
    // what a test that writes a config file by hand does.
    shared->reparseConfiguration();
    FrappeConfig::self()->setSharedConfig(shared);
    FrappeConfig::self()->read();
}

ConfigFacade *ConfigFacade::instance()
{
    static ConfigFacade s_instance;
    return &s_instance;
}

int ConfigFacade::position() const
{
    return FrappeConfig::position();
}

void ConfigFacade::setPosition(int value)
{
    if (value == position()) {
        return;
    }
    FrappeConfig::setPosition(value);
    Q_EMIT changed();
    Q_EMIT surfaceGeometryChanged();
}

int ConfigFacade::displayMode() const
{
    return FrappeConfig::displayMode();
}

void ConfigFacade::setDisplayMode(int value)
{
    if (value == displayMode()) {
        return;
    }
    FrappeConfig::setDisplayMode(value);
    Q_EMIT changed();
    Q_EMIT surfaceSetChanged();
}

QString ConfigFacade::targetOutput() const
{
    return FrappeConfig::targetOutput();
}

void ConfigFacade::setTargetOutput(const QString &value)
{
    if (value == targetOutput()) {
        return;
    }
    FrappeConfig::setTargetOutput(value);
    Q_EMIT changed();
    Q_EMIT surfaceSetChanged();
}

int ConfigFacade::tileSize() const
{
    return FrappeConfig::tileSize();
}

void ConfigFacade::setTileSize(int value)
{
    if (value == tileSize()) {
        return;
    }
    FrappeConfig::setTileSize(value);
    Q_EMIT changed();
    Q_EMIT surfaceGeometryChanged();
}

namespace
{
/// Percent-stored key to the ratio the engine takes, and back. Rounding on the
/// way in is what keeps a slider from writing a value the schema then clamps
/// differently on reload.
constexpr qreal fromPercent(int stored)
{
    return stored / 100.0;
}

int toPercent(qreal ratio)
{
    return qRound(ratio * 100.0);
}
}

bool ConfigFacade::magnificationEnabled() const
{
    return FrappeConfig::magnificationEnabled();
}

void ConfigFacade::setMagnificationEnabled(bool value)
{
    if (value == magnificationEnabled()) {
        return;
    }
    FrappeConfig::setMagnificationEnabled(value);
    Q_EMIT changed();
}

qreal ConfigFacade::magnificationFactor() const
{
    return fromPercent(FrappeConfig::magnificationFactor());
}

void ConfigFacade::setMagnificationFactor(qreal value)
{
    const int stored = toPercent(value);
    if (stored == FrappeConfig::magnificationFactor()) {
        return;
    }
    FrappeConfig::setMagnificationFactor(stored);
    Q_EMIT changed();
}

qreal ConfigFacade::falloffRadius() const
{
    return fromPercent(FrappeConfig::falloffRadius());
}

void ConfigFacade::setFalloffRadius(qreal value)
{
    const int stored = toPercent(value);
    if (stored == FrappeConfig::falloffRadius()) {
        return;
    }
    FrappeConfig::setFalloffRadius(stored);
    Q_EMIT changed();
}

qreal ConfigFacade::curveExponent() const
{
    return fromPercent(FrappeConfig::curveExponent());
}

void ConfigFacade::setCurveExponent(qreal value)
{
    const int stored = toPercent(value);
    if (stored == FrappeConfig::curveExponent()) {
        return;
    }
    FrappeConfig::setCurveExponent(stored);
    Q_EMIT changed();
}

int ConfigFacade::animationSpeed() const
{
    return FrappeConfig::animationSpeed();
}

void ConfigFacade::setAnimationSpeed(int value)
{
    if (value == animationSpeed()) {
        return;
    }
    FrappeConfig::setAnimationSpeed(value);
    Q_EMIT changed();
}

int ConfigFacade::appearanceMode() const
{
    return FrappeConfig::appearanceMode();
}

void ConfigFacade::setAppearanceMode(int value)
{
    if (value == appearanceMode()) {
        return;
    }
    FrappeConfig::setAppearanceMode(value);
    Q_EMIT changed();
}

bool ConfigFacade::showRunningIndicators() const
{
    return FrappeConfig::showRunningIndicators();
}

void ConfigFacade::setShowRunningIndicators(bool value)
{
    if (value == showRunningIndicators()) {
        return;
    }
    FrappeConfig::setShowRunningIndicators(value);
    Q_EMIT changed();
}

bool ConfigFacade::minimizeIntoIcon() const
{
    return FrappeConfig::minimizeIntoIcon();
}

void ConfigFacade::setMinimizeIntoIcon(bool value)
{
    if (value == minimizeIntoIcon()) {
        return;
    }
    FrappeConfig::setMinimizeIntoIcon(value);
    Q_EMIT changed();
    Q_EMIT contentsChanged();
}

int ConfigFacade::springLoadDelay() const
{
    return FrappeConfig::springLoadDelay();
}

void ConfigFacade::setSpringLoadDelay(int value)
{
    if (value == springLoadDelay()) {
        return;
    }
    FrappeConfig::setSpringLoadDelay(value);
    Q_EMIT changed();
}

QStringList ConfigFacade::pinnedEntries() const
{
    return FrappeConfig::pinnedEntries();
}

void ConfigFacade::setPinnedEntries(const QStringList &value)
{
    if (value == pinnedEntries()) {
        return;
    }
    FrappeConfig::setPinnedEntries(value);
    // Written through, unlike the settings keys: pinning, unpinning and
    // reordering are direct manipulation, and there is no dialog to dismiss.
    // Deferred past the rest of the gesture, though — a drag calls this several
    // times, and saving on each step rewrote the whole file per pointer event.
    saveSoon();
    Q_EMIT changed();
    Q_EMIT contentsChanged();
}

QStringList ConfigFacade::fileEntries() const
{
    return FrappeConfig::fileEntries();
}

void ConfigFacade::setFileEntries(const QStringList &value)
{
    if (value == fileEntries()) {
        return;
    }
    FrappeConfig::setFileEntries(value);
    // Written through, for the same reason pinnedEntries is, and deferred for
    // the same reason too: the file region reorders by drag as well.
    saveSoon();
    Q_EMIT changed();
    Q_EMIT contentsChanged();
}

void ConfigFacade::save()
{
    // An explicit save is a promise that it is on disk when this returns, so a
    // deferred one must not be left pending behind it.
    m_save.stop();
    FrappeConfig::self()->save();
}

void ConfigFacade::reload()
{
    // Discarding pending changes includes the pending *write* of them.
    m_save.stop();
    FrappeConfig::self()->read();
    Q_EMIT changed();
}
