#pragma once

#include <QString>

#include <optional>

namespace frappe
{

enum class TileKind {
    Application,
    Folder,
    MinimizedWindow,
    Trash,
    Separator,
    /// An ordinary file in the file region. A folder there is a Folder, which
    /// opens into a stack; this one just opens.
    File,
};

/// Ordered regions of the dock. Tiles sort by region first, then by their order
/// within it. Most regions are empty until later phases; they are tagged now
/// because the ordering is far easier to establish before there is data in it.
enum class Region {
    Head,
    Pinned,
    Recent,
    Files,
    Minimized,
    Tail,
};

struct Tile {
    TileKind kind = TileKind::Application;
    Region region = Region::Pinned;
    QString id; ///< desktop entry id, or file URL
    QString name;
    QString iconName;
    bool isPinned = false;
    bool isRunning = false;
    int windowCount = 0;
    std::optional<int> badgeCount;
    std::optional<double> progress; ///< 0.0–1.0

    bool operator==(const Tile &) const = default;
};

}
