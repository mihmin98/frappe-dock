#pragma once

#include <QObject>
#include <QVariantMap>

#include "core/config/configfacade.h"
#include "core/geometry/stackplacement.h"

namespace frappe
{

/// Where an open stack goes, as something QML can call.
///
/// A separate object from DockGeometry, which holds the dock's layout state: the
/// placement needs none of it, and giving the popup its own DockGeometry just to
/// reach a pure function would put a second layout engine in the scene.
///
/// Everything it needs beyond the tile's anchor derives from S, so the argument
/// list stays short and the proportion model keeps one origin.
class StackAnchor : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// The stack's top-left, as `{ x, y }`.
    ///
    /// \a restingTileCentre is the parent tile's centre along the dock's axis
    /// **at rest** — see StackAnchorParams for why that word is load-bearing.
    /// \a edge is a ConfigFacade::Position.
    Q_INVOKABLE QVariantMap place(int edge,
                                  qreal restingTileCentre,
                                  qreal tileSize,
                                  qreal spacingRatio,
                                  qreal popupWidth,
                                  qreal popupHeight,
                                  qreal outputWidth,
                                  qreal outputHeight) const
    {
        const qreal gap = tileSize * spacingRatio;

        geometry::StackAnchorParams p;
        p.edge = static_cast<geometry::DockEdge>(edge);
        p.restingTileCentre = restingTileCentre;
        // The proportion model, not a second copy of it: the floating gap is
        // g/3, the shelf is S + 2g, and the stack stands off by one gap.
        p.shelfGap = gap / 3.0;
        p.shelfThickness = tileSize + 2.0 * gap;
        p.clearance = gap;
        p.screenMargin = gap;
        p.popupWidth = popupWidth;
        p.popupHeight = popupHeight;
        p.outputWidth = outputWidth;
        p.outputHeight = outputHeight;

        const geometry::StackPlacement placed = geometry::placeStack(p);
        return QVariantMap{{QStringLiteral("x"), placed.x}, {QStringLiteral("y"), placed.y}};
    }
};

// ConfigFacade::Position is passed straight through as a DockEdge. The two
// agree on the three edges the dock offers; DockEdge has a fourth the config
// does not, which is fine in this direction and would not be in the other.
static_assert(static_cast<int>(ConfigFacade::Bottom) == static_cast<int>(geometry::DockEdge::Bottom));
static_assert(static_cast<int>(ConfigFacade::Left) == static_cast<int>(geometry::DockEdge::Left));
static_assert(static_cast<int>(ConfigFacade::Right) == static_cast<int>(geometry::DockEdge::Right));

}
