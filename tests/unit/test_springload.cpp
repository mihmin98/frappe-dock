#include "core/input/springloader.h"

#include <QSignalSpy>
#include <QTest>

using namespace frappe;

/*
 * The spring-load countdown.
 *
 * Timing tests are written against a deliberately short delay and assert on
 * QSignalSpy::wait() rather than on elapsed milliseconds: what matters is that
 * the signal arrives after the delay and not before, not that a loaded CI
 * machine hit it to the millisecond.
 *
 * Whether the delay *feels* right is tests/manual/04-spring-loading.md; no
 * assertion can answer that.
 */
class TestSpringLoad : public QObject
{
    Q_OBJECT

private:
    /// Short enough to keep the suite quick, long enough that "before the
    /// threshold" is a real interval on a busy machine.
    static constexpr int Delay = 120;

private Q_SLOTS:
    void firesAtThreshold()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QCOMPARE(loader.armedTile(), QStringLiteral("alpha"));

        // Nothing yet: arming is not firing.
        QCOMPARE(spy.count(), 0);

        QVERIFY(spy.wait(10 * Delay));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.constFirst().constFirst().toString(), QStringLiteral("alpha"));
        // Fired, so no longer counting down.
        QVERIFY(loader.armedTile().isEmpty());
    }

    void cancelsOnEarlyExit()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        loader.dragLeft();
        QVERIFY(loader.armedTile().isEmpty());

        QTest::qWait(3 * Delay);
        QCOMPARE(spy.count(), 0);
    }

    /// The drag ending is an exit too: a tile must not spring open after the
    /// files have already been dropped on it.
    void dropCancels()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        loader.dragFinished();

        QTest::qWait(3 * Delay);
        QCOMPARE(spy.count(), 0);
    }

    void doesNotFireTwiceForTheSameDrag()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));

        // The drag is still in the air and still over the tile it opened.
        // Leaving and coming back must not open it a second time.
        loader.dragLeft();
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(loader.armedTile().isEmpty());

        QTest::qWait(3 * Delay);
        QCOMPARE(spy.count(), 1);
    }

    /// Regression: the strip's DropArea encloses the tiles', so moving the drag
    /// from the shelf onto a tile makes the strip report an exit. The view wired
    /// that exit to dragFinished(), which wiped the fired set, and every tile
    /// sprang open again on every pass. Crossing back onto a tile that has
    /// already fired must not reopen it.
    void crossingBackOntoATileDoesNotReopenIt()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));

        // What the strip and the tile actually emit as the pointer crosses the
        // boundary between them: the enter follows the exit immediately.
        loader.dockLeft();
        loader.dragEntered(QStringLiteral("alpha"));

        QTest::qWait(3 * Delay);
        QCOMPARE(spy.count(), 1);
    }

    /// The same exit, when the drag really has gone: nothing follows it, so the
    /// slate is cleared and the tile is fair game if the drag comes back.
    void leavingTheDockEntirelyClearsTheSlate()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));

        loader.dockLeft();
        QTest::qWait(Delay);

        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));
        QCOMPARE(spy.count(), 2);
    }

    /// ...but the next drag starts with a clean slate.
    void firesAgainOnANewDrag()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));

        loader.dragFinished();
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));
        QCOMPARE(spy.count(), 2);
    }

    /// A drag that opens one tile can go on to open another: spring-loading is
    /// once per tile per drag, not once per drag.
    void anotherTileStillSpringsOpen()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(spy.wait(10 * Delay));

        loader.dragLeft();
        loader.dragEntered(QStringLiteral("beta"));
        QVERIFY(spy.wait(10 * Delay));

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.constLast().constFirst().toString(), QStringLiteral("beta"));
    }

    /// A pointer wobbling inside one cell re-enters the same tile. Restarting
    /// the countdown each time would mean it never finishes.
    void reEnteringTheArmedTileDoesNotRestartTheCountdown()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        for (int i = 0; i < 4; ++i) {
            QTest::qWait(Delay / 4);
            loader.dragEntered(QStringLiteral("alpha"));
        }

        // Roughly one delay has passed in total, so a countdown that had been
        // restarted would still be running.
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2 * Delay);
    }

    /// Moving from one tile straight to the next re-aims the countdown rather
    /// than leaving the first one armed.
    void movingToAnotherTileRearms()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        loader.dragEntered(QStringLiteral("beta"));
        QCOMPARE(loader.armedTile(), QStringLiteral("beta"));

        QVERIFY(spy.wait(10 * Delay));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.constFirst().constFirst().toString(), QStringLiteral("beta"));
    }

    /// Zero is off, not instant: spring-loading is the one interaction that
    /// happens without a release, so it has to be refusable outright.
    void zeroDelayDisablesIt()
    {
        SpringLoader loader;
        loader.setDelay(0);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QStringLiteral("alpha"));
        QVERIFY(loader.armedTile().isEmpty());

        QTest::qWait(3 * Delay);
        QCOMPARE(spy.count(), 0);
    }

    /// Nothing is armed by an empty id — the strip beside the tiles reports
    /// hovers too, and it has no tile to open.
    void emptyTileIdArmsNothing()
    {
        SpringLoader loader;
        loader.setDelay(Delay);

        QSignalSpy spy(&loader, &SpringLoader::springLoaded);
        loader.dragEntered(QString());

        QTest::qWait(3 * Delay);
        QVERIFY(loader.armedTile().isEmpty());
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSpringLoad)
#include "test_springload.moc"
