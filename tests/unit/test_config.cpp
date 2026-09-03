#include "core/config/configfacade.h"
#include "core/surfaceplan.h"

#include <QFile>
#include <QSignalSpy>
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

    /// The config file exactly as it sits on disk, or empty if it is not there.
    ///
    /// Read as bytes rather than through KConfig on purpose: the question these
    /// tests ask is whether a *write* happened, and KConfig would answer from
    /// memory.
    QString onDisk() const
    {
        QFile file(configPath());
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll());
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
        QCOMPARE(config.appearanceMode(), int(ConfigFacade::Default));
        QCOMPARE(config.animationSpeed(), 100);
        QCOMPARE(config.pinnedEntries(), QStringList());
    }

    /*
     * Which consumers a change wakes up.
     *
     * Every setter used to emit one changed(), and main() turned any of them
     * into a model rebuild, a surface reconcile and a layer-shell
     * reconfiguration of every surface. Reordering the dock is a *drag*, so
     * that ran several times a gesture to record a change in the pinned order.
     *
     * changed() is still emitted by everything — it is every property's NOTIFY,
     * and QML binds to it. The narrow signals are what C++ consumers use, and
     * this is the test that stops a key being attached to the wrong one as the
     * schema grows.
     */
    void changeSignalsNameWhatChanged()
    {
        ConfigFacade config(configPath());

        QSignalSpy all(&config, &ConfigFacade::changed);
        QSignalSpy contents(&config, &ConfigFacade::contentsChanged);
        QSignalSpy surfaceSet(&config, &ConfigFacade::surfaceSetChanged);
        QSignalSpy surfaceGeometry(&config, &ConfigFacade::surfaceGeometryChanged);

        const auto counts = [&] {
            return QList<int>{int(all.count()), int(contents.count()),
                              int(surfaceSet.count()), int(surfaceGeometry.count())};
        };
        const auto clear = [&] {
            all.clear();
            contents.clear();
            surfaceSet.clear();
            surfaceGeometry.clear();
        };

        // The tiles the dock holds.
        config.setPinnedEntries({QStringLiteral("alpha")});
        QCOMPARE(counts(), QList<int>({1, 1, 0, 0}));
        clear();

        config.setFileEntries({QStringLiteral("/tmp")});
        QCOMPARE(counts(), QList<int>({1, 1, 0, 0}));
        clear();

        config.setMinimizeIntoIcon(true);
        QCOMPARE(counts(), QList<int>({1, 1, 0, 0}));
        clear();

        // Which outputs carry a dock.
        config.setDisplayMode(ConfigFacade::SingleScreen);
        QCOMPARE(counts(), QList<int>({1, 0, 1, 0}));
        clear();

        config.setTargetOutput(QStringLiteral("WL-1"));
        QCOMPARE(counts(), QList<int>({1, 0, 1, 0}));
        clear();

        // How big the dock is on the outputs it has.
        config.setTileSize(64);
        QCOMPARE(counts(), QList<int>({1, 0, 0, 1}));
        clear();

        config.setPosition(ConfigFacade::Left);
        QCOMPARE(counts(), QList<int>({1, 0, 0, 1}));
        clear();

        // Everything else is the view's business alone. Magnification is the
        // one worth naming: it changes how large a tile is *drawn*, but the
        // surface already spans the output and the shelf is normative
        // geometry, so no surface has to be reconfigured for it.
        config.setMagnificationFactor(2.5);
        QCOMPARE(counts(), QList<int>({1, 0, 0, 0}));
        clear();

        config.setAnimationSpeed(50);
        QCOMPARE(counts(), QList<int>({1, 0, 0, 0}));
        clear();

        config.setAppearanceMode(ConfigFacade::Dark);
        QCOMPARE(counts(), QList<int>({1, 0, 0, 0}));
        clear();

        // A setter that changes nothing is not a change.
        config.setTileSize(64);
        QCOMPARE(counts(), QList<int>({0, 0, 0, 0}));
    }

    /*
     * Regression: reordering rewrote the whole config file on every step of the
     * drag. setPinnedEntries() called save() inline, and a drag calls it
     * several times.
     *
     * Direct manipulation still has no OK button — the write is not deferred to
     * one, it is deferred past the rest of the gesture.
     */
    void repeatedWritesAreCoalescedIntoOneSave()
    {
        ConfigFacade config(configPath());

        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});
        QTRY_VERIFY(onDisk().contains(QStringLiteral("pinnedEntries=alpha,beta")));

        // A gesture in flight: several changes in quick succession.
        config.setPinnedEntries({QStringLiteral("beta"), QStringLiteral("alpha")});
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});
        config.setPinnedEntries({QStringLiteral("beta"), QStringLiteral("alpha")});

        // Still the old contents: nothing has been written yet.
        QVERIFY(onDisk().contains(QStringLiteral("pinnedEntries=alpha,beta")));

        // And then one write, once the gesture has stopped.
        QTRY_VERIFY(onDisk().contains(QStringLiteral("pinnedEntries=beta,alpha")));
    }

    /// A deferred write is not a lost one: going away flushes it.
    void aPendingWriteIsFlushedOnDestruction()
    {
        {
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("gamma"), QStringLiteral("delta")});
            // No wait, and no explicit save.
        }

        QVERIFY(onDisk().contains(QStringLiteral("pinnedEntries=gamma,delta")));
    }

    void roundTrip()
    {
        {
            ConfigFacade config(configPath());
            config.setPosition(ConfigFacade::Left);
            config.setDisplayMode(ConfigFacade::SingleScreen);
            config.setTargetOutput(QStringLiteral("WL-1"));
            config.setTileSize(64);
            config.setAppearanceMode(ConfigFacade::Tinted);
            config.setAnimationSpeed(0); // reduced motion: a real setting, not a disabled one
            config.setPinnedEntries({QStringLiteral("a"), QStringLiteral("b")});
            config.save();
        }

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.position(), int(ConfigFacade::Left));
        QCOMPARE(reloaded.displayMode(), int(ConfigFacade::SingleScreen));
        QCOMPARE(reloaded.targetOutput(), QStringLiteral("WL-1"));
        QCOMPARE(reloaded.tileSize(), 64);
        QCOMPARE(reloaded.appearanceMode(), int(ConfigFacade::Tinted));
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
