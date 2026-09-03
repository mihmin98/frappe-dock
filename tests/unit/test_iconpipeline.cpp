#include "platform/iconpipeline.h"

#include "fakes/fakeiconprovider.h"

#include <QIcon>
#include <QTest>

using namespace frappe;

namespace
{
/// How far a hue may move and still be the same colour to a user. Rounding
/// through 8-bit channels and an antialiased scale both move it a little; a
/// degree or two is that, and anything larger is the pipeline having recoloured
/// something.
constexpr int hueTolerance = 8;

int hueOf(QRgb pixel)
{
    return QColor(pixel).hsvHue();
}

/// Circular distance between two hues, in degrees.
int hueDistance(int a, int b)
{
    const int direct = qAbs(a - b);
    return std::min(direct, 360 - direct);
}
}

/*
 * The icon pipeline.
 *
 * The assertion this file exists for is maskingPreservesColour(): design
 * correction #1 says a masked icon keeps its colour, and the only way that
 * survives a refactor is if it is checked in pixels rather than described in a
 * comment.
 */
class TestIconPipeline : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // The theme name is part of the cache key, so the suite fixes it rather
        // than inheriting whatever the developer has installed.
        QIcon::setThemeName(QStringLiteral("frappe-test-theme"));
    }

    /// An icon that already fills its cell is handed back exactly as it came.
    void conformingIconPassesThroughUnmodified()
    {
        const FakeIconProvider source;
        IconPipeline pipeline(&source);

        const QImage expected = source.icon(QStringLiteral("conforming"), QSize(48, 48));
        const QImage actual = pipeline.icon(QStringLiteral("conforming"), QSize(48, 48));

        QCOMPARE(actual.size(), expected.size());
        QCOMPARE(actual.convertToFormat(QImage::Format_ARGB32),
                 expected.convertToFormat(QImage::Format_ARGB32));
    }

    /// A small glyph on transparency is put on a plate, so it fills the cell
    /// the way its neighbours do.
    void nonConformingIconIsMasked()
    {
        const FakeIconProvider source;
        IconPipeline pipeline(&source);

        const QImage raw = source.icon(QStringLiteral("glyph"), QSize(64, 64));
        const QImage masked = pipeline.icon(QStringLiteral("glyph"), QSize(64, 64));

        QVERIFY(!icons::isConforming(raw));
        QVERIFY(masked != raw);
        // The plate is what the mask adds, so the result covers its cell where
        // the artwork did not.
        QVERIFY(icons::coverage(masked) > icons::coverage(raw));
        QVERIFY(icons::isConforming(masked));
    }

    /// Design correction #1, in pixels: masking keeps the icon's colour rather
    /// than dropping it onto a uniform grey plate.
    void maskingPreservesColour()
    {
        const FakeIconProvider source;
        IconPipeline pipeline(&source);

        const int side = 64;
        const QImage masked =
            pipeline.icon(QStringLiteral("glyph"), QSize(side, side)).convertToFormat(QImage::Format_ARGB32);
        const int sourceHue = FakeIconProvider::glyphColour().hsvHue();

        // The plate. Sampled a little inside the corner radius, where the plate
        // is drawn and the artwork is not.
        const QRgb plate = masked.pixel(side / 2, side / 8);
        QCOMPARE(qAlpha(plate), 255);
        QVERIFY2(QColor(plate).hsvSaturation() > 64, "the plate is a grey plate");
        QVERIFY2(hueDistance(hueOf(plate), sourceHue) <= hueTolerance,
                 "the plate does not carry the icon's hue");

        // The artwork itself, drawn on top and not recoloured.
        const QRgb centre = masked.pixel(side / 2, side / 2);
        QVERIFY2(hueDistance(hueOf(centre), sourceHue) <= hueTolerance,
                 "the artwork's own colour did not survive the mask");
        QVERIFY(QColor(centre).hsvSaturation() > 128);
    }

    /// Artwork with no colour of its own keeps its shape and borrows the
    /// scheme's accent — the one case where a uniform grey plate would be the
    /// easy answer, and the one the design correction rules out.
    void colourlessArtworkNeverBecomesGrey()
    {
        const FakeIconProvider source;
        IconPipeline pipeline(&source);
        const QColor accent = QColor::fromRgb(0x70, 0x40, 0xd0);
        pipeline.setAccent(accent);

        const int side = 64;
        const QImage masked =
            pipeline.icon(QStringLiteral("mono"), QSize(side, side)).convertToFormat(QImage::Format_ARGB32);

        const QRgb plate = masked.pixel(side / 2, side / 8);
        QVERIFY2(QColor(plate).hsvSaturation() > 64, "a colourless icon fell back to grey");
        QVERIFY(hueDistance(hueOf(plate), accent.hsvHue()) <= hueTolerance);

        // The glyph is still a disc on a plate, not a plate: shape is the
        // channel this icon identifies itself by, and the mask must not spend
        // it.
        QVERIFY(icons::coverage(masked) - icons::coverage(source.icon(QStringLiteral("mono"), QSize(side, side)))
                > 0.2);
        QVERIFY(masked.pixel(side / 2, side / 2) != plate);
    }

    /// The second request for the same artwork is served from the cache.
    void cacheReturnsIdenticalResult()
    {
        FakeIconProvider source;
        IconPipeline pipeline(&source);

        const QImage first = pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));
        const int afterFirst = source.calls;
        const QImage second = pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));

        QCOMPARE(source.calls, afterFirst);
        QCOMPARE(first, second);
        QCOMPARE(pipeline.cacheSize(), 1);
    }

    /// A new icon theme is different artwork under the same name, so the cached
    /// result is not an answer to the new question.
    void themeChangeInvalidatesCache()
    {
        FakeIconProvider source;
        IconPipeline pipeline(&source);

        pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));
        const int afterFirst = source.calls;

        QIcon::setThemeName(QStringLiteral("frappe-other-theme"));
        pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));
        QCOMPARE(source.calls, afterFirst + 1);

        QIcon::setThemeName(QStringLiteral("frappe-test-theme"));
    }

    /// Sizes are rounded up to the icon ladder, so magnification — which asks
    /// for a different size every frame — does not mask an icon every frame.
    void sizesAreQuantisedToTheLadder()
    {
        FakeIconProvider source;
        IconPipeline pipeline(&source);

        pipeline.icon(QStringLiteral("glyph"), QSize(50, 50));
        const int afterFirst = source.calls;
        QCOMPARE(source.lastRequest, QSize(64, 64));

        // Every size between two ladder steps is one cache entry, not one per
        // frame.
        for (int size = 51; size <= 64; ++size) {
            pipeline.icon(QStringLiteral("glyph"), QSize(size, size));
        }

        QCOMPARE(source.calls, afterFirst);
        QCOMPARE(pipeline.cacheSize(), 1);
    }

    /// The token changes whenever the artwork would.
    ///
    /// Dropping our own cache is necessary and not sufficient: QML caches an
    /// image by URL, so without a moving token in the URL a mode change
    /// re-renders nothing and the old artwork stays on screen. This is the
    /// unit-testable half of that; the visible half is on the 06 checklist.
    void tokenChangesWhenTreatmentDoes()
    {
        FakeIconProvider source;
        IconPipeline pipeline(&source);

        const QString initial = pipeline.token();
        QVERIFY(!initial.isEmpty());
        QCOMPARE(pipeline.token(), initial);

        pipeline.setMode(IconPipeline::Mode::Dark);
        const QString afterMode = pipeline.token();
        QVERIFY(afterMode != initial);

        pipeline.setAccent(QColor::fromRgb(0x70, 0x40, 0xd0));
        const QString afterAccent = pipeline.token();
        QVERIFY(afterAccent != afterMode);

        pipeline.invalidate();
        QVERIFY(pipeline.token() != afterAccent);

        // A setter that changes nothing is not a change.
        const QString settled = pipeline.token();
        pipeline.setMode(IconPipeline::Mode::Dark);
        QCOMPARE(pipeline.token(), settled);
    }

    /// Dropping the cache is a fresh resolution, not a silent no-op.
    void invalidateForcesReresolution()
    {
        FakeIconProvider source;
        IconPipeline pipeline(&source);

        pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));
        const int afterFirst = source.calls;

        pipeline.invalidate();
        QCOMPARE(pipeline.cacheSize(), 0);

        pipeline.icon(QStringLiteral("glyph"), QSize(48, 48));
        QCOMPARE(source.calls, afterFirst + 1);
    }
};

QTEST_MAIN(TestIconPipeline)
#include "test_iconpipeline.moc"
