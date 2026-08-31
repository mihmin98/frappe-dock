#pragma once

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QObject>
#include <QPointF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>

/*
 * Synthesised external drags, for testing DropArea from QML.
 *
 * QtQuickTest can press and move the mouse, but a drag from another
 * application is not a mouse gesture: it arrives as QDragEnter/Move/Drop
 * events carrying a QMimeData, and there is no QML API that produces one.
 * This posts those events to the window the way the platform would, so a
 * DropArea sees exactly what it sees in a real session.
 *
 * Coordinates are given in the passed item's coordinate system, which is what
 * a test already has to hand from mapToItem().
 */
class DragSimulator : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Begins a drag over \a item carrying \a urls. Returns whether the
    /// enter was accepted.
    Q_INVOKABLE bool enter(QQuickItem *item, qreal x, qreal y, const QList<QUrl> &urls)
    {
        m_mime.setUrls(urls);
        QQuickWindow *window = item ? item->window() : nullptr;
        if (!window) {
            return false;
        }

        QDragEnterEvent event(pointIn(item, x, y), Qt::CopyAction, &m_mime,
                              Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &event);
        return event.isAccepted();
    }

    /// Moves the drag to (\a x, \a y) in \a item's coordinates.
    Q_INVOKABLE bool move(QQuickItem *item, qreal x, qreal y)
    {
        QQuickWindow *window = item ? item->window() : nullptr;
        if (!window) {
            return false;
        }

        QDragMoveEvent event(pointIn(item, x, y), Qt::CopyAction, &m_mime,
                             Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &event);
        return event.isAccepted();
    }

    /// Releases the drag over (\a x, \a y). Returns whether it was taken.
    Q_INVOKABLE bool drop(QQuickItem *item, qreal x, qreal y)
    {
        QQuickWindow *window = item ? item->window() : nullptr;
        if (!window) {
            return false;
        }

        QDropEvent event(QPointF(pointIn(item, x, y)), Qt::CopyAction, &m_mime,
                         Qt::LeftButton, Qt::NoModifier);
        QGuiApplication::sendEvent(window, &event);
        return event.isAccepted();
    }

    /// Takes the drag back out of the window without dropping it.
    Q_INVOKABLE void leave(QQuickItem *item)
    {
        QQuickWindow *window = item ? item->window() : nullptr;
        if (!window) {
            return;
        }

        QDragLeaveEvent event;
        QGuiApplication::sendEvent(window, &event);
    }

private:
    static QPoint pointIn(QQuickItem *item, qreal x, qreal y)
    {
        return item->mapToScene(QPointF(x, y)).toPoint();
    }

    QMimeData m_mime;
};
