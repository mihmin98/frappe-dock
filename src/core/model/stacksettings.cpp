#include "core/model/stacksettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>

using namespace frappe;

namespace
{

/// The group the per-folder preferences live in, inside the same file the
/// settings keys use.
constexpr auto stacksGroup = "Stacks";

/// The group holding what a folder with no preference of its own falls back to.
/// A separate group rather than a reserved key in the one above, so no folder
/// path can ever collide with it however it was written.
constexpr auto defaultsGroup = "StackDefaults";

/// Suffix distinguishing a folder's sort order from its view mode. Both are
/// keyed on the same path, and one key per fact keeps the file readable.
constexpr auto sortSuffix = "/sort";

/// Config keys may not contain the separator KConfig uses between a key and its
/// value, and a path certainly can. Storing the path with '=' escaped is enough:
/// nothing else in a POSIX path collides, and the file stays readable, which is
/// worth more here than a hash that nobody can match back to a folder.
QString keyFor(const QString &folderPath)
{
    QString clean = QDir::cleanPath(folderPath);
    return clean.replace(u'=', QStringLiteral("%3D"));
}

}

class StackSettings::Private
{
public:
    KSharedConfig::Ptr config;

    KConfigGroup group() const
    {
        return KConfigGroup(config, QString::fromLatin1(stacksGroup));
    }

    KConfigGroup defaults() const
    {
        return KConfigGroup(config, QString::fromLatin1(defaultsGroup));
    }
};

StackSettings::StackSettings(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    d->config = KSharedConfig::openConfig();
}

StackSettings::StackSettings(const QString &configPath, QObject *parent)
    : QObject(parent)
    , d(new Private)
{
    redirectTo(configPath);
}

void StackSettings::redirectTo(const QString &configPath)
{
    d->config = KSharedConfig::openConfig(configPath, KConfig::SimpleConfig);
    // openConfig() caches by name and hands back the same object with its
    // existing in-memory state, so a file rewritten behind our back is never
    // seen without this. Same reasoning as ConfigFacade::redirectTo().
    d->config->reparseConfiguration();
}

StackSettings::~StackSettings()
{
    delete d;
}

StackSettings *StackSettings::instance()
{
    static StackSettings s_instance;
    return &s_instance;
}

int StackSettings::viewMode(const QString &folderPath) const
{
    if (folderPath.isEmpty()) {
        return Grid;
    }

    const int stored = d->group().readEntry(keyFor(folderPath), int(Grid));
    // A file edited by hand, or written by a later version that knows more
    // modes. Falling back to the default beats rendering nothing at all.
    if (stored < Grid || stored > List) {
        return Grid;
    }
    return stored;
}

void StackSettings::setViewMode(const QString &folderPath, int mode)
{
    if (folderPath.isEmpty() || mode < Grid || mode > List) {
        return;
    }
    if (viewMode(folderPath) == mode) {
        return;
    }

    KConfigGroup group = d->group();
    group.writeEntry(keyFor(folderPath), mode);
    group.sync();
    Q_EMIT viewModeChanged(folderPath);
}

void StackSettings::forget(const QString &folderPath)
{
    if (folderPath.isEmpty()) {
        return;
    }

    KConfigGroup group = d->group();
    const QString key = keyFor(folderPath);
    const QString sortKey = key + QString::fromLatin1(sortSuffix);
    if (!group.hasKey(key) && !group.hasKey(sortKey)) {
        return;
    }

    group.deleteEntry(key);
    group.deleteEntry(sortKey);
    group.sync();
    Q_EMIT viewModeChanged(folderPath);
    Q_EMIT sortOrderChanged(folderPath);
}

int StackSettings::defaultSortOrder() const
{
    const int stored = d->defaults().readEntry(QStringLiteral("sort"), int(Name));
    if (stored < Name || stored > Kind) {
        return Name;
    }
    return stored;
}

void StackSettings::setDefaultSortOrder(int order)
{
    if (order < Name || order > Kind || order == defaultSortOrder()) {
        return;
    }

    KConfigGroup group = d->defaults();
    group.writeEntry(QStringLiteral("sort"), order);
    group.sync();
    // Empty folder: this moved every folder that has no preference of its own,
    // and naming one of them would be naming the wrong thing.
    Q_EMIT sortOrderChanged(QString());
}

int StackSettings::sortOrder(const QString &folderPath) const
{
    if (folderPath.isEmpty()) {
        return defaultSortOrder();
    }

    const QString key = keyFor(folderPath) + QString::fromLatin1(sortSuffix);
    if (!d->group().hasKey(key)) {
        return defaultSortOrder();
    }

    const int stored = d->group().readEntry(key, defaultSortOrder());
    if (stored < Name || stored > Kind) {
        return defaultSortOrder();
    }
    return stored;
}

void StackSettings::setSortOrder(const QString &folderPath, int order)
{
    if (folderPath.isEmpty() || order < Name || order > Kind) {
        return;
    }

    KConfigGroup group = d->group();
    const QString key = keyFor(folderPath) + QString::fromLatin1(sortSuffix);

    // Compared against the stored key, not against sortOrder(), which answers
    // with the default when there is no key. Skipping the write because the
    // chosen order happens to match today's default would leave the folder
    // following the default, and moving the default later would then silently
    // move a folder the user had explicitly set.
    if (group.hasKey(key) && group.readEntry(key, int(Name)) == order) {
        return;
    }

    group.writeEntry(key, order);
    group.sync();
    Q_EMIT sortOrderChanged(folderPath);
}
