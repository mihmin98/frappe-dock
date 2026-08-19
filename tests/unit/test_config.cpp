#include "core/config/configfacade.h"
#include "core/surfaceplan.h"

#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

class TestConfig : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    /// Writes a config file by hand, then makes the facade read it. Writing the
    /// file rather than the keys is the point: it is how a corrupt or
    /// out-of-range file gets in front of the loader.
    void writeConfig(const QString &contents)
    {
        QFile file(configPath());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents.toUtf8());
        file.close();
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Must happen before anything touches config, or the suite overwrites
        // the developer's real settings.
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        QFile::remove(configPath());
    }

    void defaultsWhenMissing()
    {
        ConfigFacade config(configPath());

        QCOMPARE(config.position(), int(ConfigFacade::Bottom));
        QCOMPARE(config.displayMode(), int(ConfigFacade::AllScreens));
        QCOMPARE(config.targetOutput(), QString());
        QCOMPARE(config.tileSize(), 48);
        QCOMPARE(config.animationSpeed(), 100);
        QCOMPARE(config.pinnedEntries(), QStringList());
    }

    void roundTrip()
    {
        {
            ConfigFacade config(configPath());
            config.setPosition(ConfigFacade::Left);
            config.setDisplayMode(ConfigFacade::SingleScreen);
            config.setTargetOutput(QStringLiteral("WL-1"));
            config.setTileSize(64);
            config.setAnimationSpeed(0); // reduced motion: a real setting, not a disabled one
            config.setPinnedEntries({QStringLiteral("a"), QStringLiteral("b")});
            config.save();
        }

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.position(), int(ConfigFacade::Left));
        QCOMPARE(reloaded.displayMode(), int(ConfigFacade::SingleScreen));
        QCOMPARE(reloaded.targetOutput(), QStringLiteral("WL-1"));
        QCOMPARE(reloaded.tileSize(), 64);
        QCOMPARE(reloaded.animationSpeed(), 0);
        QCOMPARE(reloaded.pinnedEntries(), QStringList({QStringLiteral("a"), QStringLiteral("b")}));
    }

    /// The magnification keys are stored as percentages and read as ratios, so
    /// the x100 has two chances to go wrong: on the way in and on the way out.
    void magnificationParametersRoundTripAsRatios()
    {
        {
            ConfigFacade config(configPath());
            QCOMPARE(config.magnificationEnabled(), true);
            QCOMPARE(config.magnificationFactor(), 2.0);
            QCOMPARE(config.falloffRadius(), 3.0);
            QCOMPARE(config.curveExponent(), 1.6);

            config.setMagnificationFactor(2.35);
            config.setFalloffRadius(4.5);
            config.setCurveExponent(0.75);
            config.setMagnificationEnabled(false);
            config.save();
        }

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.magnificationEnabled(), false);
        QCOMPARE(reloaded.magnificationFactor(), 2.35);
        QCOMPARE(reloaded.falloffRadius(), 4.5);
        QCOMPARE(reloaded.curveExponent(), 0.75);
    }

    /// A hand-edited file is the way an absurd curve gets in. The schema clamps
    /// in stored units, and the reader must still hand back a ratio.
    void magnificationParametersAreClamped()
    {
        writeConfig(QStringLiteral("[General]\ncurveExponent=9000\nfalloffRadius=1\n"));

        ConfigFacade config(configPath());
        QCOMPARE(config.curveExponent(), 4.0);
        QCOMPARE(config.falloffRadius(), 1.0);
    }

    void survivesCorruptFile()
    {
        writeConfig(QStringLiteral("\x01\x02 not ini at all\n[[[General\ntileSize=\n=\n"));

        ConfigFacade config(configPath());
        QCOMPARE(config.tileSize(), 48);
        QCOMPARE(config.position(), int(ConfigFacade::Bottom));
    }

    void clampsOutOfRange()
    {
        writeConfig(QStringLiteral("[General]\ntileSize=4\n"));
        {
            ConfigFacade config(configPath());
            QCOMPARE(config.tileSize(), 24);
        }

        writeConfig(QStringLiteral("[General]\ntileSize=9000\n"));
        ConfigFacade config(configPath());
        QCOMPARE(config.tileSize(), 128);
    }

    void singleScreenWithMissingOutputFallsBack()
    {
        const std::vector<OutputInfo> outputs = {
            {QStringLiteral("WL-0"), QRect(0, 0, 1600, 900), 1.0, true},
            {QStringLiteral("WL-1"), QRect(1600, 0, 1600, 900), 1.0, false},
        };

        writeConfig(QStringLiteral("[General]\ndisplayMode=SingleScreen\ntargetOutput=WL-9\n"));
        ConfigFacade config(configPath());
        QCOMPARE(config.displayMode(), int(ConfigFacade::SingleScreen));

        const auto surfaces =
            desiredSurfaces(DisplayMode::SingleScreen, outputs, QStringLiteral("WL-1"), config.targetOutput());
        QCOMPARE(surfaces.size(), 1u);
        QCOMPARE(surfaces.front(), QStringLiteral("WL-0")); // the primary

        // And the configured output is honoured when it is present.
        const auto present =
            desiredSurfaces(DisplayMode::SingleScreen, outputs, QStringLiteral("WL-0"), QStringLiteral("WL-1"));
        QCOMPARE(present.front(), QStringLiteral("WL-1"));
    }
};

QTEST_MAIN(TestConfig)
#include "test_config.moc"
