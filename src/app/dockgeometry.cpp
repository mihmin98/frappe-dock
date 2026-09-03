#include "app/dockgeometry.h"

#include "core/geometry/hittest.h"

#include <QVariantMap>

#include <algorithm>

using namespace frappe;

DockGeometry::DockGeometry(QObject *parent)
    : QObject(parent)
{
    // The floor is a design correction rather than a reference measurement:
    // tiles must stay legible instead of compressing without limit.
    m_layout.minTileSize = 24.0;
    m_layout.maxTileSize = 48.0;
    m_layout.spacing = m_layout.maxTileSize * m_spacingRatio;
}

int DockGeometry::tileCount() const
{
    return m_rowCount;
}

void DockGeometry::setTileCount(int value)
{
    value = std::max(value, 0);
    if (m_rowCount == value) {
        return;
    }
    m_rowCount = value;
    rebuild();
}

qreal DockGeometry::tileSize() const
{
    return m_layout.maxTileSize;
}

void DockGeometry::setTileSize(qreal value)
{
    if (qFuzzyCompare(m_layout.maxTileSize, value)) {
        return;
    }
    m_layout.maxTileSize = value;
    // S/3, the one gap constant — derived here rather than stored, and
    // deliberately not a config key of its own (plan.md Part 0).
    m_layout.spacing = value * m_spacingRatio;
    rebuild();
}

qreal DockGeometry::spacingRatio() const
{
    return m_spacingRatio;
}

void DockGeometry::setSpacingRatio(qreal value)
{
    if (value <= 0.0 || qFuzzyCompare(m_spacingRatio, value)) {
        return;
    }
    m_spacingRatio = value;
    m_layout.spacing = m_layout.maxTileSize * value;
    rebuild();
}

qreal DockGeometry::minimumTileSize() const
{
    return m_layout.minTileSize;
}

void DockGeometry::setMinimumTileSize(qreal value)
{
    if (qFuzzyCompare(m_layout.minTileSize, value)) {
        return;
    }
    m_layout.minTileSize = value;
    rebuild();
}

qreal DockGeometry::availableLength() const
{
    return m_layout.availableLength;
}

void DockGeometry::setAvailableLength(qreal value)
{
    if (qFuzzyCompare(m_layout.availableLength, value)) {
        return;
    }
    m_layout.availableLength = value;
    rebuild();
}

QList<int> DockGeometry::separatorRows() const
{
    return m_separatorRows;
}

void DockGeometry::setSeparatorRows(const QList<int> &value)
{
    if (m_separatorRows == value) {
        return;
    }
    m_separatorRows = value;
    rebuild();
}

qreal DockGeometry::pointerPosition() const
{
    return m_pointer;
}

void DockGeometry::setPointerPosition(qreal value)
{
    // Anything negative means "away", so they must not be told apart — a
    // pointer at -1 and one at -40 are the same resting layout.
    if (value < 0.0 && m_pointer < 0.0) {
        return;
    }
    if (qFuzzyCompare(m_pointer, value)) {
        return;
    }
    m_pointer = value;
    republish();
}

bool DockGeometry::magnificationEnabled() const
{
    return m_magnificationEnabled;
}

void DockGeometry::setMagnificationEnabled(bool value)
{
    if (m_magnificationEnabled == value) {
        return;
    }
    m_magnificationEnabled = value;
    republish();
}

qreal DockGeometry::magnifiedSize() const
{
    return m_magnification.magnifiedSize;
}

void DockGeometry::setMagnifiedSize(qreal value)
{
    if (qFuzzyCompare(m_magnification.magnifiedSize, value)) {
        return;
    }
    m_magnification.magnifiedSize = value;
    republish();
}

qreal DockGeometry::falloffRadius() const
{
    return m_magnification.falloffRadius;
}

void DockGeometry::setFalloffRadius(qreal value)
{
    if (qFuzzyCompare(m_magnification.falloffRadius, value)) {
        return;
    }
    m_magnification.falloffRadius = value;
    republish();
}

qreal DockGeometry::curveExponent() const
{
    return m_magnification.curveExponent;
}

void DockGeometry::setCurveExponent(qreal value)
{
    if (qFuzzyCompare(m_magnification.curveExponent, value)) {
        return;
    }
    m_magnification.curveExponent = value;
    republish();
}

QVariantList DockGeometry::tileGeometry() const
{
    return m_published;
}

qreal DockGeometry::padding() const
{
    // The resting padding, not one derived from the magnified first tile: the
    // shelf's inner padding is a property of the layout and does not swell
    // because the pointer happens to be near the end of the strip.
    return m_base.empty() ? m_layout.spacing : m_base.front().offset;
}

qreal DockGeometry::restingLength() const
{
    if (m_base.empty()) {
        return 2.0 * m_layout.spacing;
    }
    return m_base.back().offset + m_base.back().size + padding();
}

qreal DockGeometry::shelfStart() const
{
    if (m_magnified.empty()) {
        return 0.0;
    }
    // magnify() anchors the pointed tile under the cursor, so the strip grows
    // in whichever direction the pointer is and can begin *before* the resting
    // shelf did — a negative number. That is the shelf moving, not the strip
    // sliding within it: shifting the contents back would put the pointed tile
    // somewhere other than under the pointer, which is the one thing the
    // anchoring exists to guarantee.
    return m_magnified.front().offset - padding();
}

qreal DockGeometry::shelfLength() const
{
    if (m_magnified.empty()) {
        // An empty dock is still a shelf: the inner padding at each end, with
        // nothing between them. Collapsing it to zero would make the shelf
        // vanish rather than read as empty.
        return 2.0 * m_layout.spacing;
    }
    return m_magnified.back().offset + m_magnified.back().size + padding() - shelfStart();
}

qreal DockGeometry::effectiveTileSize() const
{
    // Read off the layout rather than recomputed from the parameters: the
    // compression rule lives in one place and this is not a second copy of it.
    return m_base.empty() ? m_layout.maxTileSize : m_base.front().size;
}

qreal DockGeometry::surfaceThickness() const
{
    const qreal peak = m_magnificationEnabled ? m_magnification.magnifiedSize : m_layout.maxTileSize;
    return geometry::surfaceThickness(m_layout.maxTileSize, m_layout.spacing, peak);
}

qreal DockGeometry::shelfThickness() const
{
    return geometry::shelfThickness(m_layout.maxTileSize, m_layout.spacing);
}

qreal DockGeometry::shelfRadius() const
{
    return geometry::shelfCornerRadius(shelfThickness());
}

int DockGeometry::rowAt(qreal position) const
{
    const int tile = geometry::hitTest(m_magnified, position);
    if (tile < 0) {
        return -1;
    }

    const auto row = std::ranges::find(m_tileOfRow, tile);
    return row == m_tileOfRow.end() ? -1 : int(row - m_tileOfRow.begin());
}

qreal DockGeometry::restingCentreOfRow(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_tileOfRow.size())) {
        return -1.0;
    }
    const int tile = m_tileOfRow[std::size_t(row)];
    // A separator is a rule between regions, not a cell. Nothing opens from it.
    if (tile < 0 || tile >= static_cast<int>(m_base.size())) {
        return -1.0;
    }

    const geometry::TilePlacement &placement = m_base[std::size_t(tile)];
    return placement.offset + placement.size / 2.0;
}

void DockGeometry::rebuild()
{
    // The model carries separators as rows; the engine takes them as widened
    // gaps between tiles, because a separator is a rule with clearance and not
    // a cell (plan.md Part 0). Translating between the two is this function's
    // job.
    m_tileOfRow.assign(std::size_t(m_rowCount), -1);
    m_layout.separatorsAfter.clear();

    int tiles = 0;
    for (int row = 0; row < m_rowCount; ++row) {
        if (m_separatorRows.contains(row)) {
            m_layout.separatorsAfter.push_back(tiles - 1);
        } else {
            m_tileOfRow[std::size_t(row)] = tiles++;
        }
    }

    m_layout.tileCount = tiles;
    m_base = geometry::layout(m_layout);

    // Before republish(), so a consumer that reads both in one go sees the
    // resting layout that the magnified one was derived from.
    Q_EMIT restingChanged();
    republish();
}

std::pair<qreal, qreal> DockGeometry::separatorSpan(std::size_t row) const
{
    if (m_magnified.empty()) {
        return {0.0, 0.0};
    }

    // The tile before this separator row, if any: the gap it sits in runs from
    // that tile's trailing edge to the next tile's leading edge.
    int previous = -1;
    for (std::size_t r = row; r-- > 0;) {
        if (m_tileOfRow[r] >= 0) {
            previous = m_tileOfRow[r];
            break;
        }
    }

    if (previous < 0) {
        return {m_magnified.front().offset, 0.0};
    }
    const auto &before = m_magnified[std::size_t(previous)];
    if (std::size_t(previous) + 1 >= m_magnified.size()) {
        return {before.offset + before.size, 0.0};
    }
    const double start = before.offset + before.size;
    return {start, m_magnified[std::size_t(previous) + 1].offset - start};
}

void DockGeometry::republish()
{
    // Default-constructed parameters are the "off" case, and magnify() returns
    // the resting layout for them, so magnification off and pointer away stay
    // one code path rather than two.
    const geometry::MagnificationParams params =
        m_magnificationEnabled && m_pointer >= 0.0 ? m_magnification : geometry::MagnificationParams{};

    m_magnified = geometry::magnify(m_base, m_pointer, params, m_layout.availableLength);

    // The list keeps its buffer; only the entries are rewritten. Updating the
    // maps in place was measured *slower* — reading a QVariantMap back out of a
    // QVariant detaches it, which costs more than building a fresh one. At 21
    // tiles this is ~1.3 us per update against 0.18 us for the engine itself,
    // which at a 1000 Hz pointer is well under a percent of a core.
    if (m_published.size() != qsizetype(m_tileOfRow.size())) {
        m_published.resize(qsizetype(m_tileOfRow.size()));
    }
    for (std::size_t row = 0; row < m_tileOfRow.size(); ++row) {
        const int tile = m_tileOfRow[row];

        QVariantMap entry;
        if (tile >= 0 && std::size_t(tile) < m_magnified.size()) {
            entry.insert(QStringLiteral("offset"), m_magnified[std::size_t(tile)].offset);
            entry.insert(QStringLiteral("size"), m_magnified[std::size_t(tile)].size);
        } else {
            // A separator row occupies the widened gap the engine left for it,
            // so the view can draw the rule centred in it without knowing the
            // clearance rule.
            const auto [offset, size] = separatorSpan(row);
            entry.insert(QStringLiteral("offset"), offset);
            entry.insert(QStringLiteral("size"), size);
        }
        m_published[qsizetype(row)] = entry;
    }

    Q_EMIT changed();
}
