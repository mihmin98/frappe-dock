#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace frappe
{

/// Why a drop was refused. The dock has to be able to say *why* a target is
/// invalid rather than silently declining — the analysis records the silent
/// rejection as a decades-old papercut — and a reason code lets the wording
/// live in the view, where translation does.
enum class DropRejection {
    None,
    /// Nothing draggable in the payload: no file URLs at all.
    NoFiles,
    /// The application does not declare that it can open one of the files.
    UnsupportedType,
};

struct DropVerdict {
    DropRejection rejection = DropRejection::None;
    /// The offending file's type, in human-readable form ("PDF document"), for
    /// the message. Empty when nothing is wrong or nothing better than the raw
    /// type is known.
    QString detail;

    bool accepted() const
    {
        return rejection == DropRejection::None;
    }

    bool operator==(const DropVerdict &) const = default;
};

/// Whether an application declaring \a supportedTypes can open \a mimeType.
///
/// Inheritance counts: an editor that declares `text/plain` can open a shell
/// script, because a shell script *is* plain text. The wildcards are the ones
/// desktop entries actually use — `text/*`, and `all/all` / `all/allfiles` for
/// the handful of applications that take anything.
bool acceptsMimeType(const QStringList &supportedTypes, const QString &mimeType);

/// Whether \a supportedTypes can open every one of \a files, and if not, which
/// one stopped it.
///
/// Every one, not any one: a partial open is the worst answer available. The
/// user dropped a selection, and quietly opening two of the four files leaves
/// them to work out which two.
DropVerdict evaluateDrop(const QStringList &supportedTypes, const QList<QUrl> &files);

}
