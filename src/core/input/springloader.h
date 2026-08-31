#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace frappe
{

/*
 * The delay that turns "a drag is resting here" into "act on this tile".
 *
 * Spring-loading is the one dock interaction that happens without a release,
 * which makes both of its failure modes expensive: firing late is a delay the
 * user waits out, firing when they were only passing through interrupts the
 * drag they were actually making. So the rules are narrow and live here rather
 * than in QML: one tile is armed at a time, leaving it disarms, and a tile that
 * has fired stays fired until the drag goes somewhere else.
 *
 * The view feeds it drag events and acts on springLoaded(); it does not decide
 * when.
 */
class SpringLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)
    Q_PROPERTY(QString armedTile READ armedTile NOTIFY armedTileChanged)

public:
    explicit SpringLoader(QObject *parent = nullptr);

    /// Milliseconds a drag must rest before the tile acts. 0 disables
    /// spring-loading: nothing is ever armed and nothing ever fires.
    int delay() const;
    void setDelay(int milliseconds);

    /// The tile currently counting down, or empty when none is. A tile that has
    /// already fired is not armed.
    QString armedTile() const;

public Q_SLOTS:
    /// A drag arrived over \a tileId. Starts the countdown, unless this tile
    /// has already fired during this drag — a tile springs open once per drag,
    /// so hovering back over what just opened does not open it again.
    void dragEntered(const QString &tileId);

    /// The drag left the tile it was over, and is still in the air. Disarms;
    /// what has already fired stays fired until the drag ends.
    void dragLeft();

    /// The drag ended — dropped or cancelled. Disarms everything.
    void dragFinished();

    /// The drag left the shelf. This cannot be acted on at once: the tiles' drop
    /// areas are nested inside the shelf's, so crossing onto a tile reports a
    /// shelf exit too, and treating that as the end of the drag re-opens every
    /// tile on every pass. Resolved on the next turn of the event loop instead —
    /// an enter arriving first means the drag only moved onto a tile.
    void dockLeft();

Q_SIGNALS:
    /// \a tileId has been rested on long enough. Emitted once per arming.
    void springLoaded(const QString &tileId);
    void delayChanged();
    void armedTileChanged();

private:
    void fire();
    void disarm();

    QTimer m_timer;
    /// Fires on the next event-loop turn to confirm a shelf exit was real.
    QTimer m_leaveTimer;
    QString m_armed;
    /// The tiles this drag has already opened. Cleared when the drag ends.
    QSet<QString> m_fired;
};

}
