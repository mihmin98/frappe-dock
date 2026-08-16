#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

class TestTileModel : public QObject
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

    void modelTesterPasses()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        // Fails the test on any contract violation, which would otherwise surface
        // as a rendering bug a long way from its cause.
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("gamma"), QStringLiteral("beta")});
        model.rebuild();
        QCOMPARE(model.rowCount(), 3);

        config.setPinnedEntries({QStringLiteral("gamma")});
        model.rebuild();
        QCOMPARE(model.rowCount(), 1);

        config.setPinnedEntries({});
        model.rebuild();
        QCOMPARE(model.rowCount(), 0);
    }

    void rowsMatchConfigOrder()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("gamma"), QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0, 0), TileModel::IdRole).toString(), QStringLiteral("gamma"));
        QCOMPARE(model.data(model.index(1, 0), TileModel::IdRole).toString(), QStringLiteral("alpha"));
        QCOMPARE(model.data(model.index(2, 0), TileModel::IdRole).toString(), QStringLiteral("beta"));
        QCOMPARE(model.data(model.index(1, 0), TileModel::NameRole).toString(), QStringLiteral("Alpha"));
        QCOMPARE(model.data(model.index(1, 0), TileModel::IconNameRole).toString(), QStringLiteral("alpha-icon"));
    }

    void regionTagsAssignedCorrectly()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        for (int row = 0; row < model.rowCount(); ++row) {
            QCOMPARE(model.data(model.index(row, 0), TileModel::RegionRole).toInt(), int(Region::Pinned));
            QCOMPARE(model.data(model.index(row, 0), TileModel::KindRole).toInt(), int(TileKind::Application));
            QVERIFY(model.data(model.index(row, 0), TileModel::IsPinnedRole).toBool());
        }
    }

    void unresolvableEntryStillProducesTile()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("no-such-app")});

        TileModel model(&config, &m_launcher);
        // A gap would silently hide the fact that a pinned application went away.
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(1, 0), TileModel::IdRole).toString(), QStringLiteral("no-such-app"));
        QCOMPARE(model.data(model.index(1, 0), TileModel::NameRole).toString(), QStringLiteral("no-such-app"));
        QVERIFY(!model.data(model.index(1, 0), TileModel::IconNameRole).toString().isEmpty());
    }

    void reorderEmitsCorrectSignals()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")});

        TileModel model(&config, &m_launcher);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        QSignalSpy movedSpy(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

        QVERIFY(model.moveTile(0, 2));

        QCOMPARE(movedSpy.count(), 1);
        QCOMPARE(insertedSpy.count(), 0);
        QCOMPARE(removedSpy.count(), 0);

        const QList<QVariant> args = movedSpy.first();
        QCOMPARE(args.at(1).toInt(), 0); // start
        QCOMPARE(args.at(2).toInt(), 0); // end
        QCOMPARE(args.at(4).toInt(), 3); // destination row, before the removal

        QCOMPARE(model.data(model.index(2, 0), TileModel::IdRole).toString(), QStringLiteral("alpha"));
        // The new order is persisted, so it survives a rebuild.
        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("beta"), QStringLiteral("gamma"), QStringLiteral("alpha")}));

        model.rebuild();
        QCOMPARE(model.data(model.index(2, 0), TileModel::IdRole).toString(), QStringLiteral("alpha"));
    }

    void rebuildIsIdempotent()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &m_launcher);
        QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

        model.rebuild();
        model.rebuild();

        QCOMPARE(insertedSpy.count(), 0);
        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 0);
        QCOMPARE(model.rowCount(), 2);
    }
};

QTEST_MAIN(TestTileModel)
#include "test_tilemodel.moc"
