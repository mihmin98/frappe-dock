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
    Q_PROPERTY(bool magnificationEnabled READ magnificationEnabled WRITE setMagnificationEnabled NOTIFY changed)
    Q_PROPERTY(qreal magnificationFactor READ magnificationFactor WRITE setMagnificationFactor NOTIFY changed)
    Q_PROPERTY(qreal falloffRadius READ falloffRadius WRITE setFalloffRadius NOTIFY changed)
    Q_PROPERTY(qreal curveExponent READ curveExponent WRITE setCurveExponent NOTIFY changed)
    Q_PROPERTY(int animationSpeed READ animationSpeed WRITE setAnimationSpeed NOTIFY changed)
    Q_PROPERTY(bool showRunningIndicators READ showRunningIndicators WRITE setShowRunningIndicators NOTIFY changed)
    Q_PROPERTY(bool minimizeIntoIcon READ minimizeIntoIcon WRITE setMinimizeIntoIcon NOTIFY changed)
    Q_PROPERTY(int springLoadDelay READ springLoadDelay WRITE setSpringLoadDelay NOTIFY changed)
    Q_PROPERTY(QStringList pinnedEntries READ pinnedEntries WRITE setPinnedEntries NOTIFY changed)
    Q_PROPERTY(QStringList fileEntries READ fileEntries WRITE setFileEntries NOTIFY changed)

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

    bool magnificationEnabled() const;
    void setMagnificationEnabled(bool value);

    /// The three below are stored as percentages — KConfigXT has no
    /// range-checked real — and exposed as the ratios the engine actually
    /// takes, so the x100 stays in this file and nowhere else.
    qreal magnificationFactor() const;
    void setMagnificationFactor(qreal value);

    qreal falloffRadius() const;
    void setFalloffRadius(qreal value);

    qreal curveExponent() const;
    void setCurveExponent(qreal value);

    /// Percentage of normal speed; 0 means no animation at all.
    int animationSpeed() const;
    void setAnimationSpeed(int value);

    bool showRunningIndicators() const;
    void setShowRunningIndicators(bool value);

    bool minimizeIntoIcon() const;
    void setMinimizeIntoIcon(bool value);

    /// Milliseconds a drag must rest on a tile before it acts; 0 is off.
    int springLoadDelay() const;
    void setSpringLoadDelay(int value);

    QStringList pinnedEntries() const;
    void setPinnedEntries(const QStringList &value);

    QStringList fileEntries() const;
    void setFileEntries(const QStringList &value);

    /// Writes pending changes to disk.
    Q_INVOKABLE void save();
    /// Re-reads from disk, discarding pending changes. Out-of-range values are
    /// clamped to the schema's min/max by KConfig itself.
    Q_INVOKABLE void reload();

Q_SIGNALS:
    /// Emitted whenever any key changes. The dock has a handful of settings and
    /// rebinding all of them on any change costs nothing.
    void changed();

private:
    void redirectTo(const QString &configPath);
};

}
