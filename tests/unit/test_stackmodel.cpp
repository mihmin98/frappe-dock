#include "core/model/stackmodel.h"
#include "fakes/fakefolderbackend.h"
#include "platform/folderbackend.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

namespace
{

bool writeFile(const QString &path, const QByteArray &contents = "x")
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

QStringList namesOf(const StackModel &model)
{
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.append(model.entryAt(row).name);
    }
    return names;
}

}

/// StackModel against the real KIO-backed FolderBackend and a temp directory,
/// plus the row-reconciliation cases, which need a backend whose listing changes
/// on command rather than whenever the filesystem gets round to it.
class TestStackModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void contentsMatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFile(dir.filePath(QStringLiteral("beta.txt"))));
        QVERIFY(writeFile(dir.filePath(QStringLiteral("alpha.txt"))));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("gamma")));

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(dir.path());

        QTRY_COMPARE(model.rowCount(), 3);
        // Sorted, so the assertion is on an order the view will actually show.
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("alpha.txt"), QStringLiteral("beta.txt"), QStringLiteral("gamma")}));
        QCOMPARE(model.status(), int(StackModel::Ready));

        const FolderEntry &gamma = model.entryAt(2);
        QVERIFY(gamma.isDir);
        QCOMPARE(gamma.path, QDir(dir.path()).absoluteFilePath(QStringLiteral("gamma")));
        QVERIFY(!gamma.iconName.isEmpty());
        QVERIFY(gamma.dateModified.isValid());

        QVERIFY(!model.entryAt(0).isDir);
        QCOMPARE(model.entryAt(0).mimeType, QStringLiteral("text/plain"));
    }

    void hiddenFilesAreNotListed()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFile(dir.filePath(QStringLiteral("visible.txt"))));
        QVERIFY(writeFile(dir.filePath(QStringLiteral(".hidden"))));

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(dir.path());

        QTRY_COMPARE(model.rowCount(), 1);
        QCOMPARE(model.entryAt(0).name, QStringLiteral("visible.txt"));
    }

    void addingFileUpdates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(writeFile(dir.filePath(QStringLiteral("first.txt"))));

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(dir.path());
        QTRY_COMPARE(model.rowCount(), 1);

        QVERIFY(writeFile(dir.filePath(QStringLiteral("second.txt"))));
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 15000);
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("first.txt"), QStringLiteral("second.txt")}));

        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("first.txt"))));
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 15000);
        QCOMPARE(model.entryAt(0).name, QStringLiteral("second.txt"));
    }

    /// Regression: a deletion arriving in the window just after an addition's
    /// own update completed was dropped, leaving a row for a file that no
    /// longer existed. Reproduced roughly one run in five before the fix, so
    /// this is repeated — a single pass proves nothing about it.
    void deletionImmediatelyAfterAdditionIsNotDropped()
    {
        for (int attempt = 0; attempt < 8; ++attempt) {
            QTemporaryDir dir;
            QVERIFY(dir.isValid());
            QVERIFY(writeFile(dir.filePath(QStringLiteral("keep.txt"))));

            FolderBackend backend;
            StackModel model(&backend);
            model.setPath(dir.path());
            QTRY_COMPARE(model.rowCount(), 1);

            QVERIFY(writeFile(dir.filePath(QStringLiteral("transient.txt"))));
            QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 15000);

            // No settling delay: landing in that window is the whole point.
            QVERIFY(QFile::remove(dir.filePath(QStringLiteral("transient.txt"))));
            QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 15000);
            QCOMPARE(model.entryAt(0).name, QStringLiteral("keep.txt"));
        }
    }

    void deletingDirectoryDegradesGracefully()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString watched = dir.filePath(QStringLiteral("stack"));
        QVERIFY(QDir().mkpath(watched));
        QVERIFY(writeFile(watched + QStringLiteral("/only.txt")));

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(watched);
        QTRY_COMPARE(model.rowCount(), 1);

        QVERIFY(QDir(watched).removeRecursively());

        // The contract is that it reports the failure and empties, not that it
        // survives by luck: a stack showing stale rows for a folder that no
        // longer exists is the bug this guards.
        QTRY_COMPARE_WITH_TIMEOUT(model.status(), int(StackModel::Failed), 15000);
        QCOMPARE(model.rowCount(), 0);
    }

    void missingDirectoryFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(dir.filePath(QStringLiteral("never-existed")));

        QTRY_COMPARE_WITH_TIMEOUT(model.status(), int(StackModel::Failed), 15000);
        QCOMPARE(model.rowCount(), 0);
    }

    void largeDirectoryDoesNotBlock()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (int i = 0; i < 5000; ++i) {
            QVERIFY(writeFile(dir.filePath(QStringLiteral("entry-%1.txt").arg(i, 5, 10, QLatin1Char('0')))));
        }

        FolderBackend backend;
        StackModel model(&backend);

        // The claim being tested is that listing is asynchronous: setPath()
        // returns to the event loop immediately, whatever the directory holds.
        // A synchronous implementation would freeze the dock for the duration,
        // and the wall clock is the only way to tell the difference.
        QElapsedTimer timer;
        timer.start();
        model.setPath(dir.path());
        QVERIFY2(timer.elapsed() < 100, qPrintable(QStringLiteral("setPath blocked for %1 ms").arg(timer.elapsed())));
        QCOMPARE(model.status(), int(StackModel::Loading));

        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 5000, 60000);
        QCOMPARE(model.entryAt(0).name, QStringLiteral("entry-00000.txt"));
        QCOMPARE(model.entryAt(4999).name, QStringLiteral("entry-04999.txt"));
    }

    /// The case §5.2 names: a folder of applications is the grid's headline use,
    /// and it has to be a folder the model can actually list. Skipped where the
    /// directory is not present, since that is a property of the machine.
    void applicationDirectoryLists()
    {
        const QString applications = QStringLiteral("/usr/share/applications");
        if (!QFileInfo(applications).isDir()) {
            QSKIP("no /usr/share/applications on this machine");
        }

        FolderBackend backend;
        StackModel model(&backend);
        model.setPath(applications);

        QTRY_COMPARE_WITH_TIMEOUT(model.status(), int(StackModel::Ready), 30000);
        QVERIFY2(model.rowCount() > 50,
                 qPrintable(QStringLiteral("only %1 entries listed").arg(model.rowCount())));

        // Every row has the two things a grid cell needs to draw.
        for (int row = 0; row < model.rowCount(); ++row) {
            QVERIFY(!model.entryAt(row).name.isEmpty());
            QVERIFY(!model.entryAt(row).iconName.isEmpty());
        }
    }

    void namesSortInHumanOrder()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/stack"));

        backend.addFile(QStringLiteral("img10.png"));
        backend.addFile(QStringLiteral("Img2.png"));
        backend.addFile(QStringLiteral("img1.png"));
        backend.notifyChange();

        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("img1.png"), QStringLiteral("Img2.png"), QStringLiteral("img10.png")}));
    }

    void changesEmitRowSignalsRatherThanAReset()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/stack"));
        backend.addFile(QStringLiteral("a.txt"));
        backend.addFile(QStringLiteral("c.txt"));
        backend.notifyChange();
        QCOMPARE(model.rowCount(), 2);

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

        backend.addFile(QStringLiteral("b.txt"));
        backend.notifyChange();

        // A reset would work, and would also throw away the view's scroll
        // position on every unrelated filesystem event.
        QCOMPARE(reset.count(), 0);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.first().at(1).toInt(), 1);
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("a.txt"), QStringLiteral("b.txt"), QStringLiteral("c.txt")}));

        backend.removeEntry(QStringLiteral("a.txt"));
        backend.notifyChange();
        QCOMPARE(removed.count(), 1);
        QCOMPARE(reset.count(), 0);
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("b.txt"), QStringLiteral("c.txt")}));
    }

    void unchangedListingEmitsNothing()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/stack"));
        backend.addFile(QStringLiteral("a.txt"));
        backend.notifyChange();

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        backend.notifyChange();

        QCOMPARE(inserted.count(), 0);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(changed.count(), 0);
    }

    void changingPathReplacesTheListing()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/one"));
        backend.addFile(QStringLiteral("a.txt"));
        backend.notifyChange();
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy pathSpy(&model, &StackModel::pathChanged);
        model.setPath(QStringLiteral("/two"));

        QCOMPARE(pathSpy.count(), 1);
        QCOMPARE(model.path(), QStringLiteral("/two"));
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.status(), int(StackModel::Loading));

        backend.addFile(QStringLiteral("b.txt"));
        backend.notifyChange();
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("b.txt")}));
        QCOMPARE(model.entryAt(0).path, QStringLiteral("/two/b.txt"));
    }

    void failedListingEmptiesTheRows()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/stack"));
        backend.addFile(QStringLiteral("a.txt"));
        backend.notifyChange();
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy statusSpy(&model, &StackModel::statusChanged);
        backend.setStatus(FolderStatus::Failed);
        backend.notifyChange();

        QCOMPARE(model.status(), int(StackModel::Failed));
        QCOMPARE(statusSpy.count(), 1);
        QCOMPARE(model.rowCount(), 0);
    }

    // --- Navigation -------------------------------------------------------

    void enterFolderDrillsDownAndGoUpReturns()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path();
        QVERIFY(QDir(root).mkdir(QStringLiteral("inner")));
        QVERIFY(writeFile(root + QStringLiteral("/inner/deep.txt")));
        QVERIFY(writeFile(root + QStringLiteral("/shallow.txt")));

        FolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(root);
        QTRY_COMPARE(model.rowCount(), 2);

        QVERIFY(!model.canGoUp());
        // Row 0 is "inner", row 1 is "shallow.txt", by name order.
        QVERIFY(model.entryAt(0).isDir);
        QVERIFY(model.enterFolder(0));

        QTRY_COMPARE(model.rowCount(), 1);
        QCOMPARE(model.entryAt(0).name, QStringLiteral("deep.txt"));
        QVERIFY(model.canGoUp());

        QVERIFY(model.goUp());
        QTRY_COMPARE(model.rowCount(), 2);
        QCOMPARE(model.path(), QDir(root).absolutePath());
        QVERIFY(!model.canGoUp());
    }

    void enterFolderRefusesAFile()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(QStringLiteral("/stack"));
        backend.addFile(QStringLiteral("plain.txt"));
        backend.notifyChange();

        // False is the view's cue to open the file instead, so it has to be
        // false rather than merely harmless.
        QVERIFY(!model.enterFolder(0));
        QCOMPARE(model.path(), QStringLiteral("/stack"));

        QVERIFY(!model.enterFolder(-1));
        QVERIFY(!model.enterFolder(99));
    }

    void goUpStopsAtTheRoot()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(QStringLiteral("/stack/root"));

        // A stack is a view of one folder, not a file manager: it must not be
        // possible to walk out of the folder the tile is for.
        QVERIFY(!model.canGoUp());
        QVERIFY(!model.goUp());
        QCOMPARE(model.path(), QStringLiteral("/stack/root"));

        model.setPath(QStringLiteral("/stack/root/a/b"));
        QVERIFY(model.goUp());
        QCOMPARE(model.path(), QStringLiteral("/stack/root/a"));
        QVERIFY(model.goUp());
        QCOMPARE(model.path(), QStringLiteral("/stack/root"));
        QVERIFY(!model.goUp());
    }

    void resetToRootReturnsFromAnyDepth()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(QStringLiteral("/stack/root"));
        model.setPath(QStringLiteral("/stack/root/a/b/c"));

        model.resetToRoot();
        QCOMPARE(model.path(), QStringLiteral("/stack/root"));
        QVERIFY(!model.canGoUp());
    }

    void trailNamesEveryFolderFromTheRoot()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(QStringLiteral("/stack/Projects"));

        QVariantList trail = model.trail();
        QCOMPARE(trail.size(), 1);
        QCOMPARE(trail.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Projects"));
        QCOMPARE(trail.at(0).toMap().value(QStringLiteral("path")).toString(), QStringLiteral("/stack/Projects"));

        model.setPath(QStringLiteral("/stack/Projects/frappe/src"));
        trail = model.trail();
        QCOMPARE(trail.size(), 3);
        QStringList names;
        for (const QVariant &crumb : trail) {
            names.append(crumb.toMap().value(QStringLiteral("name")).toString());
        }
        QCOMPARE(names,
                 QStringList({QStringLiteral("Projects"), QStringLiteral("frappe"), QStringLiteral("src")}));

        // Every crumb but the last has to be somewhere the breadcrumb can go.
        QCOMPARE(trail.at(1).toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/stack/Projects/frappe"));
    }

    void trailIsEmptyForAPathOutsideTheRoot()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setRootPath(QStringLiteral("/stack/root"));

        // Nothing in the view can produce this, but a breadcrumb that walked to
        // the filesystem root looking for a parent it would never meet would
        // hang rather than show something wrong.
        model.setPath(QStringLiteral("/elsewhere/entirely"));
        QVERIFY(model.trail().isEmpty());
    }

    void rolesReachTheEntry()
    {
        FakeFolderBackend backend;
        StackModel model(&backend);
        model.setPath(QStringLiteral("/stack"));
        backend.addDir(QStringLiteral("Documents"));
        backend.notifyChange();

        const QModelIndex idx = model.index(0, 0);
        QCOMPARE(model.data(idx, StackModel::NameRole).toString(), QStringLiteral("Documents"));
        QCOMPARE(model.data(idx, StackModel::PathRole).toString(), QStringLiteral("/stack/Documents"));
        QCOMPARE(model.data(idx, StackModel::IconNameRole).toString(), QStringLiteral("folder"));
        QVERIFY(model.data(idx, StackModel::IsDirRole).toBool());

        // Names the views bind to. A rename here is a silent breakage in QML.
        const QHash<int, QByteArray> roles = model.roleNames();
        QCOMPARE(roles.value(StackModel::NameRole), QByteArray("name"));
        QCOMPARE(roles.value(StackModel::PathRole), QByteArray("path"));
        QCOMPARE(roles.value(StackModel::IsDirRole), QByteArray("isDir"));
    }
};

QTEST_MAIN(TestStackModel)
#include "test_stackmodel.moc"
