#include "core/model/filedrop.h"

#include <QMimeDatabase>
#include <QMimeType>

using namespace frappe;

namespace
{
/// The two forms of "anything at all" that desktop entries use.
bool isCatchAll(const QString &type)
{
    return type == QLatin1String("all/all") || type == QLatin1String("all/allfiles")
        || type == QLatin1String("*/*") || type == QLatin1String("*");
}

/// `text/*` and friends. Only a trailing subtype wildcard exists in practice.
bool matchesGroupWildcard(const QString &supported, const QString &mimeType)
{
    if (!supported.endsWith(QLatin1String("/*"))) {
        return false;
    }
    const QStringView group = QStringView(supported).chopped(1); // keeps the slash
    return mimeType.startsWith(group);
}
}

bool frappe::acceptsMimeType(const QStringList &supportedTypes, const QString &mimeType)
{
    if (mimeType.isEmpty()) {
        return false;
    }

    QMimeDatabase db;
    const QMimeType type = db.mimeTypeForName(mimeType);

    for (const QString &supported : supportedTypes) {
        if (isCatchAll(supported) || supported == mimeType) {
            return true;
        }
        if (matchesGroupWildcard(supported, mimeType)) {
            return true;
        }
        // The shared-mime-info spec makes every text/* type a subclass of
        // text/plain implicitly. The database does not always say so — a type
        // that declares an explicit parent loses the implicit one, which is why
        // text/x-shellscript reports application/x-executable and nothing else
        // — so the rule is applied here. Without it a shell script dropped on a
        // text editor is refused, which is nonsense to the user holding it.
        if (supported == QLatin1String("text/plain") && type.isValid()
            && type.name().startsWith(QLatin1String("text/"))) {
            return true;
        }
        // Asked of the database rather than of the string: a shell script is
        // plain text, and an application that says it opens text should not
        // have to enumerate everything derived from it.
        if (type.isValid() && type.inherits(supported)) {
            return true;
        }
    }
    return false;
}

DropVerdict frappe::evaluateDrop(const QStringList &supportedTypes, const QList<QUrl> &files)
{
    if (files.isEmpty()) {
        return {DropRejection::NoFiles, QString()};
    }

    QMimeDatabase db;
    for (const QUrl &file : files) {
        // By name, not by content: the file may be on a remote share, and a
        // drag must not turn into a download to decide whether to highlight.
        const QMimeType type = db.mimeTypeForUrl(file);
        const QString name = type.isValid() ? type.name() : QString();

        if (!acceptsMimeType(supportedTypes, name)) {
            QString detail = type.isValid() ? type.comment() : QString();
            if (detail.isEmpty()) {
                detail = name;
            }
            return {DropRejection::UnsupportedType, detail};
        }
    }

    return {};
}
