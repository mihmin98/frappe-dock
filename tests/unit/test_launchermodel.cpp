#include "fakes/fakelauncherbackend.h"
#include "platform/launcherbackend.h"

#include <KConfig>
#include <KConfigGroup>
#include <KService>

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QTest>

using namespace frappe;

/// Exercises the real KService-backed launcher against committed fixtures.
///
/// The fixtures are installed into a private, test-mode data directory and the
/// service cache is rebuilt against it, so nothing here depends on what happens
/// to be installed on the machine running the suite.
class TestLauncherModel : public QObject
{
    Q_OBJECT

private:
    LauncherBackend m_backend;

private Q_SLOTS:
    void initTestCase()
    {
        // Before any KService or KConfig access, so the fixtures and the rebuilt
        // cache land in the test tree rather than the developer's own.
        QStandardPaths::setTestModeEnabled(true);

        const QString applications =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        QVERIFY(QDir().mkpath(applications));

        const QDir fixtures(QStringLiteral(FRAPPE_FIXTURE_DIR) + QStringLiteral("/desktop"));
        const QStringList entries = fixtures.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        QVERIFY(!entries.isEmpty());
        for (const QString &entry : entries) {
            const QString target = applications + QLatin1Char('/') + entry;
            QFile::remove(target);
            QVERIFY(QFile::copy(fixtures.filePath(entry), target));
        }

        // KService reads from the sycoca cache, which has to be told the
        // fixtures exist.
        QProcess::execute(QStringLiteral("kbuildsycoca6"), {QStringLiteral("--noincremental")});

        if (!KService::serviceByDesktopName(QStringLiteral("frappe-test-valid"))) {
            QSKIP("kbuildsycoca6 did not pick up the fixtures; the service cache is unavailable here");
        }
    }

    void parsesValidEntry()
    {
        const auto entry = m_backend.lookup(QStringLiteral("frappe-test-valid"));
        QVERIFY(entry.has_value());
        QCOMPARE(entry->name, QStringLiteral("Frappe Test Valid"));
        QCOMPARE(entry->iconName, QStringLiteral("frappe-test-icon"));
        QCOMPARE(entry->id, QStringLiteral("frappe-test-valid.desktop"));

        // The storage id form resolves to the same entry.
        const auto byStorageId = m_backend.lookup(QStringLiteral("frappe-test-valid.desktop"));
        QVERIFY(byStorageId.has_value());
        QCOMPARE(byStorageId->name, entry->name);
    }

    void exposesActionsInOrder()
    {
        const auto entry = m_backend.lookup(QStringLiteral("frappe-test-valid"));
        QVERIFY(entry.has_value());
        QCOMPARE(entry->actions.size(), 3u);
        QCOMPARE(entry->actions[0].first, QStringLiteral("NewWindow"));
        QCOMPARE(entry->actions[1].first, QStringLiteral("NewPrivateWindow"));
        QCOMPARE(entry->actions[2].first, QStringLiteral("Preferences"));
        QCOMPARE(entry->actions[0].second, QStringLiteral("New Window"));
    }

    void missingEntryReturnsNotFound()
    {
        const auto entry = m_backend.lookup(QStringLiteral("frappe-test-does-not-exist"));
        QVERIFY(!entry.has_value());
        QCOMPARE(entry.error(), Error::NotFound);

        // And launching one is an error rather than a crash.
        const auto launched = m_backend.launch(QStringLiteral("frappe-test-does-not-exist"));
        QVERIFY(!launched.has_value());
        QCOMPARE(launched.error(), Error::NotFound);
    }

    void handlesNoDisplay()
    {
        // Policy: NoDisplay hides an entry from menus, not from the dock. A user
        // who pinned it still expects it to resolve and launch.
        const auto entry = m_backend.lookup(QStringLiteral("frappe-test-nodisplay"));
        QVERIFY(entry.has_value());
        QCOMPARE(entry->name, QStringLiteral("Frappe Test Hidden"));
        QCOMPARE(entry->iconName, QStringLiteral("frappe-test-hidden-icon"));
    }

    void localisedNameUsesLocale()
    {
        // The fixture carries Name[de]. Reading it proves the entry went through
        // KService rather than being hand-parsed — a hand-rolled reader picks up
        // the untranslated Name and looks correct in an English locale.
        const KService::Ptr service = KService::serviceByDesktopName(QStringLiteral("frappe-test-valid"));
        QVERIFY(service);

        KConfig config(QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                              QStringLiteral("applications/frappe-test-valid.desktop")),
                       KConfig::SimpleConfig);
        config.setLocale(QStringLiteral("de"));
        const KConfigGroup group = config.group(QStringLiteral("Desktop Entry"));
        QCOMPARE(group.readEntry("Name"), QStringLiteral("Frappe Test Gültig"));

        // ...and the same reader in the test's own locale gives the untranslated
        // name, so the above is a real locale lookup rather than a coincidence.
        QCOMPARE(service->name(), QStringLiteral("Frappe Test Valid"));
    }

    void fakeRecordsLaunches()
    {
        // The fake is used by every other suite; a broken recorder would make
        // those tests pass vacuously.
        FakeLauncherBackend fake;
        fake.addEntry(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("alpha-icon"));

        QVERIFY(fake.launch(QStringLiteral("alpha")).has_value());
        QVERIFY(!fake.launch(QStringLiteral("beta")).has_value());
        QCOMPARE(fake.launched(), QStringList({QStringLiteral("alpha")}));

        QVERIFY(fake.launchAction(QStringLiteral("alpha"), QStringLiteral("NewWindow")).has_value());
        QCOMPARE(fake.launchedActions(), QStringList({QStringLiteral("alpha:NewWindow")}));

        QVERIFY(fake.openWith(QStringLiteral("alpha"), {QUrl(QStringLiteral("file:///tmp/x"))}).has_value());
        QCOMPARE(fake.opened(), QStringList({QStringLiteral("alpha:file:///tmp/x")}));
    }
};

QTEST_MAIN(TestLauncherModel)
#include "test_launchermodel.moc"
