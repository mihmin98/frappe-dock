#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace frappe
{

/// Icon-name to pixels resolution.
///
/// Returns QImage rather than QPixmap: QPixmap requires a QGuiApplication and a
/// window system, which would make this untestable headless and drag Qt6::Gui
/// into anything that merely wants to name an icon. QQuickImageProvider consumes
/// QImage directly.
class IIconProvider
{
public:
    virtual ~IIconProvider() = default;

    /// Returns the icon at the requested size, or a placeholder if the name does
    /// not resolve. Never returns a null image for a non-empty name.
    virtual QImage icon(const QString &name, const QSize &size) const = 0;
};

}
