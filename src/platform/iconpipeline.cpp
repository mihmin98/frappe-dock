#include "platform/iconpipeline.h"

#include <KIconLoader>

#include <QIcon>
#include <QPainter>

#include <algorithm>
#include <array>

using namespace frappe;

namespace
{
/// A pixel counts as artwork above this. Half-transparent edges belong to
/// neither the shape nor the background, and picking the midpoint is the one
/// choice that does not favour either.
constexpr int opaqueEnough = 128;

/// Coverage at or above this reads as artwork that fills its cell. A rounded
/// square of the dock's own corner rule covers about 0.93 of its cell and a
/// circle about 0.79; a glyph inset in a transparent square rarely passes 0.4.
/// The threshold sits in the gap, not near either edge of it.
constexpr qreal conformingCoverage = 0.6;

/// The plate's corner, as a fraction of the cell. The shelf's rule, applied to
/// a smaller square, so a masked icon is the same shape family as the dock.
constexpr qreal plateCornerRatio = 0.28;

/// How much of the cell the inset artwork occupies. The remainder is the
/// gutter the plate shows through on all four sides.
constexpr qreal artworkInset = 0.76;

/// The plate's brightness. Fixed so that artwork of any lightness has the same
/// contrast to sit on; hue and saturation are the artwork's own.
constexpr qreal plateValue = 0.55;
/// Below this the plate would be too pale to read as a colour at all. The
/// tint uses it for the same reason.
constexpr qreal minimumPlateSaturation = 0.45;

/// What Dark multiplies lightness by. Enough to read as darkened, not so much
/// that two icons of different colours converge on black — the mode's cost is a
/// dulled colour channel, not a spent one.
constexpr qreal darkenFactor = 0.55;

/// The lightness a fully black pixel comes out at under Tinted. Without a floor
/// the dark parts of every icon land on the same black and the shape channel
/// goes with the colour one.
constexpr qreal tintFloor = 0.25;

/// The icon ladder requests are rounded up to. The theme's own rendered sizes,
/// plus the two intermediate steps a dock at a large tile size actually asks
/// for.
constexpr std::array<int, 9> ladder{16, 22, 24, 32, 48, 64, 96, 128, 256};

int quantised(int requested)
{
    const auto step = std::ranges::find_if(ladder, [requested](int size) { return size >= requested; });
    return step != ladder.end() ? *step : ladder.back();
}

/// The colour a plate takes for artwork whose dominant colour is \a source.
QColor plateColour(const QColor &source)
{
    QColor plate = QColor::fromHsvF(source.hsvHueF() < 0 ? 0.0 : source.hsvHueF(),
                                    std::max<qreal>(source.hsvSaturationF(), minimumPlateSaturation),
                                    plateValue);
    plate.setAlphaF(1.0);
    return plate;
}
}

qreal icons::coverage(const QImage &image)
{
    if (image.isNull() || image.width() == 0 || image.height() == 0) {
        return 0.0;
    }

    const QImage rgba = image.convertToFormat(QImage::Format_ARGB32);
    qint64 opaque = 0;
    for (int y = 0; y < rgba.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
        for (int x = 0; x < rgba.width(); ++x) {
            if (qAlpha(line[x]) >= opaqueEnough) {
                ++opaque;
            }
        }
    }
    return qreal(opaque) / (qint64(rgba.width()) * rgba.height());
}

bool icons::isConforming(const QImage &image)
{
    return coverage(image) >= conformingCoverage;
}

QColor icons::dominantColour(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }

    const QImage rgba = image.convertToFormat(QImage::Format_ARGB32);
    qreal weight = 0;
    qreal red = 0;
    qreal green = 0;
    qreal blue = 0;

    for (int y = 0; y < rgba.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
        for (int x = 0; x < rgba.width(); ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) < opaqueEnough) {
                continue;
            }
            // Saturation as the weight: an icon is identified by its colour,
            // and averaging a logo against the white it sits on gives back the
            // white. A grey pixel contributes nothing and a saturated one
            // contributes fully.
            const qreal saturation = QColor(pixel).hsvSaturationF();
            weight += saturation;
            red += qRed(pixel) * saturation;
            green += qGreen(pixel) * saturation;
            blue += qBlue(pixel) * saturation;
        }
    }

    if (weight <= 0.0) {
        return {};
    }
    return QColor::fromRgbF(red / weight / 255.0, green / weight / 255.0, blue / weight / 255.0);
}

QImage icons::darken(const QImage &image)
{
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0) {
                continue;
            }
            QColor colour(line[x]);
            colour = QColor::fromHsvF(colour.hsvHueF(), colour.hsvSaturationF(),
                                      colour.valueF() * darkenFactor);
            line[x] = qRgba(colour.red(), colour.green(), colour.blue(), alpha);
        }
    }
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage icons::tinted(const QImage &image, const QColor &tint)
{
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    const qreal hue = tint.hsvHueF() < 0 ? 0.0 : tint.hsvHueF();
    const qreal saturation = std::max<qreal>(tint.hsvSaturationF(), minimumPlateSaturation);

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const int alpha = qAlpha(line[x]);
            if (alpha == 0) {
                continue;
            }
            // Lightness carries through, so the artwork's internal structure —
            // its shape — arrives intact even though its colour does not.
            const qreal lightness = QColor(line[x]).lightnessF();
            const QColor colour = QColor::fromHsvF(
                hue, saturation, tintFloor + (1.0 - tintFloor) * lightness);
            line[x] = qRgba(colour.red(), colour.green(), colour.blue(), alpha);
        }
    }
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QImage icons::maskToCell(const QImage &artwork, const QColor &plate, int size)
{
    QImage cell(size, size, QImage::Format_ARGB32_Premultiplied);
    cell.fill(Qt::transparent);

    QPainter painter(&cell);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    const qreal radius = plateCornerRatio * size;
    painter.setPen(Qt::NoPen);
    painter.setBrush(plate);
    painter.drawRoundedRect(QRectF(0, 0, size, size), radius, radius);

    // The artwork goes on top unmodified. Nothing here recolours it: that is
    // the whole of design correction #1, and any filter applied at this point
    // would be spending the channel the plate was built to keep.
    if (!artwork.isNull()) {
        const int inner = qRound(artworkInset * size);
        const QImage scaled = artwork.scaled(inner, inner, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(QPointF((size - scaled.width()) / 2.0, (size - scaled.height()) / 2.0), scaled);
    }

    painter.end();
    return cell;
}

IconPipeline::IconPipeline(const IIconProvider *source, QObject *parent)
    : QObject(parent)
    , m_source(source)
{
    // The theme name is part of the cache key, so a theme change already misses
    // rather than serving the old artwork. This drops the stale entries as
    // well, which the key alone would leave to accumulate.
    connect(KIconLoader::global(), &KIconLoader::iconChanged, this, [this] { invalidate(); });
    connect(KIconLoader::global(), &KIconLoader::iconLoaderSettingsChanged, this, [this] { invalidate(); });
}

IconPipeline *IconPipeline::instance()
{
    // No source yet: main() gives it one. Resolving before then would be a
    // question asked of nothing, and icon() guards for it.
    static IconPipeline s_instance(nullptr);
    return &s_instance;
}

void IconPipeline::setSource(const IIconProvider *source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    invalidate();
}

QString IconPipeline::token() const
{
    // A query string, so the views can append it to a URL without knowing what
    // it is. QmlIconProvider strips it back off.
    return QStringLiteral("?v=%1").arg(m_generation);
}

void IconPipeline::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    // Every cached result was rendered under the old mode, so the switch is a
    // cache drop and nothing else. The views re-request as they redraw, which
    // is what makes the change live.
    invalidate();
}

IconPipeline::Mode IconPipeline::mode() const
{
    return m_mode;
}

void IconPipeline::setAccent(const QColor &accent)
{
    if (m_accent == accent) {
        return;
    }
    m_accent = accent;
    // Plates for colourless artwork are drawn in the accent, and Tinted tints
    // with it, so both are wrong the moment it moves.
    invalidate();
}

void IconPipeline::invalidate()
{
    m_cache.clear();
    // The token moves with it, so the URLs the views hold stop matching what
    // QML has cached and the icons are actually asked for again.
    ++m_generation;
    Q_EMIT changed();
}

int IconPipeline::cacheSize() const
{
    return m_cache.size();
}

QImage IconPipeline::icon(const QString &name, const QSize &size) const
{
    if (!m_source) {
        return {};
    }

    const int requested = std::max({size.width(), size.height(), 1});
    const int cell = quantised(requested);

    // The theme is in the key because artwork resolved under one theme is not
    // the answer under the next.
    const QString key = QIcon::themeName() + QLatin1Char('\0') + name + QLatin1Char('\0')
        + QString::number(cell);

    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        return *cached;
    }

    const QImage result = render(name, cell);
    m_cache.insert(key, result);
    return result;
}

QImage IconPipeline::render(const QString &name, int size) const
{
    return treated(resolved(name, size));
}

QImage IconPipeline::resolved(const QString &name, int size) const
{
    const QImage artwork = m_source->icon(name, QSize(size, size));

    // A conforming icon is already the shape the dock draws. Putting it on a
    // plate would inset it inside a second one.
    if (icons::isConforming(artwork)) {
        return artwork;
    }

    const QColor dominant = icons::dominantColour(artwork);
    // Colourless artwork keeps its shape and borrows the scheme's accent for
    // the plate — never a fixed grey, which would leave icons with neither
    // channel. Without an accent set, the plate is skipped entirely rather
    // than invented: unmasked artwork still identifies itself.
    if (!dominant.isValid()) {
        return m_accent.isValid() ? icons::maskToCell(artwork, plateColour(m_accent), size) : artwork;
    }

    return icons::maskToCell(artwork, plateColour(dominant), size);
}

QImage IconPipeline::treated(const QImage &artwork) const
{
    switch (m_mode) {
    case Mode::Dark:
        return icons::darken(artwork);
    case Mode::Tinted:
        // A tint needs a colour, and the accent is the only one the dock has.
        // Without it there is nothing to tint *with*, and a mode that invented
        // one would be choosing the user's accent for them.
        return m_accent.isValid() ? icons::tinted(artwork, m_accent) : artwork;
    case Mode::Default:
        break;
    }
    return artwork;
}
