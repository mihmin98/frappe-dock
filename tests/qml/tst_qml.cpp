#include <QQmlEngine>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQuickTest>

#include "core/model/stacksettings.h"

#include "dragsimulator.h"
#include "stackfixture.h"

/// Points the FrappeConfig singleton at the test tree before any QML touches it,
/// so running the suite cannot overwrite the developer's real settings.
class Setup : public QObject
{
    Q_OBJECT

public:
    Setup()
    {
        QStandardPaths::setTestModeEnabled(true);

        // The stack preferences are process-wide and persist. Without a fresh
        // file per run, a mode or sort order written by one run decides what the
        // next one reads, and the suite passes or fails on its own history.
        if (m_dir.isValid()) {
            frappe::StackSettings::instance()->redirectTo(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        }
    }

    QTemporaryDir m_dir;

public Q_SLOTS:
    /// Exposes the synthetic drag source to the QML tests. Registered here
    /// rather than in a QML module because it exists only for the suite.
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        qmlRegisterSingletonInstance("org.kde.frappedock.test", 1, 0, "DragSimulator",
                                     new DragSimulator(engine));
        qmlRegisterSingletonInstance("org.kde.frappedock.test", 1, 0, "StackFixture",
                                     new StackFixture(engine));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml, Setup)

#include "tst_qml.moc"
