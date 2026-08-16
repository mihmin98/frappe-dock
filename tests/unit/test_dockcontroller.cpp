#include "app/dockcontroller.h"

#include "core/config/configfacade.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// The controller is the join between a click and a process starting, so what
/// matters is that the id survives the trip intact and that a bad one is
/// survivable.
class TestDockController : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void launchesTheClickedEntry()
    {
        FakeLauncherBackend launcher;
        launcher.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));
        launcher.addEntry(QStringLiteral("beta"), QStringLiteral("Beta"), QStringLiteral("beta-icon"));

        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        config.setPinnedEntries({QStringLiteral("alpha"), QStringLiteral("beta")});

        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        controller.activateTile(QStringLiteral("beta"));
        QCOMPARE(launcher.launched(), QStringList({QStringLiteral("beta")}));
    }

    void unknownEntryIsSurvivable()
    {
        FakeLauncherBackend launcher;
        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        // A dock must degrade rather than die: an application uninstalled since
        // it was pinned is an everyday event.
        controller.activateTile(QStringLiteral("no-such-app"));
        controller.activateTile(QString());
        QCOMPARE(launcher.launched(), QStringList());
    }
};

QTEST_MAIN(TestDockController)
#include "test_dockcontroller.moc"
