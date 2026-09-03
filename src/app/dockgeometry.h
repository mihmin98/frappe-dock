#pragma once

#include <QObject>
#include <QVariantList>

#include <utility>
#include <vector>

#include "core/geometry/layout.h"
#include "core/geometry/magnification.h"

namespace frappe
{

/// The geometry engine, as something QML can bind to.
///
/// Everything of substance is in core/geometry; this only holds the inputs,
/// re-runs the pure functions when one of them changes, and publishes the
/// result. It is deliberately not part of DockController: where a tile sits is
/// a function of the tile count, the tile size and the pointer, and needs
/// neither a launcher nor a task backend. Keeping it separate is also what lets
/// there be exactly one path from the engine to the view — a second, fallback
/// way of positioning tiles is how hit regions and layout drift apart, which
/// §3.4 exists to prevent.
class DockGeometry : public QObject
{
    Q_OBJECT

    /// How many rows the model has, separator rows **included** — i.e. what a
    /// Repeater reports. Which of them are separators is `separatorRows`; the
    /// engine is told about them as gaps rather than cells, and this class does
    /// the translation.
    Q_PROPERTY(int tileCount READ tileCount WRITE setTileCount NOTIFY changed)

    /// S in the proportion model: the layout cell edge. Every other dimension
    /// derives from it, which is why it is the only size input here.
    Q_PROPERTY(qreal tileSize READ tileSize WRITE setTileSize NOTIFY changed)

    /// The floor tiles compress to. A design correction: tiles must stay
    /// legible rather than shrinking without limit.
    Q_PROPERTY(qreal minimumTileSize READ minimumTileSize WRITE setMinimumTileSize NOTIFY changed)

    /// Usable length along the dock's axis.
    Q_PROPERTY(qreal availableLength READ availableLength WRITE setAvailableLength NOTIFY changed)

    /// The inter-icon gap as a fraction of the tile, 1/3 in the proportion
    /// model. It is **not** a config key and the dock never moves it: the
    /// tuning harness needs to be able to ask whether 1/3 is the right number,
    /// and that question cannot be asked if the ratio is a literal.
    Q_PROPERTY(qreal spacingRatio READ spacingRatio WRITE setSpacingRatio NOTIFY changed)

    /// Model rows that hold a separator, so this can translate between the
    /// model's rows and the engine's tiles.
    Q_PROPERTY(QList<int> separatorRows READ separatorRows WRITE setSeparatorRows NOTIFY changed)

    /// Where the pointer is along the dock, from the strip's start. Negative
    /// means the pointer is elsewhere and the layout rests. This is the hot
    /// input: it changes on every motion event.
    Q_PROPERTY(qreal pointerPosition READ pointerPosition WRITE setPointerPosition NOTIFY changed)

    Q_PROPERTY(bool magnificationEnabled READ magnificationEnabled WRITE setMagnificationEnabled NOTIFY changed)
    Q_PROPERTY(qreal magnifiedSize READ magnifiedSize WRITE setMagnifiedSize NOTIFY changed)
    Q_PROPERTY(qreal falloffRadius READ falloffRadius WRITE setFalloffRadius NOTIFY changed)
    Q_PROPERTY(qreal curveExponent READ curveExponent WRITE setCurveExponent NOTIFY changed)

    /// Where everything is, as one `{ offset, size }` map per **model row** —
    /// separator rows included, so a Repeater can index it directly. The only
    /// thing the view positions tiles from.
    Q_PROPERTY(QVariantList tileGeometry READ tileGeometry NOTIFY changed)

    /// The strip plus the shelf's padding at each end, for sizing the shelf.
    Q_PROPERTY(qreal shelfLength READ shelfLength NOTIFY changed)

    /// Where the shelf begins, in the same coordinates everything else here is
    /// published in — negative once magnification has pushed the strip back
    /// past where it rests. The view positions the shelf with it and offsets
    /// tiles by it; see the note on the implementation for why the shelf moves
    /// rather than the strip.
    Q_PROPERTY(qreal shelfStart READ shelfStart NOTIFY changed)

    /// The shelf's length with no pointer anywhere near it. Fixed for a given
    /// tile count and size, which is what makes it usable as the origin the
    /// pointer is measured from: an origin that moved with the shelf would be
    /// a feedback loop.
    Q_PROPERTY(qreal restingLength READ restingLength NOTIFY changed)

    /// The cell edge the resting layout settled on: `tileSize`, or less once
    /// the strip has had to compress. What the view needs for anything sized
    /// against the cell but not placed as one — a separator's rule, say.
    Q_PROPERTY(qreal effectiveTileSize READ effectiveTileSize NOTIFY changed)

    /// How thick the surface has to be to draw everything the dock draws: the
    /// shelf, the magnification peak above it, and the room the drag-out gesture
    /// needs. Exposed rather than recomputed in QML so the view and the layer
    /// surface cannot drift apart — they are the same number from the same
    /// function, and a shelf-sized view clips the gesture it is meant to show.
    Q_PROPERTY(qreal surfaceThickness READ surfaceThickness NOTIFY changed)

public:
    explicit DockGeometry(QObject *parent = nullptr);

    int tileCount() const;
    void setTileCount(int value);

    qreal tileSize() const;
    void setTileSize(qreal value);

    qreal minimumTileSize() const;
    void setMinimumTileSize(qreal value);

    qreal availableLength() const;
    void setAvailableLength(qreal value);

    qreal spacingRatio() const;
    void setSpacingRatio(qreal value);

    QList<int> separatorRows() const;
    void setSeparatorRows(const QList<int> &value);

    qreal pointerPosition() const;
    void setPointerPosition(qreal value);

    bool magnificationEnabled() const;
    void setMagnificationEnabled(bool value);

    qreal magnifiedSize() const;
    void setMagnifiedSize(qreal value);

    qreal falloffRadius() const;
    void setFalloffRadius(qreal value);

    qreal curveExponent() const;
    void setCurveExponent(qreal value);

    QVariantList tileGeometry() const;
    qreal shelfLength() const;
    qreal shelfStart() const;
    qreal restingLength() const;
    qreal effectiveTileSize() const;
    qreal surfaceThickness() const;

    /// Which **model row** is at \a position, or -1. Read off the same
    /// placements the view drew from, never recomputed: two implementations
    /// drift, and the symptom is clicks landing on the neighbouring tile.
    Q_INVOKABLE int rowAt(qreal position) const;

    /// Where \a row's centre sits when nothing is magnified, in the same
    /// coordinates `tileGeometry` publishes offsets in. -1 for a row that does
    /// not exist or is a separator.
    ///
    /// Read off the *resting* layout, deliberately. It is what an open stack
    /// anchors to, and the point of it is that it does not move: pointer motion
    /// changes `tileGeometry` on every event and leaves this alone. Anchoring a
    /// stack to the magnified centre instead is the reference platform's §11
    /// defect — the stack jumps the moment the pointer moves.
    Q_INVOKABLE qreal restingCentreOfRow(int row) const;

Q_SIGNALS:
    /// One signal for every input and output. The view rebinds the whole strip
    /// whatever changed, so splitting this would buy nothing.
    void changed();

private:
    /// Rebuilds the resting layout. Everything but pointer motion goes through
    /// here.
    void rebuild();

    /// Re-magnifies for the current pointer and republishes. The hot path.
    void republish();

    /// The resting inner padding at each end of the shelf, which is also the
    /// inter-icon gap — one constant, not two.
    qreal padding() const;

    /// The gap the separator on \a row occupies, as { offset, length }.
    std::pair<qreal, qreal> separatorSpan(std::size_t row) const;

    geometry::LayoutParams m_layout;
    qreal m_spacingRatio = 1.0 / 3.0;
    geometry::MagnificationParams m_magnification{96.0, 3.0, 1.6};
    bool m_magnificationEnabled = true;
    qreal m_pointer = -1.0;

    int m_rowCount = 0;
    QList<int> m_separatorRows;

    /// Model row -> engine tile index, or -1 for a separator row. The model
    /// counts separators as rows and the engine does not; this is what keeps
    /// the published list indexable by row.
    std::vector<int> m_tileOfRow;

    /// Reused across updates so a motion event allocates nothing it need not.
    std::vector<geometry::TilePlacement> m_base;
    std::vector<geometry::TilePlacement> m_magnified;
    QVariantList m_published;
};

}
