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
