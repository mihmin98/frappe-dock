#pragma once

#include <QList>
#include <QUrl>

#include "core/model/tile.h"

namespace frappe
{

/// What is being dragged onto the dock, as far as the dock is concerned.
enum class DropItemKind {
    Unknown,
    Application,
    File,
    Folder,
};

/// Why a payload may not be dropped where it was aimed.
enum class RegionDropRejection {
    None,
    /// The item is fine, the place is not: an application aimed at the file
    /// area, or a file aimed at the applications.
    WrongRegion,
    /// Nothing recognisable in the payload.
    UnknownItem,
    /// Applications and files in one drag. There is no single region for it,
    /// and splitting the payload across two would be a guess.
    MixedPayload,
    /// A desktop entry that is not an installed application. Reported by the
    /// caller, which is the only one that can look it up.
    NotInstalled,
};

struct RegionDropVerdict {
    RegionDropRejection rejection = RegionDropRejection::None;
    DropItemKind kind = DropItemKind::Unknown;
    /// Where the payload does belong, for a message that can say so. Only
    /// meaningful when the rejection is WrongRegion.
    Region expectedRegion = Region::Pinned;

    bool accepted() const
    {
        return rejection == RegionDropRejection::None;
    }

    bool operator==(const RegionDropVerdict &) const = default;
};

/// What \a url is: a desktop entry, a directory, or an ordinary file.
DropItemKind classifyDroppedUrl(const QUrl &url);

/// The one kind the whole payload is, or Unknown if it is not all one thing.
DropItemKind classifyPayload(const QList<QUrl> &urls);

/// Where items of this kind live: applications among the pinned tiles, files
/// and folders in the file region.
Region regionForKind(DropItemKind kind);

/// Whether \a urls may be dropped into \a target, and if not, why.
RegionDropVerdict evaluateRegionDrop(const QList<QUrl> &urls, Region target);

}
