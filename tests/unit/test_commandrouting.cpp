#include "app/dockcontroller.h"

#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// Every row of the interaction matrix, from a click through to the backend call
/// it should produce.
///
/// test_dispatch covers input → Command; this covers Command → backend. Together
/// they cover the click-to-effect path without a compositor.
class TestCommandRouting : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    /// The fixture every case shares: two applications, "editor" with two windows
    /// and "player" with one, so "this app" and "the others" are distinguishable
    /// and window cycling has something to cycle through.
    struct Fixture {
        FakeLauncherBackend launcher;
        FakeTaskBackend tasks;
        std::unique_ptr<ConfigFacade> config;
        std::unique_ptr<TileModel> model;
        std::unique_ptr<DockController> controller;
    };

    std::unique_ptr<Fixture> makeFixture()
    {
        auto f = std::make_unique<Fixture>();
        f->launcher.addEntry(QStringLiteral("editor"), QStringLiteral("Editor"), QStringLiteral("editor-icon"));
        f->launcher.addEntry(QStringLiteral("player"), QStringLiteral("Player"), QStringLiteral("player-icon"));
        f->launcher.addEntry(QStringLiteral("idle"), QStringLiteral("Idle"), QStringLiteral("idle-icon"));

        f->config = std::make_unique<ConfigFacade>(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        f->config->setPinnedEntries({QStringLiteral("editor"), QStringLiteral("player"), QStringLiteral("idle")});

        f->model = std::make_unique<TileModel>(f->config.get(), &f->launcher, &f->tasks);
        f->controller = std::make_unique<DockController>(f->model.get(), &f->launcher, &f->tasks);
        return f;
    }

    void addRunningWindows(Fixture &f)
    {
        WindowInfo editorFirst;
        editorFirst.windowId = QStringLiteral("e1");
        editorFirst.appId = QStringLiteral("editor");
        editorFirst.isActive = true;

        WindowInfo editorSecond;
        editorSecond.windowId = QStringLiteral("e2");
        editorSecond.appId = QStringLiteral("editor");

        WindowInfo player;
        player.windowId = QStringLiteral("p1");
        player.appId = QStringLiteral("player");

        f.tasks.setWindows({editorFirst, editorSecond, player});
        f.model->rebuild();
    }

    static int left()
    {
        return static_cast<int>(Qt::LeftButton);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void plainClickLaunchesWhenNotRunning()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        f->controller->tileClicked(QStringLiteral("idle"), left(), Qt::NoModifier);

        QCOMPARE(f->launcher.launched(), QStringList({QStringLiteral("idle")}));
        QVERIFY(f->tasks.activated().isEmpty());
    }

    void plainClickActivatesWhenRunning()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        f->controller->tileClicked(QStringLiteral("player"), left(), Qt::NoModifier);

        // Running means raise, not start a second copy.
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("p1")}));
        QVERIFY(f->launcher.launched().isEmpty());
    }

    void clickingARunningAppSkipsTheActiveWindow()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        // "editor" has two windows and the first is already active, so clicking
        // moves to the second rather than re-raising what is already in front.
        f->controller->tileClicked(QStringLiteral("editor"), left(), Qt::NoModifier);
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("e2")}));
    }

    void metaClickRevealsInFileManager()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        f->controller->tileClicked(QStringLiteral("editor"), left(), Qt::MetaModifier);

        QCOMPARE(f->launcher.revealed(), QStringList({QStringLiteral("editor")}));
        // Revealing is not activating: the window must not come forward too.
        QVERIFY(f->tasks.activated().isEmpty());
        QVERIFY(f->launcher.launched().isEmpty());
    }

    void altClickActivatesAndHidesNothingWithoutAPrevious()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        // Nothing has been activated yet, so there is no previous app to hide.
        // The activation half must still happen.
        f->controller->tileClicked(QStringLiteral("player"), left(), Qt::AltModifier);

        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("p1")}));
        QVERIFY(f->tasks.minimized().isEmpty());
    }

    void metaAltClickActivatesAndHidesOthers()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        f->controller->tileClicked(QStringLiteral("player"), left(),
                                   Qt::MetaModifier | Qt::AltModifier);

        // hideOthers is the backend's job, and it is told which app to spare.
        QCOMPARE(f->tasks.hideOthersCalls(), QStringList({QStringLiteral("player")}));
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("p1")}));
    }

    void middleClickStartsANewInstance()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        f->controller->tileClicked(QStringLiteral("editor"), static_cast<int>(Qt::MiddleButton), Qt::NoModifier);

        // Already running, and still launched: that is the whole point of the row.
        QCOMPARE(f->launcher.launched(), QStringList({QStringLiteral("editor")}));
        QVERIFY(f->tasks.activated().isEmpty());
    }

    void rightClickRequestsTheContextMenu()
    {
        auto f = makeFixture();
        QSignalSpy menu(f->controller.get(), &DockController::contextMenuRequested);

        f->controller->tileClicked(QStringLiteral("editor"), static_cast<int>(Qt::RightButton), Qt::NoModifier);

        QCOMPARE(menu.count(), 1);
        QCOMPARE(menu.at(0).at(0).toString(), QStringLiteral("editor"));
        // The controller does not open menus; it asks for one.
        QVERIFY(f->launcher.launched().isEmpty());
    }

    void pressAndHoldRequestsTheJumpList()
    {
        auto f = makeFixture();
        QSignalSpy jumpList(f->controller.get(), &DockController::jumpListRequested);
        QSignalSpy menu(f->controller.get(), &DockController::contextMenuRequested);

        f->controller->tileHeld(QStringLiteral("player"));

        QCOMPARE(jumpList.count(), 1);
        QCOMPARE(jumpList.at(0).at(0).toString(), QStringLiteral("player"));
        // Hold is its own row, not a slow right-click.
        QCOMPARE(menu.count(), 0);
    }

    void unknownCombinationStillActivates()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        // Shift-click claims no row. It must not be silently inert.
        f->controller->tileClicked(QStringLiteral("player"), left(), Qt::ShiftModifier);
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("p1")}));
    }

    void emptyTileIdIsIgnored()
    {
        auto f = makeFixture();
        QSignalSpy menu(f->controller.get(), &DockController::contextMenuRequested);

        f->controller->tileClicked(QString(), left(), Qt::NoModifier);
        f->controller->tileClicked(QString(), static_cast<int>(Qt::RightButton), Qt::NoModifier);
        f->controller->tileHeld(QString());

        QVERIFY(f->launcher.launched().isEmpty());
        QVERIFY(f->tasks.activated().isEmpty());
        QCOMPARE(menu.count(), 0);
    }

    void unresolvableIdIsSurvivable()
    {
        auto f = makeFixture();

        // Every route, against an id no backend knows. None may crash.
        for (int modifiers : {static_cast<int>(Qt::NoModifier),
                              static_cast<int>(Qt::MetaModifier),
                              static_cast<int>(Qt::AltModifier),
                              static_cast<int>(Qt::MetaModifier | Qt::AltModifier)}) {
            f->controller->tileClicked(QStringLiteral("no-such-app"), left(), modifiers);
        }
        f->controller->tileClicked(QStringLiteral("no-such-app"), static_cast<int>(Qt::MiddleButton), Qt::NoModifier);
        f->controller->tileHeld(QStringLiteral("no-such-app"));

        QVERIFY(f->launcher.launched().isEmpty());
        QVERIFY(f->launcher.revealed().isEmpty());
    }

    /// Focus moves to \a appId, the way the compositor would report it, and the
    /// controller is told the window list changed. Exactly one window becomes
    /// active, as on a real desktop; an empty id means focus left every window.
    void focus(Fixture &f, const QString &appId)
    {
        std::vector<WindowInfo> windows = f.tasks.windows();
        bool done = false;
        for (WindowInfo &window : windows) {
            window.isActive = !done && !appId.isEmpty() && window.appId == appId;
            done = done || window.isActive;
        }
        f.tasks.setWindows(windows);
        f.controller->windowsChanged();
    }

    void altClickHidesTheAppBeingLeft()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("editor"));
        focus(*f, QStringLiteral("player"));
        f->tasks.clearRecords();

        // Player is in front, so switching away from it with alt held hides
        // player — the app being left, not the one before that.
        f->controller->tileClicked(QStringLiteral("editor"), left(), Qt::AltModifier);
        QCOMPARE(f->tasks.minimized(), QStringList({QStringLiteral("p1")}));
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("e1")}));
    }

    void hidingThePreviousAppTakesAllOfItsWindows()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("editor"));
        f->tasks.clearRecords();

        // Editor has two windows and both must go, not just the one that had
        // focus.
        f->controller->tileClicked(QStringLiteral("player"), left(), Qt::AltModifier);
        QCOMPARE(f->tasks.minimized(), QStringList({QStringLiteral("e1"), QStringLiteral("e2")}));
    }

    void trackingFollowsActivationsTheDockDidNotCause()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        // The user alt-tabbed rather than clicking a tile. The dock still has to
        // know what is in front, which is why tracking hangs off the window list
        // and not off tileClicked().
        focus(*f, QStringLiteral("player"));
        f->tasks.clearRecords();

        f->controller->tileClicked(QStringLiteral("idle"), left(), Qt::AltModifier);
        QCOMPARE(f->tasks.minimized(), QStringList({QStringLiteral("p1")}));
    }

    void repeatedNotificationsDoNotShiftTheHistory()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("player"));
        // The window list changes for all sorts of reasons that are not a focus
        // change; none of them may disturb what the dock thinks is in front.
        f->controller->windowsChanged();
        f->controller->windowsChanged();
        f->tasks.clearRecords();

        f->controller->tileClicked(QStringLiteral("idle"), left(), Qt::AltModifier);
        QCOMPARE(f->tasks.minimized(), QStringList({QStringLiteral("p1")}));
    }

    void focusOnTheDesktopKeepsTheLastApp()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("player"));
        // Clicking the wallpaper leaves nothing active. That is the absence of an
        // active application, not a new one, and the user still has somewhere to
        // go back to.
        focus(*f, QString());
        f->tasks.clearRecords();

        f->controller->tileClicked(QStringLiteral("idle"), left(), Qt::AltModifier);
        QCOMPARE(f->tasks.minimized(), QStringList({QStringLiteral("p1")}));
    }

    void previousAppIsForgottenWhenItQuits()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("editor"));

        // Editor quits, leaving nothing in front.
        f->tasks.removeWindow(QStringLiteral("e1"));
        f->tasks.removeWindow(QStringLiteral("e2"));
        f->model->rebuild();
        f->controller->windowsChanged();
        f->tasks.clearRecords();

        // There is nothing left to hide, so alt-click is just an activate.
        f->controller->tileClicked(QStringLiteral("player"), left(), Qt::AltModifier);
        QVERIFY(f->tasks.minimized().isEmpty());
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("p1")}));
    }

    void altClickingTheAppAlreadyInFrontHidesNothing()
    {
        auto f = makeFixture();
        addRunningWindows(*f);

        focus(*f, QStringLiteral("editor"));
        f->tasks.clearRecords();

        // Hiding the app and then activating it again would be a visible flicker
        // for no reason.
        f->controller->tileClicked(QStringLiteral("editor"), left(), Qt::AltModifier);
        QVERIFY(f->tasks.minimized().isEmpty());
        // e1 has focus, so cycling moves to the app's other window.
        QCOMPARE(f->tasks.activated(), QStringList({QStringLiteral("e2")}));
    }

    void routingWorksWithoutATaskBackend()
    {
        // The model and controller both accept a null task backend, and the
        // launch path has to keep working when there is one.
        FakeLauncherBackend launcher;
        launcher.addEntry(QStringLiteral("idle"), QStringLiteral("Idle"), QStringLiteral("idle-icon"));

        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        config.setPinnedEntries({QStringLiteral("idle")});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        controller.tileClicked(QStringLiteral("idle"), left(), Qt::NoModifier);
        controller.tileClicked(QStringLiteral("idle"), left(), Qt::AltModifier);
        controller.tileClicked(QStringLiteral("idle"), left(), Qt::MetaModifier | Qt::AltModifier);

        // Nothing is running, so all three reduce to a launch.
        QCOMPARE(launcher.launched().count(), 3);
    }
};

QTEST_MAIN(TestCommandRouting)
#include "test_commandrouting.moc"
