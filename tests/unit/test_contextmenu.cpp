#include "core/model/contextmenu.h"

#include <QTest>

using namespace frappe;

Q_DECLARE_METATYPE(std::vector<MenuItem>)

/// Menu assembly across the states a tile can be in.
///
/// The point of the menu living in core is that this file can enumerate the
/// combinations directly, with no backends and no compositor.
class TestContextMenu : public QObject
{
    Q_OBJECT

private:
    static WindowInfo window(const QString &id, const QString &title)
    {
        WindowInfo info;
        info.windowId = id;
        info.appId = QStringLiteral("editor");
        info.title = title;
        return info;
    }

    static MenuContext base()
    {
        MenuContext context;
        context.tileId = QStringLiteral("editor");
        context.appName = QStringLiteral("Editor");
        return context;
    }

    static bool has(const std::vector<MenuItem> &items, MenuItemKind kind)
    {
        return std::any_of(items.begin(), items.end(), [kind](const MenuItem &item) {
            return item.kind == kind;
        });
    }

    static int count(const std::vector<MenuItem> &items, MenuItemKind kind)
    {
        return static_cast<int>(std::count_if(items.begin(), items.end(), [kind](const MenuItem &item) {
            return item.kind == kind;
        }));
    }

    /// Fails the test if the menu is malformed in a way no individual case
    /// should have to restate: separators at the edges or doubled, an empty
    /// label on a real row, or both Pin and Unpin at once.
    static void checkWellFormed(const std::vector<MenuItem> &items)
    {
        QVERIFY(!items.empty());
        QVERIFY(items.front().kind != MenuItemKind::Separator);
        QVERIFY(items.back().kind != MenuItemKind::Separator);

        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].kind == MenuItemKind::Separator) {
                QVERIFY(i + 1 < items.size());
                QVERIFY(items[i + 1].kind != MenuItemKind::Separator);
            } else {
                QVERIFY(!items[i].label.isEmpty());
            }
        }

        QVERIFY(!(has(items, MenuItemKind::Pin) && has(items, MenuItemKind::Unpin)));
    }

private Q_SLOTS:
    void menuForState_data()
    {
        QTest::addColumn<bool>("pinned");
        QTest::addColumn<int>("windowCount");

        // Every combination of pinned x running, and for the running ones both
        // one window and several. "Not running with several windows" is not a
        // state that exists, so it is not a row.
        QTest::newRow("pinned, not running") << true << 0;
        QTest::newRow("pinned, one window") << true << 1;
        QTest::newRow("pinned, three windows") << true << 3;
        QTest::newRow("unpinned, not running") << false << 0;
        QTest::newRow("unpinned, one window") << false << 1;
        QTest::newRow("unpinned, three windows") << false << 3;
    }

    void menuForState()
    {
        QFETCH(bool, pinned);
        QFETCH(int, windowCount);

        MenuContext context = base();
        context.isPinned = pinned;
        for (int i = 0; i < windowCount; ++i) {
            context.windows.push_back(window(QStringLiteral("w%1").arg(i),
                                             QStringLiteral("Document %1").arg(i)));
        }

        const std::vector<MenuItem> items = buildContextMenu(context);
        checkWellFormed(items);

        // Pin state decides which of the two mutually exclusive rows appears.
        QCOMPARE(has(items, MenuItemKind::Pin), !pinned);
        QCOMPARE(has(items, MenuItemKind::Unpin), pinned);

        // Always available, whatever the state.
        QVERIFY(has(items, MenuItemKind::LaunchAtLogin));
        QVERIFY(has(items, MenuItemKind::ShowInFileManager));

        // Quit only means something while there is something to quit.
        QCOMPARE(has(items, MenuItemKind::Quit), windowCount > 0);

        // Windows are listed only when there is a choice to make.
        QCOMPARE(count(items, MenuItemKind::Window), windowCount > 1 ? windowCount : 0);
    }

    void windowRowsCarryTitlesAndIdsInOrder()
    {
        MenuContext context = base();
        context.windows = {window(QStringLiteral("w1"), QStringLiteral("notes.txt")),
                           window(QStringLiteral("w2"), QStringLiteral("draft.txt"))};

        const std::vector<MenuItem> items = buildContextMenu(context);

        // Backend order preserved, and each row carries the id needed to raise
        // that specific window.
        QCOMPARE(items.at(0).kind, MenuItemKind::Window);
        QCOMPARE(items.at(0).id, QStringLiteral("w1"));
        QCOMPARE(items.at(0).label, QStringLiteral("notes.txt"));
        QCOMPARE(items.at(1).id, QStringLiteral("w2"));
        QCOMPARE(items.at(1).label, QStringLiteral("draft.txt"));
    }

    void untitledWindowFallsBackToTheApplicationName()
    {
        MenuContext context = base();
        context.windows = {window(QStringLiteral("w1"), QString()),
                           window(QStringLiteral("w2"), QStringLiteral("draft.txt"))};

        const std::vector<MenuItem> items = buildContextMenu(context);

        // A blank row would be unclickable in practice and unreadable in any case.
        QCOMPARE(items.at(0).label, QStringLiteral("Editor"));
        checkWellFormed(items);
    }

    void desktopActionsAppearInTheEntrysOwnOrder()
    {
        MenuContext context = base();
        context.actions = {{QStringLiteral("new-window"), QStringLiteral("New Window")},
                           {QStringLiteral("new-private"), QStringLiteral("New Private Window")}};

        const std::vector<MenuItem> items = buildContextMenu(context);
        checkWellFormed(items);

        QCOMPARE(count(items, MenuItemKind::Action), 2);
        QCOMPARE(items.at(0).id, QStringLiteral("new-window"));
        QCOMPARE(items.at(1).id, QStringLiteral("new-private"));
    }

    void actionsAndWindowsAreSeparateGroups()
    {
        MenuContext context = base();
        context.windows = {window(QStringLiteral("w1"), QStringLiteral("one")),
                           window(QStringLiteral("w2"), QStringLiteral("two"))};
        context.actions = {{QStringLiteral("new-window"), QStringLiteral("New Window")}};

        const std::vector<MenuItem> items = buildContextMenu(context);
        checkWellFormed(items);

        // Windows first, then the application's own actions, then dock options,
        // then quitting: four groups, so three separators.
        QCOMPARE(count(items, MenuItemKind::Separator), 3);
    }

    void noActionsMeansNoEmptyGroup()
    {
        MenuContext context = base();

        const std::vector<MenuItem> items = buildContextMenu(context);
        checkWellFormed(items);

        // Not running, no windows, no actions: only the options group, so no
        // separator at all.
        QCOMPARE(count(items, MenuItemKind::Separator), 0);
        QCOMPARE(count(items, MenuItemKind::Action), 0);
    }

    void launchAtLoginIsCheckableAndReflectsTheCurrentState()
    {
        MenuContext off = base();
        MenuContext on = base();
        on.launchesAtLogin = true;

        const auto find = [](const std::vector<MenuItem> &items) {
            return *std::find_if(items.begin(), items.end(), [](const MenuItem &item) {
                return item.kind == MenuItemKind::LaunchAtLogin;
            });
        };

        const MenuItem offItem = find(buildContextMenu(off));
        const MenuItem onItem = find(buildContextMenu(on));

        QVERIFY(offItem.checkable);
        QVERIFY(!offItem.checked);
        QVERIFY(onItem.checkable);
        QVERIFY(onItem.checked);
    }

    void forceQuitOnlyAppearsWhenTheApplicationStopsAnswering()
    {
        MenuContext responsive = base();
        responsive.windows = {window(QStringLiteral("w1"), QStringLiteral("one"))};

        MenuContext stuck = responsive;
        stuck.isResponsive = false;

        // The destructive option must not sit under the pointer every time.
        QVERIFY(!has(buildContextMenu(responsive), MenuItemKind::ForceQuit));
        QVERIFY(has(buildContextMenu(stuck), MenuItemKind::ForceQuit));

        // And it accompanies Quit rather than replacing it.
        QVERIFY(has(buildContextMenu(stuck), MenuItemKind::Quit));
    }

    void unresponsiveButNotRunningOffersNeither()
    {
        MenuContext context = base();
        context.isResponsive = false;

        // No windows means nothing to quit, however unresponsive it is said to be.
        const std::vector<MenuItem> items = buildContextMenu(context);
        QVERIFY(!has(items, MenuItemKind::Quit));
        QVERIFY(!has(items, MenuItemKind::ForceQuit));
    }

    void assemblyIsDeterministic()
    {
        MenuContext context = base();
        context.isPinned = true;
        context.windows = {window(QStringLiteral("w1"), QStringLiteral("one")),
                           window(QStringLiteral("w2"), QStringLiteral("two"))};
        context.actions = {{QStringLiteral("a"), QStringLiteral("A")}};

        // Same context in, same rows out — the property the whole design rests on.
        QCOMPARE(buildContextMenu(context), buildContextMenu(context));
    }
};

QTEST_MAIN(TestContextMenu)
#include "test_contextmenu.moc"
