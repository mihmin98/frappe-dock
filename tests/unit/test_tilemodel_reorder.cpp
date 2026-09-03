#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace frappe;

namespace
{
QStringList idsOf(const TileModel &model)
{
    QStringList ids;
    for (int row = 0; row < model.rowCount(); ++row) {
        ids.append(model.data(model.index(row, 0), TileModel::IdRole).toString());
    }
    return ids;
}
}

/// Reordering, as the drag gesture in Dock.qml drives it: the model is moved
/// while the drag is in flight, so every intermediate state has to be a valid
/// one and the order that ends up in config has to be the order on screen.
class TestTileModelReorder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    FakeLauncherBackend m_launcher;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    /// A model with one of everything the dock can hold: two pinned
    /// applications, two running unpinned ones, two files and two minimized
    /// windows, with the separators those regions bring.
    ///
    /// The config and the backend have to outlive the model, so they travel
    /// with it.
    struct Fixture {
        std::unique_ptr<ConfigFacade> config;
        std::unique_ptr<FakeTaskBackend> tasks;
        std::unique_ptr<TileModel> model;
    };

    Fixture makeMixedModel()
    {
        QFile::remove(configPath());

        Fixture fixture;
        fixture.config = std::make_unique<ConfigFacade>(configPath());
        fixture.config->setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});
        fixture.config->setFileEntries({m_dir.filePath(QStringLiteral("one.txt")),
                                        m_dir.filePath(QStringLiteral("two.txt"))});
        fixture.config->setMinimizeIntoIcon(false);

        fixture.tasks = std::make_unique<FakeTaskBackend>();
        // Running but not pinned: they share the application band with the
        // pinned tiles and have no persisted position of their own.
        fixture.tasks->addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));
        fixture.tasks->addWindow(QStringLiteral("w2"), QStringLiteral("delta"));

        WindowInfo minimizedOne;
        minimizedOne.windowId = QStringLiteral("w3");
        minimizedOne.appId = QStringLiteral("alpha");
        minimizedOne.title = QStringLiteral("Alpha window");
        minimizedOne.isMinimized = true;
        fixture.tasks->addWindow(minimizedOne);

        WindowInfo minimizedTwo = minimizedOne;
        minimizedTwo.windowId = QStringLiteral("w4");
        minimizedTwo.title = QStringLiteral("Second window");
        fixture.tasks->addWindow(minimizedTwo);

        fixture.model = std::make_unique<TileModel>(fixture.config.get(), &m_launcher,
                                                    fixture.tasks.get());
        return fixture;
    }

private Q_SLOTS:
    /// ConfigFacade redirects a process-wide singleton at one path, so state a
    /// test leaves behind is state the next one inherits. Clearing it here is
    /// what keeps the suite order-independent.
    void cleanup()
    {
        QFile::remove(configPath());
    }

    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());

        m_launcher.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));
        m_launcher.addEntry(QStringLiteral("beta"), QStringLiteral("Beta"), QStringLiteral("beta-icon"));
        m_launcher.addEntry(QStringLiteral("gamma"), QStringLiteral("Gamma"), QStringLiteral("gamma-icon"));
        m_launcher.addEntry(QStringLiteral("delta"), QStringLiteral("Delta"), QStringLiteral("delta-icon"));
    }

    /*
     * Regression, reported from manual testing on 2026-09-03: the dock froze
     * while dragging a tile over the strip, and again — after a first, partial
     * fix — while dragging an *unpinned* one.
     *
     * The rule the model has to obey is one sentence: **a move it accepts must
     * be a fixed point of rebuild().** rebuild() re-derives the whole order from
     * config, so a move config cannot express is a move the model undoes moments
     * later. Accepting it is worse than refusing it, because the caller is a
     * drag: it asks again on the next pointer event, and every round writes
     * config — which calls save(), a synchronous disk write — rebuilds the model,
     * and reconfigures every layer surface.
     *
     * Checked exhaustively rather than case by case, because the first fix was
     * written case by case and missed two: unpinned running tiles, whose order
     * is never written to config at all, and minimized-window tiles, likewise.
     * Every pair of rows, over a model that has one of everything.
     */
    void anAcceptedMoveSurvivesARebuild()
    {
        // Sized from a fresh model so the loop covers rows that only exist
        // because of the regions below.
        const int rows = [this] {
            auto fixture = makeMixedModel();
            return fixture.model->rowCount();
        }();
        QVERIFY(rows >= 8);

        for (int from = 0; from < rows; ++from) {
            for (int to = 0; to < rows; ++to) {
                auto fixture = makeMixedModel();
                TileModel &model = *fixture.model;

                const QStringList before = idsOf(model);
                if (!model.moveTile(from, to)) {
                    // A refusal must also leave the order alone.
                    QCOMPARE(idsOf(model), before);
                    continue;
                }

                const QStringList afterMove = idsOf(model);
                model.rebuild();

                QVERIFY2(idsOf(model) == afterMove,
                         qPrintable(QStringLiteral("moving row %1 to %2 gave [%3], "
                                                   "rebuild made it [%4]")
                                        .arg(from)
                                        .arg(to)
                                        .arg(afterMove.join(QLatin1Char(',')))
                                        .arg(idsOf(model).join(QLatin1Char(',')))));
            }
        }
    }

    void correctSequenceAfterReorder_data()
    {
        QTest::addColumn<int>("from");
        QTest::addColumn<int>("to");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("forward one") << 0 << 1
            << QStringList({QStringLiteral("beta"), QStringLiteral("alpha"),
                            QStringLiteral("gamma"), QStringLiteral("delta")});
        QTest::newRow("forward to end") << 0 << 3
            << QStringList({QStringLiteral("beta"), QStringLiteral("gamma"),
                            QStringLiteral("delta"), QStringLiteral("alpha")});
        QTest::newRow("backward one") << 2 << 1
            << QStringList({QStringLiteral("alpha"), QStringLiteral("gamma"),
                            QStringLiteral("beta"), QStringLiteral("delta")});
        QTest::newRow("backward to start") << 3 << 0
            << QStringList({QStringLiteral("delta"), QStringLiteral("alpha"),
                            QStringLiteral("beta"), QStringLiteral("gamma")});
    }

    void correctSequenceAfterReorder()
    {
        QFETCH(int, from);
        QFETCH(int, to);
        QFETCH(QStringList, expected);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"),
                                 QStringLiteral("gamma"), QStringLiteral("delta")});

        TileModel model(&config, &m_launcher);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        QVERIFY(model.moveTile(from, to));
        QCOMPARE(idsOf(model), expected);
    }

    /// A drag passes through every intermediate index one step at a time, so the
    /// result of walking a tile across the strip has to be the same as moving it
    /// there directly. If it is not, the tile lands somewhere other than where
    /// the pointer left it.
    void steppedMoveMatchesDirectMove()
    {
        ConfigFacade config(configPath());
        const QStringList pinned{QStringLiteral("alpha"), QStringLiteral("beta"),
                                 QStringLiteral("gamma"), QStringLiteral("delta")};

        config.setPinnedEntries(pinned);
        TileModel direct(&config, &m_launcher);
        QVERIFY(direct.moveTile(0, 3));

        config.setPinnedEntries(pinned);
        TileModel stepped(&config, &m_launcher);
        QVERIFY(stepped.moveTile(0, 1));
        QVERIFY(stepped.moveTile(1, 2));
        QVERIFY(stepped.moveTile(2, 3));

        QCOMPARE(idsOf(stepped), idsOf(direct));
    }

    void correctSignalsEmitted()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")});

        TileModel model(&config, &m_launcher);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

        // The pair has to bracket the change: a view that saw the new order
        // before the begin, or the old one after the end, would draw a strip
        // that does not exist.
        QStringList order;
        connect(&model, &QAbstractItemModel::rowsAboutToBeMoved, this, [&order, &model] {
            order.append(QStringLiteral("begin"));
            // Still the old order at this point.
            order.append(model.data(model.index(0, 0), TileModel::IdRole).toString());
        });
        connect(&model, &QAbstractItemModel::rowsMoved, this, [&order, &model] {
            order.append(QStringLiteral("end"));
            order.append(model.data(model.index(0, 0), TileModel::IdRole).toString());
        });

        QSignalSpy movedSpy(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        QVERIFY(model.moveTile(2, 0));

        QCOMPARE(order, QStringList({QStringLiteral("begin"), QStringLiteral("alpha"),
                                     QStringLiteral("end"), QStringLiteral("gamma")}));

        QCOMPARE(movedSpy.count(), 1);
        // A move announced as a remove and an insert loses the delegate, and
        // with it the animation the reflow is made of.
        QCOMPARE(insertedSpy.count(), 0);
        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(resetSpy.count(), 0);

        const QList<QVariant> args = movedSpy.first();
        QCOMPARE(args.at(1).toInt(), 2); // start
        QCOMPARE(args.at(2).toInt(), 2); // end
        QCOMPARE(args.at(4).toInt(), 0); // destination, before the removal
    }

    void rejectedMoveEmitsNothing_data()
    {
        QTest::addColumn<int>("from");
        QTest::addColumn<int>("to");

        QTest::newRow("same row") << 1 << 1;
        QTest::newRow("negative source") << -1 << 0;
        QTest::newRow("negative target") << 0 << -1;
        QTest::newRow("source past end") << 3 << 0;
        QTest::newRow("target past end") << 0 << 3;
    }

    /// The drag calls moveTile on every motion event, most of which are no-ops.
    /// A no-op that still emitted would restart the reflow animation on every
    /// frame.
    void rejectedMoveEmitsNothing()
    {
        QFETCH(int, from);
        QFETCH(int, to);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")});

        TileModel model(&config, &m_launcher);
        QSignalSpy movedSpy(&model, &QAbstractItemModel::rowsMoved);

        QVERIFY(!model.moveTile(from, to));
        QCOMPARE(movedSpy.count(), 0);
        QCOMPARE(idsOf(model), QStringList({QStringLiteral("alpha"), QStringLiteral("beta"),
                                            QStringLiteral("gamma")}));
    }

    void configUpdatedWithNewOrder()
    {
        const QStringList expected{QStringLiteral("beta"), QStringLiteral("gamma"), QStringLiteral("alpha")};

        {
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"),
                                     QStringLiteral("gamma")});

            TileModel model(&config, &m_launcher);
            QVERIFY(model.moveTile(0, 2));

            QCOMPARE(config.pinnedEntries(), expected);
        }
        // The scope ends, which is the restart: the write-through is deferred
        // past the end of the gesture, and going away flushes it, exactly as
        // quitting does. Nothing calls save() on purpose -- see
        // orderSurvivesWithoutAnExplicitSave.

        ConfigFacade reloaded(configPath());
        QCOMPARE(reloaded.pinnedEntries(), expected);

        TileModel restarted(&reloaded, &m_launcher);
        QCOMPARE(idsOf(restarted), expected);
    }

    /// Regression: reordering survived only because the test called save()
    /// itself. Nothing in the dock ever did -- the sole callers were the tuning
    /// harness and this test -- so every reorder was discarded when the process
    /// exited. There is no dialog and no OK button behind direct manipulation,
    /// so the write has to happen at the point of the change.
    void orderSurvivesWithoutAnExplicitSave()
    {
        {
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta"),
                                     QStringLiteral("gamma")});

            TileModel model(&config, &m_launcher);
            QVERIFY(model.moveTile(0, 2));
        }

        const QStringList expected{QStringLiteral("beta"), QStringLiteral("gamma"),
                                   QStringLiteral("alpha")};
        ConfigFacade restarted(configPath());
        QCOMPARE(restarted.pinnedEntries(), expected);
    }

    /// The same defect on the other write path: pinning and unpinning from the
    /// context menu, and the drag-out gesture that calls through to it.
    void pinningSurvivesWithoutAnExplicitSave()
    {
        {
            ConfigFacade config(configPath());
            config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

            TileModel model(&config, &m_launcher);
            QVERIFY(model.setPinned(QStringLiteral("gamma"), true));
            QVERIFY(model.setPinned(QStringLiteral("alpha"), false));
        }

        ConfigFacade restarted(configPath());
        QCOMPARE(restarted.pinnedEntries(),
                 QStringList({QStringLiteral("beta"), QStringLiteral("gamma")}));
    }

    /// Only pinned tiles have a persisted position. Dragging a running but
    /// unpinned application about must not quietly pin it.
    /*
     * An unpinned running tile has no persisted position, so it cannot be
     * dragged to one.
     *
     * This test previously asserted the opposite — that moving the unpinned
     * tile to the front was **accepted** while config kept only the pinned ids.
     * That is the freeze from 2026-09-03 written down as an expectation: a move
     * the model accepts but cannot persist is one that the next rebuild() undoes,
     * and the caller is a drag that asks again on every pointer event. The
     * intent of the test — that unpinned tiles never reach config — is kept
     * below; only the accept-and-undo half is gone.
     */
    void unpinnedTilesCannotBeReorderedAndAreNotWrittenToConfig()
    {
        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        FakeTaskBackend tasks;
        tasks.addWindow(QStringLiteral("w1"), QStringLiteral("gamma"));

        TileModel model(&config, &m_launcher, &tasks);
        QCOMPARE(idsOf(model), QStringList({QStringLiteral("alpha"), QStringLiteral("beta"),
                                            QStringLiteral("gamma")}));
        QVERIFY(!model.data(model.index(2, 0), TileModel::IsPinnedRole).toBool());

        // Refused, in both directions: the running tile cannot be dragged into
        // the pinned run, and a pinned tile cannot be dragged past it.
        QVERIFY(!model.moveTile(2, 0));
        QVERIFY(!model.moveTile(0, 2));
        QCOMPARE(idsOf(model), QStringList({QStringLiteral("alpha"), QStringLiteral("beta"),
                                            QStringLiteral("gamma")}));

        // A move among the pinned tiles is still a move, and still writes only
        // the pinned ids — gamma is running, not pinned, and never reaches
        // config.
        QVERIFY(model.moveTile(0, 1));
        QCOMPARE(idsOf(model), QStringList({QStringLiteral("beta"), QStringLiteral("alpha"),
                                            QStringLiteral("gamma")}));
        QCOMPARE(config.pinnedEntries(),
                 QStringList({QStringLiteral("beta"), QStringLiteral("alpha")}));
    }
};

QTEST_MAIN(TestTileModelReorder)
#include "test_tilemodel_reorder.moc"
