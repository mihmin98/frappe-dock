#include "core/input/springloader.h"

using namespace frappe;

SpringLoader::SpringLoader(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &SpringLoader::fire);

    m_leaveTimer.setSingleShot(true);
    m_leaveTimer.setInterval(0);
    connect(&m_leaveTimer, &QTimer::timeout, this, &SpringLoader::dragFinished);
}

int SpringLoader::delay() const
{
    return m_timer.interval();
}

void SpringLoader::setDelay(int milliseconds)
{
    if (milliseconds == m_timer.interval()) {
        return;
    }

    m_timer.setInterval(qMax(0, milliseconds));
    // A change mid-drag applies to the next tile rather than retiming the one
    // already counting down, which would move the target under the user.
    Q_EMIT delayChanged();
}

QString SpringLoader::armedTile() const
{
    return m_armed;
}

void SpringLoader::dragEntered(const QString &tileId)
{
    // An enter following a shelf exit means the drag crossed onto a tile rather
    // than leaving, so the pending end-of-drag is withdrawn. Done before the
    // guards below: a tile that has already fired still cancels the leave.
    m_leaveTimer.stop();

    if (delay() <= 0 || tileId.isEmpty() || m_fired.contains(tileId)) {
        return;
    }

    // Entering the tile already counting down is not a new arrival — a drag
    // wobbling inside one cell must not keep restarting the countdown.
    if (tileId == m_armed) {
        return;
    }

    m_armed = tileId;
    m_timer.start();
    Q_EMIT armedTileChanged();
}

void SpringLoader::dragLeft()
{
    disarm();
}

void SpringLoader::dragFinished()
{
    m_leaveTimer.stop();
    m_fired.clear();
    disarm();
}

void SpringLoader::dockLeft()
{
    // Disarmed at once — the countdown must not survive the pointer leaving the
    // tile even briefly — but the fired set is only cleared if no enter arrives
    // to contradict this.
    disarm();
    m_leaveTimer.start();
}

void SpringLoader::fire()
{
    const QString tile = m_armed;
    if (tile.isEmpty()) {
        return;
    }

    m_fired.insert(tile);
    disarm();
    // Last, so a handler that asks what is armed gets the answer for after the
    // spring-load rather than during it.
    Q_EMIT springLoaded(tile);
}

void SpringLoader::disarm()
{
    m_timer.stop();
    if (m_armed.isEmpty()) {
        return;
    }

    m_armed.clear();
    Q_EMIT armedTileChanged();
}
