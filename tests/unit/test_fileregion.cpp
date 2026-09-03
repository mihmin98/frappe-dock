#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// The file region: folder and file tiles, their separator, and the two
/// configuration operations that put them there and take them away.
class TestFileRegion : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    FakeLauncherBackend m_launcher;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    QString makeDir(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        [&] {
            QVERIFY(QDir().mkpath(path));
        }();
        return path;
    }

    QString makeFile(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        [&] {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QVERIFY(file.write("x") == 1);
        }();
        return path;
    }

    /// The rows, as "kind:name", which is what the assertions are actually about.
    QStringList shapeOf(const TileModel &model) const
    {
        QStringList rows;
        for (int i = 0; i < model.rowCount(); ++i) {
            const Tile &tile = model.tileAt(i);
            QString kind;
            switch (tile.kind) {
            case TileKind::Application:
                kind = QStringLiteral("app");
                break;
            case TileKind::Folder:
                kind = QStringLiteral("folder");
                break;
            case TileKind::File:
                kind = QStringLiteral("file");
                break;
            case TileKind::Separator:
                kind = QStringLiteral("sep");
                break;
            default:
                kind = QStringLiteral("other");
                break;
            }
            rows.append(kind + u':' + (tile.kind == TileKind::Separator ? QString() : tile.name));
        }
        return rows;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_launcher.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));
    }

    void folderBecomesAFolderTile()
    {
        const QString folder = makeDir(QStringLiteral("Projects"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({folder});

        TileModel model(&config, &m_launcher);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        QCOMPARE(model.rowCount(), 1);
        const Tile &tile = model.tileAt(0);
        QCOMPARE(tile.kind, TileKind::Folder);
        QCOMPARE(tile.region, Region::Files);
        QCOMPARE(tile.name, QStringLiteral("Projects"));
        QCOMPARE(tile.id, folder);
        QCOMPARE(tile.iconName, QStringLiteral("folder"));
        QVERIFY(!tile.isPinned);
    }

    void fileBecomesAFileTileWithItsTypeIcon()
    {
        const QString file = makeFile(QStringLiteral("notes.txt"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({file});

        TileModel model(&config, &m_launcher);
        QCOMPARE(model.rowCount(), 1);

        const Tile &tile = model.tileAt(0);
        QCOMPARE(tile.kind, TileKind::File);
        QCOMPARE(tile.name, QStringLiteral("notes.txt"));
        // Not asserting the exact icon: it comes from the shared MIME database,
        // which is a property of the machine. That it resolved to something is
        // the contract — a blank tile is the failure being guarded against.
        QVERIFY(!tile.iconName.isEmpty());
        QVERIFY(tile.iconName != QStringLiteral("folder"));
    }

    void separatorAppearsOnlyBetweenTwoNonEmptyRegions()
    {
        const QString folder = makeDir(QStringLiteral("Sep"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});
        config.setFileEntries({folder});

        TileModel model(&config, &m_launcher);
        QCOMPARE(shapeOf(model),
                 QStringList({QStringLiteral("app:Alpha"), QStringLiteral("sep:"), QStringLiteral("folder:Sep")}));
        QCOMPARE(model.separatorRows(), QList<int>({1}));

        // A dock holding nothing but one folder has nothing to separate it from.
        config.setPinnedEntries({});
        model.rebuild();
        QCOMPARE(shapeOf(model), QStringList({QStringLiteral("folder:Sep")}));
        QVERIFY(model.separatorRows().isEmpty());

        // And an empty file region leaves no dangling rule behind it.
        config.setPinnedEntries({QStringLiteral("alpha")});
        config.setFileEntries({});
        model.rebuild();
        QCOMPARE(shapeOf(model), QStringList({QStringLiteral("app:Alpha")}));
        QVERIFY(model.separatorRows().isEmpty());
    }

    void fileRegionFollowsTheApplications()
    {
        const QString folder = makeDir(QStringLiteral("Order"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});
        config.setFileEntries({folder});

        TileModel model(&config, &m_launcher);
        QCOMPARE(model.regionOfRow(0), int(Region::Pinned));
        QCOMPARE(model.regionOfRow(2), int(Region::Files));
    }

    void addFileEntryAppendsAndPersists()
    {
        const QString one = makeDir(QStringLiteral("One"));
        const QString two = makeDir(QStringLiteral("Two"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({});

        TileModel model(&config, &m_launcher);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        QVERIFY(model.addFileEntry(one));
        QVERIFY(model.addFileEntry(two));
        QCOMPARE(shapeOf(model), QStringList({QStringLiteral("folder:One"), QStringLiteral("folder:Two")}));
        QCOMPARE(config.fileEntries(), QStringList({one, two}));

        // Same folder twice is not two tiles.
        QVERIFY(!model.addFileEntry(one));
        QCOMPARE(model.rowCount(), 2);
    }

    void addFileEntryStoresAnAbsolutePath()
    {
        const QString folder = makeDir(QStringLiteral("Relative"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({});
        TileModel model(&config, &m_launcher);

        // The same folder reached by a path with a redundant hop in it. Stored
        // raw, this would become a second tile for the same folder.
        const QString awkward = m_dir.path() + QStringLiteral("/./Relative");
        QVERIFY(model.addFileEntry(awkward));
        QCOMPARE(config.fileEntries(), QStringList({folder}));
        QVERIFY(!model.addFileEntry(folder));
    }

    void addFileEntryRefusesWhatIsNotThere()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({});
        TileModel model(&config, &m_launcher);

        QVERIFY(!model.addFileEntry(m_dir.filePath(QStringLiteral("never-existed"))));
        QVERIFY(!model.addFileEntry(QString()));
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(config.fileEntries().isEmpty());
    }

    void removeFileEntryTakesTheTileAway()
    {
        const QString one = makeDir(QStringLiteral("Keep"));
        const QString two = makeDir(QStringLiteral("Drop"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({one, two});

        TileModel model(&config, &m_launcher);
        QVERIFY(model.removeFileEntry(two));
        QCOMPARE(shapeOf(model), QStringList({QStringLiteral("folder:Keep")}));
        QCOMPARE(config.fileEntries(), QStringList({one}));

        QVERIFY(!model.removeFileEntry(two));
    }

    void removeFileEntryWorksAfterTheFolderIsGone()
    {
        const QString folder = makeDir(QStringLiteral("Doomed"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({});
        config.setFileEntries({folder});
        TileModel model(&config, &m_launcher);
        QCOMPARE(model.rowCount(), 1);

        QVERIFY(QDir(folder).removeRecursively());

        // The tile is still there — the user put it there, and silently dropping
        // it hides the fact that the folder went away — and is still removable.
        model.rebuild();
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(model.removeFileEntry(folder));
        QCOMPARE(model.rowCount(), 0);
    }

    void unpinTileRemovesAFolderTile()
    {
        const QString folder = makeDir(QStringLiteral("DragOut"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});
        config.setFileEntries({folder});

        TileModel model(&config, &m_launcher);
        QCOMPARE(model.rowCount(), 3);

        // Row 2 is the folder; the drag-out gesture knows only the row.
        QVERIFY(model.unpinTile(2));
        QCOMPARE(shapeOf(model), QStringList({QStringLiteral("app:Alpha")}));
        QVERIFY(config.fileEntries().isEmpty());
        // The application is untouched: the two lists are separate.
        QCOMPARE(config.pinnedEntries(), QStringList({QStringLiteral("alpha")}));
    }

    void unpinTileRefusesTheSeparator()
    {
        const QString folder = makeDir(QStringLiteral("Rule"));

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha")});
        config.setFileEntries({folder});

        TileModel model(&config, &m_launcher);
        // Row 1 is the rule between the regions. It is not a tile and dragging
        // at it must not empty the file region.
        QVERIFY(!model.unpinTile(1));
        QCOMPARE(model.rowCount(), 3);
    }
};

QTEST_MAIN(TestFileRegion)
#include "test_fileregion.moc"
