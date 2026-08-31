#include "core/model/regiondrop.h"

#include <QFileInfo>

using namespace frappe;

DropItemKind frappe::classifyDroppedUrl(const QUrl &url)
{
    if (url.isEmpty()) {
        return DropItemKind::Unknown;
    }

    // By suffix rather than by MIME type: a desktop entry is an application to
    // the dock whether or not the database has an opinion about the file, and
    // this has to hold for entries in directories the database never scans.
    if (url.fileName().endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        return DropItemKind::Application;
    }

    if (!url.isLocalFile()) {
        // Nothing remote can be a directory as far as the dock is concerned:
        // deciding would mean a network round trip during a drag.
        return DropItemKind::File;
    }

    return QFileInfo(url.toLocalFile()).isDir() ? DropItemKind::Folder : DropItemKind::File;
}

DropItemKind frappe::classifyPayload(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return DropItemKind::Unknown;
    }

    DropItemKind kind = classifyDroppedUrl(urls.first());
    for (const QUrl &url : urls) {
        const DropItemKind next = classifyDroppedUrl(url);
        if (next == DropItemKind::Unknown) {
            return DropItemKind::Unknown;
        }
        // Files and folders share a region, so a selection of both is one
        // payload. An application among them is not.
        const bool bothAreContent = next != DropItemKind::Application && kind != DropItemKind::Application;
        if (next != kind && !bothAreContent) {
            return DropItemKind::Unknown;
        }
        if (bothAreContent && next != kind) {
            kind = DropItemKind::File;
        }
    }
    return kind;
}

Region frappe::regionForKind(DropItemKind kind)
{
    return kind == DropItemKind::Application ? Region::Pinned : Region::Files;
}

RegionDropVerdict frappe::evaluateRegionDrop(const QList<QUrl> &urls, Region target)
{
    RegionDropVerdict verdict;
    verdict.kind = classifyPayload(urls);

    if (verdict.kind == DropItemKind::Unknown) {
        // Two different failures wearing the same face: nothing recognisable,
        // and several recognisable things that do not belong together. The
        // messages are different, so the codes are too.
        bool anyApplication = false;
        bool anyContent = false;
        for (const QUrl &url : urls) {
            switch (classifyDroppedUrl(url)) {
            case DropItemKind::Application:
                anyApplication = true;
                break;
            case DropItemKind::File:
            case DropItemKind::Folder:
                anyContent = true;
                break;
            case DropItemKind::Unknown:
                break;
            }
        }
        verdict.rejection = anyApplication && anyContent ? RegionDropRejection::MixedPayload
                                                         : RegionDropRejection::UnknownItem;
        return verdict;
    }

    verdict.expectedRegion = regionForKind(verdict.kind);
    if (target != verdict.expectedRegion) {
        verdict.rejection = RegionDropRejection::WrongRegion;
    }
    return verdict;
}
