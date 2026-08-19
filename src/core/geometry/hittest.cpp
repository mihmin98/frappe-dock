#include "core/geometry/hittest.h"

namespace frappe::geometry
{

int hitTest(const std::vector<TilePlacement> &tiles, double pointerPos)
{
    if (tiles.empty()) {
        return noTile;
    }

    // Outside the strip is outside: the shelf's end padding belongs to no tile,
    // so a click there must not activate the first or last one.
    if (pointerPos < tiles.front().offset
        || pointerPos >= tiles.back().offset + tiles.back().size) {
        return noTile;
    }

    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const double end = tiles[i].offset + tiles[i].size;
        if (pointerPos < end) {
            return static_cast<int>(i);
        }
        if (i + 1 == tiles.size()) {
            break;
        }
        // In the gap: it belongs to whichever neighbour is nearer, so the
        // regions stay contiguous and no position is ambiguous.
        const double boundary = (end + tiles[i + 1].offset) / 2.0;
        if (pointerPos < boundary) {
            return static_cast<int>(i);
        }
        if (pointerPos < tiles[i + 1].offset) {
            return static_cast<int>(i) + 1;
        }
    }
    return static_cast<int>(tiles.size()) - 1;
}

int hitTest(const std::vector<TilePlacement> &base,
            double pointerPos,
            const MagnificationParams &m,
            double availableLength)
{
    return hitTest(magnify(base, pointerPos, m, availableLength), pointerPos);
}

}
