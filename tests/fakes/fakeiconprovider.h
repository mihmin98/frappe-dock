#pragma once

#include "core/interfaces/iiconprovider.h"

#include <QColor>
#include <QImage>
#include <QPainter>

namespace frappe
{

/// Synthetic artwork, so the pipeline's tests do not depend on which icon theme
/// happens to be installed.
///
/// Three names, one for each case the pipeline distinguishes:
///
/// - `conforming` — a full-bleed rounded square that already fills its cell.
/// - `glyph` — a small coloured disc adrift on transparency.
/// - `mono` — the same disc with the colour taken out.
///
/// Plus `set0` … `set5`: six icons that differ in **both** colour and shape, for
/// the appearance-mode tests, which ask whether a mode has left either channel
/// intact and so need a set where both start out present.
///
/// It counts the calls it is asked to serve, which is how the cache is observed
/// without giving the pipeline a test-only accessor.
class FakeIconProvider : public IIconProvider
{
public:
    /// The colours the tests assert survive the pipeline.
    static QColor conformingColour()
    {
        return QColor::fromRgb(0x20, 0x80, 0xd0);
    }
    static QColor glyphColour()
    {
        return QColor::fromRgb(0xd0, 0x30, 0x20);
    }

    /// The six names of the varied set.
    static QStringList setNames()
    {
        QStringList names;
        for (int i = 0; i < 6; ++i) {
            names.append(QStringLiteral("set%1").arg(i));
        }
        return names;
    }

    QImage icon(const QString &name, const QSize &size) const override
    {
        ++calls;
        lastRequest = size;

        const int side = size.width();
        QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        if (name.startsWith(QLatin1String("set"))) {
            const int index = name.mid(3).toInt();
            // A hue each, and a shape each: an icon that only differed in one
            // could not tell a mode that spends the other from a mode that
            // spends both.
            painter.setBrush(QColor::fromHsv(index * 60, 220, 220));
            const QRectF box(side * 0.1, side * 0.1, side * 0.8, side * 0.8);
            switch (index % 3) {
            case 0:
                painter.drawEllipse(box);
                break;
            case 1:
                painter.drawRect(box.adjusted(side * 0.1, 0, -side * 0.1, 0));
                break;
            default:
                painter.drawPolygon(QPolygonF({QPointF(box.center().x(), box.top()),
                                               QPointF(box.right(), box.bottom()),
                                               QPointF(box.left(), box.bottom())}));
                break;
            }
        } else if (name == QLatin1String("conforming")) {
            painter.setBrush(conformingColour());
            painter.drawRoundedRect(QRectF(0, 0, side, side), 0.28 * side, 0.28 * side);
        } else if (name == QLatin1String("mono")) {
            painter.setBrush(QColor::fromRgb(0x80, 0x80, 0x80));
            painter.drawEllipse(QRectF(side * 0.3, side * 0.3, side * 0.4, side * 0.4));
        } else {
            painter.setBrush(glyphColour());
            painter.drawEllipse(QRectF(side * 0.3, side * 0.3, side * 0.4, side * 0.4));
        }

        painter.end();
        return image;
    }

    mutable int calls = 0;
    mutable QSize lastRequest;
};

}
