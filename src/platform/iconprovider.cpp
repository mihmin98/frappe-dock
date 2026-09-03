#include "platform/iconprovider.h"

#include <KIconLoader>

#include <QIcon>

using namespace frappe;

namespace
{
/// Rendered when a name does not resolve. A dock must degrade rather than show a
/// hole where the user pinned something.
constexpr auto fallbackIcon = "application-x-executable";
constexpr int defaultIconSize = 48;

QSize sanitised(const QSize &size)
{
    if (size.isValid() && size.width() > 0 && size.height() > 0) {
        return size;
    }
    return QSize(defaultIconSize, defaultIconSize);
}
}

QImage IconProvider::icon(const QString &name, const QSize &size) const
{
    const QSize target = sanitised(size);

    QIcon themed = QIcon::fromTheme(name);
    if (themed.isNull()) {
        themed = QIcon::fromTheme(QString::fromLatin1(fallbackIcon));
    }

    QImage image = themed.pixmap(target).toImage();
    if (image.isNull()) {
        // Even the fallback can be missing on a bare test system. An empty
        // transparent image still renders; a null one produces a QML warning on
        // every frame.
        image = QImage(target, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
    }
    return image;
}

QmlIconProvider::QmlIconProvider(const IIconProvider *icons)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_icons(icons)
{
}

QImage QmlIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // Everything from the '?' on is a cache token, not part of the name.
    //
    // QML caches an image by its URL, so a provider that returns different
    // artwork for the same name — which is exactly what an appearance-mode
    // change does — is never asked again and the old artwork stays on screen.
    // The views put the mode in the URL to make it a different question; here
    // is where it stops being one.
    const qsizetype token = id.indexOf(QLatin1Char('?'));
    const QString name = token < 0 ? id : id.left(token);

    const QImage image = m_icons->icon(name, requestedSize);
    if (size) {
        *size = image.size();
    }
    return image;
}
