#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QObject>

#include "core/interfaces/iiconprovider.h"

namespace frappe
{

/// The steps the pipeline is built from, exposed so they can be tested as the
/// pure image operations they are.
namespace icons
{
/// The fraction of the cell covered by artwork that is more opaque than not.
qreal coverage(const QImage &image);

/// True when the artwork fills its cell the way a themed icon designed for this
/// dock does, and false when it is a small glyph adrift on transparency.
///
/// One measurement, deliberately: the alternatives are a theme allowlist, which
/// is a config key and a guess about what the user installed, and the theme's
/// own Context metadata, which freedesktop themes fill in inconsistently.
bool isConforming(const QImage &image);

/// The artwork's dominant colour — its opaque pixels averaged with saturation
/// as the weight, so a logo's colour wins over the neutral it sits on.
///
/// Returns an invalid colour for artwork that has no colour to be dominant: a
/// monochrome glyph is not secretly red.
QColor dominantColour(const QImage &image);

/// \a image with its lightness reduced, hue and saturation untouched.
///
/// The Dark mode's whole treatment. Colour differentiation is dulled but not
/// spent, which is the difference between a mode with a cost and a mode that
/// cannot be used.
QImage darken(const QImage &image);

/// \a image rendered monochrome in \a tint, its internal lightness structure
/// kept.
///
/// This mode does spend the colour channel — every icon comes out the same hue —
/// so what it must not also spend is shape, and mapping lightness rather than
/// flattening to a silhouette is what keeps it.
QImage tinted(const QImage &image, const QColor &tint);

/// \a artwork inset and centred on a rounded plate of \a plate, at \a size.
///
/// The plate carries the artwork's own colour, which is design correction #1:
/// the reference platform drops non-conforming icons onto a uniform grey plate
/// and spends the colour channel to do it. Here both channels survive — the
/// artwork is drawn unmodified, and the plate is the artwork's own hue.
QImage maskToCell(const QImage &artwork, const QColor &plate, int size);
}

/// Resolution and treatment of icon artwork, in front of a plain provider.
///
/// The order is the one §6.2 sets out: a conforming theme icon passes through
/// untouched, a non-conforming one is masked with its colours preserved, and
/// nothing is ever reduced to a uniform grey plate.
///
/// **Sizes are quantised.** Magnification changes a tile's drawn size every
/// frame, and the view asks for artwork at the size it draws at. Resolving each
/// of those exactly would mask an icon per tile per frame and cache the result
/// under a key never asked for again. The request is rounded *up* to the next
/// step of the icon ladder instead, so the artwork is only ever scaled down,
/// the cache is bounded by icons x ladder steps, and the hot path hits it.
class IconPipeline : public QObject, public IIconProvider
{
    Q_OBJECT

    /// A string that changes whenever the artwork this pipeline would return
    /// changes — a new appearance mode, a new accent, a new icon theme.
    ///
    /// The views append it to every `image://frappeicon/` URL. QML caches an
    /// image by its URL, so without a token in there a mode change re-renders
    /// nothing: the pipeline's own cache is dropped, and QML never asks it
    /// anything again because it still has an answer for that URL. Dropping our
    /// cache is necessary and not sufficient.
    Q_PROPERTY(QString token READ token NOTIFY changed)

public:
    /// How artwork is treated once it has been resolved. Mirrors
    /// ConfigFacade::AppearanceMode, and there is no Clear equivalent for the
    /// reason given there.
    enum class Mode {
        Default,
        Dark,
        Tinted,
    };

    /// Wraps \a source, which resolves names to artwork and outlives this.
    explicit IconPipeline(const IIconProvider *source, QObject *parent = nullptr);

    /// The process-wide instance. QML binds to it as a singleton, because every
    /// view that draws an icon needs the token and threading one reference
    /// through the whole tree to deliver a string is not worth it.
    static IconPipeline *instance();

    /// Sets what resolves names to artwork. For the singleton, which is
    /// constructed before main() has a provider to give it.
    void setSource(const IIconProvider *source);

    QString token() const;

    QImage icon(const QString &name, const QSize &size) const override;

    /// Applies live: the cache is dropped, and the views re-request as they
    /// redraw. Nothing has to restart.
    void setMode(Mode mode);
    Mode mode() const;

    /// The colour a plate falls back to when the artwork has no colour of its
    /// own. Taken from the scheme's accent (DockPalette), never a fixed grey.
    void setAccent(const QColor &accent);

    /// Drops every cached result. Called for us when the icon theme changes.
    Q_INVOKABLE void invalidate();

    /// How many results are cached. For tests and for judging the bound.
    int cacheSize() const;

Q_SIGNALS:
    /// The treatment changed, so every icon already on screen is stale.
    void changed();

private:
    QImage render(const QString &name, int size) const;
    /// Name to artwork: the theme icon, masked if it does not conform.
    QImage resolved(const QString &name, int size) const;
    /// Artwork to what the mode says it should look like.
    QImage treated(const QImage &artwork) const;

    const IIconProvider *m_source;
    Mode m_mode = Mode::Default;
    /// Bumped on every invalidation; the token is built from it.
    int m_generation = 0;
    QColor m_accent;
    mutable QHash<QString, QImage> m_cache;
};

}
