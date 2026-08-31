#include <QQmlEngine>
#include <QStandardPaths>
#include <QtQuickTest>

#include "dragsimulator.h"

/// Points the FrappeConfig singleton at the test tree before any QML touches it,
/// so running the suite cannot overwrite the developer's real settings.
class Setup : public QObject
{
    Q_OBJECT

public:
    Setup()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

public Q_SLOTS:
    /// Exposes the synthetic drag source to the QML tests. Registered here
    /// rather than in a QML module because it exists only for the suite.
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        qmlRegisterSingletonInstance("org.kde.frappedock.test", 1, 0, "DragSimulator",
                                     new DragSimulator(engine));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml, Setup)

#include "tst_qml.moc"
