#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QFile>

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// Merging pinned entries with running windows.
///
/// Everything here goes through FakeTaskBackend, so the window list is whatever
/// the test says it is and no compositor is involved.
class TestTileModelMerge : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    FakeLauncherBackend m_launcher;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    /// Finds the tile for \a id, or -1. Row numbers shift as tiles come and go,
    /// so no assertion below hard-codes one.
    static int rowOf(const TileModel &model, const QString &id)
    {
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.tileAt(row).id == id) {
                return row;
            }
        }
        return -1;
    }

private Q_SLOTS:
    /*
     * Regression, reported 2026-09-03: opening a pinned application added a
     * second tile beside it instead of lighting the dot under it.
     *
     * The window's app id and the pinned entry's id are the same entry written
     * two ways. libtaskmanager's AppId role returns the KService **storage id**,
     * which ends in ".desktop"; config holds the bare name. Matching them with
     * string equality fails, so the running application looks like a different
     * one.
     *
     * (docs/decisions/2026-08-16-libtaskmanager-api-surface.md described AppId
     * as the storage id "sans extension", which is what this was built on and
     * is not what it returns. The record has been corrected.)
     */
    void aWindowMatchesItsPinnedEntryWhicheverWayTheIdIsWritten()
    {
        // As the compositor reports it: with the suffix.
        {
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("alpha")});

            FakeTaskBackend tasks;
            tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha.desktop"));

            TileModel model(&config, &m_launcher, &tasks);

            QCOMPARE(model.rowCount(), 1);
            QVERIFY(model.tileAt(0).isPinned);
            QVERIFY(model.tileAt(0).isRunning);
            QCOMPARE(model.tileAt(0).windowCount, 1);
        }

        // And the other way round: a config written with the suffix still
        // matches a window reported without it.
        {
            QFile::remove(configPath());
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("alpha.desktop")});

            FakeTaskBackend tasks;
            tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));

            TileModel model(&config, &m_launcher, &tasks);

            QCOMPARE(model.rowCount(), 1);
            QVERIFY(model.tileAt(0).isRunning);
            QCOMPARE(model.tileAt(0).windowCount, 1);
        }
    }

    /// Two windows of one application are still one tile when the ids are
    /// written differently — the count has to survive the normalisation too.
    void windowCountsAggregateAcrossIdSpellings()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));
        tasks.addWindow(QStringLiteral("w2"), QStringLiteral("alpha.desktop"));

        TileModel model(&config, &m_launcher, &tasks);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tileAt(0).windowCount, 2);
    }

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());

        m_launcher.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));
        m_launcher.addEntry(QStringLiteral("beta"), QStringLiteral("Beta"), QStringLiteral("beta-icon"));
        m_launcher.addEntry(QStringLiteral("gamma"), QStringLiteral("Gamma"), QStringLiteral("gamma-icon"));
    }

    void pinnedAndRunningDedupe()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));

        TileModel model(&config, &m_launcher, &tasks);

        // A pinned app that is running is one tile, not two.
        QCOMPARE(model.rowCount(), 2);
        const int row = rowOf(model, QStringLiteral("alpha"));
        QCOMPARE(row, 0);
        QVERIFY(model.tileAt(row).isPinned);
        QVERIFY(model.tileAt(row).isRunning);
        QCOMPARE(model.tileAt(row).windowCount, 1);

        // Its name still comes from the desktop entry, not the app id.
        QCOMPARE(model.tileAt(row).name, QStringLiteral("Alpha"));

        // The other pinned tile is unaffected.
        QVERIFY(!model.tileAt(rowOf(model, QStringLiteral("beta"))).isRunning);
    }

    void runningUnpinnedAppends()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);

        QCOMPARE(model.rowCount(), 2);
        // Pinned first, then running: the unpinned app goes after, never before.
        QCOMPARE(model.tileAt(0).id, QStringLiteral("alpha"));
        QCOMPARE(model.tileAt(1).id, QStringLiteral("gamma"));
        QVERIFY(!model.tileAt(1).isPinned);
        QVERIFY(model.tileAt(1).isRunning);
        QCOMPARE(model.tileAt(1).name, QStringLiteral("Gamma"));
    }

    void unpinnedRunningKeepFirstSeenOrder()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));
        tasks.addWindow(QStringLiteral("w2"), QStringLiteral("alpha"));
        tasks.addWindow(QStringLiteral("w3"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);

        // Order follows first window seen, not app id: an arbitrary order would
        // make tiles jump about between rebuilds.
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.tileAt(0).id, QStringLiteral("gamma"));
        QCOMPARE(model.tileAt(1).id, QStringLiteral("alpha"));
    }

    void closingUnpinnedRemovesTile()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);
        QCOMPARE(model.rowCount(), 2);

        tasks.removeWindow(QStringLiteral("w1"));
        model.rebuild();

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(rowOf(model, QStringLiteral("gamma")), -1);
    }

    void closingPinnedKeepsTile()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));

        TileModel model(&config, &m_launcher, &tasks);
        QVERIFY(model.tileAt(0).isRunning);

        tasks.removeWindow(QStringLiteral("w1"));
        model.rebuild();

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tileAt(0).id, QStringLiteral("alpha"));
        QVERIFY(model.tileAt(0).isPinned);
        QVERIFY(!model.tileAt(0).isRunning);
        QCOMPARE(model.tileAt(0).windowCount, 0);
    }

    void windowCountAggregates()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));
        tasks.addWindow(QStringLiteral("w2"), QStringLiteral("alpha"));
        tasks.addWindow(QStringLiteral("w3"), QStringLiteral("alpha"));

        TileModel model(&config, &m_launcher, &tasks);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tileAt(0).windowCount, 3);

        // And it comes back down.
        tasks.removeWindow(QStringLiteral("w2"));
        model.rebuild();
        QCOMPARE(model.tileAt(0).windowCount, 2);
    }

    void unmatchedWindowDoesNotCrash()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("org.example.notinstalled"));
        // A window the compositor gave no app id for at all.
        tasks.addWindow(QStringLiteral("w2"), QString());

        TileModel model(&config, &m_launcher, &tasks);

        // The unresolvable app is still running, so it still gets a tile — named
        // after its raw app id. The anonymous window gets none: there is nothing
        // to name or aggregate it by.
        QCOMPARE(model.rowCount(), 2);
        const int row = rowOf(model, QStringLiteral("org.example.notinstalled"));
        QVERIFY(row >= 0);
        QCOMPARE(model.tileAt(row).name, QStringLiteral("org.example.notinstalled"));
        QVERIFY(!model.tileAt(row).iconName.isEmpty());

        // Both failures are collected for diagnosis rather than special-cased.
        QCOMPARE(model.unmatchedIds(),
                 QStringList({QStringLiteral("w2"), QStringLiteral("org.example.notinstalled")}));

        // And they clear once the windows go.
        tasks.setWindows({});
        model.rebuild();
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.unmatchedIds().isEmpty());
    }

    void modelTesterPassesAfterEveryTransition()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        FakeTaskBackend tasks;
        TileModel model(&config, &m_launcher, &tasks);
        // Fails the test on any contract violation, which would otherwise surface
        // as a rendering bug a long way from its cause.
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        // A pinned app starts.
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("alpha"));
        model.rebuild();
        QCOMPARE(model.rowCount(), 2);

        // An unpinned app starts, appending a tile.
        tasks.addWindow(QStringLiteral("w2"), QStringLiteral("gamma"));
        model.rebuild();
        QCOMPARE(model.rowCount(), 3);

        // A second window of the unpinned app: data change only.
        tasks.addWindow(QStringLiteral("w3"), QStringLiteral("gamma"));
        model.rebuild();
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.tileAt(2).windowCount, 2);

        // The pinned app quits: tile stays, no longer running.
        tasks.removeWindow(QStringLiteral("w1"));
        model.rebuild();
        QCOMPARE(model.rowCount(), 3);
        QVERIFY(!model.tileAt(0).isRunning);

        // Pinning changes underneath a running app.
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")});
        model.rebuild();
        QCOMPARE(model.rowCount(), 3);
        QVERIFY(model.tileAt(2).isPinned);
        QVERIFY(model.tileAt(2).isRunning);

        // Unpinned again while still running.
        config.setPinnedEntries({QStringLiteral("alpha")});
        model.rebuild();
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.tileAt(1).id, QStringLiteral("gamma"));
        QVERIFY(!model.tileAt(1).isPinned);

        // Everything goes away.
        tasks.setWindows({});
        config.setPinnedEntries({});
        model.rebuild();
        QCOMPARE(model.rowCount(), 0);
    }

    void rebuildIsIdempotent()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        model.rebuild();
        model.rebuild();

        // Re-query and diff is only cheap if an unchanged world is silent;
        // otherwise every window event would churn the whole view.
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(changed.count(), 0);
    }
};

QTEST_MAIN(TestTileModelMerge)
#include "test_tilemodel_merge.moc"
