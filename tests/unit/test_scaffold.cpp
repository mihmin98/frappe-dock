#include <QTest>

/// The canary for the test harness itself: if this stops running, ctest is
/// finding nothing and every other green result is meaningless.
class TestScaffold : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void trivial()
    {
        QCOMPARE(1 + 1, 2);
    }
};

QTEST_MAIN(TestScaffold)
#include "test_scaffold.moc"
