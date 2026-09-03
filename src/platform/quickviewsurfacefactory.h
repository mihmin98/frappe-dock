#pragma once

#include <QHash>
#include <QObject>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include "core/model/stackmodel.h"
#include "platform/folderbackend.h"
#include "platform/surfacemanager.h"

#include <map>
#include <memory>

QT_BEGIN_NAMESPACE
class QQuickView;
QT_END_NAMESPACE

namespace frappe
{

class ConfigFacade;
class IIconProvider;
class TileModel;

/// Makes each dock surface a layer-shell QQuickView loading Dock.qml.
///
/// Owns the views; SurfaceManager only tells it which outputs need one.
class QuickViewSurfaceFactory : public QObject, public ISurfaceFactory
{
    Q_OBJECT

public:
    QuickViewSurfaceFactory(ConfigFacade *config, TileModel *model, const IIconProvider *icons, QObject *parent = nullptr);

    /// The object QML routes interactions through. Set before the first surface
    /// is created; it is passed as an initial property and not re-read after.
    void setController(QObject *controller);
    ~QuickViewSurfaceFactory() override;

    void createSurface(const OutputInfo &output) override;
    void destroySurface(const QString &outputId) override;
    void moveSurface(const QString &fromOutputId, const OutputInfo &to) override;
    std::vector<QString> surfaces() const override;

    /// Re-applies size, margins and exclusive zone to every surface. Call after
    /// tileSize or position changes.
    void updateGeometry();

    /// The rectangle an open stack occupies on \a outputId, in surface
    /// coordinates, or a null rect when no stack is open there.
    ///
    /// The surface spans the whole output but accepts input over the shelf
    /// alone, so anything drawn outside it is visible and unclickable. A stack
    /// is drawn outside it by definition, and this is what makes it reachable.
    /// One rect, not a set: only one stack is open at a time.
    void setStackRegion(const QString &outputId, const QRect &rect);

    /// The rectangle the shelf occupies on \a outputId, in surface
    /// coordinates. Told to the compositor as the region to blur behind.
    ///
    /// The surface spans the whole output, so blurring "the window" would blur
    /// the screen. The shelf also grows under magnification, which is why this
    /// is a running report from the view rather than a figure computed once.
    void setShelfRegion(const QString &outputId, const QRect &rect);

Q_SIGNALS:
    /// A tile was interacted with. Wired to the controller in main(), which
    /// decides what the interaction means.
    void tileClicked(const QString &tileId, int button, int modifiers);
    void tileHeld(const QString &tileId);

private Q_SLOTS:
    /// Dock.qml reporting where its open stack is. A by-name connection, so it
    /// has to be a slot.
    void onStackRegionChanged(const QRectF &region);

    /// Dock.qml reporting where its shelf is, for the same reason.
    void onShelfRegionChanged(const QRectF &region);

private:
    void configureSurface(QQuickView *view, const OutputInfo &output);
    /// The input region for \a outputId: the shelf, plus any open stack.
    void applyMask(QQuickView *view, const QString &outputId);
    /// Asks the compositor to blur behind \a outputId's shelf.
    void applyBlur(QQuickView *view, const QString &outputId);
    /// Applies every blur update the shelf asked for this turn, once each.
    void flushBlur();

    ConfigFacade *m_config;
    TileModel *m_model;
    const IIconProvider *m_icons;
    QObject *m_controller = nullptr;
    QHash<QString, QQuickView *> m_views;
    QHash<QString, QRect> m_stackRegions;
    QHash<QString, QRect> m_shelfRegions;
    /// Outputs whose blur region has changed but not yet been sent. Every
    /// rectangle of a region is a Wayland request and the shelf reports four
    /// changes per frame, so these are coalesced rather than sent as they
    /// arrive.
    QSet<QString> m_blurPending;
    QTimer m_blurUpdate;

    /// One folder backend and model per surface. Only one stack is open at a
    /// time, so one model serves whichever folder that is — a model per folder
    /// tile would be a KIO directory watch per folder tile, kept alive for
    /// folders nobody is looking at.
    // std::map, not QHash: QHash requires a copyable value type and these
    // are unique_ptr.
    std::map<QString, std::unique_ptr<FolderBackend>> m_folderBackends;
    std::map<QString, std::unique_ptr<StackModel>> m_stackModels;
};

}
