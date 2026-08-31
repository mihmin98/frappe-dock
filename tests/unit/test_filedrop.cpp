#include "app/dockcontroller.h"
#include "core/config/configfacade.h"
#include "core/model/filedrop.h"
#include "core/model/tilemodel.h"
#include "fakes/fakelauncherbackend.h"
#include "fakes/faketaskbackend.h"

#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace frappe;

namespace
{
QUrl fileUrl(const QString &name)
{
    // The files need not exist: the type is read from the name, which is what
    // keeps the decision cheap enough to make while the drag is in the air.
    return QUrl::fromLocalFile(QStringLiteral("/tmp/frappe-test/") + name);
}
}

/// Files dropped on an application tile.
///
/// Two layers, both here: what `evaluateDrop` decides about a payload, and what
/// the controller does with the decision — which call it builds, and that it
/// builds none at all when the answer was no.
class TestFileDrop : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

    /// A launcher holding a PDF reader, an image editor and an application that
    /// declares nothing at all.
    static void populate(FakeLauncherBackend &launcher)
    {
        DesktopEntry reader;
        reader.id = QStringLiteral("reader");
        reader.name = QStringLiteral("Reader");
        reader.mimeTypes = {QStringLiteral("application/pdf")};
        launcher.addEntry(reader);

        DesktopEntry editor;
        editor.id = QStringLiteral("editor");
        editor.name = QStringLiteral("Editor");
        editor.mimeTypes = {QStringLiteral("image/*"), QStringLiteral("text/plain")};
        launcher.addEntry(editor);

        DesktopEntry terminal;
        terminal.id = QStringLiteral("terminal");
        terminal.name = QStringLiteral("Terminal");
        launcher.addEntry(terminal);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    // --- The decision -------------------------------------------------

    void acceptsExactType()
    {
        QVERIFY(acceptsMimeType({QStringLiteral("application/pdf")}, QStringLiteral("application/pdf")));
        QVERIFY(!acceptsMimeType({QStringLiteral("application/pdf")}, QStringLiteral("image/png")));
    }

    void acceptsGroupWildcard()
    {
        const QStringList supported{QStringLiteral("image/*")};
        QVERIFY(acceptsMimeType(supported, QStringLiteral("image/png")));
        QVERIFY(acceptsMimeType(supported, QStringLiteral("image/svg+xml")));
        QVERIFY(!acceptsMimeType(supported, QStringLiteral("application/pdf")));
    }

    void acceptsCatchAll()
    {
        QVERIFY(acceptsMimeType({QStringLiteral("all/all")}, QStringLiteral("application/pdf")));
        QVERIFY(acceptsMimeType({QStringLiteral("all/allfiles")}, QStringLiteral("image/png")));
    }

    /// A shell script *is* plain text, and an editor that declares text/plain
    /// should not have to enumerate everything derived from it.
    void acceptsInheritedType()
    {
        QVERIFY(acceptsMimeType({QStringLiteral("text/plain")}, QStringLiteral("application/x-shellscript")));
        QVERIFY(!acceptsMimeType({QStringLiteral("text/plain")}, QStringLiteral("application/pdf")));
    }

    void declaringNothingAcceptsNothing()
    {
        QVERIFY(!acceptsMimeType({}, QStringLiteral("text/plain")));

        const DropVerdict verdict = evaluateDrop({}, {fileUrl(QStringLiteral("notes.txt"))});
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, DropRejection::UnsupportedType);
    }

    void emptyPayloadIsRejectedAsSuch()
    {
        const DropVerdict verdict = evaluateDrop({QStringLiteral("all/all")}, {});
        QVERIFY(!verdict.accepted());
        // Distinct from "cannot open this", because it is a different sentence.
        QCOMPARE(verdict.rejection, DropRejection::NoFiles);
    }

    /// Every file, not any file: opening two of four and saying nothing about
    /// the rest is the worst answer available.
    void oneUnsupportedFileRejectsTheWholeDrop()
    {
        const QList<QUrl> files{fileUrl(QStringLiteral("a.pdf")),
                                fileUrl(QStringLiteral("b.png"))};

        const DropVerdict verdict = evaluateDrop({QStringLiteral("application/pdf")}, files);
        QVERIFY(!verdict.accepted());
        QCOMPARE(verdict.rejection, DropRejection::UnsupportedType);
        // The reason names the type that stopped it, in words the message can
        // use as-is.
        QVERIFY(!verdict.detail.isEmpty());
    }

    // --- What the controller does with it -----------------------------

    void dispatchBuildsRightCallForOneFile()
    {
        FakeLauncherBackend launcher;
        populate(launcher);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("reader")});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        const QUrl file = fileUrl(QStringLiteral("paper.pdf"));
        QVERIFY(controller.openDroppedFiles(QStringLiteral("reader"), {file}));

        QCOMPARE(launcher.opened(), QStringList({QStringLiteral("reader:") + file.toString()}));
    }

    void dispatchBuildsRightCallForSeveralFiles()
    {
        FakeLauncherBackend launcher;
        populate(launcher);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("editor")});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        const QList<QUrl> files{fileUrl(QStringLiteral("one.png")),
                                fileUrl(QStringLiteral("two.jpg")),
                                fileUrl(QStringLiteral("three.txt"))};
        QVERIFY(controller.openDroppedFiles(QStringLiteral("editor"), files));

        // One call carrying all three, in the order they were dropped — not
        // three calls, which would be three windows.
        QStringList expected;
        for (const QUrl &file : files) {
            expected.append(QStringLiteral("editor:") + file.toString());
        }
        QCOMPARE(launcher.opened(), expected);
    }

    void unsupportedTypeRejected()
    {
        FakeLauncherBackend launcher;
        populate(launcher);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("reader")});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        const QVariantMap verdict =
            controller.evaluateDrop(QStringLiteral("reader"), {fileUrl(QStringLiteral("photo.png"))});
        QVERIFY(!verdict.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(verdict.value(QStringLiteral("rejection")).toInt(), int(DropRejection::UnsupportedType));
        QCOMPARE(verdict.value(QStringLiteral("appName")).toString(), QStringLiteral("Reader"));
        QVERIFY(!verdict.value(QStringLiteral("detail")).toString().isEmpty());

        // And nothing is opened even if the drop is carried out anyway: the
        // check is not the view's to skip.
        QVERIFY(!controller.openDroppedFiles(QStringLiteral("reader"), {fileUrl(QStringLiteral("photo.png"))}));
        QVERIFY(launcher.opened().isEmpty());
    }

    void unknownTileOpensNothing()
    {
        FakeLauncherBackend launcher;
        populate(launcher);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("reader")});
        TileModel model(&config, &launcher);
        DockController controller(&model, &launcher);

        QVERIFY(!controller.openDroppedFiles(QStringLiteral("no-such-app"),
                                             {fileUrl(QStringLiteral("paper.pdf"))}));
        QVERIFY(launcher.opened().isEmpty());
    }

    /// A minimized-window tile stands for a window, not an application. There is
    /// nothing to open a file with.
    void nonApplicationTileAcceptsNothing()
    {
        FakeLauncherBackend launcher;
        populate(launcher);

        ConfigFacade config(configPath());
        config.setPinnedEntries({QStringLiteral("reader")});

        FakeTaskBackend tasks;
        WindowInfo window;
        window.windowId = QStringLiteral("w1");
        window.appId = QStringLiteral("reader");
        window.isMinimized = true;
        tasks.addWindow(window);

        TileModel model(&config, &launcher, &tasks);
        DockController controller(&model, &launcher);

        // The last row is the minimized window's tile, behind the separator.
        const int last = model.rowCount() - 1;
        QCOMPARE(model.data(model.index(last, 0), TileModel::KindRole).toInt(),
                 int(TileKind::MinimizedWindow));

        const QString tileId = model.data(model.index(last, 0), TileModel::IdRole).toString();
        const QVariantMap verdict =
            controller.evaluateDrop(tileId, {fileUrl(QStringLiteral("paper.pdf"))});
        QVERIFY(!verdict.value(QStringLiteral("accepted")).toBool());

        QVERIFY(!controller.openDroppedFiles(tileId, {fileUrl(QStringLiteral("paper.pdf"))}));
        QVERIFY(launcher.opened().isEmpty());
    }
};

QTEST_MAIN(TestFileDrop)
#include "test_filedrop.moc"
