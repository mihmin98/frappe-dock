#include "app/dockcontroller.h"
#include "core/config/configfacade.h"
#include "core/model/regiondrop.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace frappe;

/// Which items may be dropped into which region, and what the dock says when
/// they may not.
///
/// The matrix is the point: every item kind against every region, so a rule
/// added later cannot quietly widen one of the cells. The refusal *reasons* are
/// tested alongside it, because a rejection the user cannot understand is the
/// papercut §4.4 exists to correct — an accepted/rejected boolean would pass
/// this suite while failing the design.
class TestDropRules : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QUrl application() const
    {
        return QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("alpha.desktop")));
    }

    QUrl file() const
    {
        return QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("notes.txt")));
    }

    QUrl folder() const
    {
        return QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("papers")));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());

        // The classifier asks the filesystem whether a local URL is a
        // directory, so these have to exist to be what they claim.
        QVERIFY(QDir(m_dir.path()).mkdir(QStringLiteral("papers")));
        for (const QString &name : {QStringLiteral("alpha.desktop"), QStringLiteral("notes.txt")}) {
            QFile made(m_dir.filePath(name));
            QVERIFY(made.open(QIODevice::WriteOnly));
        }
    }

    // --- Classification ------------------------------------------------

    void payloadsAreClassified()
    {
        QCOMPARE(classifyDroppedUrl(application()), DropItemKind::Application);
        QCOMPARE(classifyDroppedUrl(file()), DropItemKind::File);
        QCOMPARE(classifyDroppedUrl(folder()), DropItemKind::Folder);
        QCOMPARE(classifyDroppedUrl(QUrl()), DropItemKind::Unknown);

        // Remote: a file, decided without asking the network.
        QCOMPARE(classifyDroppedUrl(QUrl(QStringLiteral("sftp://host/data/report"))), DropItemKind::File);
        // A desktop entry is one wherever it lives, including remotely.
        QCOMPARE(classifyDroppedUrl(QUrl(QStringLiteral("sftp://host/apps/beta.desktop"))),
                 DropItemKind::Application);
    }

    /// Files and folders share a region, so a selection of both is one payload.
    void filesAndFoldersAreOnePayload()
    {
        QCOMPARE(classifyPayload({file(), folder()}), DropItemKind::File);
        QCOMPARE(classifyPayload({folder(), folder()}), DropItemKind::Folder);
        QCOMPARE(classifyPayload({application(), application()}), DropItemKind::Application);
    }

    // --- The matrix ----------------------------------------------------

    void validityMatrix_data()
    {
        QTest::addColumn<int>("kind"); // DropItemKind
        QTest::addColumn<int>("region");
        QTest::addColumn<bool>("accepted");

        const std::pair<const char *, DropItemKind> kinds[] = {
            {"app", DropItemKind::Application},
            {"file", DropItemKind::File},
            {"folder", DropItemKind::Folder},
        };
        const std::pair<const char *, Region> regions[] = {
            {"head", Region::Head},
            {"pinned", Region::Pinned},
            {"recent", Region::Recent},
            {"files", Region::Files},
            {"minimized", Region::Minimized},
            {"tail", Region::Tail},
        };

        for (const auto &[kindName, kind] : kinds) {
            for (const auto &[regionName, region] : regions) {
                const Region home = kind == DropItemKind::Application ? Region::Pinned : Region::Files;
                QTest::addRow("%s in %s", kindName, regionName)
                    << static_cast<int>(kind) << static_cast<int>(region) << (region == home);
            }
        }
    }

    void validityMatrix()
    {
        QFETCH(int, kind);
        QFETCH(int, region);
        QFETCH(bool, accepted);

        QList<QUrl> payload;
        switch (static_cast<DropItemKind>(kind)) {
        case DropItemKind::Application:
            payload = {application()};
            break;
        case DropItemKind::File:
            payload = {file()};
            break;
        case DropItemKind::Folder:
            payload = {folder()};
            break;
        case DropItemKind::Unknown:
            break;
        }

        const RegionDropVerdict verdict = evaluateRegionDrop(payload, static_cast<Region>(region));
        QCOMPARE(verdict.accepted(), accepted);
        QCOMPARE(verdict.kind, static_cast<DropItemKind>(kind));

        if (!accepted) {
            // Not merely refused: refused for a reason that names where the
            // payload does belong, so the view can say it.
            QCOMPARE(verdict.rejection, RegionDropRejection::WrongRegion);
            QCOMPARE(verdict.expectedRegion,
                     static_cast<DropItemKind>(kind) == DropItemKind::Application ? Region::Pinned
                                                                                  : Region::Files);
        }
    }

    // --- The named cases the plan calls out ----------------------------

    void appInAppRegionIsAccepted()
    {
        QVERIFY(evaluateRegionDrop({application()}, Region::Pinned).accepted());
    }

    void appInFileRegionIsRejected()
    {
        const RegionDropVerdict verdict = evaluateRegionDrop({application()}, Region::Files);
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, RegionDropRejection::WrongRegion);
        QCOMPARE(verdict.expectedRegion, Region::Pinned);
    }

    void fileInFileRegionIsAccepted()
    {
        QVERIFY(evaluateRegionDrop({file()}, Region::Files).accepted());
    }

    void fileInAppRegionIsRejected()
    {
        const RegionDropVerdict verdict = evaluateRegionDrop({file()}, Region::Pinned);
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, RegionDropRejection::WrongRegion);
        QCOMPARE(verdict.expectedRegion, Region::Files);
    }

    void folderInFileRegionIsAccepted()
    {
        QVERIFY(evaluateRegionDrop({folder()}, Region::Files).accepted());
    }

    void folderInAppRegionIsRejected()
    {
        const RegionDropVerdict verdict = evaluateRegionDrop({folder()}, Region::Pinned);
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, RegionDropRejection::WrongRegion);
        QCOMPARE(verdict.expectedRegion, Region::Files);
    }

    // --- Payloads that are not one thing -------------------------------

    void mixedPayloadIsItsOwnRefusal()
    {
        const RegionDropVerdict verdict = evaluateRegionDrop({application(), file()}, Region::Pinned);
        QVERIFY(!verdict.accepted());
        // Distinct from WrongRegion: there is no right region for it, so
        // "it belongs elsewhere" would be a lie.
        QCOMPARE(verdict.rejection, RegionDropRejection::MixedPayload);
    }

    void emptyPayloadIsRefusedAsUnknown()
    {
        const RegionDropVerdict verdict = evaluateRegionDrop({}, Region::Pinned);
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, RegionDropRejection::UnknownItem);
    }

    // --- Through the controller ----------------------------------------

    void droppedApplicationIsPinned()
    {
        FakeLauncherBackend launcher;
        launcher.addEntry(QStringLiteral("alpha.desktop"), QStringLiteral("Alpha"), QStringLiteral("icon"));

        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        config.setPinnedEntries({});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        QVERIFY(controller.acceptRegionDrop(int(Region::Pinned), {application()}));
        QCOMPARE(config.pinnedEntries(), QStringList({QStringLiteral("alpha.desktop")}));
        QCOMPARE(model.rowCount(), 1);

        // Dropping it a second time is a no-op, not a duplicate tile.
        QVERIFY(!controller.acceptRegionDrop(int(Region::Pinned), {application()}));
        QCOMPARE(model.rowCount(), 1);
    }

    void droppedEntryThatIsNotInstalledIsRefusedWithItsName()
    {
        FakeLauncherBackend launcher;

        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        config.setPinnedEntries({});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        const QVariantMap verdict = controller.evaluateRegionDrop(int(Region::Pinned), {application()});
        QVERIFY(!verdict.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(verdict.value(QStringLiteral("rejection")).toInt(),
                 int(RegionDropRejection::NotInstalled));
        // The message needs something to name.
        QCOMPARE(verdict.value(QStringLiteral("detail")).toString(), QStringLiteral("alpha.desktop"));

        QVERIFY(!controller.acceptRegionDrop(int(Region::Pinned), {application()}));
        QVERIFY(config.pinnedEntries().isEmpty());
    }

    void controllerRefusesWhatTheRulesRefuse()
    {
        FakeLauncherBackend launcher;
        launcher.addEntry(QStringLiteral("alpha.desktop"), QStringLiteral("Alpha"), QStringLiteral("icon"));

        ConfigFacade config(m_dir.filePath(QStringLiteral("frappe-dockrc")));
        config.setPinnedEntries({});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        // A file aimed at the applications, and an application aimed at the
        // files: neither may take effect, however it reached the controller.
        QVERIFY(!controller.acceptRegionDrop(int(Region::Pinned), {file()}));
        QVERIFY(!controller.acceptRegionDrop(int(Region::Files), {application()}));
        QVERIFY(config.pinnedEntries().isEmpty());
    }
};

QTEST_MAIN(TestDropRules)
#include "test_droprules.moc"
