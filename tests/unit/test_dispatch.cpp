#include "core/input/dispatch.h"

#include <QSet>
#include <QTest>

using namespace frappe;
using namespace frappe::input;

/// The interaction matrix.
///
/// This tests input → Command only; Command → backend is test_commandrouting,
/// and whether the compositor lets the input through at all is
/// tests/manual/03-bindings.md, which no automated test can replace.
class TestDispatch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void matrixRow_data()
    {
        QTest::addColumn<Qt::MouseButton>("button");
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
        QTest::addColumn<Trigger>("trigger");
        QTest::addColumn<Command>("expected");

        // One row per row of the table in plan.md §2.4, in the same order, so
        // the two can be read side by side.
        QTest::newRow("click")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::NoModifier) << Trigger::Click
            << Command::LaunchOrActivate;
        QTest::newRow("meta-click")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::MetaModifier) << Trigger::Click
            << Command::RevealInFileManager;
        QTest::newRow("alt-click")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::AltModifier) << Trigger::Click
            << Command::ActivateAndHidePrevious;
        QTest::newRow("meta-alt-click")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::MetaModifier | Qt::AltModifier) << Trigger::Click
            << Command::ActivateAndHideOthers;
        QTest::newRow("middle-click")
            << Qt::MiddleButton << Qt::KeyboardModifiers(Qt::NoModifier) << Trigger::Click
            << Command::NewInstance;
        QTest::newRow("right-click")
            << Qt::RightButton << Qt::KeyboardModifiers(Qt::NoModifier) << Trigger::Click
            << Command::ShowContextMenu;
        QTest::newRow("press-and-hold")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::NoModifier) << Trigger::PressAndHold
            << Command::ShowJumpList;
    }

    void matrixRow()
    {
        QFETCH(Qt::MouseButton, button);
        QFETCH(Qt::KeyboardModifiers, modifiers);
        QFETCH(Trigger, trigger);
        QFETCH(Command, expected);

        QCOMPARE(dispatch(button, modifiers, trigger), expected);
    }

    void everyBindingIsReachableThroughDispatch()
    {
        // The table is the source of truth for the settings page, so an entry
        // the dispatcher can never return would be documentation of a lie.
        for (const Binding &binding : bindings()) {
            QCOMPARE(dispatch(binding.button, binding.modifiers, binding.trigger), binding.command);
        }
    }

    void everyCommandHasABinding()
    {
        QSet<int> bound;
        for (const Binding &binding : bindings()) {
            bound.insert(static_cast<int>(binding.command));
        }

        // The last enumerator; update alongside Command.
        const int last = static_cast<int>(Command::ShowJumpList);
        for (int command = 0; command <= last; ++command) {
            QVERIFY2(bound.contains(command),
                     qPrintable(QStringLiteral("Command %1 is unreachable: no row produces it").arg(command)));
        }
    }

    void unknownCombinationFallsBackToActivate_data()
    {
        QTest::addColumn<Qt::MouseButton>("button");
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");

        QTest::newRow("shift-click") << Qt::LeftButton << Qt::KeyboardModifiers(Qt::ShiftModifier);
        QTest::newRow("ctrl-click") << Qt::LeftButton << Qt::KeyboardModifiers(Qt::ControlModifier);
        QTest::newRow("shift-meta-click")
            << Qt::LeftButton << Qt::KeyboardModifiers(Qt::ShiftModifier | Qt::MetaModifier);
        QTest::newRow("meta-middle-click") << Qt::MiddleButton << Qt::KeyboardModifiers(Qt::MetaModifier);
        QTest::newRow("alt-right-click") << Qt::RightButton << Qt::KeyboardModifiers(Qt::AltModifier);
        QTest::newRow("back-button") << Qt::BackButton << Qt::KeyboardModifiers(Qt::NoModifier);
        QTest::newRow("no-button") << Qt::NoButton << Qt::KeyboardModifiers(Qt::NoModifier);
    }

    void unknownCombinationFallsBackToActivate()
    {
        QFETCH(Qt::MouseButton, button);
        QFETCH(Qt::KeyboardModifiers, modifiers);

        // Never silently nothing: a tile that ignores a click reads as a broken
        // dock, and launching is the least surprising thing to do instead.
        QCOMPARE(dispatch(button, modifiers), Command::LaunchOrActivate);
    }

    void unknownHoldCombinationAlsoFallsBack()
    {
        QCOMPARE(dispatch(Qt::RightButton, Qt::NoModifier, Trigger::PressAndHold),
                 Command::LaunchOrActivate);
        QCOMPARE(dispatch(Qt::LeftButton, Qt::MetaModifier, Trigger::PressAndHold),
                 Command::LaunchOrActivate);
    }

    void irrelevantModifiersAreIgnored()
    {
        // Keypad state travels with the event on some platforms and must not
        // turn a known combination into an unknown one.
        QCOMPARE(dispatch(Qt::LeftButton, Qt::MetaModifier | Qt::KeypadModifier),
                 Command::RevealInFileManager);
        QCOMPARE(dispatch(Qt::LeftButton, Qt::KeypadModifier), Command::LaunchOrActivate);
    }

    void matchingIsExactNotSubset()
    {
        // Alt+Meta is its own row, not "the Meta row with something extra": if
        // matching were subset-based the two would be indistinguishable and the
        // order of the table would silently decide behaviour.
        QCOMPARE(dispatch(Qt::LeftButton, Qt::MetaModifier | Qt::AltModifier),
                 Command::ActivateAndHideOthers);
        QCOMPARE(dispatch(Qt::LeftButton, Qt::MetaModifier), Command::RevealInFileManager);
        QCOMPARE(dispatch(Qt::LeftButton, Qt::AltModifier), Command::ActivateAndHidePrevious);
    }

    void bindingsHaveUniqueCombinations()
    {
        QSet<QString> seen;
        for (const Binding &binding : bindings()) {
            const QString key = QStringLiteral("%1/%2/%3")
                                    .arg(static_cast<int>(binding.button))
                                    .arg(static_cast<int>(binding.modifiers.toInt()))
                                    .arg(static_cast<int>(binding.trigger));

            // Two rows claiming one input makes the table order load-bearing and
            // makes the settings page show a binding that never fires.
            QVERIFY2(!seen.contains(key),
                     qPrintable(QStringLiteral("Duplicate input combination: %1").arg(key)));
            seen.insert(key);
        }
    }

    void everyBindingHasDescription()
    {
        for (const Binding &binding : bindings()) {
            // The settings page in 7.5 renders these; an empty one is a blank row.
            QVERIFY2(!binding.description.trimmed().isEmpty(),
                     qPrintable(QStringLiteral("Binding for command %1 has no description")
                                    .arg(static_cast<int>(binding.command))));
        }
    }

    void descriptionsAreDistinct()
    {
        QSet<QString> seen;
        for (const Binding &binding : bindings()) {
            // Two rows described identically are indistinguishable to the reader
            // of the settings page, which defeats the point of having it.
            QVERIFY2(!seen.contains(binding.description),
                     qPrintable(QStringLiteral("Duplicate description: %1").arg(binding.description)));
            seen.insert(binding.description);
        }
    }

    void bindingsIsStable()
    {
        // The settings page holds on to this reference, and the dispatcher reads
        // it on every click.
        const std::vector<Binding> &first = bindings();
        const std::vector<Binding> &second = bindings();
        QCOMPARE(&first, &second);
        QVERIFY(!first.empty());
    }
};

QTEST_MAIN(TestDispatch)
#include "test_dispatch.moc"
