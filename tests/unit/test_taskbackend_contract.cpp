#include "fakes/faketaskbackend.h"

#include <QTest>

using namespace frappe;

/// The semantics every ITaskBackend implementation owes its callers.
///
/// This is not a test of the window-management library — that needs a live
/// compositor and lives in tests/manual/. It pins down the contract itself, run
/// here against FakeTaskBackend, so that a second implementation has something
/// to be measured against and so the fake cannot quietly drift from the real
/// one's semantics.
class TestTaskBackendContract : public QObject
{
    Q_OBJECT

private:
    /// Two windows of one app plus one of another: enough to tell "same app"
    /// from "same window" apart in every assertion below.
    static std::vector<WindowInfo> sampleWindows()
    {
        WindowInfo editorMain;
        editorMain.windowId = QStringLiteral("w1");
        editorMain.appId = QStringLiteral("org.example.editor");
        editorMain.title = QStringLiteral("notes.txt");
        editorMain.isActive = true;

        WindowInfo editorSecond;
        editorSecond.windowId = QStringLiteral("w2");
        editorSecond.appId = QStringLiteral("org.example.editor");
        editorSecond.title = QStringLiteral("draft.txt");
        editorSecond.isMinimized = true;

        WindowInfo terminal;
        terminal.windowId = QStringLiteral("w3");
        terminal.appId = QStringLiteral("org.example.terminal");
        terminal.title = QStringLiteral("~");

        return {editorMain, editorSecond, terminal};
    }

private Q_SLOTS:
    void activatingUnknownIdIsNoOp()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.activate(QStringLiteral("nosuchwindow"));

        QCOMPARE(backend.windows().size(), size_t{3});
        QCOMPARE(backend.windows().at(0).isActive, true);
        QCOMPARE(backend.windows().at(2).isActive, false);
    }

    void minimizingUnknownIdIsNoOp()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.minimize(QStringLiteral("nosuchwindow"));

        QCOMPARE(backend.windows().size(), size_t{3});
        QCOMPARE(backend.windows().at(0).isMinimized, false);
        QCOMPARE(backend.windows().at(1).isMinimized, true);
    }

    void closingUnknownIdIsNoOp()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.close(QStringLiteral("nosuchwindow"));

        QCOMPARE(backend.windows().size(), size_t{3});
    }

    void windowsIsStableBetweenChanges()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        const std::vector<WindowInfo> first = backend.windows();
        const std::vector<WindowInfo> second = backend.windows();

        QCOMPARE(first.size(), second.size());
        for (size_t i = 0; i < first.size(); ++i) {
            // Ids above all: core diffs on them, so an implementation that
            // renumbered between queries would make every window look new.
            QCOMPARE(second.at(i).windowId, first.at(i).windowId);
            QCOMPARE(second.at(i).appId, first.at(i).appId);
            QCOMPARE(second.at(i).title, first.at(i).title);
            QCOMPARE(second.at(i).isMinimized, first.at(i).isMinimized);
            QCOMPARE(second.at(i).isActive, first.at(i).isActive);
        }
    }

    void windowIdsSurviveListChanges()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.removeWindow(QStringLiteral("w1"));

        // Removing a window must not renumber the survivors; the dock holds ids
        // across a change and uses them to route the next command.
        const std::vector<WindowInfo> remaining = backend.windows();
        QCOMPARE(remaining.size(), size_t{2});
        QCOMPARE(remaining.at(0).windowId, QStringLiteral("w2"));
        QCOMPARE(remaining.at(1).windowId, QStringLiteral("w3"));
    }

    void changeCallbackInvokedOnNotifyChange()
    {
        FakeTaskBackend backend;
        int calls = 0;
        backend.setChangeCallback([&calls] {
            ++calls;
        });

        backend.notifyChange();
        QCOMPARE(calls, 1);

        backend.notifyChange();
        QCOMPARE(calls, 2);
    }

    void changeNotificationWithoutCallbackIsHarmless()
    {
        FakeTaskBackend backend;
        backend.notifyChange();
        QCOMPARE(backend.windows().size(), size_t{0});
    }

    void changeCallbackIsReplaceable()
    {
        FakeTaskBackend backend;
        int first = 0;
        int second = 0;

        backend.setChangeCallback([&first] {
            ++first;
        });
        backend.setChangeCallback([&second] {
            ++second;
        });
        backend.notifyChange();

        // One channel, not a signal with many receivers: the second registration
        // replaces the first.
        QCOMPARE(first, 0);
        QCOMPARE(second, 1);
    }

    void recordedCallsTrackAllMethods()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.activate(QStringLiteral("w1"));
        backend.minimize(QStringLiteral("w2"));
        backend.close(QStringLiteral("w3"));
        backend.hideOthers(QStringLiteral("org.example.editor"));

        QCOMPARE(backend.activated(), QStringList({QStringLiteral("w1")}));
        QCOMPARE(backend.minimized(), QStringList({QStringLiteral("w2")}));
        QCOMPARE(backend.closed(), QStringList({QStringLiteral("w3")}));
        QCOMPARE(backend.hideOthersCalls(), QStringList({QStringLiteral("org.example.editor")}));
    }

    void recordedCallsPreserveOrderAndRepeats()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.activate(QStringLiteral("w3"));
        backend.activate(QStringLiteral("w1"));
        backend.activate(QStringLiteral("w3"));

        QCOMPARE(backend.activated(),
                 QStringList({QStringLiteral("w3"), QStringLiteral("w1"), QStringLiteral("w3")}));

        backend.clearRecords();
        QVERIFY(backend.activated().isEmpty());
        // Clearing the log is not undoing the commands.
        QCOMPARE(backend.windows().size(), size_t{3});
    }

    void controlMethodsDoNotMutateTheWindowList()
    {
        FakeTaskBackend backend;
        backend.setWindows(sampleWindows());

        backend.minimize(QStringLiteral("w1"));
        backend.close(QStringLiteral("w3"));

        // A command is a request to the compositor, not an edit of local state.
        // The list only changes when the backend reports it changed.
        QCOMPARE(backend.windows().size(), size_t{3});
        QCOMPARE(backend.windows().at(0).isMinimized, false);
    }
};

QTEST_MAIN(TestTaskBackendContract)
#include "test_taskbackend_contract.moc"
