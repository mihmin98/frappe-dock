#pragma once

#include <QQuickImageProvider>

#include "core/interfaces/iiconprovider.h"

namespace frappe
{

/// IIconProvider over KIconThemes.
class IconProvider : public IIconProvider
{
public:
    QImage icon(const QString &name, const QSize &size) const override;
};

/// Bridges QML's image:// scheme onto IIconProvider.
///
/// QML asks for "image://frappeicon/<icon-name>"; the requested size comes from
/// the Image element's sourceSize, so tiles get artwork rendered at the size
/// they are actually drawn at rather than a scaled-up cached bitmap.
class QmlIconProvider : public QQuickImageProvider
{
public:
    explicit QmlIconProvider(const IIconProvider *icons);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const IIconProvider *m_icons;
};

}
