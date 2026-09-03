#include "platform/palette.h"

#include <QColor>
#include <QFile>
#include <QMetaProperty>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

namespace
{
/// The fixture scheme's five source colours, deliberately far apart so that a
/// role reading the wrong one is not a near miss.
constexpr auto backgroundHex = "#102030";
constexpr auto foregroundHex = "#e0f0ff";
constexpr auto positiveHex = "#20a040";
constexpr auto negativeHex = "#c02030";
constexpr auto accentHex = "#7040d0";

/// A second scheme, sharing no colour with the first.
constexpr auto altBackgroundHex = "#fbf7f0";
constexpr auto altForegroundHex = "#2a2018";
constexpr auto altPositiveHex = "#106020";
constexpr auto altNegativeHex = "#901018";
constexpr auto altAccentHex = "#d07020";

QString schemeFile(const char *background, const char *foreground, const char *positive,
                   const char *negative, const char *accent)
{
    const auto rgb = [](const char *hex) {
        const QColor colour = QColor::fromString(QLatin1StringView(hex));
        return QStringLiteral("%1,%2,%3").arg(colour.red()).arg(colour.green()).arg(colour.blue());
    };

    // Complementary, because that is the set the dock reads: writing the Window
    // set here and having the test pass would mean the palette had quietly
    // changed which set it takes.
    return QStringLiteral("[Colors:Complementary]\n"
                          "BackgroundNormal=%1\n"
                          "ForegroundNormal=%2\n"
                          "ForegroundPositive=%3\n"
                          "ForegroundNegative=%4\n"
                          "DecorationFocus=%5\n")
        .arg(rgb(background), rgb(foreground), rgb(positive), rgb(negative), rgb(accent));
}
}

/*
 * The palette against a fixture colour scheme.
 *
 * The scheme is written as a file rather than set through an API because that
 * is how a scheme arrives in real life — someone else's process rewrites
 * kdeglobals — and it is the only way to test that the palette notices.
 */
class TestPalette : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString schemePath() const
    {
        return m_dir.filePath(QStringLiteral("test-schemerc"));
    }

    void writeScheme(const QString &contents)
    {
        QFile file(schemePath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents.toUtf8());
        file.close();
    }

    KSharedConfig::Ptr fixtureConfig() const
    {
        return KSharedConfig::openConfig(schemePath(), KConfig::SimpleConfig);
    }

    /// Every colour the palette exposes, read through the meta-object rather
    /// than by name. A role added later is covered by these tests the moment it
    /// is declared, which is the only way "no hardcoded colours" stays true as
    /// the class grows.
    static QList<QColor> allRoles(const DockPalette &palette)
    {
        QList<QColor> colours;
        const QMetaObject *meta = palette.metaObject();
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (property.metaType().id() == QMetaType::QColor) {
                colours.append(property.read(&palette).value<QColor>());
            }
        }
        return colours;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        writeScheme(schemeFile(backgroundHex, foregroundHex, positiveHex, negativeHex, accentHex));
    }

    /// Every role's RGB is one the scheme supplied. A colour invented in the
    /// palette — or carried over from a literal in QML — is not in the set and
    /// fails here.
    void coloursFromSchemeNotHardcoded()
    {
        const DockPalette palette(fixtureConfig());

        const QSet<QRgb> fromScheme{
            QColor(QLatin1String(backgroundHex)).rgb(), QColor(QLatin1String(foregroundHex)).rgb(),
            QColor(QLatin1String(positiveHex)).rgb(),   QColor(QLatin1String(negativeHex)).rgb(),
            QColor(QLatin1String(accentHex)).rgb(),
        };

        const QList<QColor> roles = allRoles(palette);
        // A palette that exposed nothing would pass the loop vacuously.
        QVERIFY(roles.size() >= 10);

        for (const QColor &colour : roles) {
            QVERIFY2(colour.isValid(), "a role resolved to an invalid colour");
            QVERIFY2(fromScheme.contains(colour.rgb()),
                     qPrintable(QStringLiteral("%1 is not a colour the scheme supplied")
                                    .arg(colour.name(QColor::HexArgb))));
        }
    }

    /// The roles land on the scheme entries they are meant to, and translucency
    /// is applied without disturbing the colour underneath.
    void fixtureSchemeAppliedCorrectly()
    {
        const DockPalette palette(fixtureConfig());

        QCOMPARE(palette.text(), QColor(QLatin1String(foregroundHex)));
        QCOMPARE(palette.accent(), QColor(QLatin1String(accentHex)));

        QCOMPARE(palette.shelf().rgb(), QColor(QLatin1String(backgroundHex)).rgb());
        QCOMPARE(palette.plate().rgb(), QColor(QLatin1String(backgroundHex)).rgb());
        QCOMPARE(palette.rim().rgb(), QColor(QLatin1String(foregroundHex)).rgb());
        QCOMPARE(palette.separator().rgb(), QColor(QLatin1String(foregroundHex)).rgb());
        QCOMPARE(palette.indicator().rgb(), QColor(QLatin1String(foregroundHex)).rgb());
        QCOMPARE(palette.rimAccepted().rgb(), QColor(QLatin1String(positiveHex)).rgb());
        QCOMPARE(palette.dropAccepted().rgb(), QColor(QLatin1String(positiveHex)).rgb());
        QCOMPARE(palette.rimRejected().rgb(), QColor(QLatin1String(negativeHex)).rgb());
        QCOMPARE(palette.dropRejected().rgb(), QColor(QLatin1String(negativeHex)).rgb());

        // The shelf is translucent and the text it carries is not: a shelf that
        // came back opaque would mean the blur behind it had nothing to show.
        QVERIFY(palette.shelf().alphaF() < 1.0);
        QVERIFY(palette.shelf().alphaF() > 0.0);
        QCOMPARE(palette.text().alphaF(), 1.0);

        // A tint sits behind artwork and a rim does not, so the two states are
        // never equally loud.
        QVERIFY(palette.dropAccepted().alphaF() < palette.rimAccepted().alphaF());
    }

    /// Rewriting the scheme moves every role and says so once.
    void schemeChangePropagatesToPalette()
    {
        DockPalette palette(fixtureConfig());
        const QColor shelfBefore = palette.shelf();
        const QColor textBefore = palette.text();
        const QColor accentBefore = palette.accent();

        QSignalSpy spy(&palette, &DockPalette::changed);

        writeScheme(schemeFile(altBackgroundHex, altForegroundHex, altPositiveHex, altNegativeHex,
                               altAccentHex));
        palette.reload();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(palette.text(), QColor(QLatin1String(altForegroundHex)));
        QCOMPARE(palette.accent(), QColor(QLatin1String(altAccentHex)));
        QCOMPARE(palette.shelf().rgb(), QColor(QLatin1String(altBackgroundHex)).rgb());

        QVERIFY(palette.shelf() != shelfBefore);
        QVERIFY(palette.text() != textBefore);
        QVERIFY(palette.accent() != accentBefore);

        // The shelf's translucency is the palette's, not the scheme's, and
        // survives a scheme it was not written for.
        QCOMPARE(palette.shelf().alphaF(), shelfBefore.alphaF());
    }

    /// A reload that finds the same scheme is silent. Without this, the two
    /// change routes — the file watcher and the application palette event —
    /// would each redraw the dock for the other's notification.
    void unchangedSchemeIsSilent()
    {
        DockPalette palette(fixtureConfig());
        QSignalSpy spy(&palette, &DockPalette::changed);

        palette.reload();

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestPalette)
#include "test_palette.moc"
