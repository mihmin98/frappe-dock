#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QAbstractItemModelTester>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// Minimized windows under both settings, and the transitions between them.
class TestMinimized : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    FakeLauncherBackend m_launcher;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    static WindowInfo window(const QString &id, const QString &appId, const QString &title, bool minimized)
    {
        WindowInfo info;
        info.windowId = id;
        info.appId = appId;
        info.title = title;
        info.isMinimized = minimized;
        return info;
    }

    static std::vector<Tile> tilesIn(const TileModel &model, Region region)
    {
        std::vector<Tile> found;
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.tileAt(row).region == region && model.tileAt(row).kind != TileKind::Separator) {
                found.push_back(model.tileAt(row));
            }
        }
        return found;
    }

    static int separatorCount(const TileModel &model)
    {
        int count = 0;
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.tileAt(row).kind == TileKind::Separator) {
                ++count;
            }
        }
        return count;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());

        m_launcher.addEntry(QStringLiteral("editor"), QStringLiteral("Editor"), QStringLiteral("editor-icon"));
        m_launcher.addEntry(QStringLiteral("player"), QStringLiteral("Player"), QStringLiteral("player-icon"));
    }

    void windowAppearsInMinimizedRegion()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("notes.txt"), false),
                          window(QStringLiteral("e2"), QStringLiteral("editor"), QStringLiteral("draft.txt"), true)});

        TileModel model(&config, &m_launcher, &tasks);

        const std::vector<Tile> minimized = tilesIn(model, Region::Minimized);
        QCOMPARE(minimized.size(), size_t{1});
        QCOMPARE(minimized.at(0).kind, TileKind::MinimizedWindow);
        // Keyed on the window, and labelled with its title, not the app's name.
        QCOMPARE(minimized.at(0).id, QStringLiteral("e2"));
        QCOMPARE(minimized.at(0).name, QStringLiteral("draft.txt"));
        // The icon still comes from the application.
        QCOMPARE(minimized.at(0).iconName, QStringLiteral("editor-icon"));

        // The application tile is unaffected: an application with a minimized
        // window is still running, and the window still counts.
        const std::vector<Tile> apps = tilesIn(model, Region::Pinned);
        QCOMPARE(apps.size(), size_t{1});
        QVERIFY(apps.at(0).isRunning);
        QCOMPARE(apps.at(0).windowCount, 2);
    }

    void eachMinimizedWindowIsItsOwnTile()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true),
                          window(QStringLiteral("e2"), QStringLiteral("editor"), QStringLiteral("two"), true)});

        TileModel model(&config, &m_launcher, &tasks);

        // Two windows of one application are two tiles: the user is choosing a
        // window, so they have to be distinguishable.
        const std::vector<Tile> minimized = tilesIn(model, Region::Minimized);
        QCOMPARE(minimized.size(), size_t{2});
        QCOMPARE(minimized.at(0).id, QStringLiteral("e1"));
        QCOMPARE(minimized.at(1).id, QStringLiteral("e2"));
    }

    void minimizeIntoTileCollapsesWindows()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(true);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true),
                          window(QStringLiteral("e2"), QStringLiteral("editor"), QStringLiteral("two"), true)});

        TileModel model(&config, &m_launcher, &tasks);

        // Two minimized windows, one tile: the application's own.
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tileAt(0).id, QStringLiteral("editor"));
        QVERIFY(model.tileAt(0).isRunning);
        QCOMPARE(model.tileAt(0).windowCount, 2);
        QVERIFY(tilesIn(model, Region::Minimized).empty());
    }

    void toggleMovesExistingMinimizedWindows()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true)});

        TileModel model(&config, &m_launcher, &tasks);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(tilesIn(model, Region::Minimized).size(), size_t{1});

        // Live: nothing restarts, the window is already minimized, and it has to
        // move on the strength of the setting alone.
        config.setMinimizeIntoIcon(true);
        model.rebuild();
        QVERIFY(tilesIn(model, Region::Minimized).empty());
        QCOMPARE(model.rowCount(), 1);

        config.setMinimizeIntoIcon(false);
        model.rebuild();
        QCOMPARE(tilesIn(model, Region::Minimized).size(), size_t{1});
        QCOMPARE(tilesIn(model, Region::Minimized).at(0).id, QStringLiteral("e1"));
    }

    void restoredWindowLeavesMinimizedRegion()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true)});

        TileModel model(&config, &m_launcher, &tasks);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(tilesIn(model, Region::Minimized).size(), size_t{1});

        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), false)});
        model.rebuild();

        QVERIFY(tilesIn(model, Region::Minimized).empty());
        // The application is still running; only the minimized tile went away.
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.tileAt(0).isRunning);
    }

    void closedMinimizedWindowLeavesMinimizedRegion()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true)});

        TileModel model(&config, &m_launcher, &tasks);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        tasks.setWindows({});
        model.rebuild();

        QVERIFY(tilesIn(model, Region::Minimized).empty());
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(!model.tileAt(0).isRunning);
    }

    void separatorAppearsOnlyBetweenTwoNonEmptyRegions()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        TileModel model(&config, &m_launcher, &tasks);

        // Applications but nothing minimized: no rule.
        QCOMPARE(separatorCount(model), 0);

        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true)});
        model.rebuild();
        QCOMPARE(separatorCount(model), 1);

        // And it sits between the two regions, not at either end.
        int separatorRow = -1;
        for (int row = 0; row < model.rowCount(); ++row) {
            if (model.tileAt(row).kind == TileKind::Separator) {
                separatorRow = row;
            }
        }
        QVERIFY(separatorRow > 0);
        QVERIFY(separatorRow < model.rowCount() - 1);
        QCOMPARE(model.tileAt(separatorRow - 1).region, Region::Pinned);
        QCOMPARE(model.tileAt(separatorRow + 1).region, Region::Minimized);

        tasks.setWindows({});
        model.rebuild();
        QCOMPARE(separatorCount(model), 0);
    }

    void noApplicationsMeansNoSeparator()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        // A minimized window always brings its application's tile with it, so
        // the region above is never actually empty — but the rule must not
        // depend on that, and this pins the leading-separator case shut.
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true)});

        TileModel model(&config, &m_launcher, &tasks);
        QVERIFY(model.tileAt(0).kind != TileKind::Separator);
    }

    void untitledMinimizedWindowStillGetsALabel()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QString(), true)});

        TileModel model(&config, &m_launcher, &tasks);
        QVERIFY(!tilesIn(model, Region::Minimized).at(0).name.isEmpty());
    }

    void minimizedWindowWithNoAppIdIsSkipped()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("x1"), QString(), QStringLiteral("mystery"), true)});

        TileModel model(&config, &m_launcher, &tasks);

        // Same rule as the application region: with no app id there is no icon to
        // draw and nothing to attribute it to.
        QVERIFY(tilesIn(model, Region::Minimized).empty());
        QCOMPARE(separatorCount(model), 0);
    }

    void severalApplicationsShareOneMinimizedRegion()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor"), QStringLiteral("player")});
        config.setMinimizeIntoIcon(false);

        FakeTaskBackend tasks;
        tasks.setWindows({window(QStringLiteral("e1"), QStringLiteral("editor"), QStringLiteral("one"), true),
                          window(QStringLiteral("p1"), QStringLiteral("player"), QStringLiteral("two"), true)});

        TileModel model(&config, &m_launcher, &tasks);

        // One region, one rule, whatever the windows belong to.
        QCOMPARE(tilesIn(model, Region::Minimized).size(), size_t{2});
        QCOMPARE(separatorCount(model), 1);
    }
};

QTEST_MAIN(TestMinimized)
#include "test_minimized.moc"
