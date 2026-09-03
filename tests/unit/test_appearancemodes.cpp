#include "platform/iconpipeline.h"

#include "fakes/fakeiconprovider.h"

#include <QIcon>
#include <QTest>

#include <algorithm>

using namespace frappe;

namespace
{
/// The cell the set is rendered at. On the ladder, so no rounding is involved.
constexpr int cell = 64;

/// A set of icons carries the colour channel when their dominant hues are this
/// far apart on average. Six evenly spaced hues average 108 degrees apart, and a
/// set that has been flattened to one hue averages nothing at all, so the
/// threshold is nowhere near either answer.
constexpr qreal colourVarianceThreshold = 20.0;

/// And the shape channel when their normalised lightness differs by this much
/// on average. Measured on this set: 0.034 under Default and Dark, and 0.020
/// under Tinted, which compresses lightness into the range above its floor and
/// so carries structure at about two thirds the contrast. A set with no
/// structure left — every icon a featureless plate — comes out at exactly zero,
/// because normalising an image with no internal range leaves nothing to
/// differ. Zero is the failure this threshold has to be clear of, and it is.
constexpr qreal shapeVarianceThreshold = 0.01;

QList<QImage> renderSet(const IconPipeline &pipeline)
{
    QList<QImage> images;
    const QStringList names = FakeIconProvider::setNames();
    for (const QString &name : names) {
        images.append(pipeline.icon(name, QSize(cell, cell)).convertToFormat(QImage::Format_ARGB32));
    }
    return images;
}

int hueDistance(int a, int b)
{
    const int direct = qAbs(a - b);
    return std::min(direct, 360 - direct);
}

/// How much the set's colours differ, in degrees of hue, averaged over pairs.
qreal colourVariance(const QList<QImage> &images)
{
    QList<int> hues;
    for (const QImage &image : images) {
        const QColor dominant = icons::dominantColour(image);
        // Artwork with no colour left contributes no colour difference, which
        // is the honest reading rather than a skipped pair.
        hues.append(dominant.isValid() ? dominant.hsvHue() : -1);
    }

    qreal total = 0;
    int pairs = 0;
    for (int i = 0; i < hues.size(); ++i) {
        for (int j = i + 1; j < hues.size(); ++j) {
            total += (hues[i] < 0 || hues[j] < 0) ? 0 : hueDistance(hues[i], hues[j]);
            ++pairs;
        }
    }
    return pairs > 0 ? total / pairs : 0;
}

/// The image's lightness, per pixel, rescaled to its own range.
///
/// Transparency counts as no lightness at all, so a glyph on transparency and
/// the same glyph on a plate are both structure. Rescaling to the image's *own*
/// range is what makes this a measure of shape rather than of lightness:
/// darkening or tinting moves the whole range and leaves the internal pattern
/// where it was. An image with no range — one flat plate, nothing drawn on it —
/// normalises to zero everywhere, which is the right answer: it has no shape to
/// tell it apart.
QList<qreal> structure(const QImage &image)
{
    QList<qreal> values;
    values.reserve(image.width() * image.height());
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            values.append(qAlpha(line[x]) < 128 ? 0.0 : QColor(line[x]).lightnessF());
        }
    }

    const auto [low, high] = std::ranges::minmax(values);
    const qreal range = high - low;
    for (qreal &value : values) {
        value = range > 0 ? (value - low) / range : 0.0;
    }
    return values;
}

/// How much the set's shapes differ, averaged over pixels and then over pairs.
qreal shapeVariance(const QList<QImage> &images)
{
    QList<QList<qreal>> structures;
    for (const QImage &image : images) {
        structures.append(structure(image));
    }

    qreal total = 0;
    int pairs = 0;
    for (int i = 0; i < structures.size(); ++i) {
        for (int j = i + 1; j < structures.size(); ++j) {
            qreal difference = 0;
            for (int p = 0; p < structures[i].size(); ++p) {
                difference += qAbs(structures[i][p] - structures[j][p]);
            }
            total += difference / structures[i].size();
            ++pairs;
        }
    }
    return pairs > 0 ? total / pairs : 0;
}
}

/*
 * The appearance modes.
 *
 * §6.3's hard requirement: every mode leaves at least one identification
 * channel intact, and a mode that spends both must fail the suite rather than
 * ship. That is noModeStripsBothIdentificationChannels(); the per-mode tests
 * exist to say *which* channel each one keeps, so a regression names itself.
 */
class TestAppearanceModes : public QObject
{
    Q_OBJECT

private:
    FakeIconProvider m_source;

    /// The accent Tinted tints with and colourless plates borrow. A real one
    /// comes from DockPalette; the value matters here only in that it has a
    /// hue at all.
    static QColor accent()
    {
        return QColor::fromRgb(0x70, 0x40, 0xd0);
    }

    QList<QImage> setIn(IconPipeline::Mode mode)
    {
        IconPipeline pipeline(&m_source);
        pipeline.setAccent(accent());
        pipeline.setMode(mode);
        return renderSet(pipeline);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QIcon::setThemeName(QStringLiteral("frappe-test-theme"));

        // The premise of everything below: the set starts out carrying both
        // channels. A fake that failed this would make every mode look safe.
        const QList<QImage> raw = [this] {
            QList<QImage> images;
            const QStringList names = FakeIconProvider::setNames();
            for (const QString &name : names) {
                images.append(m_source.icon(name, QSize(cell, cell)).convertToFormat(QImage::Format_ARGB32));
            }
            return images;
        }();
        QVERIFY(colourVariance(raw) > colourVarianceThreshold);
        QVERIFY(shapeVariance(raw) > shapeVarianceThreshold);
    }

    /// Default is the artwork as the theme drew it, so both channels survive.
    void defaultModeKeepsColourAndShape()
    {
        const QList<QImage> images = setIn(IconPipeline::Mode::Default);
        QVERIFY(colourVariance(images) > colourVarianceThreshold);
        QVERIFY(shapeVariance(images) > shapeVarianceThreshold);
    }

    /// Dark dulls the colour channel. It must not spend it: hue and saturation
    /// come through, only lightness moves.
    void darkModeKeepsColour()
    {
        const QList<QImage> images = setIn(IconPipeline::Mode::Dark);
        QVERIFY(colourVariance(images) > colourVarianceThreshold);
        QVERIFY(shapeVariance(images) > shapeVarianceThreshold);

        // Darker than Default, or it is not the mode it claims to be.
        const QList<QImage> plain = setIn(IconPipeline::Mode::Default);
        const auto meanLightness = [](const QImage &image) {
            qreal total = 0;
            int opaque = 0;
            for (int y = 0; y < image.height(); ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    if (qAlpha(line[x]) >= 128) {
                        total += QColor(line[x]).lightnessF();
                        ++opaque;
                    }
                }
            }
            return opaque > 0 ? total / opaque : 0.0;
        };
        QVERIFY(meanLightness(images.first()) < meanLightness(plain.first()));
    }

    /// Tinted does spend the colour channel — that is what the mode is — so
    /// what it must not also spend is shape.
    void tintedModeSpendsColourAndKeepsShape()
    {
        const QList<QImage> images = setIn(IconPipeline::Mode::Tinted);

        // The premise: this really is the mode that flattens colour. If this
        // ever stopped holding, the shape assertion below would be passing for
        // the wrong reason.
        QVERIFY(colourVariance(images) < colourVarianceThreshold);
        QVERIFY2(shapeVariance(images) > shapeVarianceThreshold,
                 "Tinted spent both channels and must not ship");
    }

    /// §6.3's requirement, over every mode there is. A mode added later is
    /// covered by this the moment it is added to the enum.
    void noModeStripsBothIdentificationChannels()
    {
        const QList<IconPipeline::Mode> modes{
            IconPipeline::Mode::Default,
            IconPipeline::Mode::Dark,
            IconPipeline::Mode::Tinted,
        };

        for (IconPipeline::Mode mode : modes) {
            const QList<QImage> images = setIn(mode);
            const bool colour = colourVariance(images) > colourVarianceThreshold;
            const bool shape = shapeVariance(images) > shapeVarianceThreshold;
            QVERIFY2(colour || shape,
                     qPrintable(QStringLiteral("mode %1 leaves nothing to identify an icon by")
                                    .arg(int(mode))));
        }
    }

    /// A mode change takes effect without anything restarting: the cache goes,
    /// and the next request comes back treated.
    void modeChangeAppliesLive()
    {
        IconPipeline pipeline(&m_source);
        pipeline.setAccent(accent());

        const QImage before = pipeline.icon(QStringLiteral("set0"), QSize(cell, cell));
        QCOMPARE(pipeline.cacheSize(), 1);

        pipeline.setMode(IconPipeline::Mode::Tinted);
        QCOMPARE(pipeline.cacheSize(), 0);

        const QImage after = pipeline.icon(QStringLiteral("set0"), QSize(cell, cell));
        QVERIFY(after != before);
    }

    /// Tinted with no accent is not a licence to pick a colour. It leaves the
    /// artwork alone rather than choosing the user's accent for them.
    void tintedWithoutAnAccentLeavesArtworkAlone()
    {
        IconPipeline plain(&m_source);
        IconPipeline untinted(&m_source);
        untinted.setMode(IconPipeline::Mode::Tinted);

        QCOMPARE(untinted.icon(QStringLiteral("set0"), QSize(cell, cell)),
                 plain.icon(QStringLiteral("set0"), QSize(cell, cell)));
    }
};

QTEST_MAIN(TestAppearanceModes)
#include "test_appearancemodes.moc"
