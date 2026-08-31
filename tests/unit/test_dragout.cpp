#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// Unpinning, which is what the drag-out gesture does on release.
///
/// The point of the suite is the negative: unpinning edits one configuration
/// key and touches nothing else. A dock that could uninstall by gesture would be
/// one slip of the hand away from real damage, so "the launcher was not asked to
/// do anything" is asserted rather than assumed.
class TestDragOut : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    FakeLauncherBackend m_launcher;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());

        m_launcher.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));
        m_launcher.addEntry(QStringLiteral("beta"), QStringLiteral("Beta"), QStringLiteral("beta-icon"));
        m_launcher.addEntry(QStringLiteral("gamma"), QStringLiteral("Gamma"), QStringLiteral("gamma-icon"));
    }

    void unpinTouchesConfigOnly()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")});

        TileModel model(&config, &m_launcher);
        QVERIFY(model.unpinTile(1));

        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("gamma")}));

        // The tile is gone from the dock...
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), TileModel::IdRole).toString(), QStringLiteral("alpha"));
        QCOMPARE(model.data(model.index(1, 0), TileModel::IdRole).toString(), QStringLiteral("gamma"));

        // ...and the application is not: still installed, still resolvable,
        // still launchable from anywhere else.
        const auto entry = m_launcher.lookup(QStringLiteral("beta"));
        QVERIFY(entry.has_value());
        QCOMPARE(entry->name, QStringLiteral("Beta"));
    }

    /// A pinned application that is running keeps its tile after unpinning —
    /// what was removed is the promise to keep it, not the window.
    void unpinningARunningApplicationLeavesItsTile()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("beta"));

        TileModel model(&config, &m_launcher, &tasks);
        QVERIFY(model.unpinTile(1));

        QCOMPARE(config.pinnedEntries(), QStringList({QStringLiteral("alpha")}));
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(1, 0), TileModel::IdRole).toString(), QStringLiteral("beta"));
        QVERIFY(!model.data(model.index(1, 0), TileModel::IsPinnedRole).toBool());
        QVERIFY(model.data(model.index(1, 0), TileModel::IsRunningRole).toBool());
    }

    void unpinIsPersisted()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        QVERIFY(model.unpinTile(0));
        config.save();

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.pinnedEntries(), QStringList({QStringLiteral("beta")}));
    }

    void unpinningWhatIsNotPinnedChangesNothing()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);
        QCOMPARE(model.rowCount(), 2);

        // Row 1 is the running, unpinned application. There is nothing to
        // remove, and the gesture must not remove the tile from under a live
        // window.
        QVERIFY(!model.unpinTile(1));
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(config.pinnedEntries(), QStringList({QStringLiteral("alpha")}));
    }

    /// The other half of the gesture: what was dragged out has to be able to
    /// come back. Removing a pinned application leaves its tile behind while it
    /// is still running, and Keep in Dock on that tile re-pins it.
    void repinningARunningUnpinnedApplication()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("beta"));

        TileModel model(&config, &m_launcher, &tasks);
        QCOMPARE(model.rowCount(), 2);

        // Dragged out while running: the tile stays, the pin goes.
        QVERIFY(model.unpinTile(1));
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(!model.data(model.index(1, 0), TileModel::IsPinnedRole).toBool());

        // Keep in Dock, as the menu drives it.
        QVERIFY(model.setPinned(QStringLiteral("beta"), true));
        QVERIFY(model.data(model.index(1, 0), TileModel::IsPinnedRole).toBool());

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.pinnedEntries(),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
    }

    void outOfRangeRowIsRejected_data()
    {
        QTest::addColumn<int>("row");

        QTest::newRow("negative") << -1;
        QTest::newRow("past end") << 2;
    }

    void outOfRangeRowIsRejected()
    {
        QFETCH(int, row);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        QVERIFY(!model.unpinTile(row));
        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
    }

    /// The context menu's Remove row and the drag-out gesture write the same
    /// list through the same call, so they cannot disagree about what pinning
    /// means.
    void setPinnedIsSymmetric()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});

        TileModel model(&config, &m_launcher);

        QVERIFY(model.setPinned(QStringLiteral("beta"), true));
        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));

        // Pinning what is already pinned is a no-op rather than a duplicate row.
        QVERIFY(!model.setPinned(QStringLiteral("beta"), true));
        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));

        QVERIFY(model.setPinned(QStringLiteral("beta"), false));
        QCOMPARE(config.pinnedEntries(), QStringList({QStringLiteral("alpha")}));

        QVERIFY(!model.setPinned(QStringLiteral("beta"), false));
        QVERIFY(!model.setPinned(QString(), false));
    }
};

QTEST_MAIN(TestDragOut)
#include "test_dragout.moc"
