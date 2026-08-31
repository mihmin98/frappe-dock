#include "core/config/configfacade.h"

#include "frappeconfig.h"

#include <KSharedConfig>

using namespace frappe;

ConfigFacade::ConfigFacade(QObject *parent)
    : QObject(parent)
{
}

ConfigFacade::ConfigFacade(const QString &configPath, QObject *parent)
    : QObject(parent)
{
    redirectTo(configPath);
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
    // Written through immediately, unlike the settings keys. Pinning, unpinning
    // and reordering are direct manipulation: there is no dialog to dismiss and
    // no OK button, so the gesture itself is the only save point there is.
    FrappeConfig::self()->save();
    Q_EMIT changed();
}

void ConfigFacade::save()
{
    FrappeConfig::self()->save();
}

void ConfigFacade::reload()
{
    FrappeConfig::self()->read();
    Q_EMIT changed();
}
