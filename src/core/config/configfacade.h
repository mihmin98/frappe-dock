#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace frappe
{

/// Thin wrapper around the KConfigXT-generated FrappeConfig singleton.
///
/// Two reasons it exists rather than everything calling FrappeConfig directly:
/// tests can point it at a temporary file instead of the user's real settings,
/// and QML needs an instance with notifying properties, which a class of static
/// accessors cannot provide.
class ConfigFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY changed)
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode NOTIFY changed)
    Q_PROPERTY(QString targetOutput READ targetOutput WRITE setTargetOutput NOTIFY changed)
    Q_PROPERTY(int tileSize READ tileSize WRITE setTileSize NOTIFY changed)
    Q_PROPERTY(QStringList pinnedEntries READ pinnedEntries WRITE setPinnedEntries NOTIFY changed)

public:
    /// Mirrors FrappeConfig::EnumPosition, so QML and tests need not include the
    /// generated header.
    enum Position { Bottom, Left, Right };
    Q_ENUM(Position)

    /// Mirrors FrappeConfig::EnumDisplayMode.
    enum DisplayMode { FollowActive, AllScreens, SingleScreen };
    Q_ENUM(DisplayMode)

    explicit ConfigFacade(QObject *parent = nullptr);

    /// Redirects the underlying singleton at \a configPath and reloads. Intended
    /// for tests; call before anything reads config.
    explicit ConfigFacade(const QString &configPath, QObject *parent = nullptr);

    /// The process-wide instance QML and the platform layer bind to.
    static ConfigFacade *instance();

    int position() const;
    void setPosition(int value);

    int displayMode() const;
    void setDisplayMode(int value);

    QString targetOutput() const;
    void setTargetOutput(const QString &value);

    int tileSize() const;
    void setTileSize(int value);

    QStringList pinnedEntries() const;
    void setPinnedEntries(const QStringList &value);

    /// Writes pending changes to disk.
    Q_INVOKABLE void save();
    /// Re-reads from disk, discarding pending changes. Out-of-range values are
    /// clamped to the schema's min/max by KConfig itself.
    Q_INVOKABLE void reload();

Q_SIGNALS:
    /// Emitted whenever any key changes. The dock has five settings in Phase 1
    /// and rebinding all of them on any change costs nothing.
    void changed();

private:
    void redirectTo(const QString &configPath);
};

}
